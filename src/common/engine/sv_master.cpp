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
#include <cstring>
#include <string>
#include <string_view>
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
#include "hcde_master_protocol.h"

CVAR(Int, sv_natport, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

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

namespace
{
constexpr u_short MASTERPORT = hcde::master_protocol::DefaultMasterPort;
constexpr uint64_t MASTER_RERESOLVE_MS = 1000ull * hcde::master_protocol::ServerAddressReresolveIntervalSeconds;
constexpr uint64_t MASTER_HEARTBEAT_MS = 1000ull * hcde::master_protocol::ServerHeartbeatIntervalSeconds;

static constexpr std::string_view def_masterlist[] = {
	hcde::master_protocol::DefaultMasterHost
};

struct masterserver
{
	std::string masterip;
	sockaddr_in masteraddr = {};
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
		const int portConversion = atoi(portName + 1);
		if (portConversion <= 0 || portConversion > UINT16_MAX)
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
	WriteShort(packet, offset, sv_natport > 0 ? static_cast<uint16_t>(sv_natport) : I_GetGamePort());

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
		closesocket(MasterSocket);
		MasterSocket = INVALID_SOCKET;
	}
}

void SV_UpdateMasterServers(void)
{
	for (const auto& master : masters)
		SendHeartbeat(master);
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
	for (const auto& master : masters)
	{
		Printf("%s [%s]\n", master.masterip.c_str(), inet_ntoa(master.masteraddr.sin_addr));
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
			if (!ResolveMasterAddress(master))
				Printf("Failed to re-resolve master server: %s\n", master.masterip.c_str());
		}

		last_address_resolution = current_time;
	}

	if (force || current_time - last_keep_alive >= MASTER_HEARTBEAT_MS)
	{
		DebugTrace::Markf("master", "heartbeat cycle %zu masters", masters.size());
		SV_UpdateMasterServers();

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
