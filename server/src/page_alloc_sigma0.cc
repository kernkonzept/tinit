/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/sys/consts.h>
#include <string.h>
#include <l4/sys/kip>
#include <l4/sys/kip.h>

#include "debug.h"
#include "page_alloc.h"

namespace {

class Ram_tracker
{
  // A region where dynamic allocations can be made. Pools may be restricted
  // to AMP nodes.
  struct Pool
  {
    Pool() = default;
    constexpr Pool(unsigned long start, unsigned long end,
                   unsigned long nodes = ~0UL, Pool *next = 0)
    : next(next), start(start), end(end), nodes(nodes)
    {}

    Pool *next;

    unsigned long start;  ///< Start address of pool
    unsigned long end;    ///< End address (inclusice) of pool
    unsigned long nodes;  ///< Bit mask of applicable AMP nodes
  };

  // A range in RAM. Start and end are inclusive.
  struct Range
  {
    Range() : prev(this), next(this) {}

    Range(unsigned long start, unsigned long end)
    : start(start), end(end)
    {}

    Range *prev, *next;
    unsigned long start, end;
    bool shared = false;

    static void insert_after(Range *l, Range *e)
    {
      e->next = l->next;
      e->prev = l;
      e->next->prev = e;
      l->next = e;
    }

    static void insert_before(Range *l, Range *e)
    {
      e->prev = l->prev;
      e->next = l;
      e->prev->next = e;
      l->prev = e;
    }

    static Range *erase(Range *e)
    {
      Range *ret = e->next;
      e->prev->next = ret;
      e->next->prev = e->prev;
      return ret;
    }

    static bool empty(Range *l)
    {
      return l->next == l && l->prev == l;
    }
  };

public:
  void add(unsigned long start, unsigned long end)
  {
    Range *spot = &list;

    for (Range *i = list.next; i != &list; i = i->next)
      {
        if (i->end < start)
          spot = i;
        else if (i->start <= end)
          Fatal().panic("Region collision");
      }

    Range *n = alloc_range(start, end);
    Range::insert_after(spot, n);

    // We assume optimization of adjacent regions is not needed.
  }

  void sub(unsigned long start, unsigned long end)
  {
    Range *i = list.next;
    while (i != &list && i->start <= end)
      {
        if (i->end < start)
          {
            // unrelated region on left side
            i = i->next;
          }
        else if (start <= i->start && i->end <= end)
          {
            // fully covered, just remove
            auto *d = i;
            i = Range::erase(i);
            free_range(d);
          }
        else if (i->start < start && end < i->end)
          {
            // punch hole into region
            Range::insert_after(i, alloc_range(end + 1U, i->end));
            i->end = start - 1U;
            break;
          }
        else if (start <= i->start)
          {
            // adjust left side
            i->start = end + 1U;
            break;
          }
        else
          {
            // adjust right side
            i->end = start - 1U;
            break;
          }
      }
  }

  void add_pool(unsigned long address, unsigned long size, unsigned long nodes)
  {
    pool = new Pool(address, address + size - 1U, nodes, pool);
  }

  unsigned long alloc(unsigned long size, unsigned long align, unsigned node)
  {
    static constexpr Pool whole_ram{0, ~0UL};
    unsigned long mask = ~(align - 1U);

    for (Range *i = list.next; i != &list; i = i->next)
      {
        if (i->shared)
          continue;

        for (Pool const *p = pool ? pool : &whole_ram; p; p = p->next)
          {
            if ((p->nodes & (1UL << node)) == 0)
              continue;

            unsigned long i_start = i->start;
            if (i_start < p->start)
              i_start = p->start;

            unsigned long i_end = i->end;
            if (i_end > p->end)
              i_end = p->end;

            if (i_start >= i_end)
              continue;

            unsigned long start = (i_start + align - 1U) & mask;
            if (start + size - 1U > i_end)
              continue;

            if (reserve(start, start + size - 1U, true))
              return start;
          }
      }

    return 0;
  }

  /**
   * Reserve a region.
   *
   * The requested region must be fully backed by an existing region. If
   * the region is requested exclusively we simply remove it from the list.
   * Otherwise the region is marked as shared and may be shared again.
   *
   * Extending shared regions by requesting overlapping or adjacent regions
   * as shared is not supported.
   */
  bool reserve(unsigned long start, unsigned long end, bool exclusive)
  {
    Range *i = list.next;
    while (i != &list)
      {
        if (i->start <= start && i->end >= end)
          break;
        i = i->next;
      }

    if (i == &list)
      return false;

    if (exclusive && i->shared)
      {
        Info().printf("Cannot reserve [%lx-%lx] in shared region [%lx-%lx]\n",
                      start, end, i->start, i->end);
        return false;
      }

    if (exclusive)
      sub(start, end);
    else if (!i->shared)
      {
        if (i->start != start && i->end != end)
          {
            // split into three regions
            Range::insert_before(i, alloc_range(i->start, start - 1U));
            Range::insert_after(i, alloc_range(end + 1U, i->end));
          }
        else if (i->start == start && i->end != end)
          {
            // split into two regions at start
            Range::insert_after(i, alloc_range(end + 1U, i->end));
          }
        else if (i->start != start && i->end == end)
          {
            // split into two regions at end
            Range::insert_before(i, alloc_range(i->start, start - 1U));
          }

        i->start = start;
        i->end = end;
        i->shared = true;
      }

    return true;
  }

  unsigned long avail() const
  {
    unsigned long ret = 0;

    for (Range *i = list.next; i != &list; i = i->next)
      ret += i->end - i->start + 1U;

    return ret;
  }

  template<typename DBG>
  void dump_free_list(DBG &dbg)
  {
    for (Range *i = list.next; i != &list; i = i->next)
      dbg.printf("  [%8lx - %8lx]%s\n", i->start, i->end,
                 i->shared ? " (shared)" : "");
  }

private:
  Range *alloc_range(unsigned long start, unsigned long end)
  {
    if (Range::empty(&slab))
      return new Range(start, end);

    Range *n = slab.next;
    Range::erase(n);

    n->start = start;
    n->end = end;
    n->shared = false;

    return n;
  }

  void free_range(Range *r)
  {
    Range::insert_after(&slab, r);
  }

  Range list;
  Range slab;
  Pool *pool;
};

static Ram_tracker ram;

}

void Page_alloc::init()
{
  for (auto const &md: L4::Kip::Mem_desc::all(l4_kip()))
    {
      if (md.is_virtual())
        continue;

      L4::Kip::Mem_desc::Mem_type type = md.type();
      unsigned long start, end;

      // See CD-412
      if (type == L4::Kip::Mem_desc::Conventional)
        {
          // Drop partially covered pages
          start = l4_round_page(md.start());
          end = l4_trunc_page(md.end() + 1) - 1;
        }
      else
        {
          // Fully cover partially reserved pages
          start = l4_trunc_page(md.start());
          end = l4_round_page(md.end() + 1) - 1;
        }

      if (type == L4::Kip::Mem_desc::Conventional)
        ram.add(start, end);
      else if (type != L4::Kip::Mem_desc::Undefined)
        ram.sub(start, end);
    }
}

void Page_alloc::add_pool(unsigned long address, unsigned long size,
                          unsigned long nodes)
{
  ram.add_pool(address, size, nodes);
}

unsigned long Page_alloc::alloc_ram(unsigned long size, unsigned long align,
                                    unsigned node)
{
  return ram.alloc(size, align, node);
}

bool Page_alloc::reserve_ram(unsigned long address, unsigned long size)
{
  return ram.reserve(address, address + size - 1U, true);
}

bool Page_alloc::share_ram(unsigned long address, unsigned long size)
{
  return ram.reserve(address, address + size - 1U, false);
}

bool Page_alloc::map_iomem(unsigned long address, unsigned long size)
{
  unsigned long io_start = l4_trunc_page(address);
  unsigned long io_end = l4_round_page(address + size - 1) - 1;

  for (auto const &md: L4::Kip::Mem_desc::all(l4_kip()))
    {
      if (md.is_virtual())
        continue;

      L4::Kip::Mem_desc::Mem_type type = md.type();
      unsigned long md_start, md_end;

      switch (type)
        {
        case L4::Kip::Mem_desc::Conventional:
          // Drop partially covered pages
          md_start = l4_round_page(md.start());
          md_end = l4_trunc_page(md.end() + 1) - 1;
          break;
        case L4::Kip::Mem_desc::Info:
        case L4::Kip::Mem_desc::Arch:
        case L4::Kip::Mem_desc::Shared:
          // These pages do not count as reserved
          continue;
        default:
          // Fully cover partially reserved pages
          md_start = l4_trunc_page(md.start());
          md_end = l4_round_page(md.end() + 1) - 1;
          break;
        }

      // Collides with reserved range?
      if (io_end >= md_start && io_start <= md_end)
        return false;
    }

  return true;
}

unsigned long Page_alloc::avail()
{
  return ram.avail();
}

void Page_alloc::dump()
{
  Info info;
  info.printf("Remaining free memory:\n");
  ram.dump_free_list(info);
}
