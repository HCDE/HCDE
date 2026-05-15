/*
** sv_master_nms1.h
**
** HCDE-side NMS1 master-server packet helpers.
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

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "hcde_master_protocol.h"

namespace hcde::master_nms1
{
struct ChallengeToken
{
	uint32_t IssuedUnix = 0;
	std::array<uint8_t, master_protocol::Nms1ChallengeTokenSize> Token = {};
};

struct EntryToken
{
	std::array<uint8_t, master_protocol::Nms1EntryTokenSize> Token = {};
};

struct PacketBuffer
{
	std::array<uint8_t, master_protocol::Nms1MaxPacketSize> Bytes = {};
	size_t Size = 0;

	const uint8_t* Data() const { return Bytes.data(); }
	uint8_t* Data() { return Bytes.data(); }
	bool Empty() const { return Size == 0; }
};

struct RegisterRequest
{
	ChallengeToken Challenge;
	std::string_view ProtocolFamily = master_protocol::Nms1DefaultProtocolFamily;
	uint16_t GamePort = 0;
	uint16_t QueryPort = 0;
	uint16_t CurrentPlayers = 0;
	uint16_t MaxPlayers = 0;
	uint32_t ServerFlags = 0;
	std::string_view BuildLabel = {};
	std::string_view DisplayName = {};
	std::string_view GameName = {};
	std::string_view MapName = {};
};

struct HeartbeatRequest
{
	std::string_view ProtocolFamily = master_protocol::Nms1DefaultProtocolFamily;
	uint16_t GamePort = 0;
	EntryToken Entry;
	uint16_t CurrentPlayers = 0;
	uint16_t MaxPlayers = 0;
	uint32_t ServerFlags = 0;
};

struct UnregisterRequest
{
	std::string_view ProtocolFamily = master_protocol::Nms1DefaultProtocolFamily;
	uint16_t GamePort = 0;
	EntryToken Entry;
};

enum class ParseResult
{
	Ok,
	NotForRequest,
	ErrorResponse,
	Malformed
};

struct ErrorResponse
{
	uint16_t Code = 0;
	std::string Text = {};
};

bool IsValidProtocolFamily(std::string_view value);
bool WriteChallengeRequest(uint32_t requestId, master_protocol::Nms1ChallengePurpose purpose, PacketBuffer& out);
bool WriteRegisterRequest(uint32_t requestId, const RegisterRequest& request, PacketBuffer& out);
bool WriteHeartbeatRequest(uint32_t requestId, const HeartbeatRequest& request, PacketBuffer& out);
bool WriteUnregisterRequest(uint32_t requestId, const UnregisterRequest& request, PacketBuffer& out);
ParseResult ReadChallengeResponse(const uint8_t* data, size_t length, uint32_t requestId, ChallengeToken& out, uint16_t& ttlSeconds, ErrorResponse* error);
ParseResult ReadRegisterAck(const uint8_t* data, size_t length, uint32_t requestId, EntryToken& out, uint16_t& ttlSeconds, ErrorResponse* error);
ParseResult ReadHeartbeatAck(const uint8_t* data, size_t length, uint32_t requestId, uint16_t& ttlSeconds, ErrorResponse* error);
ParseResult ReadUnregisterAck(const uint8_t* data, size_t length, uint32_t requestId, ErrorResponse* error);
}
