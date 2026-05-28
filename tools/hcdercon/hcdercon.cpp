/*
** hcdercon.cpp
**
** Standalone HCDE RCON client.
**
**-----------------------------------------------------------------------------
**
** Copyright 2026 HCDE Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**-----------------------------------------------------------------------------
*/

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "hcde_rcon_protocol.h"

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <winsock2.h>
#	include <ws2tcpip.h>
using socklen_hcde_t = int;
#else
#	include <arpa/inet.h>
#	include <cerrno>
#	include <netdb.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#	include <sys/time.h>
#	include <unistd.h>
using SOCKET = int;
using socklen_hcde_t = socklen_t;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
#endif

namespace
{
struct Options
{
	std::string target;
	std::string password;
	std::string command;
	int timeoutMs = 2000;
};

void WriteBE32(std::vector<uint8_t>& packet, uint32_t value)
{
	packet.push_back(uint8_t(value >> 24));
	packet.push_back(uint8_t(value >> 16));
	packet.push_back(uint8_t(value >> 8));
	packet.push_back(uint8_t(value));
}

uint32_t ReadBE32(const uint8_t* data)
{
	return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3]);
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

void PrintUsage()
{
	puts("Usage: hcdercon [--timeout-ms <ms>] <host[:port]> <password> <command...>");
	puts("");
	puts("Sends one authenticated RCON command to an HCDE server UDP game port.");
	puts("Defaults: port 5029, timeout 2000 ms.");
}

bool ParseInt(const char* value, int& out)
{
	char* end = nullptr;
	const long parsed = strtol(value, &end, 10);
	if (end == value || *end != '\0' || parsed < 100 || parsed > 60000)
		return false;
	out = int(parsed);
	return true;
}

bool ParseOptions(int argc, char** argv, Options& options)
{
	int positional = 0;
	for (int i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];
		if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
		{
			PrintUsage();
			exit(0);
		}
		if (strcmp(arg, "--timeout-ms") == 0)
		{
			if (i + 1 >= argc || !ParseInt(argv[++i], options.timeoutMs))
			{
				fprintf(stderr, "--timeout-ms requires a value from 100 to 60000\n");
				return false;
			}
			continue;
		}

		if (positional == 0)
			options.target = arg;
		else if (positional == 1)
			options.password = arg;
		else
		{
			if (!options.command.empty())
				options.command += ' ';
			options.command += arg;
		}
		++positional;
	}

	if (options.target.empty() || options.password.empty() || options.command.empty())
	{
		PrintUsage();
		return false;
	}
	if (options.command.size() > hcde::rcon_protocol::MaxCommandBytes)
	{
		fprintf(stderr, "Command is too long; max is %zu bytes\n", hcde::rcon_protocol::MaxCommandBytes);
		return false;
	}
	if (options.command.find('\n') != std::string::npos || options.command.find('\r') != std::string::npos)
	{
		fprintf(stderr, "Command must be a single line\n");
		return false;
	}
	return true;
}

bool ResolveTarget(const std::string& target, sockaddr_in& address)
{
	std::string host = target;
	uint16_t port = hcde::rcon_protocol::DefaultGamePort;
	const size_t colon = target.rfind(':');
	if (colon != std::string::npos)
	{
		host = target.substr(0, colon);
		const unsigned long parsed = strtoul(target.c_str() + colon + 1u, nullptr, 10);
		if (parsed == 0 || parsed > 65535)
		{
			fprintf(stderr, "Invalid port in target: %s\n", target.c_str());
			return false;
		}
		port = uint16_t(parsed);
	}

	addrinfo hints {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	addrinfo* result = nullptr;
	if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || result == nullptr)
	{
		fprintf(stderr, "Failed to resolve %s: %s\n", host.c_str(), SocketErrorText());
		return false;
	}

	address = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
	address.sin_port = htons(port);
	freeaddrinfo(result);
	return true;
}

uint32_t RandomNonce()
{
	std::random_device device;
	std::mt19937 generator(device());
	std::uniform_int_distribution<uint32_t> distribution(1u, UINT32_MAX);
	return distribution(generator);
}

bool BuildRequest(const Options& options, uint32_t nonce, std::vector<uint8_t>& packet)
{
	packet.clear();
	packet.push_back(uint8_t(hcde::rcon_protocol::RequestMarker >> 24));
	packet.push_back(uint8_t(hcde::rcon_protocol::RequestMarker >> 16));
	packet.push_back(uint8_t(hcde::rcon_protocol::RequestMarker >> 8));
	packet.push_back(uint8_t(hcde::rcon_protocol::RequestMarker));
	packet.push_back(hcde::rcon_protocol::Version);
	WriteBE32(packet, nonce);
	WriteBE32(packet, hcde::rcon_protocol::ComputeAuth(options.password.c_str(), nonce, options.command.c_str()));
	packet.insert(packet.end(), options.command.begin(), options.command.end());
	packet.push_back(0u);
	return true;
}

const char* StatusName(hcde::rcon_protocol::Status status)
{
	switch (status)
	{
	case hcde::rcon_protocol::StatusQueued: return "queued";
	case hcde::rcon_protocol::StatusRejected: return "rejected";
	case hcde::rcon_protocol::StatusAuthFailed: return "auth-failed";
	case hcde::rcon_protocol::StatusDisabled: return "disabled";
	case hcde::rcon_protocol::StatusMalformed: return "malformed";
	default: return "unknown";
	}
}

bool ReadResponse(const uint8_t* data, int length, uint32_t expectedNonce, hcde::rcon_protocol::Status& status, std::string& message)
{
	if (length < 10)
		return false;
	if (ReadBE32(data) != hcde::rcon_protocol::ResponseMarker)
		return false;
	if (data[4] != hcde::rcon_protocol::Version)
		return false;

	status = hcde::rcon_protocol::Status(data[5]);
	const uint32_t nonce = ReadBE32(data + 6u);
	if (nonce != expectedNonce)
		return false;

	const char* text = reinterpret_cast<const char*>(data + 10u);
	const size_t textLength = strnlen(text, size_t(length - 10));
	message.assign(text, textLength);
	return true;
}

bool SendCommand(const Options& options)
{
	sockaddr_in address {};
	if (!ResolveTarget(options.target, address))
		return false;

	SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketHandle == INVALID_SOCKET)
	{
		fprintf(stderr, "Failed to create UDP socket: %s\n", SocketErrorText());
		return false;
	}

#ifdef _WIN32
	DWORD timeout = static_cast<DWORD>(options.timeoutMs);
	setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
	timeval timeout {};
	timeout.tv_sec = options.timeoutMs / 1000;
	timeout.tv_usec = (options.timeoutMs % 1000) * 1000;
	setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

	const uint32_t nonce = RandomNonce();
	std::vector<uint8_t> request;
	BuildRequest(options, nonce, request);

	if (sendto(socketHandle, reinterpret_cast<const char*>(request.data()), int(request.size()), 0,
		reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
	{
		fprintf(stderr, "Failed to send RCON request: %s\n", SocketErrorText());
		CloseSocket(socketHandle);
		return false;
	}

	uint8_t buffer[hcde::rcon_protocol::MaxMessageBytes + 32u] = {};
	socklen_hcde_t fromSize = sizeof(address);
	const int received = recvfrom(socketHandle, reinterpret_cast<char*>(buffer), int(sizeof(buffer)), 0,
		reinterpret_cast<sockaddr*>(&address), &fromSize);
	CloseSocket(socketHandle);

	if (received == SOCKET_ERROR)
	{
		fprintf(stderr, "Timed out waiting for RCON response\n");
		return false;
	}

	hcde::rcon_protocol::Status status = hcde::rcon_protocol::StatusMalformed;
	std::string message;
	if (!ReadResponse(buffer, received, nonce, status, message))
	{
		fprintf(stderr, "Received malformed RCON response\n");
		return false;
	}

	printf("status=%s message=%s\n", StatusName(status), message.c_str());
	return status == hcde::rcon_protocol::StatusQueued;
}
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		fprintf(stderr, "WSAStartup failed\n");
		return 1;
	}
#endif

	Options options;
	if (!ParseOptions(argc, argv, options))
	{
#ifdef _WIN32
		WSACleanup();
#endif
		return 1;
	}

	const bool ok = SendCommand(options);

#ifdef _WIN32
	WSACleanup();
#endif
	return ok ? 0 : 1;
}
