/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <assert.h>
#include <l4/re/elf_aux.h>
#include <l4/re/env>
#include <string.h>

#include "app_task.h"
#include "boot_fs.h"
#include "cap_alloc.h"
#include "debug.h"
#include "ex_regs_flags"
#include "page_alloc.h"

App_task::Stack::Stack()
: _bottom(0), _top(0)
{}

void App_task::Stack::init(void *bottom, l4_size_t size)
{
  _bottom = (char *)bottom;
  _top = (char *)bottom + size;
  _front = _bottom;
  _back = _top;

  add(l4_umword_t(0)); // argc
}

void App_task::Stack::add_arg(cxx::String const &arg)
{
  argc() += 1;

  push_object("", 1);
  char *a = push_object(arg.start(), arg.len());
  add(relocate(a));
}

l4_addr_t App_task::Stack::pack()
{
  l4_size_t len = _front - _bottom;
  char *dest = (char *)(((l4_umword_t)_back - len) & ~15UL);
  memmove(dest, _bottom, len);
  return (l4_addr_t)dest;
}

char *App_task::Stack::add_object(void const *src, unsigned long size)
{
  char *ret = _front;
  memcpy(_front, src, size);
  _front += size;
  return ret;
}

char *App_task::Stack::push_object(void const *src, unsigned long size)
{
  _back -= size;
  return (char *)memcpy(_back, src, size);
}


void App_task::push_named_cap(cxx::String const &name, L4::Cap<void> cap,
                              unsigned rights)
{
  l4_cap_idx_t idx = _first_free_cap;
  _first_free_cap += L4_CAP_OFFSET;

  if (l4_error(_task->map(L4Re::This_task, cap.fpage(rights | L4_CAP_FPAGE_RO),
                    L4::Cap<void>(idx).snd_base() | (rights & 0xf0U))) < 0)
    Fatal().abort("map cap failed\n");

  l4re_env_cap_entry_t e;
  e.cap = idx;
  e.flags = 0;
  memset(&e.name, 0, sizeof(e.name));
  memcpy(&e.name, name.start(),
         (unsigned)name.len() > sizeof(e.name) ? sizeof(e.name) : name.len());

  *_named_caps-- = e;
}

void App_task::push_known_cap(Known_caps type, L4::Cap<void> cap,
                              unsigned rights)
{
  _known_caps[type] = cap.fpage(rights | L4_CAP_FPAGE_RO);
}

bool App_task::dynamic_reloc(Loader::Elf_binary &elf, l4_addr_t *reloc,
                             unsigned node)
{
#ifdef CONFIG_TINIT_DYNAMIC_LOADER
  l4_addr_t task_min = ~0UL, task_max = 0, task_align = 1;
  elf.iterate_phdr([&task_min, &task_max, &task_align](Loader::Elf_phdr ph, void const *) {
    if (ph.type() != PT_LOAD)
      return;
    if (!ph.memsz())
      return;

    l4_addr_t start = ph.paddr();
    if (start < task_min)
      task_min = start;

    l4_addr_t end = start + l4_round_page(ph.memsz());
    if (end > task_max)
      task_max = end;

    if (ph.align() > task_align)
      task_align = ph.align();
  });

  l4_addr_t base = (l4_addr_t)Page_alloc::alloc_ram(task_max - task_min,
                                                    task_align, node);
  if (!base)
    {
      Fatal().printf("Could not allocate %lu bytes with alignment 0x%lx\n",
                     task_max - task_min, task_align);
      Page_alloc::dump();
      return false;
    }

  *reloc = base - task_min;
#else
  (void)elf;
  (void)reloc;
  (void)node;
#endif
  return true;
}

App_task::App_task(My_registry *registry, cxx::String const &arg0,
                   unsigned prio, unsigned utcb_pages_order, l4_addr_t reloc)
: Loader::Child_task(Util::cap_alloc.alloc<L4::Task>()),
  _thread(Util::cap_alloc.alloc<L4::Thread>()),
  _utcb(l4_fpage(0, L4_PAGESHIFT + utcb_pages_order, 0)),
  _first_free_cap(Caps::First_free << L4_CAP_SHIFT),
  _phdrs(0), _ex_regs_flags(Default_ex_regs_flags), _prio(prio)
{
  for (auto &i : _known_caps)
    i = l4_fpage_invalid();

  registry->register_obj(this);

  auto *e = L4Re::Env::env();

  // Creating the task on no-MMU systems will adjust the utcb flexpage to the
  // allocated address.
  auto ret = e->factory()->create_task(_task, _utcb);
  if (l4_error(ret) < 0)
    Fatal().abort("create_task failed\n");

  ret = e->factory()->create(_thread);
  if (l4_error(ret) < 0)
    Fatal().abort("create_thread failed\n");

  l4_debugger_set_object_name(_task.cap(), arg0);
  l4_debugger_set_object_name(_thread.cap(), arg0);

  void const *f = Boot_fs::find(arg0);
  if (!f)
    Fatal().abort("App_task: file missing\n");

  Loader::Elf_binary elf(f);
  if (!elf.is_valid())
    Fatal().abort("App_task: invalid ELF file\n");

  if (!dynamic_reloc(elf, &reloc, l4_kip()->node))
    Fatal().abort("Loader OOM\n");

  bool neg = reloc >= ~(l4_addr_t)0 / 2U;
  Info().printf("Loading '%.*s', offset %c0x%lx\n", arg0.len(), arg0.start(),
                neg ? '-' : '+', neg ? (~reloc + 1U) : reloc);

  l4_debugger_add_image_info(_task.cap(), reloc, arg0);

  _num_phdrs = elf.num_phdrs();
  elf.iterate_phdr([this, reloc](Loader::Elf_phdr ph, void const *f) {
    char *src = (char *)f + ph.offset();
    if (ph.type() == PT_LOAD)
      {
        l4_addr_t dest = ph.paddr() + reloc;
        l4_addr_t size = l4_round_page(ph.memsz());
        if (!size)
          return;

        unsigned char flags = L4_FPAGE_RO;
        if (ph.flags() & PF_W)
          flags |= L4_FPAGE_W;
        if (ph.flags() & PF_X)
          flags |= L4_FPAGE_X;

        if (!(flags & L4_FPAGE_W) && ph.memsz() <= ph.filesz()
            && src == (void *)dest)
          {
            // XIP
            Dbg().printf("Map ELF binary @0x%lx/0x%lx\n", dest, size);
          }
        else
          {
#ifndef CONFIG_TINIT_DYNAMIC_LOADER
            if (!Page_alloc::reserve_ram(dest, size))
              {
                Fatal fatal;
                fatal.printf("Failed to load ELF kernel binary. "
                             "Region [0x%lx/0x%lx] not available.\n",
                             ph.paddr(), size);
                fatal.abort("Cannot load app section\n");
              }
#endif
            Dbg().printf("Copy in ELF binary section @0x%lx/0x%lx from 0x%lx/0x%lx\n",
                         dest, size, ph.offset(), ph.filesz());

            memcpy((void *)dest, src, ph.filesz());
            memset((void *)(dest + ph.filesz()), 0, size - ph.filesz());
          }

        map_to_task(dest, dest, size, flags);
      }
    else if (ph.type() == PT_PHDR)
      _phdrs = ph.paddr() + reloc;
    else if (ph.type() == PT_L4_AUX)
      {
        l4re_elf_aux_t const *e = reinterpret_cast<l4re_elf_aux_t const *>(src);
        l4re_elf_aux_t const *end =
          reinterpret_cast<l4re_elf_aux_t const *>(src + ph.filesz());
        while (e < end && e->type)
          {
            if (e->type == L4RE_ELF_AUX_T_EX_REGS_FLAGS)
              {
                l4re_elf_aux_mword_t const *v = (l4re_elf_aux_mword_t const *)e;
                _ex_regs_flags = v->value;
              }
            e = (l4re_elf_aux_t const *)((char const *)e + e->length);
          }
      }
    else if (ph.type() == PT_L4_STACK)
      {
        l4_addr_t dest = ph.paddr() + reloc;
        l4_addr_t size = l4_round_page(ph.memsz());
        if (size > 0)
          _stack.init((void *)dest, size);
      }
  });
  _entry = elf.entry() + reloc;

  /* First only argc, argv and the argument strings are on the stack.
   * Somewhere in the middle the named capailities are temporarily aggregated:
   *
   *   <bottom>[argc][argv] <gap> [caps] <gap> [args]<top>
   *
   * Later the remaining arguments are added, the rest is added and the stack
   * gap is removed:
   *
   *   <bottom> <gap> [argc][argv][envv][auxv][L4Re Env][caps][args][arg0]<top>
   */
  assert(_stack.top());
  _named_caps = (l4re_env_cap_entry_t *)(_stack.bottom() + _stack.size() / 2U);
  _named_caps_end = _named_caps;

  _stack.add_arg(arg0);
  *_named_caps-- = l4re_env_cap_entry_t(); // terminating L4Re::Env entry
}

App_task& App_task::map(l4_addr_t base, l4_size_t size)
{
  map_to_task(base, base, size, L4_FPAGE_RW);
  return *this;
}

App_task& App_task::map_mmio(l4_addr_t base, l4_size_t size)
{
  if (!Page_alloc::map_iomem(base, size))
    Fatal().abort("map iomem");

  map_to_task(base, base, size, L4_FPAGE_RW, L4_FPAGE_UNCACHEABLE << 4);

  return *this;
}

App_task& App_task::map_shm(l4_addr_t base, l4_size_t size)
{
  size = l4_round_page(size);
  if (!Page_alloc::share_ram(base, size))
    Fatal().abort("shm not available\n");

  map_to_task(base, base, size, L4_FPAGE_RW, L4_FPAGE_BUFFERABLE << 4);

  return *this;
}

void App_task::start()
{
  // Put named caps at top of stack
  _stack.align(alignof(l4re_env_cap_entry_t));
  auto *caps = _stack.push(_named_caps, _named_caps_end - _named_caps);

  // finalize L4Re Env
  _stack.align(alignof(L4Re::Env));
  L4Re::Env *env = _stack.push(L4Re::Env());
  env->mem_alloc(L4::Cap<L4Re::Mem_alloc>(Caps::Allocator_cap << L4_CAP_SHIFT));
  env->parent(L4::Cap<L4Re::Parent>(Caps::Parent_cap << L4_CAP_SHIFT));
  env->scheduler(L4::Cap<L4::Scheduler>(Caps::Scheduler_cap << L4_CAP_SHIFT));
  env->rm(L4::Cap<L4Re::Rm>(Caps::Rm_thread_cap << L4_CAP_SHIFT));
  env->log(L4::Cap<L4Re::Log>(Caps::Log_cap << L4_CAP_SHIFT));
  env->main_thread(L4::Cap<L4::Thread>(Caps::Rm_thread_cap << L4_CAP_SHIFT));
  env->factory(L4::Cap<L4::Factory>(Caps::Factory_cap << L4_CAP_SHIFT));
  env->first_free_cap(_first_free_cap >> L4_CAP_SHIFT);
  env->utcb_area(_utcb);
  env->first_free_utcb(l4_fpage_memaddr(_utcb) + L4_UTCB_OFFSET);
  env->initial_caps(_stack.relocate(caps));

  // Top of the stack is finished. Now move on to the bottom of the stack that
  // points to the elements on the stack top...

  _stack.add(l4_umword_t(0)); // final argv marker
  _stack.add(l4_umword_t(0)); // final envv marker (we don't support env vars)

  // Add standard auxv entries expected by uclibc
  _stack.add(l4_umword_t(AT_EGID));
  _stack.add(l4_umword_t(0));
  _stack.add(l4_umword_t(AT_GID));
  _stack.add(l4_umword_t(0));
  _stack.add(l4_umword_t(AT_EUID));
  _stack.add(l4_umword_t(0));
  _stack.add(l4_umword_t(AT_UID));
  _stack.add(l4_umword_t(0));
  _stack.add(l4_umword_t(AT_PAGESZ));
  _stack.add(l4_umword_t(L4_PAGESIZE));
  if (_phdrs)
    {
      _stack.add(l4_umword_t(AT_PHDR));
      _stack.add(l4_umword_t(_phdrs));
      _stack.add(l4_umword_t(AT_PHNUM));
      _stack.add(l4_umword_t(_num_phdrs));
    }

  // No l4re_aux_t

  // L4Re Env Pointer
  _stack.add(l4_umword_t(0xF1));
  _stack.add(_stack.relocate(env));

  // KIP Pointer
  _stack.add(l4_umword_t(0xF2));
  _stack.add(l4_umword_t(l4re_kip()));

  // Terminating NULL
  _stack.add(l4_umword_t(0));
  _stack.add(l4_umword_t(0));

  // Remove gap in the middle
  l4_addr_t sp = _stack.pack();

  // Map known caps
  if (l4_error(_task->map(L4_BASE_TASK_CAP, _known_caps[Cap_log],
                          env->log().snd_base())) < 0)
    Fatal().abort("map cap failed\n");

  if (l4_error(_task->map(L4_BASE_TASK_CAP, _known_caps[Cap_factory],
                          env->factory().snd_base())) < 0)
    Fatal().abort("map cap failed\n");

  if (l4_error(_task->map(L4_BASE_TASK_CAP, _known_caps[Cap_scheduler],
                          env->scheduler().snd_base())) < 0)
    Fatal().abort("map cap failed\n");

  // We are the parent
  if (l4_error(_task->map(L4_BASE_TASK_CAP, obj_cap().fpage(L4_CAP_FPAGE_RW),
                          env->parent().snd_base())) < 0)
    Fatal().abort("map cap failed\n");

  // Give child access to its task and thread object
  if (l4_error(_task->map(L4_BASE_TASK_CAP, _task.fpage(L4_CAP_FPAGE_RWSD),
                          L4::Cap<L4::Task>(L4Re::This_task).snd_base())) < 0)
    Fatal().abort("map cap failed\n");
  if (l4_error(_task->map(L4_BASE_TASK_CAP, _thread.fpage(L4_CAP_FPAGE_RWSD),
                          env->main_thread().snd_base())) < 0)
    Fatal().abort("map cap failed\n");

#ifdef CONFIG_TINIT_MAP_DEBUG_CAP
  L4::Cap<void> jdb(L4_BASE_DEBUGGER_CAP);
  if (l4_error(_task->map(L4_BASE_TASK_CAP, jdb.fpage(), jdb.snd_base())) < 0)
    Fatal().abort("map cap failed\n");
#endif

  // The main thread is it's own pager. Thus it will halt on a page fault!
  L4::Thread::Attr th_attr;
  th_attr.pager(env->main_thread());
  th_attr.exc_handler(env->main_thread());
  th_attr.bind((l4_utcb_t*)l4_fpage_memaddr(_utcb), _task);
  if (l4_error(_thread->control(th_attr)) < 0)
    Fatal().abort("thread control failed\n");

  // Let's go...
  l4_sched_param_t sched_param = l4_sched_param(_prio);
  L4::Cap<L4::Scheduler> s(L4_BASE_SCHEDULER_CAP);
  s->run_thread(_thread, sched_param);
  _thread->ex_regs(_entry, sp, _ex_regs_flags);
}

bool
App_task::reserve_ram(cxx::String const &arg0, l4_addr_t reloc, unsigned node)
{
  void const *f = Boot_fs::find(arg0);
  if (!f)
    return false;

  Loader::Elf_binary elf(f);
  if (!elf.is_valid())
    return false;

  if (!dynamic_reloc(elf, &reloc, node))
    return false;

  bool ret = true;
#ifndef CONFIG_TINIT_DYNAMIC_LOADER
  elf.iterate_phdr([reloc, &ret](Loader::Elf_phdr ph, void const *f) {
    if (ph.type() == PT_LOAD)
      {
        l4_addr_t dest = ph.paddr() + reloc;
        l4_addr_t size = l4_round_page(ph.memsz());
        if (!size)
          return;

        char *src = (char *)f + ph.offset();
        if ((ph.flags() & PF_W) || ph.memsz() > ph.filesz()
            || src != (void *)dest)
          {
            if (!Page_alloc::reserve_ram(dest, size))
              ret = false;
          }
      }
  });
#endif

  return ret;
}

int
App_task::op_signal(L4Re::Parent::Rights, unsigned long sig, unsigned long val)
{
  switch (sig)
    {
    case 0: // exit
        {
          Fatal().printf("Task terminated w/ %lu\n", val);
          return -L4_ENOREPLY;
        }
    default: break;
    }
  return L4_EOK;
}