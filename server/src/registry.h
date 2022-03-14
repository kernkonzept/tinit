/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/sys/cxx/ipc_epiface>

class My_registry : public L4::Basic_registry
{
public:
  My_registry(L4::Ipc_svr::Server_iface *sif)
  : _sif(sif)
  {}

  L4::Cap<void> register_obj(L4::Epiface *o);

private:
  L4::Ipc_svr::Server_iface *_sif;
};
