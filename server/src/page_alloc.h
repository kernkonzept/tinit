/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#pragma once

#include <l4/sys/types.h>

struct Page_alloc
{
  static void init();

  static void add_pool(unsigned long address, unsigned long size,
                       unsigned nodes = ~0);
  static void *alloc(unsigned long size, unsigned long align = 1,
                     unsigned nodes = ~0U);
  static bool reserve(unsigned long address, unsigned long size,
                      unsigned nodes = ~0U);
  static bool share(unsigned long address, unsigned long size);
  static bool map_iomem(unsigned long address, unsigned long size);

  static unsigned long avail() __attribute__((pure));
  static void dump();
};
