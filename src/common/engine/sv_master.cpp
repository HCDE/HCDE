/*
** sv_master.cpp
**
** Master-server heartbeat support.
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

#include <cstdio>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <windows.h>
#	include <winsock.h>
#else
#	include <arpa/inet.h>
#	include <errno.h>
#	include <netdb.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#	include <unistd.h>
#endif

#include "c_cvars.h"
#include "c_dispatch.h"
#include "debugtrace.h"
#include "i_net.h"
#include "i_system.h"
#include "i_time.h"
#include "m_argv.h"
#include "printf.h"
#include "sv_master.h"
#include "sv_master_nms1.h"
#include "hcde_master_protocol.h"

CVAR(Int, sv_natport, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Bool, sv_upnp, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self)
	{
		Printf("HCDE server: UPnP port mapping is not implemented yet; forward the server port manually.\n");
	}
}

#ifndef _WIN32
typedef int SOCKET;
#define SOCKET_ERROR		-1
#define INVALID_SOCKET		-1
#define closesocket			close
#define neterror() strerror(errno)
#else
static const char* neterror()
{
	static char neterr[32];
	snprintf(neterr, sizeof(neterr), "WSA error %d", WSAGetLastError());
	return neterr;
}
#endif

#ifdef _WIN32
using hcde_socklen_t = int;
#else
using hcde_socklen_t = socklen_t;
#endif

namespace
{
constexpr u_short MASTERPORT = hcde::master_protocol::DefaultMasterPort;
constexpr uint64_t MASTER_RERESOLVE_MS = 1000ull * hcde::master_protocol::ServerAddressReresolveIntervalSeconds;
constexpr uint64_t MASTER_HEARTBEAT_MS = 1000ull * hcde::master_protocol::ServerHeartbeatIntervalSeconds;
constexpr uint64_t MASTER_NMS1_REGISTER_RETRY_MS = 30000ull;
constexpr int MASTER_NMS1_RESPONSE_TIMEOUT_MS = 750;

static constexpr std::string_view def_masterlist[] = {
	hcde::master_protocol::DefaultMasterHost
};

struct masterserver
{
	std::string masterip;
	sockaddr_in masteraddr = {};
	uint32_t nms1NextRequestId = 1u;
	bool nms1Registered = false;
	hcde::master_nms1::EntryToken nms1Entry = {};
	uint16_t nms1GamePort = 0u;
	uint16_t nms1TtlSeconds = 0u;
	uint64_t nms1LastRefresh = 0u;
	uint64_t lastNms1RegisterAttempt = 0u;
	std::string nms1LastError = {};
};

enum class Nms1HeartbeatResult
{
	Refreshed,
	RetryLater,
	NeedsRegister
};

static SOCKET MasterSocket = INVALID_SOCKET;
static std::vector<masterserver> masters = {};

static bool IsMasterName(const std::string& host)
{
	for (char c : host)
	{
		if ((c < '0' || c > '9') && c != '.')
			return true;
	}

	return false;
}

static bool ResolveMasterAddress(masterserver& master)
{
	std::string host = master.masterip;
	u_short port = MASTERPORT;
	const char* portName = strchr(master.masterip.c_str(), ':');
	if (portName != nullptr)
	{
		host.assign(master.masterip.c_str(), portName - master.masterip.c_str());
		char* portEnd = nullptr;
		const long portConversion = strtol(portName + 1, &portEnd, 10);
		if (portName[1] == 0 || portEnd == portName + 1 || *portEnd != 0 || portConversion <= 0 || portConversion > UINT16_MAX)
		{
			Printf("Malformed master port: %s (using %u)\n", portName + 1, MASTERPORT);
		}
		else
		{
			port = static_cast<u_short>(portConversion);
		}
	}

	sockaddr_in resolved = {};
	resolved.sin_family = AF_INET;
	resolved.sin_port = htons(port);

	if (!IsMasterName(host))
	{
		resolved.sin_addr.s_addr = inet_addr(host.c_str());
		if (resolved.sin_addr.s_addr == INADDR_NONE && host != "255.255.255.255")
		{
			DebugTrace::Markf("master", "resolve ip failed %s", master.masterip.c_str());
			return false;
		}
	}
	else
	{
		hostent* hostEntry = gethostbyname(host.c_str());
		if (hostEntry == nullptr || hostEntry->h_addr_list == nullptr || hostEntry->h_addr_list[0] == nullptr)
		{
			DebugTrace::Markf("master", "resolve dns failed %s", master.masterip.c_str());
			return false;
		}

		memcpy(&resolved.sin_addr.s_addr, hostEntry->h_addr_list[0], sizeof(resolved.sin_addr.s_addr));
	}

	master.masteraddr = resolved;
	return true;
}

static SOCKET CreateMasterSocket()
{
	SOCKET s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
		Printf("Master advertising disabled: couldn't create socket: %s\n", neterror());

	return s;
}

static uint16_t GetAdvertisedGamePort()
{
	if (sv_natport > 0 && sv_natport <= UINT16_MAX)
		return static_cast<uint16_t>(sv_natport);
	return I_GetGamePort();
}

static bool SameMasterAddress(const sockaddr_in& left, const sockaddr_in& right)
{
	return left.sin_family == right.sin_family
		&& left.sin_port == right.sin_port
		&& left.sin_addr.s_addr == right.sin_addr.s_addr;
}

static void ResetNms1Registration(masterserver& master)
{
	master.nms1Registered = false;
	master.nms1Entry = {};
	master.nms1GamePort = 0u;
	master.nms1TtlSeconds = 0u;
	master.nms1LastRefresh = 0u;
	master.lastNms1RegisterAttempt = 0u;
}

static void ClearNms1Error(masterserver& master)
{
	master.nms1LastError.clear();
}

static void SetNms1Error(masterserver& master, std::string_view text)
{
	master.nms1LastError.assign(text.begin(), text.end());
}

static void SetNms1Error(masterserver& master, std::string_view operation, const hcde::master_nms1::ErrorResponse& error)
{
	master.nms1LastError.assign(operation.begin(), operation.end());
	if (!error.Text.empty())
	{
		master.nms1LastError += ": ";
		master.nms1LastError += error.Text;
	}
	else
	{
		char codeText[32] = {};
		snprintf(codeText, sizeof(codeText), ": error %u", error.Code);
		master.nms1LastError += codeText;
	}
}

static uint64_t ElapsedSecondsSince(uint64_t timestamp, uint64_t now)
{
	if (timestamp == 0u || now < timestamp)
		return 0u;
	return (now - timestamp) / 1000u;
}

static uint16_t RemainingNms1Ttl(const masterserver& master, uint64_t now)
{
	const uint64_t elapsed = ElapsedSecondsSince(master.nms1LastRefresh, now);
	if (elapsed >= master.nms1TtlSeconds)
		return 0u;
	return static_cast<uint16_t>(master.nms1TtlSeconds - elapsed);
}

static uint32_t NextNms1RequestId(masterserver& master)
{
	const uint32_t requestId = master.nms1NextRequestId++;
	if (master.nms1NextRequestId == 0u)
		master.nms1NextRequestId = 1u;
	return requestId == 0u ? NextNms1RequestId(master) : requestId;
}

static std::string MakeNms1Text(const char* raw, size_t maxBytes, const char* fallback)
{
	std::string text;
	if (raw != nullptr)
	{
		for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(raw);
			*cursor != 0u && text.size() < maxBytes;
			++cursor)
		{
			const unsigned char c = *cursor;
			if (c >= 0x20u || c == '\t')
			{
				text.push_back(static_cast<char>(c));
			}
			else if (!text.empty() && text.back() != ' ')
			{
				text.push_back(' ');
			}
		}
	}

	while (!text.empty() && text.front() == ' ')
		text.erase(text.begin());
	while (!text.empty() && text.back() == ' ')
		text.pop_back();

	if (text.empty() && fallback != nullptr && fallback[0] != 0)
		return MakeNms1Text(fallback, maxBytes, nullptr);
	return text;
}

static bool SendNms1Packet(const masterserver& master, const hcde::master_nms1::PacketBuffer& packet, const char* label)
{
	if (MasterSocket == INVALID_SOCKET || packet.Empty())
		return false;

	if (sendto(MasterSocket, reinterpret_cast<const char*>(packet.Data()), static_cast<int>(packet.Size), 0,
		reinterpret_cast<const sockaddr*>(&master.masteraddr), sizeof(master.masteraddr)) == SOCKET_ERROR)
	{
		DebugTrace::Markf("master", "nms1 %s send failed %s", label, master.masterip.c_str());
		Printf("Failed to contact master server %s: %s\n", master.masterip.c_str(), neterror());
		return false;
	}

	DebugTrace::Markf("master", "nms1 %s sent %s", label, master.masterip.c_str());
	return true;
}

static bool ReceiveNms1Reply(const masterserver& master, std::array<uint8_t, hcde::master_protocol::Nms1MaxPacketSize>& reply, int& replySize, uint64_t deadline)
{
	for (;;)
	{
		const uint64_t now = I_msTime();
		if (now >= deadline)
			return false;

		const uint64_t remaining = deadline - now;
		timeval timeout = {};
		timeout.tv_sec = static_cast<long>(remaining / 1000u);
		timeout.tv_usec = static_cast<long>((remaining % 1000u) * 1000u);

		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(MasterSocket, &readset);
		const int selectResult = select(static_cast<int>(MasterSocket + 1), &readset, nullptr, nullptr, &timeout);
		if (selectResult == SOCKET_ERROR)
		{
#ifndef _WIN32
			if (errno == EINTR)
				continue;
#endif
			DebugTrace::Markf("master", "nms1 select failed %s", master.masterip.c_str());
			Printf("Failed to read master server response from %s: %s\n", master.masterip.c_str(), neterror());
			return false;
		}
		if (selectResult == 0)
			return false;

		sockaddr_in from = {};
		hcde_socklen_t fromSize = sizeof(from);
		replySize = recvfrom(MasterSocket, reinterpret_cast<char*>(reply.data()), static_cast<int>(reply.size()), 0,
			reinterpret_cast<sockaddr*>(&from), &fromSize);
		if (replySize == SOCKET_ERROR)
		{
#ifndef _WIN32
			if (errno == EINTR)
				continue;
#endif
			DebugTrace::Markf("master", "nms1 recv failed %s", master.masterip.c_str());
			Printf("Failed to read master server response from %s: %s\n", master.masterip.c_str(), neterror());
			return false;
		}

		if (!SameMasterAddress(from, master.masteraddr))
		{
			DebugTrace::Markf("master", "nms1 ignored response from unexpected address");
			continue;
		}

		return true;
	}
}

static void PrintNms1Error(const masterserver& master, const char* operation, const hcde::master_nms1::ErrorResponse& error)
{
	if (!error.Text.empty())
	{
		Printf("Master server %s rejected NMS1 %s: %u (%s)\n",
			master.masterip.c_str(), operation, error.Code, error.Text.c_str());
	}
	else
	{
		Printf("Master server %s rejected NMS1 %s: %u\n",
			master.masterip.c_str(), operation, error.Code);
	}
}

static bool IsStaleNms1TokenError(const hcde::master_nms1::ErrorResponse& error)
{
	return error.Code == static_cast<uint16_t>(hcde::master_protocol::Nms1ErrorCode::StaleEntry)
		|| error.Code == static_cast<uint16_t>(hcde::master_protocol::Nms1ErrorCode::TokenInvalid);
}

static bool WaitForNms1Challenge(masterserver& master, uint32_t requestId, hcde::master_nms1::ChallengeToken& challenge, uint16_t& ttlSeconds)
{
	const uint64_t deadline = I_msTime() + MASTER_NMS1_RESPONSE_TIMEOUT_MS;
	for (;;)
	{
		std::array<uint8_t, hcde::master_protocol::Nms1MaxPacketSize> reply = {};
		int replySize = 0;
		if (!ReceiveNms1Reply(master, reply, replySize, deadline))
			return false;

		hcde::master_nms1::ErrorResponse error = {};
		const hcde::master_nms1::ParseResult result = hcde::master_nms1::ReadChallengeResponse(
			reply.data(), static_cast<size_t>(replySize), requestId, challenge, ttlSeconds, &error);
		switch (result)
		{
		case hcde::master_nms1::ParseResult::Ok:
			return true;
		case hcde::master_nms1::ParseResult::NotForRequest:
			continue;
		case hcde::master_nms1::ParseResult::ErrorResponse:
			SetNms1Error(master, "challenge", error);
			PrintNms1Error(master, "challenge", error);
			return false;
		case hcde::master_nms1::ParseResult::Malformed:
			DebugTrace::Markf("master", "nms1 malformed challenge response %s", master.masterip.c_str());
			continue;
		}
	}
}

static bool WaitForNms1RegisterAck(masterserver& master, uint32_t requestId, hcde::master_nms1::EntryToken& entry, uint16_t& ttlSeconds)
{
	const uint64_t deadline = I_msTime() + MASTER_NMS1_RESPONSE_TIMEOUT_MS;
	for (;;)
	{
		std::array<uint8_t, hcde::master_protocol::Nms1MaxPacketSize> reply = {};
		int replySize = 0;
		if (!ReceiveNms1Reply(master, reply, replySize, deadline))
			return false;

		hcde::master_nms1::ErrorResponse error = {};
		const hcde::master_nms1::ParseResult result = hcde::master_nms1::ReadRegisterAck(
			reply.data(), static_cast<size_t>(replySize), requestId, entry, ttlSeconds, &error);
		switch (result)
		{
		case hcde::master_nms1::ParseResult::Ok:
			return true;
		case hcde::master_nms1::ParseResult::NotForRequest:
			continue;
		case hcde::master_nms1::ParseResult::ErrorResponse:
			SetNms1Error(master, "register", error);
			PrintNms1Error(master, "register", error);
			return false;
		case hcde::master_nms1::ParseResult::Malformed:
			DebugTrace::Markf("master", "nms1 malformed register ack %s", master.masterip.c_str());
			continue;
		}
	}
}

static Nms1HeartbeatResult WaitForNms1HeartbeatAck(masterserver& master, uint32_t requestId, uint16_t& ttlSeconds)
{
	const uint64_t deadline = I_msTime() + MASTER_NMS1_RESPONSE_TIMEOUT_MS;
	for (;;)
	{
		std::array<uint8_t, hcde::master_protocol::Nms1MaxPacketSize> reply = {};
		int replySize = 0;
		if (!ReceiveNms1Reply(master, reply, replySize, deadline))
			return Nms1HeartbeatResult::RetryLater;

		hcde::master_nms1::ErrorResponse error = {};
		const hcde::master_nms1::ParseResult result = hcde::master_nms1::ReadHeartbeatAck(
			reply.data(), static_cast<size_t>(replySize), requestId, ttlSeconds, &error);
		switch (result)
		{
		case hcde::master_nms1::ParseResult::Ok:
			return Nms1HeartbeatResult::Refreshed;
		case hcde::master_nms1::ParseResult::NotForRequest:
			continue;
		case hcde::master_nms1::ParseResult::ErrorResponse:
			SetNms1Error(master, "heartbeat", error);
			PrintNms1Error(master, "heartbeat", error);
			return IsStaleNms1TokenError(error) ? Nms1HeartbeatResult::NeedsRegister : Nms1HeartbeatResult::RetryLater;
		case hcde::master_nms1::ParseResult::Malformed:
			DebugTrace::Markf("master", "nms1 malformed heartbeat ack %s", master.masterip.c_str());
			continue;
		}
	}
}

static bool WaitForNms1UnregisterAck(masterserver& master, uint32_t requestId)
{
	const uint64_t deadline = I_msTime() + MASTER_NMS1_RESPONSE_TIMEOUT_MS;
	for (;;)
	{
		std::array<uint8_t, hcde::master_protocol::Nms1MaxPacketSize> reply = {};
		int replySize = 0;
		if (!ReceiveNms1Reply(master, reply, replySize, deadline))
			return false;

		hcde::master_nms1::ErrorResponse error = {};
		const hcde::master_nms1::ParseResult result = hcde::master_nms1::ReadUnregisterAck(
			reply.data(), static_cast<size_t>(replySize), requestId, &error);
		switch (result)
		{
		case hcde::master_nms1::ParseResult::Ok:
			return true;
		case hcde::master_nms1::ParseResult::NotForRequest:
			continue;
		case hcde::master_nms1::ParseResult::ErrorResponse:
			SetNms1Error(master, "unregister", error);
			if (error.Code != static_cast<uint16_t>(hcde::master_protocol::Nms1ErrorCode::StaleEntry))
				PrintNms1Error(master, "unregister", error);
			return error.Code == static_cast<uint16_t>(hcde::master_protocol::Nms1ErrorCode::StaleEntry);
		case hcde::master_nms1::ParseResult::Malformed:
			DebugTrace::Markf("master", "nms1 malformed unregister ack %s", master.masterip.c_str());
			continue;
		}
	}
}

static bool TryRegisterNms1Master(masterserver& master, bool force)
{
	if (MasterSocket == INVALID_SOCKET || master.nms1Registered)
		return master.nms1Registered;

	const uint64_t now = I_msTime();
	if (!force
		&& master.lastNms1RegisterAttempt != 0u
		&& now - master.lastNms1RegisterAttempt < MASTER_NMS1_REGISTER_RETRY_MS)
	{
		return false;
	}
	ClearNms1Error(master);
	master.lastNms1RegisterAttempt = now;

	hcde::master_nms1::PacketBuffer packet = {};
	const uint32_t challengeRequestId = NextNms1RequestId(master);
	if (!hcde::master_nms1::WriteChallengeRequest(challengeRequestId, hcde::master_protocol::Nms1ChallengePurpose::Registration, packet)
		|| !SendNms1Packet(master, packet, "challenge"))
	{
		SetNms1Error(master, "challenge send failed");
		return false;
	}

	hcde::master_nms1::ChallengeToken challenge = {};
	uint16_t challengeTtlSeconds = 0u;
	if (!WaitForNms1Challenge(master, challengeRequestId, challenge, challengeTtlSeconds))
	{
		if (master.nms1LastError.empty())
			SetNms1Error(master, "challenge timeout");
		DebugTrace::Markf("master", "nms1 challenge timed out %s", master.masterip.c_str());
		return false;
	}

	FServerQuerySnapshot snapshot = {};
	I_GetLocalServerSnapshot(snapshot);
	const std::string buildLabel = MakeNms1Text(snapshot.Version.GetChars(), hcde::master_protocol::Nms1MaxBuildLabelBytes, "HCDE");
	const std::string displayName = MakeNms1Text(snapshot.HostName.GetChars(), hcde::master_protocol::Nms1MaxDisplayNameBytes, "HCDE server");

	hcde::master_nms1::RegisterRequest request = {};
	request.Challenge = challenge;
	request.ProtocolFamily = hcde::master_protocol::Nms1DefaultProtocolFamily;
	request.GamePort = GetAdvertisedGamePort();
	request.QueryPort = request.GamePort;
	request.CurrentPlayers = snapshot.PlayerCount;
	request.MaxPlayers = snapshot.MaxPlayers;
	request.ServerFlags = 0u;
	request.BuildLabel = buildLabel;
	request.DisplayName = displayName;

	const uint32_t registerRequestId = NextNms1RequestId(master);
	if (!hcde::master_nms1::WriteRegisterRequest(registerRequestId, request, packet)
		|| !SendNms1Packet(master, packet, "register"))
	{
		SetNms1Error(master, "register send failed");
		return false;
	}

	hcde::master_nms1::EntryToken entry = {};
	uint16_t entryTtlSeconds = 0u;
	if (!WaitForNms1RegisterAck(master, registerRequestId, entry, entryTtlSeconds))
	{
		if (master.nms1LastError.empty())
			SetNms1Error(master, "register timeout");
		DebugTrace::Markf("master", "nms1 register timed out %s", master.masterip.c_str());
		return false;
	}

	master.nms1Entry = entry;
	master.nms1GamePort = request.GamePort;
	master.nms1TtlSeconds = entryTtlSeconds;
	master.nms1LastRefresh = I_msTime();
	master.nms1Registered = true;
	ClearNms1Error(master);
	Printf("Registered with HCDE master server %s (ttl=%u)\n", master.masterip.c_str(), entryTtlSeconds);
	DebugTrace::Markf("master", "nms1 registered %s ttl=%u", master.masterip.c_str(), entryTtlSeconds);
	return true;
}

static bool TryUnregisterNms1Master(masterserver& master)
{
	if (MasterSocket == INVALID_SOCKET || !master.nms1Registered)
		return false;

	hcde::master_nms1::UnregisterRequest request = {};
	request.ProtocolFamily = hcde::master_protocol::Nms1DefaultProtocolFamily;
	request.GamePort = master.nms1GamePort != 0u ? master.nms1GamePort : GetAdvertisedGamePort();
	request.Entry = master.nms1Entry;

	hcde::master_nms1::PacketBuffer packet = {};
	const uint32_t requestId = NextNms1RequestId(master);
	if (!hcde::master_nms1::WriteUnregisterRequest(requestId, request, packet)
		|| !SendNms1Packet(master, packet, "unregister"))
	{
		SetNms1Error(master, "unregister send failed");
		ResetNms1Registration(master);
		return false;
	}

	const bool ok = WaitForNms1UnregisterAck(master, requestId);
	if (ok)
	{
		DebugTrace::Markf("master", "nms1 unregistered %s", master.masterip.c_str());
	}
	else
	{
		if (master.nms1LastError.empty())
			SetNms1Error(master, "unregister timeout");
		DebugTrace::Markf("master", "nms1 unregister timed out %s", master.masterip.c_str());
	}

	ResetNms1Registration(master);
	return ok;
}

static Nms1HeartbeatResult TryHeartbeatNms1Master(masterserver& master)
{
	if (MasterSocket == INVALID_SOCKET || !master.nms1Registered)
		return Nms1HeartbeatResult::NeedsRegister;

	ClearNms1Error(master);
	const uint16_t advertisedPort = GetAdvertisedGamePort();
	if (master.nms1GamePort != 0u && master.nms1GamePort != advertisedPort)
	{
		TryUnregisterNms1Master(master);
		DebugTrace::Markf("master", "nms1 port changed, re-register %s", master.masterip.c_str());
		return Nms1HeartbeatResult::NeedsRegister;
	}

	FServerQuerySnapshot snapshot = {};
	I_GetLocalServerSnapshot(snapshot);

	hcde::master_nms1::HeartbeatRequest request = {};
	request.ProtocolFamily = hcde::master_protocol::Nms1DefaultProtocolFamily;
	request.GamePort = master.nms1GamePort != 0u ? master.nms1GamePort : advertisedPort;
	request.Entry = master.nms1Entry;
	request.CurrentPlayers = snapshot.PlayerCount;
	request.MaxPlayers = snapshot.MaxPlayers;
	request.ServerFlags = 0u;

	hcde::master_nms1::PacketBuffer packet = {};
	const uint32_t requestId = NextNms1RequestId(master);
	if (!hcde::master_nms1::WriteHeartbeatRequest(requestId, request, packet)
		|| !SendNms1Packet(master, packet, "heartbeat"))
	{
		SetNms1Error(master, "heartbeat send failed");
		return Nms1HeartbeatResult::RetryLater;
	}

	uint16_t ttlSeconds = 0u;
	const Nms1HeartbeatResult result = WaitForNms1HeartbeatAck(master, requestId, ttlSeconds);
	if (result == Nms1HeartbeatResult::Refreshed)
	{
		master.nms1TtlSeconds = ttlSeconds;
		master.nms1LastRefresh = I_msTime();
		ClearNms1Error(master);
		DebugTrace::Markf("master", "nms1 heartbeat ack %s ttl=%u", master.masterip.c_str(), ttlSeconds);
	}
	else if (result == Nms1HeartbeatResult::NeedsRegister)
	{
		ResetNms1Registration(master);
		DebugTrace::Markf("master", "nms1 heartbeat requested re-register %s", master.masterip.c_str());
	}
	else
	{
		if (master.nms1LastError.empty())
			SetNms1Error(master, "heartbeat timeout");
		DebugTrace::Markf("master", "nms1 heartbeat timed out %s", master.masterip.c_str());
	}

	return result;
}

static void WriteLong(std::array<uint8_t, hcde::master_protocol::ServerHeartbeatPacketSize>& packet, size_t& offset, uint32_t value)
{
	packet[offset++] = static_cast<uint8_t>(value & 0xffu);
	packet[offset++] = static_cast<uint8_t>((value >> 8) & 0xffu);
	packet[offset++] = static_cast<uint8_t>((value >> 16) & 0xffu);
	packet[offset++] = static_cast<uint8_t>((value >> 24) & 0xffu);
}

static void WriteShort(std::array<uint8_t, hcde::master_protocol::ServerHeartbeatPacketSize>& packet, size_t& offset, uint16_t value)
{
	packet[offset++] = static_cast<uint8_t>(value & 0xffu);
	packet[offset++] = static_cast<uint8_t>((value >> 8) & 0xffu);
}

static void SendHeartbeat(const masterserver& master)
{
	if (MasterSocket == INVALID_SOCKET)
		return;

	std::array<uint8_t, hcde::master_protocol::ServerHeartbeatPacketSize> packet = {};
	size_t offset = 0;
	WriteLong(packet, offset, hcde::master_protocol::ServerHeartbeatMarker);
	WriteShort(packet, offset, GetAdvertisedGamePort());

	if (sendto(MasterSocket, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
		reinterpret_cast<const sockaddr*>(&master.masteraddr), sizeof(master.masteraddr)) == SOCKET_ERROR)
	{
		DebugTrace::Markf("master", "heartbeat failed %s", master.masterip.c_str());
		Printf("Failed to contact master server %s: %s\n", master.masterip.c_str(), neterror());
	}
	else
	{
		DebugTrace::Markf("master", "heartbeat sent %s", master.masterip.c_str());
	}
}

static bool MasterListContains(std::string_view masterip)
{
	for (const auto& master : masters)
	{
		if (master.masterip.size() == masterip.size())
		{
			bool same = true;
			for (size_t i = 0; i < master.masterip.size(); ++i)
			{
				if (std::tolower(static_cast<unsigned char>(master.masterip[i])) != std::tolower(static_cast<unsigned char>(masterip[i])))
				{
					same = false;
					break;
				}
			}

			if (same)
				return true;
		}
	}

	return false;
}

static bool MasterMatchesPrefix(const std::string& masterip, const char* needle)
{
	const size_t needleLen = strlen(needle);
	if (needleLen > masterip.size())
		return false;

	for (size_t i = 0; i < needleLen; ++i)
	{
		if (std::tolower(static_cast<unsigned char>(masterip[i])) != std::tolower(static_cast<unsigned char>(needle[i])))
			return false;
	}

	return true;
}

static void SV_UpdateMasterServersInternal(bool force)
{
	for (auto& master : masters)
	{
		if (master.nms1Registered)
		{
			if (TryHeartbeatNms1Master(master) == Nms1HeartbeatResult::NeedsRegister)
				TryRegisterNms1Master(master, true);
		}
		else
		{
			TryRegisterNms1Master(master, force);
		}

		SendHeartbeat(master);
	}
}

static void SV_UnregisterMasterServersInternal()
{
	for (auto& master : masters)
		TryUnregisterNms1Master(master);
}
}

CVAR(Bool, sv_usemasters, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
FARG(master, "Multiplayer", "Adds an HCDE master server for this run.", "host[:port]",
	"Adds a master server before dedicated-server advertising starts. May be specified multiple times.");

static bool IsMissingStartupMasterValue(int arg)
{
	return arg + 1 >= Args->NumArgs()
		|| Args->GetArg(arg + 1) == nullptr
		|| Args->GetArg(arg + 1)[0] == '-'
		|| Args->GetArg(arg + 1)[0] == '+';
}

static void SV_AddStartupMasters()
{
	if (Args == nullptr)
		return;

	int start = 1;
	for (;;)
	{
		const int arg = Args->CheckParm(FArg_master, start);
		if (arg == 0)
			break;

		if (IsMissingStartupMasterValue(arg))
		{
			Printf("Missing value for -master; expected host[:port]\n");
			start = arg + 1;
			continue;
		}

		SV_AddMaster(Args->GetArg(arg + 1));
		start = arg + 2;
	}
}

CCMD(addmaster)
{
	if (argv.argc() <= 1)
	{
		Printf("addmaster <host[:port]>\n");
		return;
	}

	SV_AddMaster(argv[1]);
}

CCMD(delmaster)
{
	if (argv.argc() <= 1)
	{
		Printf("delmaster <host[:port]>\n");
		return;
	}

	SV_RemoveMaster(argv[1]);
}

CCMD(masters)
{
	SV_ListMasters();
}

void SV_InitMasters()
{
	DebugTrace::Markf("master", "init enabled=%d", sv_usemasters ? 1 : 0);
	if (!sv_usemasters)
	{
		Printf("Master advertising is disabled because sv_usemasters is 0\n");
		return;
	}

	if (MasterSocket == INVALID_SOCKET)
	{
		MasterSocket = CreateMasterSocket();
		if (MasterSocket == INVALID_SOCKET)
			return;
	}

	SV_AddStartupMasters();

	if (masters.empty())
	{
		for (std::string_view master : def_masterlist)
			SV_AddMaster(master);
	}

	SV_UpdateMaster(true);
}

void SV_ShutdownMasters()
{
	DebugTrace::Mark("master", "shutdown");
	if (MasterSocket != INVALID_SOCKET)
	{
		SV_UnregisterMasterServersInternal();
		closesocket(MasterSocket);
		MasterSocket = INVALID_SOCKET;
	}
}

void SV_UpdateMasterServers(void)
{
	SV_UpdateMasterServersInternal(false);
}

bool SV_AddMaster(std::string_view masterip)
{
	if (masterip.empty())
	{
		Printf("Failed to add master: empty address\n");
		return false;
	}

	if (MasterListContains(masterip))
	{
		Printf("Master %.*s is already on the list\n", static_cast<int>(masterip.size()), masterip.data());
		return false;
	}

	masterserver master = {};
	master.masterip.assign(masterip.begin(), masterip.end());
	if (!ResolveMasterAddress(master))
	{
		Printf("Failed to resolve master server: %s, not added\n", master.masterip.c_str());
		return false;
	}

	Printf("Added master: %s [%s]\n", master.masterip.c_str(), inet_ntoa(master.masteraddr.sin_addr));
	DebugTrace::Markf("master", "add %s", master.masterip.c_str());
	masters.push_back(std::move(master));
	SV_UpdateMaster(true);
	return true;
}

bool SV_RemoveMaster(const char* masterip)
{
	for (size_t index = 0; index < masters.size(); ++index)
	{
		if (MasterMatchesPrefix(masters[index].masterip, masterip))
		{
			TryUnregisterNms1Master(masters[index]);
			Printf("Removed master server: %s\n", masters[index].masterip.c_str());
			DebugTrace::Markf("master", "remove %s", masters[index].masterip.c_str());
			masters.erase(masters.begin() + index);
			SV_UpdateMaster(true);
			return true;
		}
	}

	Printf("Failed to remove master: %s, not in list\n", masterip);
	return false;
}

void SV_ListMasters()
{
	Printf("Use addmaster/delmaster commands to modify this list\n");
	const uint64_t now = I_msTime();
	for (const auto& master : masters)
	{
		if (master.nms1Registered)
		{
			const uint64_t refreshAgeSeconds = ElapsedSecondsSince(master.nms1LastRefresh, now);
			const uint16_t ttlRemaining = RemainingNms1Ttl(master, now);
			const char* state = ttlRemaining == 0u ? "registered-expired" : "registered";
			if (!master.nms1LastError.empty())
			{
				Printf("%s [%s] nms1=%s port=%u ttl=%us refreshed=%llus ago last=\"%s\"\n",
					master.masterip.c_str(),
					inet_ntoa(master.masteraddr.sin_addr),
					state,
					master.nms1GamePort,
					ttlRemaining,
					static_cast<unsigned long long>(refreshAgeSeconds),
					master.nms1LastError.c_str());
			}
			else
			{
				Printf("%s [%s] nms1=%s port=%u ttl=%us refreshed=%llus ago\n",
					master.masterip.c_str(),
					inet_ntoa(master.masteraddr.sin_addr),
					state,
					master.nms1GamePort,
					ttlRemaining,
					static_cast<unsigned long long>(refreshAgeSeconds));
			}
		}
		else if (!master.nms1LastError.empty())
		{
			Printf("%s [%s] nms1=pending last=\"%s\"\n",
				master.masterip.c_str(),
				inet_ntoa(master.masteraddr.sin_addr),
				master.nms1LastError.c_str());
		}
		else
		{
			Printf("%s [%s] nms1=pending\n", master.masterip.c_str(), inet_ntoa(master.masteraddr.sin_addr));
		}
	}
}

void SV_ArchiveMasters(FILE* fp)
{
	if (fp == nullptr)
		return;

	for (const auto& master : masters)
		fprintf(fp, "addmaster %s\n", master.masterip.c_str());
}

void SV_UpdateMaster(bool force)
{
	if (!sv_usemasters || MasterSocket == INVALID_SOCKET || masters.empty())
		return;

	const uint64_t current_time = I_msTime();
	static uint64_t last_address_resolution = 0;
	static uint64_t last_keep_alive = 0;

	if (force || current_time - last_address_resolution >= MASTER_RERESOLVE_MS)
	{
		DebugTrace::Markf("master", "re-resolve %zu masters", masters.size());
		for (auto& master : masters)
		{
			const sockaddr_in previousAddress = master.masteraddr;
			masterserver previousMaster = master;
			if (!ResolveMasterAddress(master))
			{
				Printf("Failed to re-resolve master server: %s\n", master.masterip.c_str());
			}
			else if (!SameMasterAddress(previousAddress, master.masteraddr))
			{
				previousMaster.masteraddr = previousAddress;
				TryUnregisterNms1Master(previousMaster);
				ResetNms1Registration(master);
			}
		}

		last_address_resolution = current_time;
	}

	if (force || current_time - last_keep_alive >= MASTER_HEARTBEAT_MS)
	{
		DebugTrace::Markf("master", "heartbeat cycle %zu masters", masters.size());
		SV_UpdateMasterServersInternal(force);

		last_keep_alive = current_time;
	}
}

bool SV_IsMasterAdvertisingEnabled()
{
	return sv_usemasters;
}

size_t SV_GetMasterCount()
{
	return masters.size();
}
