/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#pragma once

#include <l4/cxx/string>
#include <l4/sys/l4int.h>

struct Boot_fs
{
  static void *find(cxx::String const &name, l4_size_t *size = nullptr);
};
