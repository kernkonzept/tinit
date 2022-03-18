/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#pragma once

#include <l4/cxx/string>
#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <stdio.h>

#if defined(CONFIG_TINIT_VERBOSITY_ERROR)
#define ENABLE_ERRORS
#endif
#if defined(CONFIG_TINIT_VERBOSITY_INFO)
#define ENABLE_ERRORS
#define ENABLE_INFO
#endif
#if defined(CONFIG_TINIT_VERBOSITY_ALL)
#define ENABLE_ERRORS
#define ENABLE_INFO
#define ENABLE_DEBUG
#endif

struct Debug
{
  static int printf_impl(char const *fmt, ...);
};

struct Fatal : Debug
{
  static int L4_NORETURN abort(char const *msg);

#ifdef ENABLE_ERRORS
  int printf(char const *fmt, ...) const
    __attribute__((format(printf,2,3)))
  { return printf_impl(fmt, __builtin_va_arg_pack()); }
#else
  int printf(char const * /*fmt*/, ...) const
    __attribute__((format(printf, 2, 3)))
  { return 0; }
#endif
};

struct Err : Debug
{
#ifdef ENABLE_ERRORS
  int printf(char const *fmt, ...) const
    __attribute__((format(printf,2,3)))
  { return printf_impl(fmt, __builtin_va_arg_pack()); }
#else
  int printf(char const * /*fmt*/, ...) const
    __attribute__((format(printf, 2, 3)))
  { return 0; }
#endif
};

struct Info : Debug
{
#ifdef ENABLE_INFO
  int printf(char const *fmt, ...) const
    __attribute__((format(printf,2,3)))
  { return printf_impl(fmt, __builtin_va_arg_pack()); }
#else
  int printf(char const * /*fmt*/, ...) const
    __attribute__((format(printf, 2, 3)))
  { return 0; }
#endif
};

struct Dbg : Debug
{
#ifdef ENABLE_DEBUG
  int printf(char const *fmt, ...) const
    __attribute__((format(printf,2,3)))
  { return printf_impl(fmt, __builtin_va_arg_pack()); }
#else
  int printf(char const * /*fmt*/, ...) const
    __attribute__((format(printf, 2, 3)))
  { return 0; }
#endif
};

void l4_debugger_set_object_name(l4_cap_idx_t cap, cxx::String const &name);
void l4_debugger_add_image_info(l4_cap_idx_t cap, l4_addr_t base,
                                cxx::String const &name);

#ifdef NDEBUG
inline void l4_debugger_set_object_name(l4_cap_idx_t, cxx::String const &)
{}
void l4_debugger_add_image_info(l4_cap_idx_t, l4_addr_t, cxx::String const &)
{}
#endif
