/*
** hcdemaster.cpp
**
** Standalone HCDE master server.
**
**-----------------------------------------------------------------------------
**
** Copyright 2026 HCDE Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**-----------------------------------------------------------------------------
*/

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "hcde_master_protocol.h"

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <winsock2.h>
#	include <ws2tcpip.h>
using socklen_hcde_t = int;
#else
#	include <arpa/inet.h>
#	include <cerrno>
#	include <netinet/in.h>
#	include <sys/socket.h>
#	include <unistd.h>
using SOCKET = int;
using socklen_hcde_t = socklen_t;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
#endif

namespace
{
constexpr uint16_t kDefaultMasterPort = hcde::master_protocol::DefaultMasterPort;
constexpr uint32_t kServerHeartbeat = hcde::master_protocol::ServerHeartbeatMarker;
constexpr uint32_t kMasterListChallenge = hcde::master_protocol::LauncherListQueryMarker;
constexpr uint32_t kMasterListResponse = hcde::master_protocol::MasterListResponseMarker;
constexpr int kDefaultTtlSeconds = hcde::master_protocol::DefaultEntryTtlSeconds;

volatile std::sig_atomic_t g_running = 1;

struct Options
{
	std::string bindAddress = "0.0.0.0";
	uint16_t port = kDefaultMasterPort;
	int ttlSeconds = kDefaultTtlSeconds;
	int maxPackets = 0;
	bool quiet = false;
};

struct ServerEntry
{
	uint32_t address = 0;
	uint16_t port = 0;
	std::chrono::steady_clock::time_point lastSeen {};
};

uint32_t ReadLE32(const uint8_t* data)
{
	return uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
}

uint16_t ReadLE16(const uint8_t* data)
{
	return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
}

void WriteLE32(std::vector<uint8_t>& data, uint32_t value)
{
	data.push_back(uint8_t(value));
	data.push_back(uint8_t(value >> 8));
	data.push_back(uint8_t(value >> 16));
	data.push_back(uint8_t(value >> 24));
}

void WriteLE16(std::vector<uint8_t>& data, uint16_t value)
{
	data.push_back(uint8_t(value));
	data.push_back(uint8_t(value >> 8));
}

void CloseSocket(SOCKET socketHandle)
{
	if (socketHandle == INVALID_SOCKET)
		return;
#ifdef _WIN32
	closesocket(socketHandle);
#else
	close(socketHandle);
#endif
}

const char* SocketErrorText()
{
#ifdef _WIN32
	static thread_local char buffer[48];
	snprintf(buffer, sizeof(buffer), "WSA error %d", WSAGetLastError());
	return buffer;
#else
	return strerror(errno);
#endif
}

std::string AddressToString(uint32_t address)
{
	char buffer[INET_ADDRSTRLEN];
	in_addr addr {};
	addr.s_addr = address;
	if (inet_ntop(AF_INET, &addr, buffer, sizeof(buffer)))
		return buffer;
	return "0.0.0.0";
}

std::string ServerKey(uint32_t address, uint16_t port)
{
	return AddressToString(address) + ":" + std::to_string(port);
}

bool ParseUInt16(const char* value, uint16_t& out)
{
	char* end = nullptr;
	const unsigned long parsed = strtoul(value, &end, 10);
	if (end == value || *end != '\0' || parsed == 0 || parsed > 65535)
		return false;
	out = uint16_t(parsed);
	return true;
}

bool ParseInt(const char* value, int& out)
{
	char* end = nullptr;
	const long parsed = strtol(value, &end, 10);
	if (end == value || *end != '\0' || parsed < 0 || parsed > 86400)
		return false;
	out = int(parsed);
	return true;
}

void PrintUsage()
{
	puts("Usage: hcdemaster [--bind <ipv4>] [--port <port>] [--ttl <seconds>] [--max-packets <count>] [--quiet]");
	puts("");
	puts("Receives HCDE dedicated-server heartbeats and answers Doom Connector master-list queries.");
	puts("Defaults: --bind 0.0.0.0 --port 15000 --ttl 180");
}

bool ParseOptions(int argc, char** argv, Options& options)
{
	for (int i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];
		auto requireValue = [&](const char* name) -> const char*
		{
			if (i + 1 >= argc)
			{
				fprintf(stderr, "%s requires a value\n", name);
				return nullptr;
			}
			return argv[++i];
		};

		if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
		{
			PrintUsage();
			exit(0);
		}
		else if (strcmp(arg, "--bind") == 0)
		{
			const char* value = requireValue(arg);
			if (value == nullptr)
				return false;
			options.bindAddress = value;
		}
		else if (strcmp(arg, "--port") == 0 || strcmp(arg, "-p") == 0)
		{
			const char* value = requireValue(arg);
			if (value == nullptr || !ParseUInt16(value, options.port))
			{
				fprintf(stderr, "Invalid port: %s\n", value != nullptr ? value : "");
				return false;
			}
		}
		else if (strcmp(arg, "--ttl") == 0)
		{
			const char* value = requireValue(arg);
			if (value == nullptr || !ParseInt(value, options.ttlSeconds) || options.ttlSeconds == 0)
			{
				fprintf(stderr, "Invalid ttl: %s\n", value != nullptr ? value : "");
				return false;
			}
		}
		else if (strcmp(arg, "--max-packets") == 0)
		{
			const char* value = requireValue(arg);
			if (value == nullptr || !ParseInt(value, options.maxPackets))
			{
				fprintf(stderr, "Invalid max-packets: %s\n", value != nullptr ? value : "");
				return false;
			}
		}
		else if (strcmp(arg, "--quiet") == 0 || strcmp(arg, "-q") == 0)
		{
			options.quiet = true;
		}
		else
		{
			fprintf(stderr, "Unknown argument: %s\n", arg);
			return false;
		}
	}

	return true;
}

void HandleSignal(int)
{
	g_running = 0;
}

void PruneExpired(std::unordered_map<std::string, ServerEntry>& servers, int ttlSeconds)
{
	const auto now = std::chrono::steady_clock::now();
	for (auto it = servers.begin(); it != servers.end();)
	{
		const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.lastSeen).count();
		if (age > ttlSeconds)
			it = servers.erase(it);
		else
			++it;
	}
}

bool SendServerList(SOCKET socketHandle, const sockaddr_in& to, const std::unordered_map<std::string, ServerEntry>& servers)
{
	const size_t maxCount = std::min<size_t>(servers.size(),
		(65507u - hcde::master_protocol::MasterListResponseHeaderSize) / hcde::master_protocol::MasterListResponseEntrySize);
	std::vector<uint8_t> packet;
	packet.reserve(hcde::master_protocol::MasterListResponseHeaderSize + maxCount * hcde::master_protocol::MasterListResponseEntrySize);
	WriteLE32(packet, kMasterListResponse);
	WriteLE16(packet, uint16_t(maxCount));

	size_t emitted = 0;
	for (const auto& pair : servers)
	{
		if (emitted >= maxCount)
			break;

		const ServerEntry& server = pair.second;
		const uint8_t* ip = reinterpret_cast<const uint8_t*>(&server.address);
		packet.push_back(ip[0]);
		packet.push_back(ip[1]);
		packet.push_back(ip[2]);
		packet.push_back(ip[3]);
		WriteLE16(packet, server.port);
		++emitted;
	}

	const int sent = sendto(socketHandle, reinterpret_cast<const char*>(packet.data()), int(packet.size()), 0,
		reinterpret_cast<const sockaddr*>(&to), sizeof(to));
	return sent != SOCKET_ERROR;
}

bool BindSocket(SOCKET socketHandle, const Options& options)
{
	sockaddr_in local {};
	local.sin_family = AF_INET;
	local.sin_port = htons(options.port);
	if (options.bindAddress == "0.0.0.0")
		local.sin_addr.s_addr = INADDR_ANY;
	else
	{
		if (inet_pton(AF_INET, options.bindAddress.c_str(), &local.sin_addr) <= 0)
		{
			fprintf(stderr, "Invalid bind address: %s\n", options.bindAddress.c_str());
			return false;
		}
	}

	if (bind(socketHandle, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR)
	{
		fprintf(stderr, "Failed to bind UDP %s:%u: %s\n", options.bindAddress.c_str(), options.port, SocketErrorText());
		return false;
	}
	return true;
}
}

int main(int argc, char** argv)
{
	Options options;
	if (!ParseOptions(argc, argv, options))
	{
		PrintUsage();
		return 2;
	}

#ifdef _WIN32
	WSADATA wsa {};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		fprintf(stderr, "WSAStartup failed\n");
		return 1;
	}
#endif

	std::signal(SIGINT, HandleSignal);
	std::signal(SIGTERM, HandleSignal);

	SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketHandle == INVALID_SOCKET)
	{
		fprintf(stderr, "Failed to create UDP socket: %s\n", SocketErrorText());
#ifdef _WIN32
		WSACleanup();
#endif
		return 1;
	}

	if (!BindSocket(socketHandle, options))
	{
		CloseSocket(socketHandle);
#ifdef _WIN32
		WSACleanup();
#endif
		return 1;
	}

	if (!options.quiet)
		printf("HCDE master listening on %s:%u (ttl=%ds)\n", options.bindAddress.c_str(), options.port, options.ttlSeconds);

	std::unordered_map<std::string, ServerEntry> servers;
	auto lastPrune = std::chrono::steady_clock::now();
	int handledPackets = 0;
	while (g_running)
	{
		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(socketHandle, &readset);
		timeval timeout = { 1, 0 }; // 1 second timeout to check g_running and prune
		const int selectResult = select(int(socketHandle + 1), &readset, nullptr, nullptr, &timeout);

		if (selectResult == 0) // timeout
		{
			const auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(now - lastPrune).count() >= 60)
			{
				PruneExpired(servers, options.ttlSeconds);
				lastPrune = now;
			}
			continue;
		}

		if (selectResult == SOCKET_ERROR)
		{
			if (g_running)
				fprintf(stderr, "select failed: %s\n", SocketErrorText());
			break;
		}

		uint8_t buffer[1500] = {};
		sockaddr_in from {};
		socklen_hcde_t fromSize = sizeof(from);
		const int received = recvfrom(socketHandle, reinterpret_cast<char*>(buffer), int(sizeof(buffer)), 0,
			reinterpret_cast<sockaddr*>(&from), &fromSize);
		if (received == SOCKET_ERROR)
		{
			if (g_running)
				fprintf(stderr, "recvfrom failed: %s\n", SocketErrorText());
			break;
		}

		if (received < 4)
			continue;

		const uint32_t message = ReadLE32(buffer);
		if (message == kServerHeartbeat && received >= hcde::master_protocol::ServerHeartbeatPacketSize)
		{
			const uint16_t gamePort = ReadLE16(buffer + 4);
			if (gamePort == 0)
				continue;

			ServerEntry entry {};
			entry.address = from.sin_addr.s_addr;
			entry.port = gamePort;
			entry.lastSeen = std::chrono::steady_clock::now();
			const std::string key = ServerKey(entry.address, entry.port);
			servers[key] = entry;
			++handledPackets;
			if (!options.quiet)
				printf("heartbeat %s (%zu active)\n", key.c_str(), servers.size());
		}
		else if (message == kMasterListChallenge)
		{
			PruneExpired(servers, options.ttlSeconds);
			lastPrune = std::chrono::steady_clock::now();
			if (!SendServerList(socketHandle, from, servers))
				fprintf(stderr, "Failed to send server list to %s: %s\n", AddressToString(from.sin_addr.s_addr).c_str(), SocketErrorText());
			else if (!options.quiet)
				printf("query %s -> %zu server(s)\n", AddressToString(from.sin_addr.s_addr).c_str(), servers.size());
			++handledPackets;
		}

		if (options.maxPackets > 0 && handledPackets >= options.maxPackets)
			break;
	}

	CloseSocket(socketHandle);
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}
