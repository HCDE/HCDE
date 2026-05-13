/*
** sv_master_nms1.cpp
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

#include "sv_master_nms1.h"

#include <cstring>
#include <string>
#include <utility>

namespace
{
using hcde::master_protocol::Nms1FieldType;
using hcde::master_protocol::Nms1HeaderSize;
using hcde::master_protocol::Nms1MaxPacketSize;
using hcde::master_protocol::Nms1MessageType;

void WriteBE16(uint8_t* dest, uint16_t value)
{
	dest[0] = uint8_t(value >> 8);
	dest[1] = uint8_t(value);
}

void WriteBE32(uint8_t* dest, uint32_t value)
{
	dest[0] = uint8_t(value >> 24);
	dest[1] = uint8_t(value >> 16);
	dest[2] = uint8_t(value >> 8);
	dest[3] = uint8_t(value);
}

uint16_t ReadBE16(const uint8_t* data)
{
	return uint16_t(uint16_t(data[0]) << 8) | uint16_t(data[1]);
}

uint32_t ReadBE32(const uint8_t* data)
{
	return (uint32_t(data[0]) << 24)
		| (uint32_t(data[1]) << 16)
		| (uint32_t(data[2]) << 8)
		| uint32_t(data[3]);
}

bool TextFits(std::string_view value, size_t maxBytes)
{
	if (value.size() > maxBytes)
		return false;
	for (char c : value)
	{
		const unsigned char uc = static_cast<unsigned char>(c);
		if (uc == 0 || (uc < 0x20 && uc != '\t'))
			return false;
	}
	return true;
}

class PacketWriter
{
public:
	PacketWriter(hcde::master_nms1::PacketBuffer& out, Nms1MessageType type, uint32_t requestId)
		: Out(out)
	{
		Out.Bytes.fill(0);
		Out.Size = Nms1HeaderSize;
		std::memcpy(Out.Bytes.data(), hcde::master_protocol::Nms1Magic, sizeof(hcde::master_protocol::Nms1Magic));
		Out.Bytes[4] = hcde::master_protocol::Nms1Version;
		Out.Bytes[5] = static_cast<uint8_t>(type);
		WriteBE16(&Out.Bytes[6], 0);
		WriteBE32(&Out.Bytes[8], requestId);
		WriteBE16(&Out.Bytes[12], 0);
		WriteBE16(&Out.Bytes[14], 0);
	}

	bool WriteU8(Nms1FieldType type, uint8_t value)
	{
		return WriteField(type, &value, 1);
	}

	bool WriteU16(Nms1FieldType type, uint16_t value)
	{
		uint8_t bytes[2] = {};
		WriteBE16(bytes, value);
		return WriteField(type, bytes, sizeof(bytes));
	}

	bool WriteU32(Nms1FieldType type, uint32_t value)
	{
		uint8_t bytes[4] = {};
		WriteBE32(bytes, value);
		return WriteField(type, bytes, sizeof(bytes));
	}

	bool WriteBytes(Nms1FieldType type, const uint8_t* data, size_t length)
	{
		return WriteField(type, data, length);
	}

	bool WriteText(Nms1FieldType type, std::string_view value, size_t maxBytes)
	{
		if (!TextFits(value, maxBytes))
			return false;
		return WriteField(type, reinterpret_cast<const uint8_t*>(value.data()), value.size());
	}

	bool Finish()
	{
		if (Out.Size < Nms1HeaderSize || Out.Size > Nms1MaxPacketSize)
			return false;
		const size_t payloadLength = Out.Size - Nms1HeaderSize;
		if (payloadLength > UINT16_MAX)
			return false;
		WriteBE16(&Out.Bytes[12], static_cast<uint16_t>(payloadLength));
		return true;
	}

private:
	bool WriteField(Nms1FieldType type, const uint8_t* data, size_t length)
	{
		if (length > UINT16_MAX || Out.Size + 4u + length > Nms1MaxPacketSize)
			return false;
		if (length > 0 && data == nullptr)
			return false;

		WriteBE16(&Out.Bytes[Out.Size], static_cast<uint16_t>(type));
		WriteBE16(&Out.Bytes[Out.Size + 2u], static_cast<uint16_t>(length));
		Out.Size += 4u;
		if (length > 0)
		{
			std::memcpy(&Out.Bytes[Out.Size], data, length);
			Out.Size += length;
		}
		return true;
	}

	hcde::master_nms1::PacketBuffer& Out;
};

struct FieldView
{
	const uint8_t* Data = nullptr;
	size_t Length = 0;
	bool Found = false;
	bool Duplicate = false;
};

struct PacketView
{
	Nms1MessageType Type = Nms1MessageType::Error;
	uint32_t RequestId = 0;
	const uint8_t* Payload = nullptr;
	size_t PayloadLength = 0;
};

bool TryReadHeader(const uint8_t* data, size_t length, PacketView& packet)
{
	if (data == nullptr || length < Nms1HeaderSize || length > Nms1MaxPacketSize)
		return false;
	if (std::memcmp(data, hcde::master_protocol::Nms1Magic, sizeof(hcde::master_protocol::Nms1Magic)) != 0)
		return false;
	if (data[4] != hcde::master_protocol::Nms1Version)
		return false;
	const uint8_t rawType = data[5];
	if (rawType < static_cast<uint8_t>(Nms1MessageType::ChallengeRequest)
		|| rawType > static_cast<uint8_t>(Nms1MessageType::Error))
	{
		return false;
	}
	if (ReadBE16(&data[6]) != 0 || ReadBE16(&data[14]) != 0)
		return false;

	const uint16_t payloadLength = ReadBE16(&data[12]);
	if (payloadLength != length - Nms1HeaderSize)
		return false;

	packet.Type = static_cast<Nms1MessageType>(rawType);
	packet.RequestId = ReadBE32(&data[8]);
	packet.Payload = data + Nms1HeaderSize;
	packet.PayloadLength = payloadLength;
	return true;
}

bool FindField(const PacketView& packet, Nms1FieldType type, FieldView& out)
{
	out = {};
	size_t offset = 0;
	while (offset < packet.PayloadLength)
	{
		if (packet.PayloadLength - offset < 4u)
			return false;
		const uint16_t rawType = ReadBE16(packet.Payload + offset);
		const uint16_t fieldLength = ReadBE16(packet.Payload + offset + 2u);
		offset += 4u;
		if (fieldLength > packet.PayloadLength - offset)
			return false;
		if (rawType == static_cast<uint16_t>(type))
		{
			if (out.Found)
			{
				out.Duplicate = true;
				return false;
			}
			out.Data = packet.Payload + offset;
			out.Length = fieldLength;
			out.Found = true;
		}
		offset += fieldLength;
	}
	return true;
}

bool ReadU16Field(const PacketView& packet, Nms1FieldType type, uint16_t& value)
{
	FieldView field = {};
	if (!FindField(packet, type, field) || !field.Found || field.Length != 2u)
		return false;
	value = ReadBE16(field.Data);
	return true;
}

bool ReadU32Field(const PacketView& packet, Nms1FieldType type, uint32_t& value)
{
	FieldView field = {};
	if (!FindField(packet, type, field) || !field.Found || field.Length != 4u)
		return false;
	value = ReadBE32(field.Data);
	return true;
}

bool ReadBytesField(const PacketView& packet, Nms1FieldType type, uint8_t* dest, size_t length)
{
	FieldView field = {};
	if (!FindField(packet, type, field) || !field.Found || field.Length != length)
		return false;
	std::memcpy(dest, field.Data, length);
	return true;
}

bool ReadOptionalTextField(const PacketView& packet, Nms1FieldType type, size_t maxBytes, std::string& value)
{
	value.clear();
	FieldView field = {};
	if (!FindField(packet, type, field))
		return false;
	if (!field.Found)
		return true;
	if (field.Length > maxBytes)
		return false;
	for (size_t i = 0; i < field.Length; ++i)
	{
		const unsigned char c = field.Data[i];
		if (c == 0 || (c < 0x20 && c != '\t'))
			return false;
	}
	value.assign(reinterpret_cast<const char*>(field.Data), field.Length);
	return true;
}

bool ReadErrorResponse(const PacketView& packet, hcde::master_nms1::ErrorResponse* error)
{
	if (error == nullptr)
		return true;
	error->Code = 0;
	error->Text.clear();
	uint16_t code = 0;
	if (!ReadU16Field(packet, Nms1FieldType::ErrorCode, code))
		return false;
	std::string text;
	if (!ReadOptionalTextField(packet, Nms1FieldType::ErrorText, 96u, text))
		return false;
	error->Code = code;
	error->Text = std::move(text);
	return true;
}

hcde::master_nms1::ParseResult ReadExpectedPacket(
	const uint8_t* data,
	size_t length,
	uint32_t requestId,
	Nms1MessageType expectedType,
	PacketView& packet,
	hcde::master_nms1::ErrorResponse* error)
{
	if (!TryReadHeader(data, length, packet))
		return hcde::master_nms1::ParseResult::Malformed;
	if (packet.RequestId != requestId)
		return hcde::master_nms1::ParseResult::NotForRequest;
	if (packet.Type == Nms1MessageType::Error)
		return ReadErrorResponse(packet, error)
			? hcde::master_nms1::ParseResult::ErrorResponse
			: hcde::master_nms1::ParseResult::Malformed;
	if (packet.Type != expectedType)
		return hcde::master_nms1::ParseResult::Malformed;
	return hcde::master_nms1::ParseResult::Ok;
}
}

namespace hcde::master_nms1
{
bool IsValidProtocolFamily(std::string_view value)
{
	if (value.empty() || value.size() > master_protocol::Nms1MaxProtocolFamilyBytes)
		return false;
	for (char c : value)
	{
		const bool ok = (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9')
			|| c == '.'
			|| c == '_'
			|| c == '-';
		if (!ok)
			return false;
	}
	return true;
}

bool WriteChallengeRequest(uint32_t requestId, master_protocol::Nms1ChallengePurpose purpose, PacketBuffer& out)
{
	PacketWriter writer(out, master_protocol::Nms1MessageType::ChallengeRequest, requestId);
	return writer.WriteU8(master_protocol::Nms1FieldType::Purpose, static_cast<uint8_t>(purpose))
		&& writer.Finish();
}

bool WriteRegisterRequest(uint32_t requestId, const RegisterRequest& request, PacketBuffer& out)
{
	if (!IsValidProtocolFamily(request.ProtocolFamily)
		|| request.GamePort == 0
		|| request.QueryPort == 0
		|| !TextFits(request.BuildLabel, master_protocol::Nms1MaxBuildLabelBytes)
		|| !TextFits(request.DisplayName, master_protocol::Nms1MaxDisplayNameBytes))
	{
		out.Size = 0;
		return false;
	}

	PacketWriter writer(out, master_protocol::Nms1MessageType::Register, requestId);
	if (!writer.WriteU32(master_protocol::Nms1FieldType::ChallengeIssuedUnix, request.Challenge.IssuedUnix)
		|| !writer.WriteBytes(master_protocol::Nms1FieldType::ChallengeToken, request.Challenge.Token.data(), request.Challenge.Token.size())
		|| !writer.WriteText(master_protocol::Nms1FieldType::ProtocolFamily, request.ProtocolFamily, master_protocol::Nms1MaxProtocolFamilyBytes)
		|| !writer.WriteU16(master_protocol::Nms1FieldType::GamePort, request.GamePort)
		|| !writer.WriteU16(master_protocol::Nms1FieldType::QueryPort, request.QueryPort)
		|| !writer.WriteU16(master_protocol::Nms1FieldType::CurrentPlayers, request.CurrentPlayers)
		|| !writer.WriteU16(master_protocol::Nms1FieldType::MaxPlayers, request.MaxPlayers)
		|| !writer.WriteU32(master_protocol::Nms1FieldType::ServerFlags, request.ServerFlags))
	{
		out.Size = 0;
		return false;
	}

	if (!request.BuildLabel.empty()
		&& !writer.WriteText(master_protocol::Nms1FieldType::BuildLabel, request.BuildLabel, master_protocol::Nms1MaxBuildLabelBytes))
	{
		out.Size = 0;
		return false;
	}

	if (!request.DisplayName.empty()
		&& !writer.WriteText(master_protocol::Nms1FieldType::DisplayName, request.DisplayName, master_protocol::Nms1MaxDisplayNameBytes))
	{
		out.Size = 0;
		return false;
	}

	return writer.Finish();
}

bool WriteHeartbeatRequest(uint32_t requestId, const HeartbeatRequest& request, PacketBuffer& out)
{
	if (!IsValidProtocolFamily(request.ProtocolFamily) || request.GamePort == 0)
	{
		out.Size = 0;
		return false;
	}

	PacketWriter writer(out, master_protocol::Nms1MessageType::Heartbeat, requestId);
	return writer.WriteText(master_protocol::Nms1FieldType::ProtocolFamily, request.ProtocolFamily, master_protocol::Nms1MaxProtocolFamilyBytes)
		&& writer.WriteU16(master_protocol::Nms1FieldType::GamePort, request.GamePort)
		&& writer.WriteBytes(master_protocol::Nms1FieldType::EntryToken, request.Entry.Token.data(), request.Entry.Token.size())
		&& writer.WriteU16(master_protocol::Nms1FieldType::CurrentPlayers, request.CurrentPlayers)
		&& writer.WriteU16(master_protocol::Nms1FieldType::MaxPlayers, request.MaxPlayers)
		&& writer.WriteU32(master_protocol::Nms1FieldType::ServerFlags, request.ServerFlags)
		&& writer.Finish();
}

bool WriteUnregisterRequest(uint32_t requestId, const UnregisterRequest& request, PacketBuffer& out)
{
	if (!IsValidProtocolFamily(request.ProtocolFamily) || request.GamePort == 0)
	{
		out.Size = 0;
		return false;
	}

	PacketWriter writer(out, master_protocol::Nms1MessageType::Unregister, requestId);
	return writer.WriteText(master_protocol::Nms1FieldType::ProtocolFamily, request.ProtocolFamily, master_protocol::Nms1MaxProtocolFamilyBytes)
		&& writer.WriteU16(master_protocol::Nms1FieldType::GamePort, request.GamePort)
		&& writer.WriteBytes(master_protocol::Nms1FieldType::EntryToken, request.Entry.Token.data(), request.Entry.Token.size())
		&& writer.Finish();
}

ParseResult ReadChallengeResponse(const uint8_t* data, size_t length, uint32_t requestId, ChallengeToken& out, uint16_t& ttlSeconds, ErrorResponse* error)
{
	PacketView packet = {};
	const ParseResult result = ReadExpectedPacket(data, length, requestId, master_protocol::Nms1MessageType::ChallengeResponse, packet, error);
	if (result != ParseResult::Ok)
		return result;

	uint32_t issued = 0;
	uint16_t ttl = 0;
	if (!ReadU32Field(packet, master_protocol::Nms1FieldType::ChallengeIssuedUnix, issued)
		|| !ReadBytesField(packet, master_protocol::Nms1FieldType::ChallengeToken, out.Token.data(), out.Token.size())
		|| !ReadU16Field(packet, master_protocol::Nms1FieldType::TtlSeconds, ttl))
	{
		return ParseResult::Malformed;
	}

	out.IssuedUnix = issued;
	ttlSeconds = ttl;
	return ParseResult::Ok;
}

ParseResult ReadRegisterAck(const uint8_t* data, size_t length, uint32_t requestId, EntryToken& out, uint16_t& ttlSeconds, ErrorResponse* error)
{
	PacketView packet = {};
	const ParseResult result = ReadExpectedPacket(data, length, requestId, master_protocol::Nms1MessageType::RegisterAck, packet, error);
	if (result != ParseResult::Ok)
		return result;

	uint16_t ttl = 0;
	if (!ReadBytesField(packet, master_protocol::Nms1FieldType::EntryToken, out.Token.data(), out.Token.size())
		|| !ReadU16Field(packet, master_protocol::Nms1FieldType::TtlSeconds, ttl))
	{
		return ParseResult::Malformed;
	}

	ttlSeconds = ttl;
	return ParseResult::Ok;
}

ParseResult ReadHeartbeatAck(const uint8_t* data, size_t length, uint32_t requestId, uint16_t& ttlSeconds, ErrorResponse* error)
{
	PacketView packet = {};
	const ParseResult result = ReadExpectedPacket(data, length, requestId, master_protocol::Nms1MessageType::HeartbeatAck, packet, error);
	if (result != ParseResult::Ok)
		return result;

	uint16_t ttl = 0;
	if (!ReadU16Field(packet, master_protocol::Nms1FieldType::TtlSeconds, ttl))
		return ParseResult::Malformed;

	ttlSeconds = ttl;
	return ParseResult::Ok;
}

ParseResult ReadUnregisterAck(const uint8_t* data, size_t length, uint32_t requestId, ErrorResponse* error)
{
	PacketView packet = {};
	const ParseResult result = ReadExpectedPacket(data, length, requestId, master_protocol::Nms1MessageType::UnregisterAck, packet, error);
	if (result != ParseResult::Ok)
		return result;
	return packet.PayloadLength == 0u ? ParseResult::Ok : ParseResult::Malformed;
}
}
