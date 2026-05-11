/*
** hcde_master_protocol.h
**
** Protocol-only constants for HCDE master discovery.
**
** This header intentionally contains no engine, launcher, or master-server
** implementation code. It is safe to share as a neutral protocol contract.
**
**-----------------------------------------------------------------------------
**
** Copyright 2026 HCDE Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**-----------------------------------------------------------------------------
*/

#ifndef HCDE_MASTER_PROTOCOL_H
#define HCDE_MASTER_PROTOCOL_H

#include <cstdint>

namespace hcde::master_protocol
{
inline constexpr uint32_t Version = 1u;

inline constexpr const char DefaultMasterHost[] = "hcde.servebeer.com";
inline constexpr uint16_t DefaultMasterPort = 15000u;
inline constexpr int DefaultEntryTtlSeconds = 180;
inline constexpr int ServerHeartbeatIntervalSeconds = 25;
inline constexpr int ServerAddressReresolveIntervalSeconds = 10800;

inline constexpr uint32_t ServerHeartbeatMarker = 5560020u;
inline constexpr uint32_t LauncherListQueryMarker = 777123u;
inline constexpr uint32_t MasterListResponseMarker = 777123u;

inline constexpr uint16_t ServerHeartbeatPacketSize = 6u;
inline constexpr uint16_t LauncherListQueryPacketSize = 4u;
inline constexpr uint16_t MasterListResponseHeaderSize = 6u;
inline constexpr uint16_t MasterListResponseEntrySize = 6u;
}

#endif
