/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include "cap_alloc.h"
#include "globals.h"

namespace Util {

Cap_alloc::Cap_alloc()
: _next_cap(First_free_cap)
{}

Cap_alloc cap_alloc;

}
