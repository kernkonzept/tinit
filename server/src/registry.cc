/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/re/env>

#include "cap_alloc.h"
#include "registry.h"

L4::Cap<void> My_registry::register_obj(L4::Epiface *o)
{
  auto cap = Util::cap_alloc.alloc<L4::Kobject>();

  l4_umword_t id = l4_umword_t(o);
  int err = l4_error(L4Re::Env::env()->factory()
                     ->create_gate(cap, L4Re::Env::env()->main_thread(), id));
  if (err < 0)
    return L4::Cap<void>(err | L4_INVALID_CAP_BIT);

  err = o->set_server(_sif, cap, true);
  if (err < 0)
    return L4::Cap<void>(err | L4_INVALID_CAP_BIT);

  return cap;
}
