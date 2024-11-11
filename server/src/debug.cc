/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/sys/debugger.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"

void Fatal::panic(char const *msg)
{
  write(2, "FATAL: ", 7);
  write(2, msg, strlen(msg));
  _exit(1);
}

#ifndef NDEBUG
void l4_debugger_set_object_name(l4_cap_idx_t cap, cxx::String const &name)
{
  char dbg[16];
  memset(dbg, 0, sizeof(dbg));
  unsigned name_len = static_cast<unsigned>(name.len());
  memcpy(&dbg, name.start(), name_len >= sizeof(dbg)
                             ? sizeof(dbg) - 1 : name_len);
  l4_debugger_set_object_name(cap, dbg);
}

void l4_debugger_add_image_info(l4_cap_idx_t cap, l4_addr_t base,
                                cxx::String const &name)
{
  char dbg[16];
  memset(dbg, 0, sizeof(dbg));
  unsigned name_len = static_cast<unsigned>(name.len());
  memcpy(&dbg, name.start(), name_len >= sizeof(dbg)
                             ? sizeof(dbg) - 1 : name_len);
  l4_debugger_add_image_info(cap, base, dbg);
}
#endif
