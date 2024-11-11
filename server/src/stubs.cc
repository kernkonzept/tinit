/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <assert.h>
#include <errno.h>
#include <l4/re/env>
#include <l4/util/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "debug.h"
#include "stubs.h"

#ifdef CONFIG_BID_STATIC_HEAP
// provided by main_stat.ld linkmap file
extern char __heap_start[];
extern char __heap_end[];
#else
#define HEAP_ELEMENTS (CONFIG_TINIT_HEAP_SIZE / sizeof(unsigned long))
static unsigned long __heap[HEAP_ELEMENTS];
char * const __heap_start = reinterpret_cast<char *>(&__heap[0]);
char * const __heap_end = reinterpret_cast<char *>(&__heap[HEAP_ELEMENTS]);
#endif

static char *heap_pos = __heap_start;

extern "C" void _exit(int status);
void _exit(int)
{
  l4_sleep_forever();
}

void *malloc(size_t size)
{
  if (0)
    printf("malloc(%zu) from %zu bytes\n", size, __heap_end - heap_pos);

  size = (size + sizeof(unsigned long) - 1U) & ~(sizeof(unsigned long) - 1U);
  char *ret = heap_pos;
  heap_pos += size;

  if (heap_pos > __heap_end)
    Fatal().panic("OOM\n");

  return reinterpret_cast<void *>(ret);
}

void free(void *)
{}

unsigned long heap_avail()
{ return __heap_end - heap_pos; }

unsigned long heap_size()
{ return __heap_end - __heap_start; }

void *operator new(size_t sz) { return malloc(sz); }
void operator delete(void *m) { free(m); }
#if __cplusplus >= 201400
void operator delete(void *m, size_t) noexcept { free(m); }
#endif
