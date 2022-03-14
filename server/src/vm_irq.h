/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/sys/irq>

class Vm_irq
{
public:
  explicit Vm_irq(unsigned line);

  L4::Cap<L4::Irq> cap() const { return _irq; }

private:
  L4::Cap<L4::Irq> _irq;
};
