/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/re/consts>

#include "debug.h"
#include "loader.h"

namespace {

/**
 * Check whether a log2-sized page containing address is inside a region
 *
 * \param align    log2 of the page alignment.
 * \param addr     Address to check.
 * \param start    Start of region.
 * \param end      Last byte of region; do not check end of region if zero.
 * \return true if there is a log2-aligned page containing the address
 *                 inside the region
 */
bool log2_page_in_range(unsigned char align, l4_addr_t addr,
                        l4_addr_t start, l4_addr_t end)
{
  auto log2page = l4_trunc_size(addr, align);
  return    start <= log2page
         && (!end || (log2page + (1UL << align) - 1) <= end);
}

bool log2_alignment_compatible(unsigned char align, l4_addr_t addr1,
                               l4_addr_t addr2)
{ return (addr1 & ((1UL << align) - 1)) == (addr2 & ((1UL << align) - 1)); }

/**
 * Calculate log_2(pagesize) for a location in a region
 *
 * \param addr     Guest-physical address where the access occurred.
 * \param start    Guest-physical address of start of memory region.
 * \param end      Guest-physical address of last byte of memory region.
 * \param offset   Accessed address relative to the beginning of the region.
 * \param l_start  Local address of start of memory region, default 0.
 * \param l_end    Local address of end of memory region, default 0.
 *
 * \return largest possible pageshift.
 */
char get_page_shift(l4_addr_t addr, l4_addr_t start, l4_addr_t end,
                    l4_addr_t offset, l4_addr_t l_start = 0,
                    l4_addr_t l_end = 0)
{
  if (end <= start)
    return L4_PAGESHIFT;

  // Start with a reasonable maximum value: log2 of the memory region size
  l4_addr_t const size = end - start + 1;
  unsigned char align = sizeof(l4_addr_t) * 8 - (__builtin_clzl(size) + 1);
  for (; align > L4_PAGESHIFT; --align)
    {
      // Check whether a log2-sized page is inside the regions
      if (   !log2_page_in_range(align, addr, start, end)
          || !log2_page_in_range(align, l_start + offset, l_start, l_end))
        continue;

      if (!log2_alignment_compatible(align, start, l_start))
        continue;

      return align;
    }

  return L4_PAGESHIFT;
}

}


namespace Loader {

void
Child_task::map_to_task(l4_addr_t local, l4_addr_t dest, l4_addr_t size,
                        unsigned char rights, unsigned char snd_base)
{
  l4_addr_t dest_end = dest + size - 1U;
  l4_addr_t offs = 0;
  int err;

  while (offs < size)
    {
      auto doffs = dest + offs;
      char ps = get_page_shift(doffs, dest, dest_end, offs, local);
      if ((err = l4_error(_task->map(L4Re::This_task,
                                     l4_fpage(local + offs, ps, rights),
                                     doffs | snd_base))) < 0)
        {
          Fatal().printf("map_to_task(0x%lx, 0x%0lx, %lu, %d): failed for "
                         "0x%lx/%d -> %lx: %d\n", local, dest, size, rights,
                         local + offs, ps, doffs, err);
          Fatal().panic("task->map failed\n");
        }
      offs += l4_addr_t{1} << ps;
    }
}

}
