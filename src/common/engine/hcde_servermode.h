/*
** hcde_servermode.h
**
** HCDE dedicated-server runtime boundary.
**
**-----------------------------------------------------------------------------
**
** Copyright 2026 HCDE Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**-----------------------------------------------------------------------------
*/

#pragma once

#include <stddef.h>

void HCDE_ServerMode_InitFromArgs();
void HCDE_ServerMode_SetSelectedIWAD(const char* iwadName);
void HCDE_ServerMode_SetNetworkDetails(int visibleSlots, int internalSlots, int gamePort, bool reservedServerSlot, const char* flowState);
void HCDE_ServerMode_SetMasterState(bool advertisingEnabled, size_t configuredMasters);
void HCDE_ServerMode_PrintDiagnostics(const char* phase);
void HCDE_ServerMode_GuardClientSubsystem(const char* subsystem);

bool HCDE_ServerMode_IsDedicatedServer();
bool HCDE_ServerMode_IsDedicatedExecutable();
bool HCDE_ServerMode_IsDedicatedJoin();
bool HCDE_ServerMode_ShouldSuppressRoomUI();
bool HCDE_ServerMode_ShouldSuppressStartupUI();
