/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "cap_alloc.h"
#include "globals.h"

namespace Util {

Cap_alloc::Cap_alloc()
: _next_cap(First_free_cap)
{}

Cap_alloc cap_alloc;

}
