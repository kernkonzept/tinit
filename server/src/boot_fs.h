/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/cxx/string>
#include <l4/sys/l4int.h>

struct Boot_fs
{
  static char const *find(cxx::String const &name, l4_size_t *size = nullptr);
};
