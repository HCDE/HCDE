/*
** sv_master.h
**
** Server master heartbeat and server list support.
**
**-----------------------------------------------------------------------------
**
** Copyright 2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**-----------------------------------------------------------------------------
**
*/

#pragma once

#include <cstdint>
#include <cstdio>
#include <string_view>

void SV_InitMasters();
void SV_ShutdownMasters();
void SV_ListMasters();
bool SV_AddMaster(std::string_view masterip);
bool SV_RemoveMaster(const char *masterip);
void SV_UpdateMasterServers(void);
void SV_UpdateMaster(bool force = false);
void SV_ArchiveMasters(FILE *fp);
