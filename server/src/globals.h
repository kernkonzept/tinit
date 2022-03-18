/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#pragma once

#include <l4/sys/consts.h>

enum
{
  Sigma0_cap     = L4_BASE_PAGER_CAP,
  First_free_cap = 0x100 << L4_CAP_SHIFT,
};
