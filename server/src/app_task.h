/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/cxx/string>
#include <l4/re/parent>
#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/scheduler>
#include <l4/sys/thread>
#include <l4/sys/vcon>

#include "loader.h"
#include "registry.h"

class App_task :
  public L4::Epiface_t<App_task, L4Re::Parent>,
  public Loader::Child_task
{
  class Stack
  {
  public:
    void init(void *bottom, l4_size_t size);

    template< typename T >
    T *add(T const &v)
    { return reinterpret_cast<T*>(add_object(&v, sizeof(T))); }

    template< typename T >
    T *push(T const &v)
    { return reinterpret_cast<T*>(push_object(&v, sizeof(T))); }

    template< typename T >
    T *push(T const *v, unsigned long num)
    { return reinterpret_cast<T*>(push_object(v, sizeof(T) * num)); }

    template < typename T >
    T relocate(T src) const
    { return src; }

    void align(l4_addr_t size)
    {
      _back = reinterpret_cast<char *>(
        reinterpret_cast<l4_addr_t>(_back) & ~(size - 1U));
    }

    void add_arg(cxx::String const &arg);
    l4_addr_t pack();

    char *bottom() const { return _bottom; }
    char *top() const { return _top; }
    l4_size_t size() const { return _top - _bottom; }

  private:
    char *add_object(void const *src, unsigned long size);
    char *push_object(void const *src, unsigned long size);

    l4_umword_t &argc() { return *reinterpret_cast<l4_umword_t*>(_bottom); }

    char *_bottom = nullptr;
    char *_top = nullptr;
    char *_front = nullptr;
    char *_back = nullptr;
  };

  enum Known_caps {
    Cap_log,
    Cap_factory,
    Cap_scheduler,

    Max_known_caps
  };

  struct Caps {
    enum Base_cap
    {
      Task_cap               = 1,
      Factory_cap            = 2,
      Rm_thread_cap          = 3,
      Log_cap                = 5,
      Scheduler_cap          = 7,
      // skip base caps
      Allocator_cap          = 0x10,
      Parent_cap,
      First_free,
    };
  };

public:
  App_task(My_registry *registry, cxx::String const &arg0, unsigned prio = 0xfe,
           unsigned utcb_pages_order = 0, l4_addr_t reloc = 0);


  App_task& cap(cxx::String const &name, L4::Cap<void> cap, unsigned rights)
  {
    push_named_cap(name, cap, rights);
    return *this;
  }

  App_task& cap_log(L4::Cap<L4::Vcon> cap, unsigned rights)
  {
    push_known_cap(Cap_log, cap, rights);
    return *this;
  }

  App_task& cap_factory(L4::Cap<L4::Factory> cap, unsigned rights)
  {
    push_known_cap(Cap_factory, cap, rights);
    return *this;
  }

  App_task& cap_scheduler(L4::Cap<L4::Scheduler> cap, unsigned rights)
  {
    push_known_cap(Cap_scheduler, cap, rights);
    return *this;
  }

  App_task& arg(cxx::String const &arg)
  {
    _stack.add_arg(arg);
    return *this;
  }

  App_task& map(l4_addr_t base, l4_size_t size);
  App_task& map_mmio(l4_addr_t base, l4_size_t size);
  App_task& map_shm(l4_addr_t base, l4_size_t size);

  void set_priority(unsigned prio)
  { _prio = prio; }

  void start();

  static bool reserve_ram(cxx::String const &arg0, l4_addr_t reloc,
                          unsigned node);

  static unsigned long used_ram()
  { return _used_ram; }

  static void check_tasks_ready()
  {
    // In case no tvmm was started, print now...
    if (!_started_tasks)
      dump_kernel_stats();
  }

private:
  Stack _stack;
  L4::Cap<L4::Thread> _thread;
  l4_fpage_t _utcb;
  l4_cap_idx_t _first_free_cap;
  l4_fpage_t _known_caps[Max_known_caps];
  cxx::String const _arg0;
  l4re_env_cap_entry_t *_named_caps, *_named_caps_end;
  l4_umword_t _entry;
  l4_umword_t _phdrs;
  l4_umword_t _ex_regs_flags;
  unsigned char _num_phdrs;
  unsigned char _prio;
  static unsigned long _used_ram;
  static unsigned char _started_tasks;
  static unsigned char _ready_tasks;

  void push_named_cap(cxx::String const &name, L4::Cap<void> cap,
                      unsigned rights);
  void push_known_cap(Known_caps type, L4::Cap<void> cap, unsigned rights);
  static bool dynamic_reloc(Loader::Elf_binary &elf, l4_addr_t *reloc,
                            unsigned node);
  static void dump_kernel_stats();

public:
  // Implements L4Re::Parent
  int op_signal(L4Re::Parent::Rights, unsigned long, unsigned long);
};
