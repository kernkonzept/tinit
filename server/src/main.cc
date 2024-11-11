/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <ctype.h>
#include <l4/cxx/string>
#include <l4/cxx/slist>
#include <l4/re/env>
#include <l4/sigma0/sigma0.h>
#include <l4/sys/cxx/ipc_server_loop>
#include <l4/sys/debugger.h>
#include <l4/sys/scheduler>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <l4/sys/kip>

#include "app_task.h"
#include "boot_fs.h"
#include "cap_alloc.h"
#include "debug.h"
#include "globals.h"
#include "page_alloc.h"
#include "stubs.h"
#include "vm_irq.h"
#include "vm_task.h"

static L4Re::Env my_env;
static L4::Server<L4::Ipc_svr::Default_loop_hooks> server;
static My_registry registry(&server);

static void find_kip()
{
#ifdef CONFIG_TINIT_RUN_ROOTTASK
  l4_global_kip = l4sigma0_map_kip(Sigma0_cap, 0, L4_WHOLE_ADDRESS_SPACE);
#endif
  if (!l4_global_kip)
    Fatal().panic("no KIP\n");

  Dbg().printf("KIP @%p\n", l4_kip());
}

static void find_memory()
{
  Page_alloc::init();
  Info().printf("found %ld KByte free memory\n", Page_alloc::avail() / 1024);
}

static void init_env()
{
  // setup log capability in the global env, so that the libc backend can use
  // L4::Env::env()->log() to send logoutput to
  l4re_global_env = reinterpret_cast<l4re_env_t*>(&my_env);
  my_env.main_thread(L4_BASE_THREAD_CAP);
  my_env.factory(L4_BASE_FACTORY_CAP);
  my_env.log(L4_BASE_LOG_CAP);
  my_env.scheduler(L4_BASE_SCHEDULER_CAP);
}

static __attribute__((used, section(".preinit_array")))
   void (* pre_init_env)() = &init_env;

#ifdef CONFIG_BID_PIE
static inline unsigned long elf_machine_dynamic()
{
  extern const unsigned long _GLOBAL_OFFSET_TABLE_[] __attribute__((visibility ("hidden")));
  return _GLOBAL_OFFSET_TABLE_[0];
}

static inline unsigned long elf_machine_load_address()
{
  extern char _DYNAMIC[] __attribute__((visibility ("hidden")));
  return (unsigned long)&_DYNAMIC - elf_machine_dynamic ();
}
#else
static inline unsigned long elf_machine_load_address()
{ return 0; }
#endif


static char *num2hex(char *s, unsigned long n)
{
  static char hex[] = "0123456789abcdef";

  // determine number of digits
  unsigned d = 8*sizeof(n) - __builtin_clzl(n | 1) - 1;
  d = (d >> 2) + 1;

  for (int i = d-1; i >= 0; i--)
    *s++ = hex[(n >> i*4) & 0x0fU];

  return s;
}

struct Channel : public cxx::S_list_item
{
  cxx::String const name;
  L4::Cap<void> cap;

  Channel() = default;
  Channel(cxx::String const &name, L4::Cap<void> cap)
  : S_list_item(), name(name), cap(cap)
  {}
};

class Inittab_parser
{
  enum State { Init, Start, Def_vm, Skip_start, Skip_vm };

public:
  void feed(cxx::String line)
  {
    bool ok = false;
    _line++;

    auto kw = pop_arg(&line);
    if (kw.empty())
      return;

    switch (_state)
      {
        case Init:
          if (kw == "start")
            ok = parse_start(line);
          else if (kw == "pool")
            ok = parse_pool(line);
          break;

        case Start:
          if (kw == "defvm")
            {
              ok = parse_defvm(line);
              _state = Def_vm;
            }
          else if (kw == "end")
            {
              ok = end_start();
              _state = Init;
            }
          else if (kw == "arg")
            ok = parse_arg(line);
          else if (kw == "cap")
            ok = parse_cap(line);
          else if (kw == "chan")
            ok = parse_chan(line);
          else if (kw == "irq")
            ok = parse_irq(line);
          else if (kw == "mmio")
            ok = parse_mmio(line);
          else if (kw == "shm")
            ok = parse_shm(line);
          break;

        case Def_vm:
          if (kw == "ram")
            ok = parse_vm_ram(line);
          else if (kw == "mmio")
            ok = parse_vm_mmio(line);
          else if (kw == "shm")
            ok = parse_vm_shm(line);
          else if (kw == "irq")
            ok = parse_vm_irq(line);
          else if (kw == "irq-priorities")
            ok = parse_vm_irq_priorities(line);
          else if (kw == "load")
            ok = parse_vm_load(line);
          else if (kw == "reload")
            ok = parse_vm_reload(line);
          else if (kw == "entry")
            ok = parse_vm_entry(line);
          else if (kw == "end")
            {
              ok = end_vm();
              _state = Start;
            }
          break;

        case Skip_start:
          if (kw == "end")
            _state = Init;
          else if (kw == "defvm")
            _state = Skip_vm;
          ok = true;
          break;

        case Skip_vm:
          ok = true;
          if (kw == "ram")
            ok = parse_vm_ram(line, true);
          else if (kw == "shm")
            ok = parse_vm_shm(line, true);
          else if (kw == "end")
            _state = Skip_start;
          break;
      }

    if (!ok)
      {
        Fatal().printf("Offending line: %u: %.*s %.*s\n", _line,
                       kw.len(), kw.start(), line.len(), line.start());
        Fatal().panic("Invalid inittab\n");
      }
  }

  ~Inittab_parser()
  {
    // TODO: free channel caps
  }

private:
  static cxx::String pop_arg(cxx::String *line)
  {
    // skip whitespace
    int s = 0;
    while (s < line->len() && isspace((*line)[s]))
      s++;

    // until first whitespace
    int e = s;
    while (e < line->len() && !isspace((*line)[e]))
      e++;

    cxx::String ret = line->substr(s, e-s);
    *line = line->substr(e);
    return ret;
  }

  static bool string_to_ul(cxx::String const &s, unsigned long *ret)
  {
    int conv;
    if (s.starts_with("0x"))
      conv = s.substr(2).from_hex(ret);
    else
      conv = s.from_dec(ret);

    return conv > 0;
  }

  bool parse_start(cxx::String line)
  {
    auto prog = pop_arg(&line);
    unsigned long utcb_order = 0;
    l4_addr_t reloc = 0;
    unsigned long node = 0;
    unsigned long prio = 0xfe;

    cxx::String arg;
    while (arg = pop_arg(&line), !arg.empty())
      {
        bool ok = false;

        if (arg.starts_with("utcb:"))
          ok = string_to_ul(arg.substr(5), &utcb_order);
        else if (arg.starts_with("reloc:"))
          ok = string_to_ul(arg.substr(6), &reloc);
        else if (arg.starts_with("node:"))
          ok = string_to_ul(arg.substr(5), &node);
        else if (arg.starts_with("prio:"))
          ok = string_to_ul(arg.substr(5), &prio);

        if (!ok)
          return false;
      }

    if (node != l4_kip()->node)
      {
        Dbg().printf("skip: fork '%.*s'\n", prog.len(), prog.start());
        if (!App_task::reserve_ram(prog, reloc, node))
          Err().printf("Start '%.*s' will fail on node %lu! Insfficient resources.\n",
                       prog.len(), prog.start(), node);
        _state = Skip_start;
        return true;
      }

    Dbg().printf("start: fork '%.*s', prio:%lu, utcb:%lu, reloc:0x%lx\n", prog.len(),
                 prog.start(), prio, utcb_order, reloc);
    _app = new App_task(&registry, prog, prio, utcb_order, reloc);
    _app->cap_log(L4Re::Env::env()->log(), L4_CAP_FPAGE_RW)
         .cap_factory(L4Re::Env::env()->factory(), L4_CAP_FPAGE_RWS);
    _state = Start;
    _irq = 1;
    _thread = 0;
    return true;
  }

  bool parse_pool(cxx::String line)
  {
    l4_addr_t base, size;
    unsigned long nodes = ~0UL;

    if (!string_to_ul(pop_arg(&line), &base)
        || !string_to_ul(pop_arg(&line), &size))
      return false;

    cxx::String arg;
    while (arg = pop_arg(&line), !arg.empty())
      {
        bool ok = false;

        if (arg.starts_with("nodes:"))
          ok = string_to_ul(arg.substr(6), &nodes);

        if (!ok)
          return false;
      }
    Dbg().printf("pool 0x%lx/0x%lx, nodes 0x%lx\n", base, size, nodes);
    Page_alloc::add_pool(base, size, nodes);

    return true;
  }

  bool parse_arg(cxx::String line)
  {
    auto arg = pop_arg(&line);
    Dbg().printf("  arg '%.*s'\n", arg.len(), arg.start());
    _app->arg(arg);
    return true;
  }

  unsigned parse_rights(cxx::String const &rights_str)
  {
    unsigned rights = L4_CAP_FPAGE_R;

    for (char const *r = rights_str.start(); r != rights_str.end(); r++)
      {
        switch (*r)
          {
          case 'r':
          case 'R': rights |= L4_CAP_FPAGE_R; break;
          case 'w':
          case 'W': rights |= L4_CAP_FPAGE_W; break;
          case 's':
          case 'S': rights |= L4_CAP_FPAGE_S; break;
          case 'd':
          case 'D': rights |= L4_CAP_FPAGE_D; break;
          case 'n':
          case 'N': rights |= L4_FPAGE_C_NO_REF_CNT; break;
          case 'c':
          case 'C': rights |= L4_FPAGE_C_OBJ_RIGHT1; break;
          }
      }

    return rights;
  }

  bool parse_cap(cxx::String line)
  {
    cxx::String capname = pop_arg(&line);
    unsigned rights = parse_rights(pop_arg(&line));

    if (capname == "sched")
      _app->cap_scheduler(L4_BASE_SCHEDULER_CAP, rights);
    else
      return false;

    return true;
  }

  L4::Cap<void> get_or_create_cap(cxx::String const &name)
  {
    for (auto *i : _channels)
      {
        if (i->name == name)
          return i->cap;
      }

    auto cap = Util::cap_alloc.alloc<void>();
    L4::Cap<L4::Factory> factory(L4_BASE_FACTORY_CAP);
    if (l4_error(factory->create_gate(cap, L4::Cap<L4::Thread>(), 0)) < 0)
      Fatal().panic("Cannot create gate\n");

    _channels.add(new Channel(name, cap));

    return cap;
  }

  bool parse_chan(cxx::String line)
  {
    cxx::String capname = pop_arg(&line);
    unsigned rights = parse_rights(pop_arg(&line));

    if (capname.empty())
      return false;

    L4::Cap<void> cap = get_or_create_cap(capname);

    _app->cap(capname, cap, rights);
    return true;
  }

  bool parse_irq(cxx::String line)
  {
    unsigned long id;
    if (!string_to_ul(pop_arg(&line), &id))
      return false;
    cxx::String capname = pop_arg(&line);
    if (capname.empty())
      return false;

    Vm_irq irq(id);

    Dbg().printf("  irq %lu %.*s\n", id, capname.len(), capname.start());
    _app->cap(capname, irq.cap(), L4_CAP_FPAGE_RWSD);

    return true;
  }

  bool parse_mmio(cxx::String line)
  {
    l4_addr_t base, size;
    if (!string_to_ul(pop_arg(&line), &base)
        || !string_to_ul(pop_arg(&line), &size))
      return false;

    Dbg().printf("  mmio 0x%lx/0x%lx\n", base, size);
    _app->map_mmio(base, size);
    return true;
  }

  bool parse_shm(cxx::String line)
  {
    l4_addr_t base, size;
    if (!string_to_ul(pop_arg(&line), &base)
        || !string_to_ul(pop_arg(&line), &size))
      return false;

    Dbg().printf("  shm 0x%lx/0x%lx\n", base, size);
    _app->map_shm(base, size);
    return true;
  }

  bool end_start()
  {
    _app->start();
    _app = nullptr;
    return true;
  }

  bool parse_defvm(cxx::String line)
  {
    auto name = pop_arg(&line);
    unsigned long prio;
    unsigned long asid = ~0UL;

    if (!string_to_ul(pop_arg(&line), &prio))
      return false;

    cxx::String arg;
    while (arg = pop_arg(&line), !arg.empty())
      {
        bool ok = false;

        if (arg.starts_with("asid:"))
          ok = string_to_ul(arg.substr(5), &asid);

        if (!ok)
          return false;
      }

    char vm_arg_buf[32] = "V";
    memcpy(&vm_arg_buf[1], name.start(), name.len());
    char *vm_arg_end = &vm_arg_buf[name.len() + 1];

    if (_thread != 0)
      {
        // create a new thread
        auto thread = Util::cap_alloc.alloc<L4::Thread>();
        if (l4_error(L4Re::Env::env()->factory()->create(thread)) < 0)
          Fatal().panic("create_thread failed\n");

        // Schedule already with chosen priority. Will only run after tvmm
        // has called ex_regs().
        l4_sched_param_t sched_param = l4_sched_param(prio);
        L4::Cap<L4::Scheduler> s(L4_BASE_SCHEDULER_CAP);
        s->run_thread(thread, sched_param);

        // map to tvmm task
        char capname_buf[16] = "t";
        char *capname_end = num2hex(&capname_buf[1], _thread);
        cxx::String capname(capname_buf, capname_end);
        _app->cap(capname, thread, L4_CAP_FPAGE_RWSD);

        // pass with VM definition
        *vm_arg_end++ = ':';
        memcpy(vm_arg_end, capname.start(), capname.len());
        vm_arg_end += capname.len();
      }
    else
      {
        // first VM reuses main thread of tvmm
        _app->set_priority(prio);
      }

    _thread++;


    cxx::String vm_arg(vm_arg_buf, vm_arg_end);

    Dbg().printf("  defvm '%.*s', prio:%lu\n", name.len(), name.start(), prio);

    _vm = new Vm_task(name);
    if (asid != ~0UL)
      _vm->set_asid(asid);
    // Omit W-right so that the vmm is unable to map more resources to the
    // guest.
    _app->cap(name, _vm->cap(), L4_CAP_FPAGE_RSD);
    _app->arg(vm_arg);
    return true;
  }

  bool parse_vm_ram(cxx::String line, bool skip = false)
  {
    l4_addr_t base, size;
    l4_addr_t off = 0;

    if (!string_to_ul(pop_arg(&line), &base)
        || !string_to_ul(pop_arg(&line), &size))
      return false;

    cxx::String arg;
    while (arg = pop_arg(&line), !arg.empty())
      {
        bool ok = false;

        if (arg.starts_with("off:"))
          ok = string_to_ul(arg.substr(4), &off);

        if (!ok)
          return false;
      }

    if (skip)
      {
        size = l4_round_page(size);
        if (!Page_alloc::reserve_ram(base + off, size))
          Err().printf("VM RAM [%8lx - %8lx] unavailable on other node!\n",
                       base, base + size - 1U);
      }
    else
      {
        Dbg().printf("    ram 0x%lx/0x%lx, load offset 0x%lx\n", base, size, off);
        _vm->map_ram(base, size, off);
        _app->map(base, size);
        if (off)
          _app->map(base + off, size);
      }

    return true;
  }

  bool parse_vm_mmio(cxx::String line)
  {
    l4_addr_t base, size;
    if (!string_to_ul(pop_arg(&line), &base)
        || !string_to_ul(pop_arg(&line), &size))
      return false;

    Dbg().printf("    mmio 0x%lx/0x%lx\n", base, size);
    _vm->map_mmio(base, size);
    return true;
  }

  bool parse_vm_shm(cxx::String line, bool skip = false)
  {
    l4_addr_t base, size;
    if (!string_to_ul(pop_arg(&line), &base)
        || !string_to_ul(pop_arg(&line), &size))
      return false;

    if (skip)
      {
        size = l4_round_page(size);
        if (!Page_alloc::share_ram(base, size))
          Err().printf("VM SHM [%8lx - %8lx] unavailable on other node!\n",
                       base, base + size - 1U);
      }
    else
      {
        Dbg().printf("    shm 0x%lx/0x%lx\n", base, size);
        _vm->map_shm(base, size);
      }

    return true;
  }

  bool parse_vm_irq(cxx::String line)
  {
    unsigned long id, src_id;

    if (!string_to_ul(pop_arg(&line), &id))
      return false;
    if (!string_to_ul(pop_arg(&line), &src_id))
      src_id = id;

    Vm_irq irq(src_id);

    unsigned num = _irq++;
    char capname_buf[16] = "i", arg_buf[24] = "Ii";

    char *capname_end = num2hex(&capname_buf[1], num);
    cxx::String capname(capname_buf, capname_end);

    memcpy(&arg_buf[2], &capname_buf[1], capname.len() - 1);
    char *arg_end = &arg_buf[capname.len() + 1];
    *arg_end++ = ':';
    *arg_end++ = '0';
    *arg_end++ = 'x';
    arg_end = num2hex(arg_end, id);
    cxx::String arg(arg_buf, arg_end);

    Dbg().printf("    irq %lu %lu\n", id, src_id);
    _app->cap(capname, irq.cap(), L4_CAP_FPAGE_RWSD).arg(arg);

    return true;
  }

  bool parse_vm_irq_priorities(cxx::String line)
  {
    unsigned long lower;
    unsigned long upper;
    if (!string_to_ul(pop_arg(&line), &lower)
        || !string_to_ul(pop_arg(&line), &upper))
      return false;

    char prio_buf[16] = "P0x";
    char *prio_end = num2hex(&prio_buf[3], lower);
    *prio_end++ = ':';
    *prio_end++ = '0';
    *prio_end++ = 'x';
    prio_end = num2hex(prio_end, upper);

    cxx::String prio(prio_buf, prio_end);
    _app->arg(prio);
    return true;
  }

  bool parse_vm_load(cxx::String line)
  {
    auto name = pop_arg(&line);
    l4_addr_t entry = 0;

    _vm->load(name, &entry);
    Dbg().printf("    load '%.*s' entry:0x%lx\n", name.len(), name.start(),
                  entry);

    char entry_buf[16] = "E0x";
    char *entry_end = num2hex(&entry_buf[3], entry);
    _app->arg(cxx::String(entry_buf, entry_end));
    return true;
  }

  bool parse_vm_reload(cxx::String line)
  {
    auto name = pop_arg(&line);

    l4_size_t size = 0;
    char const *file = Boot_fs::find(name, &size);
    if (!file)
      {
        Err().printf("File not found: %.*s\n", name.len(), name.start());
        return false;
      }

    Dbg().printf("    reload '%.*s' 0x%p\n", name.len(), name.start(), file);

    _app->map(reinterpret_cast<l4_addr_t>(file), size);

    char arg_buf[16] = "L0x";
    char *arg_end = num2hex(&arg_buf[3], reinterpret_cast<l4_addr_t>(file));
    _app->arg(cxx::String(arg_buf, arg_end));
    return true;
  }

  bool parse_vm_entry(cxx::String line)
  {
    l4_addr_t entry;
    if (!string_to_ul(pop_arg(&line), &entry))
      return false;
    Dbg().printf("    entry:0x%lx\n", entry);
    char entry_buf[16] = "E0x";
    char *entry_end = num2hex(&entry_buf[3], entry);
    _app->arg(cxx::String(entry_buf, entry_end));
    return true;
  }

  bool end_vm()
  {
    _vm = nullptr;
    return true;
  }

  App_task *_app = nullptr;
  Vm_task *_vm = nullptr;
  State _state = Init;
  unsigned _line = 0;
  unsigned _irq, _thread;
  cxx::S_list<Channel> _channels;
};

static void parse_inittab()
{
  l4_size_t size = 0;
  char const *inittab_str = Boot_fs::find("inittab", &size);
  if (!inittab_str)
    {
      Err().printf("No inittab!\n");
      return;
    }

  Inittab_parser parser;
  cxx::String inittab(inittab_str, size);
  while (!inittab.empty())
    {
      cxx::String line(inittab.start(), inittab.find('\n'));
      inittab = cxx::String(line.end() == inittab.end() ? inittab.end()
                                                        : line.end() + 1,
                            inittab.end());

      line = cxx::String(line.start(), line.find("#"));
      parser.feed(line);
    }
}

// Global variable with external linkage so that a debugger can read it.
unsigned long used_ram;

int main(int, char**)
{
  Info().printf("Starting...\n");

  L4Re::Env::env()->scheduler()
    ->run_thread(L4::Cap<L4::Thread>(L4_BASE_THREAD_CAP), l4_sched_param(0xff));

  find_kip();
  find_memory();

  Info().printf("Node: %u\n", l4_kip()->node);

#ifdef CONFIG_TINIT_RUN_ROOTTASK
  if (l4_kip()->node != 0)
    Fatal().panic("Cannot run as roottask on AMP!\n");
#endif

#ifndef NDEBUG
  l4_debugger_set_object_name(L4_BASE_TASK_CAP,   "tinit");
  l4_debugger_set_object_name(L4_BASE_THREAD_CAP, "tinit");
#ifdef CONFIG_TINIT_RUN_ROOTTASK
  l4_debugger_set_object_name(L4_BASE_PAGER_CAP,  "tinit->s0");
#endif
  l4_debugger_add_image_info(L4_BASE_TASK_CAP, elf_machine_load_address(),
                             "tinit");
#endif

  parse_inittab();
  Page_alloc::dump();
  Info().printf("Heap: %lu/%lu bytes free.\n", heap_avail(), heap_size());

  unsigned long used_bootstrap = 0;
  unsigned long used_kernel = 0;
  unsigned long used_tinit = 0;
  unsigned long used_apps = App_task::used_ram();

  used_ram = App_task::used_ram();
  for (auto const &md: L4::Kip::Mem_desc::all(l4_kip()))
    {
      if (md.is_virtual())
        continue;

      L4::Kip::Mem_desc::Mem_type type = md.type();

      // Fully account partially reserved pages. Note that md.end() address is
      // inclusive!
      unsigned long start = l4_trunc_page(md.start());
      unsigned long end = l4_round_page(md.end() + 1) - 1;
      unsigned long size = end - start + 1;

      switch (type)
        {
        case L4::Kip::Mem_desc::Reserved:
          used_ram += size;
          used_kernel += size;
          break;
        case L4::Kip::Mem_desc::Dedicated:
          used_ram += size;
          used_tinit += size;
          break;
        case L4::Kip::Mem_desc::Bootloader:
          used_ram += size;
          used_bootstrap += size;
          break;
        default:
          continue;
        }
    }

  Info().printf("System RAM usage: %lu KiB\n", used_ram / 1024);
  Info().printf("  Bootstrap: %8lu KiB\n", used_bootstrap / 1024);
  Info().printf("  Kernel:    %8lu KiB\n", used_kernel / 1024);
  Info().printf("  Userspace: %8lu KiB\n", (used_tinit + used_apps) / 1024);
  Info().printf("    tinit:   %8lu KiB\n", used_tinit / 1024);
  Info().printf("    Apps:    %8lu KiB\n", used_apps / 1024);

  App_task::check_tasks_ready();

  server.loop_noexc(&registry);

  return 0;
}
