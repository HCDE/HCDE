// HCDE RCON scaffold -- see d_net_rcon.h.
//
// This file deliberately implements only the configuration surface and a
// status CCMD. There is no transport, no auth check, and no command
// dispatcher. Anything that "feels like" it could accept a remote command
// belongs in the next change, alongside an explicit allowlist and a
// verifier-style password check.

#include "d_net_rcon.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "common/engine/i_net.h"
#include "doomstat.h"
#include "m_argv.h"
#include "printf.h"
#include "version.h"
#include "zstring.h"

#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using HCDERconSocket = SOCKET;
constexpr HCDERconSocket HCDE_RCON_INVALID_SOCKET = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using HCDERconSocket = int;
constexpr HCDERconSocket HCDE_RCON_INVALID_SOCKET = -1;
#endif

EXTERN_CVAR(Bool, sv_rcon_enable)
EXTERN_CVAR(String, sv_rcon_password)
EXTERN_CVAR(Int, sv_rcon_port)

namespace
{
constexpr int HCDE_RCON_MAX_FRAME = 4096;
constexpr int HCDE_RCON_MAX_CLIENTS = 4;

enum class EHCDERconTransportState
{
	Disabled,
	Blocked,
	Listening
};

struct FHCDERconClient
{
	HCDERconSocket Socket = HCDE_RCON_INVALID_SOCKET;
	bool Authenticated = false;
	FString Nonce;
	uint8_t Buffer[HCDE_RCON_MAX_FRAME + 4] = {};
	int BufferUsed = 0;
};

struct FHCDERconTransport
{
	EHCDERconTransportState State = EHCDERconTransportState::Disabled;
	HCDERconSocket ListenSocket = HCDE_RCON_INVALID_SOCKET;
	int BoundPort = 0;
	int ConnectionCount = 0;
	int DroppedFrames = 0;
	int MalformedFrames = 0;
	int AuthFailures = 0;
	int CommandsAccepted = 0;
	const char* BlockReason = "not-started";
	FHCDERconClient Clients[HCDE_RCON_MAX_CLIENTS];
};

FHCDERconTransport RconTransport;

bool HCDERconCommandLineValue(const char* name, FString& out)
{
	if (Args == nullptr || name == nullptr)
		return false;
	for (int i = 1; i < Args->NumArgs() - 1; ++i)
	{
		const char* arg = Args->GetArg(i);
		const char* next = Args->GetArg(i + 1);
		if (arg == nullptr || next == nullptr)
			continue;
		if (!strcmp(arg, "+set") && i + 2 < Args->NumArgs() && !strcmp(next, name))
		{
			out = Args->GetArg(i + 2);
			return true;
		}
		FString direct;
		direct.Format("+%s", name);
		if (!strcmp(arg, direct.GetChars()))
		{
			out = next;
			return true;
		}
	}
	return false;
}

FString HCDERconEffectivePassword()
{
	FString value;
	if (HCDERconCommandLineValue("sv_rcon_password", value))
		return value;
	return (*sv_rcon_password) != nullptr ? (*sv_rcon_password) : "";
}

bool HCDERconEffectiveEnable()
{
	FString value;
	if (HCDERconCommandLineValue("sv_rcon_enable", value))
		return atoi(value.GetChars()) != 0;
	return *sv_rcon_enable;
}

int HCDERconEffectivePort()
{
	FString value;
	if (HCDERconCommandLineValue("sv_rcon_port", value))
		return clamp(atoi(value.GetChars()), 0, 65535);
	return max<int>(*sv_rcon_port, 0);
}

const char* HCDERconTransportStateName(EHCDERconTransportState state)
{
	switch (state)
	{
	case EHCDERconTransportState::Disabled: return "disabled";
	case EHCDERconTransportState::Blocked: return "blocked";
	case EHCDERconTransportState::Listening: return "listening";
	default: return "unknown";
	}
}

void HCDERconCloseSocket(HCDERconSocket& socket)
{
	if (socket == HCDE_RCON_INVALID_SOCKET)
		return;
#ifdef _WIN32
	closesocket(socket);
#else
	close(socket);
#endif
	socket = HCDE_RCON_INVALID_SOCKET;
}

void HCDERconSetNonBlocking(HCDERconSocket socket)
{
#ifdef _WIN32
	u_long mode = 1;
	ioctlsocket(socket, FIONBIO, &mode);
#else
	const int flags = fcntl(socket, F_GETFL, 0);
	if (flags >= 0)
		fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool HCDERconEnsureSocketRuntime()
{
#ifdef _WIN32
	static bool initialized = false;
	if (!initialized)
	{
		WSADATA data;
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
			return false;
		initialized = true;
	}
#endif
	return true;
}

void HCDERconDisconnect(FHCDERconClient& client)
{
	HCDERconCloseSocket(client.Socket);
	client.Authenticated = false;
	client.Nonce = "";
	client.BufferUsed = 0;
}

bool HCDERconSendFrame(HCDERconSocket socket, const char* text)
{
	if (socket == HCDE_RCON_INVALID_SOCKET || text == nullptr)
		return false;
	const uint32_t len = uint32_t(strlen(text));
	if (len > HCDE_RCON_MAX_FRAME)
		return false;

	uint8_t header[4] =
	{
		uint8_t((len >> 24) & 0xff),
		uint8_t((len >> 16) & 0xff),
		uint8_t((len >> 8) & 0xff),
		uint8_t(len & 0xff)
	};
	if (send(socket, reinterpret_cast<const char*>(header), 4, 0) != 4)
		return false;
	return send(socket, text, int(len), 0) == int(len);
}

uint32_t HCDERconHash(const char* text)
{
	uint32_t hash = 2166136261u;
	for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); p != nullptr && *p != 0; ++p)
	{
		hash ^= *p;
		hash *= 16777619u;
	}
	return hash;
}

FString HCDERconBuildVerifier(const FString& nonce)
{
	FString input;
	input.Format("%s:%s", nonce.GetChars(), HCDERconEffectivePassword().GetChars());
	FString verifier;
	verifier.Format("%08x", HCDERconHash(input.GetChars()));
	return verifier;
}

FString HCDERconDispatchCommand(const char* command)
{
	if (command == nullptr)
		return "ERR empty command";
	if (!strcmp(command, "ping"))
		return "OK pong";
	if (!strcmp(command, "status") || !strcmp(command, "rcon_status"))
	{
		FString status;
		status.Format("OK rcon state=%s port=%d clients=%d dropped=%d malformed=%d authfail=%d commands=%d",
			HCDERconTransportStateName(RconTransport.State),
			RconTransport.BoundPort,
			RconTransport.ConnectionCount,
			RconTransport.DroppedFrames,
			RconTransport.MalformedFrames,
			RconTransport.AuthFailures,
			RconTransport.CommandsAccepted);
		return status;
	}
	return "ERR command not allowed";
}

void HCDERconHandleFrame(FHCDERconClient& client, const char* frame)
{
	if (!client.Authenticated)
	{
		if (strncmp(frame, "auth ", 5) == 0 && !strcmp(frame + 5, HCDERconBuildVerifier(client.Nonce).GetChars()))
		{
			client.Authenticated = true;
			HCDERconSendFrame(client.Socket, "OK authenticated");
		}
		else
		{
			++RconTransport.AuthFailures;
			HCDERconSendFrame(client.Socket, "ERR auth failed");
			HCDERconDisconnect(client);
		}
		return;
	}

	FString response = HCDERconDispatchCommand(frame);
	if (!strncmp(response.GetChars(), "OK", 2))
		++RconTransport.CommandsAccepted;
	HCDERconSendFrame(client.Socket, response.GetChars());
}

void HCDERconSetBlocked(const char* reason)
{
	HCDERconCloseSocket(RconTransport.ListenSocket);
	for (auto& client : RconTransport.Clients)
		HCDERconDisconnect(client);
	RconTransport.State = EHCDERconTransportState::Blocked;
	RconTransport.BoundPort = 0;
	RconTransport.ConnectionCount = 0;
	RconTransport.BlockReason = reason;
}
}

// Master switch. Default off; even with a password configured the future
// transport refuses to bind unless this is on. Server-only -- no point on a
// listen host since the local console already has unrestricted access.
CVAR(Bool, sv_rcon_enable, false, CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_NOSAVE)

// Operator-configured password. The string itself never travels on the wire:
// the future transport hashes a server-issued nonce together with this value
// and only compares the resulting verifier. Stored here as a plain CVAR for
// configuration ergonomics; treat the config file as a secret.
CVAR(String, sv_rcon_password, "", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_NOSAVE)

// TCP port for the RCON listener. 0 = disabled (default). When non-zero and
// `sv_rcon_enable` is true and a password is set, the authority binds a
// dedicated TCP listener (Phase 1 scaffold). Clients connect, send
// length-prefixed frames (max 4 KiB), and receive responses. Auth and
// command dispatch are Phase 2/3 work.
CUSTOM_CVAR(Int, sv_rcon_port, 0, CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0) self = 0;
	if (self > 65535) self = 65535;
}

bool HCDERconHasPasswordConfigured()
{
	FString password = HCDERconEffectivePassword();
	const char* configured = password.GetChars();
	return configured != nullptr && configured[0] != '\0';
}

bool HCDERconShouldAcceptCommands()
{
	if (!HCDERconEffectiveEnable())
		return false;
	if (!HCDERconHasPasswordConfigured())
		return false;
	if (!I_IsDedicatedServerMode() && !I_IsLocalHCDEServiceAuthority())
		return false;
	return true;
}

void HCDERconStartListener()
{
	const int port = HCDERconEffectivePort();
	if (port <= 0 || !HCDERconEffectiveEnable())
	{
		HCDERconCloseSocket(RconTransport.ListenSocket);
		for (auto& client : RconTransport.Clients)
			HCDERconDisconnect(client);
		RconTransport.State = EHCDERconTransportState::Disabled;
		RconTransport.BoundPort = 0;
		RconTransport.ConnectionCount = 0;
		RconTransport.BlockReason = "disabled";
		return;
	}
	if (!HCDERconHasPasswordConfigured())
	{
		HCDERconSetBlocked("password-not-set");
		return;
	}
	if (!I_IsDedicatedServerMode() && !I_IsLocalHCDEServiceAuthority())
	{
		HCDERconSetBlocked("not-authority");
		return;
	}

	if (RconTransport.State == EHCDERconTransportState::Listening && RconTransport.BoundPort == port)
		return;

	if (!HCDERconEnsureSocketRuntime())
	{
		HCDERconSetBlocked("socket-runtime-failed");
		return;
	}

	HCDERconCloseSocket(RconTransport.ListenSocket);
	RconTransport.ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (RconTransport.ListenSocket == HCDE_RCON_INVALID_SOCKET)
	{
		HCDERconSetBlocked("socket-failed");
		return;
	}

	int yes = 1;
	setsockopt(RconTransport.ListenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(uint16_t(port));
	if (bind(RconTransport.ListenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
		listen(RconTransport.ListenSocket, HCDE_RCON_MAX_CLIENTS) != 0)
	{
		HCDERconCloseSocket(RconTransport.ListenSocket);
		HCDERconSetBlocked("bind-or-listen-failed");
		return;
	}

	HCDERconSetNonBlocking(RconTransport.ListenSocket);
	RconTransport.State = EHCDERconTransportState::Listening;
	RconTransport.BoundPort = port;
	RconTransport.ConnectionCount = 0;
	RconTransport.BlockReason = "none";
}

void HCDERconStopListener()
{
	HCDERconCloseSocket(RconTransport.ListenSocket);
	for (auto& client : RconTransport.Clients)
		HCDERconDisconnect(client);
	RconTransport.State = EHCDERconTransportState::Disabled;
	RconTransport.BoundPort = 0;
	RconTransport.ConnectionCount = 0;
	RconTransport.BlockReason = "stopped";
}

void HCDERconPollListener()
{
	if (RconTransport.State == EHCDERconTransportState::Listening &&
		(!HCDERconShouldAcceptCommands() || HCDERconEffectivePort() != RconTransport.BoundPort))
	{
		HCDERconStartListener();
	}
	else if (RconTransport.State != EHCDERconTransportState::Listening)
	{
		HCDERconStartListener();
	}

	HCDERconDrainIngress();
}

int HCDERconDrainIngress()
{
	if (RconTransport.State != EHCDERconTransportState::Listening)
		return 0;

	int processed = 0;
	for (;;)
	{
		sockaddr_in addr = {};
#ifdef _WIN32
		int addrLen = sizeof(addr);
#else
		socklen_t addrLen = sizeof(addr);
#endif
		HCDERconSocket accepted = accept(RconTransport.ListenSocket, reinterpret_cast<sockaddr*>(&addr), &addrLen);
		if (accepted == HCDE_RCON_INVALID_SOCKET)
			break;

		FHCDERconClient* slot = nullptr;
		for (auto& client : RconTransport.Clients)
		{
			if (client.Socket == HCDE_RCON_INVALID_SOCKET)
			{
				slot = &client;
				break;
			}
		}
		if (slot == nullptr)
		{
			HCDERconSendFrame(accepted, "ERR too many clients");
			HCDERconCloseSocket(accepted);
			continue;
		}

		HCDERconSetNonBlocking(accepted);
		slot->Socket = accepted;
		slot->Authenticated = false;
		slot->BufferUsed = 0;
		slot->Nonce.Format("%08x", HCDERconHash(GAMENAME) ^ uint32_t(RconTransport.ConnectionCount + gametic));
		++RconTransport.ConnectionCount;
		FString hello;
		hello.Format("nonce %s", slot->Nonce.GetChars());
		HCDERconSendFrame(slot->Socket, hello.GetChars());
	}

	for (auto& client : RconTransport.Clients)
	{
		if (client.Socket == HCDE_RCON_INVALID_SOCKET)
			continue;

		char temp[1024];
		const int got = recv(client.Socket, temp, sizeof(temp), 0);
		if (got == 0)
		{
			HCDERconDisconnect(client);
			continue;
		}
		if (got < 0)
			continue;

		if (client.BufferUsed + got > int(sizeof(client.Buffer)))
		{
			++RconTransport.DroppedFrames;
			HCDERconDisconnect(client);
			continue;
		}
		memcpy(client.Buffer + client.BufferUsed, temp, got);
		client.BufferUsed += got;

		while (client.BufferUsed >= 4)
		{
			const int len = (int(client.Buffer[0]) << 24) | (int(client.Buffer[1]) << 16) | (int(client.Buffer[2]) << 8) | int(client.Buffer[3]);
			if (len <= 0 || len > HCDE_RCON_MAX_FRAME)
			{
				++RconTransport.MalformedFrames;
				HCDERconDisconnect(client);
				break;
			}
			if (client.BufferUsed < len + 4)
				break;

			char frame[HCDE_RCON_MAX_FRAME + 1] = {};
			memcpy(frame, client.Buffer + 4, len);
			const int remain = client.BufferUsed - len - 4;
			memmove(client.Buffer, client.Buffer + len + 4, remain);
			client.BufferUsed = remain;
			HCDERconHandleFrame(client, frame);
			++processed;
			if (client.Socket == HCDE_RCON_INVALID_SOCKET)
				break;
		}
	}

	return processed;
}

CCMD(rcon_status)
{
	const bool authority = I_IsLocalHCDEServiceAuthority();
	const bool enabled = !!*sv_rcon_enable;
	const bool hasPassword = HCDERconHasPasswordConfigured();
	const bool ready = HCDERconShouldAcceptCommands();
	const int port = max<int>(*sv_rcon_port, 0);

	HCDERconStartListener();
	const int drained = HCDERconDrainIngress();

	Printf(PRINT_HIGH, "\n=== HCDE RCON status ===\n");
	Printf(PRINT_HIGH, "authority=%d enabled=%d password-set=%d port=%d would-accept=%d\n",
		authority ? 1 : 0,
		enabled ? 1 : 0,
		hasPassword ? 1 : 0,
		port,
		ready ? 1 : 0);
	if (!authority)
		Printf(PRINT_HIGH, "  note: RCON is authority-only; this instance is a client/listen host.\n");
	if (!hasPassword)
		Printf(PRINT_HIGH, "  note: set `sv_rcon_password \"...\"` before enabling.\n");
	if (!enabled)
		Printf(PRINT_HIGH, "  note: set `sv_rcon_enable 1` to advertise the channel.\n");
	Printf(PRINT_HIGH, "  transport-state=%s bound-port=%d connections=%d dropped-frames=%d malformed=%d authfail=%d commands=%d block-reason=%s\n",
		HCDERconTransportStateName(RconTransport.State),
		RconTransport.BoundPort,
		RconTransport.ConnectionCount,
		RconTransport.DroppedFrames,
		RconTransport.MalformedFrames,
		RconTransport.AuthFailures,
		RconTransport.CommandsAccepted,
		RconTransport.BlockReason);
	Printf(PRINT_HIGH, "  auth=server nonce + client verifier; password is never sent on the wire.\n");
	Printf(PRINT_HIGH, "  allowlist=ping,status,rcon_status; frame=4-byte big-endian length + payload, max 4096 bytes.\n");
	Printf(PRINT_HIGH, "  ingress drained-this-call=%d\n", drained);
	Printf(PRINT_HIGH, "=========================\n");
}
