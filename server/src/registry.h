/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
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
