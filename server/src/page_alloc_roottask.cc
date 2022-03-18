/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <l4/sigma0/sigma0.h>
#include <l4/sys/consts.h>
#include <string.h>

#include <l4/cxx/list_alloc>

#include "debug.h"
#include "globals.h"
#include "page_alloc.h"

namespace {
cxx::List_alloc pa;

// A region where dynamic allocations can be made
struct Pool
{
  Pool() = default;
  constexpr Pool(unsigned long start, unsigned long end,
       Pool *next = 0)
  : next(next), start(start), end(end)
  {}

  Pool *next;
  unsigned long start;
  unsigned long end;
};

Pool *pool;
Pool whole_ram{0, ~0UL, 0};

}

void Page_alloc::init()
{
  l4_addr_t addr;
  l4_addr_t min_addr = ~0UL;
  l4_addr_t max_addr = 0;

  for (unsigned order = 30 /*1G*/; order >= L4_LOG2_PAGESIZE; --order)
    {
      while (!l4sigma0_map_anypage(Sigma0_cap, 0, L4_WHOLE_ADDRESS_SPACE,
                                   &addr, order))
        {
          unsigned long size = 1UL << order;

          if (addr == 0)
            {
              addr = L4_PAGESIZE;
              size -= L4_PAGESIZE;
              if (!size)
                continue;
            }

          if (addr < min_addr) min_addr = addr;
          if (addr + size > max_addr) max_addr = addr + size - 1U;

          pa.free((void*)addr, size, true);
        }
    }
}

void Page_alloc::add_pool(unsigned long address, unsigned long size,
                          unsigned /*nodes*/)
{
  pool = new Pool(address, address + size - 1U, pool);
}

void *Page_alloc::alloc(unsigned long size, unsigned long align, unsigned)
{
  void *ret = 0;

  for (Pool const *p = pool ? pool : &whole_ram; p && !ret; p = p->next)
    ret = pa.alloc(size, align, p->start, p->end);

  return ret;
}

bool Page_alloc::reserve(unsigned long address, unsigned long size, unsigned)
{
  return pa.alloc(size, 0, address, address + size - 1U);
}

bool Page_alloc::share(unsigned long, unsigned long)
{
  // TODO: track shared regions
  return true;
}

bool Page_alloc::map_iomem(unsigned long address, unsigned long size)
{
  return l4sigma0_map_iomem(Sigma0_cap, address, address, size, 0) >= 0;
}

unsigned long Page_alloc::avail()
{
  return pa.avail();
}

void Page_alloc::dump()
{
  Info info;
  info.printf("Remaining free memory:\n");
  pa.dump_free_list(info);
}
