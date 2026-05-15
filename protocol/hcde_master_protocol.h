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
inline constexpr uint32_t Version = 2u;

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

inline constexpr char Nms1Magic[4] = { 'N', 'M', 'S', '1' };
inline constexpr uint8_t Nms1Version = 1u;
inline constexpr uint16_t Nms1HeaderSize = 16u;
inline constexpr uint16_t Nms1ChallengeTokenSize = 32u;
inline constexpr uint16_t Nms1EntryTokenSize = 32u;
inline constexpr uint16_t Nms1MaxPacketSize = 1200u;
inline constexpr uint16_t Nms1MaxProtocolFamilyBytes = 32u;
inline constexpr uint16_t Nms1MaxBuildLabelBytes = 64u;
inline constexpr uint16_t Nms1MaxDisplayNameBytes = 96u;
inline constexpr uint16_t Nms1MaxGameNameBytes = 64u;
inline constexpr uint16_t Nms1MaxMapNameBytes = 64u;
inline constexpr const char Nms1DefaultProtocolFamily[] = "raw";

enum class Nms1MessageType : uint8_t
{
	ChallengeRequest = 1,
	ChallengeResponse = 2,
	Register = 3,
	RegisterAck = 4,
	Heartbeat = 5,
	HeartbeatAck = 6,
	Unregister = 7,
	UnregisterAck = 8,
	ListRequest = 9,
	ListResponse = 10,
	Error = 11
};

enum class Nms1ChallengePurpose : uint8_t
{
	Registration = 1,
	ListQuery = 2
};

enum class Nms1FieldType : uint16_t
{
	Purpose = 1,
	ChallengeIssuedUnix = 2,
	ChallengeToken = 3,
	ProtocolFamily = 16,
	GamePort = 17,
	QueryPort = 18,
	EntryToken = 19,
	CurrentPlayers = 20,
	MaxPlayers = 21,
	ServerFlags = 22,
	PublicIp = 23,
	BuildLabel = 24,
	DisplayName = 25,
	GameName = 26,
	MapName = 27,
	Cursor = 31,
	PageSize = 32,
	EntryCount = 33,
	TotalCount = 34,
	TtlSeconds = 35,
	ErrorCode = 100,
	ErrorText = 101,
	Entries = 200
};

enum class Nms1ErrorCode : uint16_t
{
	BadPacket = 1,
	UnsupportedVersion = 2,
	UnknownMessage = 3,
	MissingField = 4,
	InvalidField = 5,
	ChallengeRequired = 6,
	ChallengeInvalid = 7,
	RateLimited = 8,
	PrivateAddress = 9,
	EntryLimit = 10,
	StaleEntry = 11,
	TokenInvalid = 12,
	ServerBusy = 13
};
}

#endif
