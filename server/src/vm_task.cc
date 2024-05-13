/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/re/env>
#include <l4/sigma0/sigma0.h>
#include <l4/sys/cache.h>
#include <l4/sys/platform_control.h>
#include <string.h>

#include "boot_fs.h"
#include "cap_alloc.h"
#include "globals.h"
#include "loader.h"
#include "page_alloc.h"
#include "vm_task.h"


void
Vm_task::Ram_area::copy_from(void const *from, l4_addr_t offset, l4_addr_t dest,
                             l4_size_t size) const
{
  memcpy(reinterpret_cast<void *>(dest + off),
         reinterpret_cast<char const *>(from) + offset, size);
  l4_cache_coherent(dest + off, dest + size);
}

void
Vm_task::Ram_area::clear(l4_addr_t dest, l4_size_t size) const
{
  memset(reinterpret_cast<void *>(dest + off), 0, size);
  l4_cache_coherent(dest + off, dest + size);
}


class Vm_task::Binary
{
public:
  Binary(void const *data)
  : _data(data),
    _elf(data)
  {}

  bool is_elf_binary()
  {
    return _elf.is_valid();
  }

  l4_addr_t load_as_elf(Ram_list const &ram)
  {
    _elf.iterate_phdr([this, &ram](Loader::Elf_phdr ph, void const *) {
      if (ph.type() == PT_LOAD)
        {
          l4_addr_t dest = ph.paddr();
          l4_addr_t size = ph.memsz();
          if (!size)
            return;

          l4_addr_t map_dest = l4_trunc_page(dest);
          l4_addr_t map_size = l4_round_page(size + dest - map_dest);

          auto *region = ram.find(map_dest, map_size);
          if (!region)
            {
              Fatal fatal;
              fatal.printf("Failed to load ELF kernel binary. "
                           "Region [0x%lx/0x%lx] not in RAM.\n",
                           map_dest, map_size);
              fatal.panic("Cannot load vm image\n");
            }

          Dbg().printf("Copy in ELF binary section @0x%lx from 0x%lx/0x%lx\n",
                       ph.paddr(), ph.offset(), ph.filesz());

          region->copy_from(_data, ph.offset(), dest, ph.filesz());
          region->clear(dest + ph.filesz(), size - ph.filesz());
        }
    });

    return _elf.entry();
  }

private:
  void const *_data;
  Loader::Elf_binary _elf;
};


Vm_task::Vm_task(cxx::String const &name)
: Loader::Child_task(Util::cap_alloc.alloc<L4::Vm>()), _name(name)
{
  // create the VM task
  auto *e = L4Re::Env::env();
  auto ret = e->factory()->create(_task, L4_PROTO_VM);

  if (l4_error(ret) < 0)
    Fatal().panic("Cannot create guest VM. Virtualization support may be missing.\n");

  l4_debugger_set_object_name(_task.cap(), name);
}

Vm_task&
Vm_task::map_ram(l4_addr_t base, l4_size_t size, l4_addr_t off)
{
  size = l4_round_page(size);
  if (!Page_alloc::reserve_ram(base + off, size))
    Fatal().panic("Vm_task: ram not available\n");

  map_to_task(base, base, size);

  _ram.add(base, size, off);

  return *this;
}

Vm_task&
Vm_task::map_mmio(l4_addr_t base, l4_size_t size)
{
  if (!Page_alloc::map_iomem(base, size))
    Fatal().panic("map iomem");

  map_to_task(base, base, size, L4_FPAGE_RW, L4_FPAGE_UNCACHEABLE << 4);

  return *this;
}

Vm_task&
Vm_task::map_shm(l4_addr_t base, l4_size_t size)
{
  size = l4_round_page(size);
  if (!Page_alloc::share_ram(base, size))
    Fatal().panic("Vm_task: shm not available\n");

  map_to_task(base, base, size);

  _ram.add(base, size, 0);

  return *this;
}

Vm_task&
Vm_task::load(cxx::String const &name, l4_addr_t *entry)
{
  void const *f = Boot_fs::find(name);
  if (!f)
    {
      Fatal().printf("vm: cannot find image '%.*s'\n", name.len(),
                     name.start());
      Fatal().panic("vm: file missing\n");
    }

  Binary image(f);
  if (!image.is_elf_binary())
    Fatal().panic("Vm_task: no elf file\n");

  *entry = image.load_as_elf(_ram);

  Info().printf("Loaded '%.*s' into VM '%.*s': entry @ 0x%lx\n",
                name.len(), name.start(), _name.len(), _name.start(),
                *entry);

  return *this;
}

Vm_task&
Vm_task::set_asid(l4_umword_t asid)
{
  if (l4_error(l4_platform_ctl_set_task_asid(L4_BASE_ICU_CAP, cap().cap(), asid)) < 0)
    Fatal().panic("Cannot set VMID.\n");

  return *this;
}
