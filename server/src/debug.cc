/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <l4/sys/debugger.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

int Debug::printf_impl(char const *fmt, ...)
{
  int n;
  va_list args;

  va_start    (args, fmt);
  n = vdprintf (1, fmt, args);
  va_end      (args);

  return n;
}

int Fatal::abort(char const *msg)
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
  memcpy(&dbg, name.start(), (unsigned)name.len() >= sizeof(dbg)
                             ? sizeof(dbg) - 1 : name.len());
  l4_debugger_set_object_name(cap, dbg);
}

void l4_debugger_add_image_info(l4_cap_idx_t cap, l4_addr_t base,
                                cxx::String const &name)
{
  char dbg[16];
  memset(dbg, 0, sizeof(dbg));
  memcpy(&dbg, name.start(), (unsigned)name.len() >= sizeof(dbg)
                             ? sizeof(dbg) - 1 : name.len());
  l4_debugger_add_image_info(cap, base, dbg);
}
#endif
