/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/sys/factory>

#include "cap_alloc.h"
#include "debug.h"
#include "vm_irq.h"

Vm_irq::Vm_irq(unsigned line)
: _irq(Util::cap_alloc.alloc<L4::Irq>())
{
  L4::Cap<L4::Factory> factory(L4_BASE_FACTORY_CAP);
  if (l4_error(factory->create(_irq)) < 0)
    Fatal().panic("Cannot create irq\n");

  L4::Cap<L4::Icu> icu(L4_BASE_ICU_CAP);
  int ret = l4_error(icu->bind(line, _irq));
  if (ret < 0)
    Fatal().panic("Cannot bind irq\n");
  else if (ret > 0)
    Fatal().panic("Invalid eoi mode\n");
}
