/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/sigma0/sigma0.h>
#include <l4/sys/ipc.h>
#include <l4/sys/kip.h>
#include <l4/util/l4mod.h>
#include <l4/util/splitlog2.h>
#include <l4/util/util.h>
#include <string.h>

#include "boot_fs.h"
#include "globals.h"

static inline cxx::String cmdline_to_name(char const *cmdl)
{
  int len = strlen(cmdl);
  int i;
  for (i = 0; i < len; ++i)
    {
      if (i > 0 && cmdl[i] == ' ' && cmdl[i-1] != '\\')
        break;
    }

  int s;
  for (s = i - 1; s >= 0; --s)
    {
      if (cmdl[s] == '/')
        break;
    }

  ++s;

  return cxx::String(cmdl + s, i - s);
}

#ifdef CONFIG_TINIT_RUN_ROOTTASK
static long
s0_request_ram(l4_addr_t s, l4_addr_t, int order)
{
  l4_msg_regs_t *m = l4_utcb_mr();
  l4_buf_regs_t *b = l4_utcb_br();
  l4_msgtag_t tag = l4_msgtag(L4_PROTO_SIGMA0, 2, 0, 0);
  m->mr[0] = SIGMA0_REQ_FPAGE_RAM;
  m->mr[1] = l4_fpage(s, order, L4_FPAGE_RWX).raw;

  b->bdr   = 0;
  b->br[0] = L4_ITEM_MAP;
  b->br[1] = l4_fpage(s, order, L4_FPAGE_RWX).raw;
  return l4_error(l4_ipc_call(Sigma0_cap, l4_utcb(), tag, L4_IPC_NEVER));
}
#endif

char const *Boot_fs::find(cxx::String const &name, l4_size_t *size)
{
  l4util_l4mod_info const *mbi
    = reinterpret_cast<l4util_l4mod_info const *>(l4_kip()->user_ptr);
  l4util_l4mod_mod const *modules
    = reinterpret_cast<l4util_l4mod_mod const *>(mbi->mods_addr);
  unsigned num_modules = mbi->mods_count;

  for (unsigned mod = 2; mod < num_modules; ++mod)
    {
      cxx::String opts;
      cxx::String mod_name
        = cmdline_to_name(reinterpret_cast<char const *>(modules[mod].cmdline));
      if (mod_name != name)
        continue;

      l4_addr_t start = modules[mod].mod_start;
      l4_addr_t end = modules[mod].mod_end;
      if (size)
        *size = end - start;

#ifdef CONFIG_TINIT_RUN_ROOTTASK
      end = l4_round_page(end) - 1U; // address is inclusive
      if (l4util_splitlog2_hdl(start, end, s0_request_ram) < 0)
        return nullptr;
#else
      l4_touch_ro(reinterpret_cast<void *>(start), end - start);
#endif
      return reinterpret_cast<char const *>(start);
    }

  return nullptr;
}
