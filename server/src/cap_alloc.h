/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#pragma once

#include <l4/sys/capability>

namespace Util {

struct Cap_alloc
{
  Cap_alloc();

  template<typename T>
  L4::Cap<T> alloc()
  {
    l4_cap_idx_t ret = _next_cap;
    _next_cap += L4_CAP_SIZE;
    return L4::Cap<T>(ret);
  }

private:
  l4_cap_idx_t _next_cap;
};

extern Cap_alloc cap_alloc;

}
