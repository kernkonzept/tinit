/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/sys/types.h>

struct Page_alloc
{
  static void init();

  static void add_pool(unsigned long address, unsigned long size,
                       unsigned long nodes);
  static unsigned long alloc_ram(unsigned long size, unsigned long align,
                                 unsigned node);
  static bool reserve_ram(unsigned long address, unsigned long size);
  static bool share_ram(unsigned long address, unsigned long size);
  static bool map_iomem(unsigned long address, unsigned long size);

  static unsigned long avail() __attribute__((pure));
  static void dump();
};
