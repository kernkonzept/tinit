/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
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
    _next_cap += L4_CAP_OFFSET;
    return L4::Cap<T>(ret);
  }

private:
  l4_cap_idx_t _next_cap;
};

extern Cap_alloc cap_alloc;

}
