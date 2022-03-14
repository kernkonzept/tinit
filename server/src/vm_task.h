/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/cxx/string>
#include <l4/sys/l4int.h>
#include <l4/sys/vm>
#include <l4/cxx/slist>

#include "debug.h"
#include "loader.h"

class Vm_task : protected Loader::Child_task
{
  struct Ram_area : public cxx::S_list_item
  {
    l4_addr_t base;
    l4_size_t size;
    l4_addr_t off;

    Ram_area() = default;
    Ram_area(l4_addr_t base, l4_size_t size, l4_addr_t off)
    : S_list_item(), base(base), size(size), off(off)
    {}

    void copy_from(void const *from, l4_addr_t offset, l4_addr_t dest,
                   l4_size_t size) const;
    void clear(l4_addr_t dest, l4_size_t size) const;
  };

  class Ram_list
  {
    cxx::S_list<Ram_area> _areas;

  public:
    void add(l4_addr_t base, l4_size_t size, l4_addr_t off)
    {
      _areas.add(new Ram_area(base, size, off));
    }

    /**
     * Find `Ram_area` that is fully backed by `[start, start+size)`.
     */
    Ram_area const *find(l4_addr_t start, l4_size_t size) const
    {
      for (auto *i : _areas)
        if (i->base <= start && (i->base + i->size) >= (start + size))
          return i;

      return nullptr;
    }
  };

  class Binary;

public:
  Vm_task(cxx::String const &name);

  Vm_task& map_ram(l4_addr_t base, l4_size_t size, l4_addr_t off);
  Vm_task& map_mmio(l4_addr_t base, l4_size_t size);
  Vm_task& map_shm(l4_addr_t base, l4_size_t size);
  Vm_task& load(cxx::String const &name, l4_addr_t *entry);
  Vm_task& set_asid(l4_umword_t asid);

  L4::Cap<L4::Vm> cap() const { return L4::cap_cast<L4::Vm>(_task); };

private:
  cxx::String _name;
  Ram_list _ram;
};
