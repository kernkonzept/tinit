/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/cxx/string>
#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <stdio.h>
#include <unistd.h>

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

struct Fatal
{
  static void L4_NORETURN panic(char const *msg);

#ifdef ENABLE_ERRORS
  template<typename... Args>
  int printf(char const *fmt, Args... args) const
  {
    ::write(1, "tinit: ", 7);
    return dprintf(1, fmt, args...);
  }
#else
  int printf(char const * /*fmt*/, ...) const
    __attribute__((format(printf, 2, 3)))
  { return 0; }
#endif
};

struct Err
{
#ifdef ENABLE_ERRORS
  template<typename... Args>
  int printf(char const *fmt, Args... args) const
  {
    ::write(1, "tinit: ", 7);
    return dprintf(1, fmt, args...);
  }
#else
  int printf(char const * /*fmt*/, ...) const
    __attribute__((format(printf, 2, 3)))
  { return 0; }
#endif
};

struct Info
{
#ifdef ENABLE_INFO
  template<typename... Args>
  int printf(char const *fmt, Args... args) const
  {
    ::write(1, "tinit: ", 7);
    return dprintf(1, fmt, args...);
  }
#else
  int printf(char const * /*fmt*/, ...) const
    __attribute__((format(printf, 2, 3)))
  { return 0; }
#endif
};

struct Dbg
{
#ifdef ENABLE_DEBUG
  template<typename... Args>
  int printf(char const *fmt, Args... args) const
  {
    ::write(1, "tinit: ", 7);
    return dprintf(1, fmt, args...);
  }
#else
  int printf(char const * /*fmt*/, ...) const
    __attribute__((format(printf, 2, 3)))
  { return 0; }
#endif
};

#pragma GCC diagnostic pop

void l4_debugger_set_object_name(l4_cap_idx_t cap, cxx::String const &name);
void l4_debugger_add_image_info(l4_cap_idx_t cap, l4_addr_t base,
                                cxx::String const &name);

#ifdef NDEBUG
inline void l4_debugger_set_object_name(l4_cap_idx_t, cxx::String const &)
{}
inline void l4_debugger_add_image_info(l4_cap_idx_t, l4_addr_t, cxx::String const &)
{}
#endif
