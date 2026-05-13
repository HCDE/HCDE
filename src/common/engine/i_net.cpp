/*
** i_net.cpp
**
** Low-level networking code. Uses BSD sockets for UDP networking.
**
**---------------------------------------------------------------------------
**
** Copyright 1993-1996 by id Software, Inc.
** Copyright 1999-2016 Marisa Heit
** Copyright 2009-2016 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
** Code written prior to 2026 is also licensed under:
**
** SPDX-License-Identifier: LicenseRef-Doom-Source-License
**
**---------------------------------------------------------------------------
**
*/

#include <stdlib.h>
#include <string.h>
#include <array>

/* [Petteri] Use Winsock if compiling for Win32: */
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
#	include <sys/ioctl.h>
#	include <sys/socket.h>
#	include <unistd.h>
#	ifdef __sun
#		include <fcntl.h>
#	endif
#endif

#include "c_cvars.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "engineerrors.h"
#include "debugtrace.h"
#include "hcde_servermode.h"
#include "i_interface.h"
#include "i_net.h"
#include "m_argv.h"
#include "m_crc32.h"
#include "m_random.h"
#include "printf.h"
#include "version.h"
#include "d_net.h"
#include "sv_master.h"
#include "widgets/netstartwindow.h"
#include "g_levellocals.h"
#include "playsim/d_player.h"
#include "filesystem.h"

#if defined(_WIN32) && defined(HCDE_DEDICATED_SERVER)
extern void I_PumpDedicatedServerConsoleWindow();
extern void I_SetDedicatedServerConsoleStatus(const char* status);
#endif

EXTERN_CVAR(Int, fraglimit)
EXTERN_CVAR(Float, timelimit)
CVAR(String, sv_hostname, GAMENAME " server", CVAR_ARCHIVE | CVAR_SERVERINFO)
CVAR(String, sv_motd, "Welcome to " GAMENAME, CVAR_ARCHIVE | CVAR_SERVERINFO)
CUSTOM_CVAR(Int, sv_maxplayers, 0, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > MAXPLAYERS - 1)
	{
		self = MAXPLAYERS - 1;
	}
}

/* [Petteri] Get more portable: */
#ifndef _WIN32
typedef int SOCKET;
#define SOCKET_ERROR		-1
#define INVALID_SOCKET		-1
#define closesocket			close
#define ioctlsocket			ioctl
#define Sleep(x)			usleep (x * 1000)
#define WSAEWOULDBLOCK		EWOULDBLOCK
#define WSAECONNRESET		ECONNRESET
#define WSAGetLastError()	errno
#endif

#ifndef IPPORT_USERRESERVED
#define IPPORT_USERRESERVED 5000
#endif

#ifdef _WIN32
# include "common/scripting/dap/GameEventEmit.h"
typedef int socklen_t;
const char* neterror(void);
#else
#define neterror() strerror(errno)
#endif

FARG(host, "Multiplayer", "Designates the machine as the host for a multiplayer game.", "x",
	"This machine will function as a host for a multiplayer game with x players (including this"
	" machine). It will wait for other machines to connect using the -join. parameter and then"
	" start the game when everyone is connected.");
FARG(join, "Multiplayer", "Connects to a multiplayer host.", "host's IP address[:host's port]",
	 "Connect to a host for a multiplayer game.");
FARG(server, "Multiplayer", "Starts a dedicated multiplayer server without the session window.", "x",
	"This machine will function as a dedicated multiplayer server with x players (including this"
	" machine). Use this for HCDE's separate server mode so the launcher can start a server process"
	" and a local join client without showing the old room/session window.");
FARG(netwaitsilent, "Multiplayer", "Suppresses the multiplayer connection status window.", "",
	"Run the pregame network handshake without showing the old room/session window. Launchers use"
	" this when starting a local client for a separate dedicated server.");
FARG(dedicatedjoin, "Multiplayer", "Connects to a dedicated server with a reserved server authority slot.", "",
	"Treat the network arbitrator as a transport-only slot. This is used by launchers when joining"
	" HCDE's separate dedicated server executable so the server does not appear as an in-game player.");
FARG(dup, "Multiplayer", "Send less player movement commands over the network.", "x",
	"Causes " GAMENAME " to transmit fewer player movement commands across the network. Valid"
	" values range from 1–9. For example, -dup 2 would cause " GAMENAME " to send half as many"
	" movements as normal.");
FARG(port, "Multiplayer", "Specifies an alternative IP port for a network game.", "x",
	"Specifies an alternate IP port for this machine to use during a network game. By default,"
	" port 5029 is used.");
FARG(password, "", "", "",
	"");

// As per http://support.microsoft.com/kb/q192599/ the standard
// size for network buffers is 8k.
constexpr size_t MaxTransmitSize = 8000u;
constexpr size_t MinCompressionSize = 10u;
constexpr size_t MaxPasswordSize = 256u;
constexpr size_t HCDEServiceSequenceOffset = 7u;
constexpr size_t HCDEServiceAckOffset = 11u;
constexpr size_t HCDEServiceHeaderSize = 15u;
constexpr size_t MaxHCDEReliableServices = 16u;
constexpr uint64_t HCDEServiceResendMS = 250u;
constexpr uint64_t HCDEServiceTimeoutMS = 15000u;
constexpr uint8_t HCDEConnectProtocolVersion = 1u;
constexpr uint8_t HCDEConnectMagic[4] = { 'H', 'C', 'D', '3' };

enum ENetConnectType : uint8_t
{
	PRE_HEARTBEAT,			// Clients are keeping each other's connections alive
	PRE_CONNECT,			// Sent from guest to host for initial connection
	PRE_CONNECT_ACK,		// Sent from host to guest to confirm they've been connected
	PRE_DISCONNECT,			// Sent from host to guest when another guest leaves
	PRE_USER_INFO,			// Clients are sending each other user infos
	PRE_USER_INFO_ACK,		// Clients are confirming sent user infos
	PRE_GAME_INFO,			// Sent from host to guest containing general game info
	PRE_GAME_INFO_ACK,		// Sent from guest to host confirming game info was gotten
	PRE_GO,					// Sent from host to guest telling them to start the game

	PRE_FULL,				// Sent from host to guest if the server is full
	PRE_IN_PROGRESS,		// Sent from host to guest if the game has already started
	PRE_WRONG_PASSWORD,		// Sent from host to guest if their provided password was wrong
	PRE_VERIFICATION_ERROR,	// Sent from host to guest if something failed during the verification step.
	PRE_KICKED,				// Sent from host to guest if the host kicked them from the game
	PRE_BANNED,				// Sent from host to guest if the host banned them from the game
	PRE_PROTOCOL_ERROR,		// Sent from host to guest if HCDE service negotiation failed
	PRE_HCDE_SERVICE,		// Carries negotiated HCDE pregame service messages
	PRE_SETUP_TIMEOUT,		// Sent from host to guest if required HCDE setup messages timed out
};

enum EPreConnectAckFlags : uint8_t
{
	PRE_CONNECT_ACK_DEDICATED = 1u << 0,
	PRE_CONNECT_ACK_HCDE_SERVICE = 1u << 1,
	PRE_CONNECT_ACK_SERVER_AUTHORITY = 1u << 2,
};

enum EHCDEConnectFlags : uint8_t
{
	HCDE_CONNECT_DEDICATED_JOIN = 1u << 0,
	HCDE_CONNECT_SUPPRESS_ROOM_UI = 1u << 1,
	HCDE_CONNECT_SERVER_AUTHORITY = 1u << 2,
};

enum EConnectionStatus
{
	CSTAT_NONE,			// Guest isn't connected
	CSTAT_CONNECTING,	// Guest is trying to connect
	CSTAT_WAITING,		// Guest is waiting for game info
	CSTAT_READY,		// Guest is ready to start the game
};

enum ENetConnectFlow
{
	NCF_IDLE,
	NCF_SERVER_WAITING,
	NCF_CLIENT_AUTH,
	NCF_SYNCING,
};

enum EHCDEPregameService : uint8_t
{
	HPS_HEARTBEAT = 1,
	HPS_CLIENT_USER_INFO,
	HPS_USER_INFO_ACK,
	HPS_GAME_INFO,
	HPS_GAME_INFO_ACK,
	HPS_PEER_USER_INFO,
	HPS_START_GAME,
	HPS_CONSOLE_PLAYER,
	HPS_MAP_LOAD,
	HPS_MAP_LOAD_ACK,
	HPS_START_GAME_ACK,
};

struct FHCDEPendingService
{
	bool Active = false;
	EHCDEPregameService Service = HPS_HEARTBEAT;
	uint8_t Key = 0u;
	uint32_t Sequence = 0u;
	uint64_t FirstSendTime = 0u;
	uint64_t LastSendTime = 0u;
	uint32_t SendCount = 0u;
	TArray<uint8_t> Packet = {};

	void Clear()
	{
		Active = false;
		Service = HPS_HEARTBEAT;
		Key = 0u;
		Sequence = 0u;
		FirstSendTime = 0u;
		LastSendTime = 0u;
		SendCount = 0u;
		Packet.Clear();
	}
};

// These need to be synced with the window backends so information about each
// client can be properly displayed.
enum EConnectionFlags : unsigned int
{
	CFL_NONE			= 0,
	CFL_CONSOLEPLAYER	= 1,
	CFL_HOST			= 1 << 1,
};

struct FConnection
{
	EConnectionStatus Status = CSTAT_NONE;
	sockaddr_in Address = {};
	uint64_t InfoAck = 0u;
	bool bHasGameInfo = false;
	bool bHasMapLoadInfo = false;
	bool bHasStartGameAck = false;
	uint32_t SessionToken = 0u;
	bool bHCDEConnect = false;
	uint8_t HCDEConnectVersion = 0u;
	uint8_t HCDEConnectFlags = 0u;
	uint32_t HCDEServiceTxSeq = 0u;
	uint32_t HCDEServiceRxSeq = 0u;
	uint32_t HCDEServicePeerAck = 0u;
	uint32_t HCDEServiceDuplicateCount = 0u;
	FHCDEPendingService HCDEReliableServices[MaxHCDEReliableServices] = {};

	void Clear()
	{
		Status = CSTAT_NONE;
		Address = {};
		InfoAck = 0u;
		bHasGameInfo = false;
		bHasMapLoadInfo = false;
		bHasStartGameAck = false;
		SessionToken = 0u;
		bHCDEConnect = false;
		HCDEConnectVersion = 0u;
		HCDEConnectFlags = 0u;
		HCDEServiceTxSeq = 0u;
		HCDEServiceRxSeq = 0u;
		HCDEServicePeerAck = 0u;
		HCDEServiceDuplicateCount = 0u;
		for (auto& service : HCDEReliableServices)
			service.Clear();
	}
};

struct FHCDEConnectInfo
{
	bool Present = false;
	uint8_t Version = 0u;
	uint8_t Flags = 0u;
};

static ENetConnectFlow NetConnectFlowState = NCF_IDLE;
static bool DedicatedServerMode = false;
static bool SilentNetStartMode = false;
static bool DedicatedJoinMode = false;
static bool DedicatedServerStartRequested = false;
static bool DedicatedServerAbortRequested = false;

bool netgame = false;
bool multiplayer = false;
int consoleplayer = 0;
int Net_Arbitrator = 0;
FClientStack NetworkClients = {};

uint8_t	TicDup = 1u;
int	MaxClients = 1;
int RemoteClient = -1;
size_t NetBufferLength = 0u;
uint8_t NetBuffer[MAX_MSGLEN] = {};

static FRandom		GameIDGen = {};
static uint8_t		GameID[8] = {};
static u_short		GamePort = (IPPORT_USERRESERVED + 29);
static SOCKET		MySocket = INVALID_SOCKET;
static FConnection	Connected[MAXPLAYERS] = {};
static uint8_t		TransmitBuffer[MaxTransmitSize] = {};
static TArray<sockaddr_in> BannedConnections = {};
static bool bGameStarted = false;

namespace
{
static uint32_t ReadBE32(const uint8_t* data)
{
	return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

static void WriteBE32(uint8_t* data, uint32_t value)
{
	data[0] = uint8_t(value >> 24);
	data[1] = uint8_t(value >> 16);
	data[2] = uint8_t(value >> 8);
	data[3] = uint8_t(value);
}

static bool WriteSessionToken(size_t offset, uint32_t token);

static void BeginSetupPacket(uint8_t type, uint32_t token, size_t tokenOffset = 2u)
{
	NetBuffer[0] = NCMD_SETUP;
	NetBuffer[1] = type;
	if (!WriteSessionToken(tokenOffset, token))
		I_FatalError("Setup packet overflow");
}

static const char* HCDEServiceName(EHCDEPregameService service)
{
	switch (service)
	{
	case HPS_HEARTBEAT: return "heartbeat";
	case HPS_CLIENT_USER_INFO: return "client-user-info";
	case HPS_USER_INFO_ACK: return "user-info-ack";
	case HPS_GAME_INFO: return "game-info";
	case HPS_GAME_INFO_ACK: return "game-info-ack";
	case HPS_PEER_USER_INFO: return "peer-user-info";
	case HPS_START_GAME: return "start-game";
	case HPS_CONSOLE_PLAYER: return "console-player";
	case HPS_MAP_LOAD: return "map-load";
	case HPS_MAP_LOAD_ACK: return "map-load-ack";
	case HPS_START_GAME_ACK: return "start-game-ack";
	default: return "unknown";
	}
}

static void BeginHCDEPregameService(EHCDEPregameService service, FConnection& connection)
{
	BeginSetupPacket(PRE_HCDE_SERVICE, connection.SessionToken, 3u);
	NetBuffer[2] = uint8_t(service);
	const uint32_t seq = ++connection.HCDEServiceTxSeq;
	WriteBE32(&NetBuffer[HCDEServiceSequenceOffset], seq);
	WriteBE32(&NetBuffer[HCDEServiceAckOffset], connection.HCDEServiceRxSeq);
	NetBufferLength = HCDEServiceHeaderSize;
	DebugTrace::Markf("net", "write service %s seq=%u ack=%u", HCDEServiceName(service), seq, connection.HCDEServiceRxSeq);
}

static FHCDEPendingService* FindHCDEReliableService(FConnection& connection, EHCDEPregameService service, uint8_t key)
{
	for (auto& pending : connection.HCDEReliableServices)
	{
		if (pending.Active && pending.Service == service && pending.Key == key)
			return &pending;
	}
	return nullptr;
}

static FHCDEPendingService* FindFreeHCDEReliableService(FConnection& connection)
{
	for (auto& pending : connection.HCDEReliableServices)
	{
		if (!pending.Active)
			return &pending;
	}
	return nullptr;
}

static FHCDEPendingService* FindOldestHCDEReliableService(FConnection& connection)
{
	FHCDEPendingService* oldest = nullptr;
	for (auto& pending : connection.HCDEReliableServices)
	{
		if (pending.Active && (oldest == nullptr || pending.Sequence < oldest->Sequence))
			oldest = &pending;
	}
	return oldest;
}

static bool HasPendingHCDEReliableService(FConnection& connection)
{
	return FindOldestHCDEReliableService(connection) != nullptr;
}

static void ClearAckedHCDEReliableServices(FConnection& connection)
{
	for (auto& pending : connection.HCDEReliableServices)
	{
		if (pending.Active && pending.Sequence <= connection.HCDEServicePeerAck)
		{
			DebugTrace::Markf("net", "acked reliable service %s key=%u seq=%u", HCDEServiceName(pending.Service), pending.Key, pending.Sequence);
			pending.Clear();
		}
	}
}

static FHCDEPendingService* FindTimedOutHCDEReliableService(FConnection& connection, uint64_t now)
{
	ClearAckedHCDEReliableServices(connection);
	for (auto& pending : connection.HCDEReliableServices)
	{
		if (pending.Active && pending.SendCount > 0u && pending.FirstSendTime > 0u
			&& now - pending.FirstSendTime >= HCDEServiceTimeoutMS)
		{
			return &pending;
		}
	}
	return nullptr;
}

static uint32_t MakeSessionToken(const sockaddr_in& address, int client)
{
	uint32_t token = CalcCRC32(GameID, sizeof(GameID));
	token = AddCRC32(token, reinterpret_cast<const uint8_t*>(&address.sin_addr.s_addr), sizeof(address.sin_addr.s_addr));
	token = AddCRC32(token, reinterpret_cast<const uint8_t*>(&address.sin_port), sizeof(address.sin_port));
	token = AddCRC32(token, reinterpret_cast<const uint8_t*>(&client), sizeof(client));
	token ^= uint32_t(I_msTime() & 0xffffffffu);
	return token == 0u ? 1u : token;
}

static bool CheckSessionToken(const FConnection& connection, uint32_t token, const char* context)
{
	if (connection.SessionToken != token)
	{
		DebugTrace::Markf("net", "%s token mismatch expected=%08x got=%08x", context, connection.SessionToken, token);
		return false;
	}
	return true;
}

static bool CheckSetupPacket(size_t client, size_t minimumSize, size_t tokenOffset, const char* context)
{
	return NetBufferLength >= minimumSize && CheckSessionToken(Connected[client], ReadBE32(&NetBuffer[tokenOffset]), context);
}

static bool CheckHCDEPregameService(size_t client, size_t minimumSize, const char* context)
{
	if (NetBufferLength < minimumSize)
	{
		DebugTrace::Markf("net", "%s service packet too short len=%zu minimum=%zu", context, NetBufferLength, minimumSize);
		return false;
	}
	auto& connection = Connected[client];
	if (!CheckSessionToken(connection, ReadBE32(&NetBuffer[3]), context))
		return false;

	const uint32_t seq = ReadBE32(&NetBuffer[HCDEServiceSequenceOffset]);
	const uint32_t ack = ReadBE32(&NetBuffer[HCDEServiceAckOffset]);
	if (seq == 0u)
	{
		DebugTrace::Markf("net", "%s service packet has invalid zero sequence", context);
		return false;
	}
	if (ack > connection.HCDEServiceTxSeq)
	{
		DebugTrace::Markf("net", "%s service ack beyond sent range ack=%u sent=%u", context, ack, connection.HCDEServiceTxSeq);
	}
	else if (ack > connection.HCDEServicePeerAck)
	{
		connection.HCDEServicePeerAck = ack;
		ClearAckedHCDEReliableServices(connection);
	}
	if (seq <= connection.HCDEServiceRxSeq)
	{
		++connection.HCDEServiceDuplicateCount;
		DebugTrace::Markf("net", "%s duplicate/replayed service seq=%u last=%u count=%u", context, seq, connection.HCDEServiceRxSeq, connection.HCDEServiceDuplicateCount);
		return false;
	}

	connection.HCDEServiceRxSeq = seq;
	return true;
}

static bool FindStringEnd(size_t start, size_t limit, size_t& end)
{
	for (size_t i = start; i < limit; ++i)
	{
		if (NetBuffer[i] == 0u)
		{
			end = i + 1u;
			return true;
		}
	}
	return false;
}

static bool ReadHCDEConnectInfo(size_t offset, FHCDEConnectInfo& info)
{
	info = {};
	if (offset + sizeof(HCDEConnectMagic) + 2u > NetBufferLength)
		return false;
	if (memcmp(&NetBuffer[offset], HCDEConnectMagic, sizeof(HCDEConnectMagic)) != 0)
		return false;

	info.Present = true;
	info.Version = NetBuffer[offset + sizeof(HCDEConnectMagic)];
	info.Flags = NetBuffer[offset + sizeof(HCDEConnectMagic) + 1u];
	return true;
}

static void AppendHCDEConnectInfo(uint8_t flags)
{
	if (NetBufferLength + sizeof(HCDEConnectMagic) + 2u > MAX_MSGLEN)
		I_FatalError("HCDE connect packet overflow");

	memcpy(&NetBuffer[NetBufferLength], HCDEConnectMagic, sizeof(HCDEConnectMagic));
	NetBufferLength += sizeof(HCDEConnectMagic);
	NetBuffer[NetBufferLength++] = HCDEConnectProtocolVersion;
	NetBuffer[NetBufferLength++] = flags;
}

static uint8_t BuildLocalHCDEConnectFlags()
{
	uint8_t flags = 0u;
	if (DedicatedJoinMode)
		flags |= HCDE_CONNECT_DEDICATED_JOIN;
	if (SilentNetStartMode)
		flags |= HCDE_CONNECT_SUPPRESS_ROOM_UI;
	if (DedicatedJoinMode)
		flags |= HCDE_CONNECT_SERVER_AUTHORITY;
	return flags;
}

static const char* ConnectFlowName(ENetConnectFlow flow)
{
	switch (flow)
	{
	case NCF_SERVER_WAITING: return DedicatedServerMode ? "server-waiting" : "host-waiting";
	case NCF_CLIENT_AUTH: return DedicatedJoinMode ? "client-auth" : "guest-contacting";
	case NCF_SYNCING: return "syncing";
	default: return "idle";
	}
}

static int CountConnectedClients()
{
	int count = 0;
	const int firstClient = I_GetFirstPlayableClientSlot();
	for (int i = firstClient; i < MaxClients; ++i)
	{
		if (Connected[i].Status != CSTAT_NONE)
			++count;
	}
	return count;
}

static FServerQuerySnapshot BuildServerQuerySnapshot()
{
	FServerQuerySnapshot snapshot = {};
	const char* hostname = sv_hostname;
	snapshot.HostName = hostname != nullptr && hostname[0] != 0 ? FString(hostname) : FString(GAMENAME " server");
	snapshot.MapName = level.LevelName.IsNotEmpty() ? level.LevelName : FString("unknown");
	snapshot.SessionState = ConnectFlowName(NetConnectFlowState);
	snapshot.Version = GetVersionString();
	snapshot.GitHash = GetGitHash();
	const int connectedClients = CountConnectedClients();
	const int visibleMaxClients = I_GetVisibleMaxClients();
	const int advertisedMaxClients = sv_maxplayers > 0 ? clamp<int>(sv_maxplayers, connectedClients, visibleMaxClients) : visibleMaxClients;
	snapshot.MaxPlayers = uint8_t(clamp<int>(advertisedMaxClients, 0, UINT8_MAX));
	snapshot.PlayerCount = uint8_t(clamp<int>(connectedClients, 0, UINT8_MAX));
	snapshot.Skill = uint8_t(clamp<int>(gameskill, 0, UINT8_MAX));
	snapshot.Deathmatch = deathmatch != 0;
	snapshot.Teamplay = teamplay;
	snapshot.FragLimit = fraglimit > 0 ? uint16_t(clamp<int>(fraglimit, 0, UINT16_MAX)) : 0u;
	if (timelimit > 0.f)
	{
		const int timeleft = (int)(timelimit - level.time / (TICRATE * 60));
		snapshot.TimeLeft = uint16_t(max(timeleft, 0));
	}

	snapshot.Players.Reserve(snapshot.PlayerCount);
	const int firstClient = I_GetFirstPlayableClientSlot();
	for (int i = firstClient; i < MaxClients; ++i)
	{
		if (playeringame[i])
		{
			FServerQueryPlayer player = {};
			player.Name = players[i].userinfo.GetName(0u);
			player.Ping = uint16_t(clamp<unsigned int>(ClientStates[i].AverageLatency, 0u, UINT16_MAX));
			player.Frags = int16_t(clamp<int>(players[i].fragcount, INT16_MIN, INT16_MAX));
			player.Kills = int16_t(clamp<int>(players[i].killcount, INT16_MIN, INT16_MAX));
			player.Deaths = 0;
			snapshot.Players.Push(player);
		}
	}

	return snapshot;
}

} // namespace

void I_GetLocalServerSnapshot(FServerQuerySnapshot& snapshot)
{
	snapshot = BuildServerQuerySnapshot();
}

namespace
{
struct FQueryWriter
{
	std::array<uint8_t, MAX_MSGLEN> Buffer = {};
	size_t Offset = 0u;

	bool WriteByte(uint8_t value)
	{
		if (Offset + 1u > Buffer.size())
			return false;
		Buffer[Offset++] = value;
		return true;
	}

	bool WriteShort(uint16_t value)
	{
		if (Offset + 2u > Buffer.size())
			return false;
		Buffer[Offset++] = uint8_t(value >> 8);
		Buffer[Offset++] = uint8_t(value);
		return true;
	}

	bool WriteLong(uint32_t value)
	{
		if (Offset + 4u > Buffer.size())
			return false;
		Buffer[Offset++] = uint8_t(value >> 24);
		Buffer[Offset++] = uint8_t(value >> 16);
		Buffer[Offset++] = uint8_t(value >> 8);
		Buffer[Offset++] = uint8_t(value);
		return true;
	}

	bool WriteString(const char* value)
	{
		const size_t len = strlen(value) + 1u;
		if (Offset + len > Buffer.size())
			return false;
		memcpy(&Buffer[Offset], value, len);
		Offset += len;
		return true;
	}
};

static bool SendLauncherInfo(const sockaddr_in& to, const uint8_t* request, int msgSize)
{
	FQueryWriter writer = {};
	const FServerQuerySnapshot snapshot = BuildServerQuerySnapshot();

	if (!writer.WriteLong(uint32_t(MSG_CHALLENGE)) ||
	    !writer.WriteLong(uint32_t(I_msTime() & 0xffffffffu)))
	{
		return false;
	}

	if (msgSize >= 8)
	{
		if (!writer.WriteLong(ReadBE32(request + 4u)))
			return false;
	}

	if (!writer.WriteString(snapshot.HostName.GetChars()) ||
	    !writer.WriteByte(snapshot.PlayerCount) ||
	    !writer.WriteByte(snapshot.MaxPlayers) ||
	    !writer.WriteString(snapshot.MapName.GetChars()) ||
	    !writer.WriteString(snapshot.SessionState.GetChars()) ||
	    !writer.WriteByte(snapshot.Deathmatch ? 1u : 0u) ||
	    !writer.WriteByte(snapshot.Skill) ||
	    !writer.WriteByte(snapshot.Teamplay ? 1u : 0u) ||
	    !writer.WriteShort(snapshot.TimeLeft) ||
	    !writer.WriteShort(snapshot.FragLimit) ||
	    !writer.WriteString(snapshot.Version.GetChars()) ||
	    !writer.WriteString(snapshot.GitHash.GetChars()) ||
	    !writer.WriteByte(uint8_t(snapshot.Players.Size())))
	{
		return false;
	}

	for (const auto& player : snapshot.Players)
	{
		if (!writer.WriteString(player.Name.GetChars()) ||
		    !writer.WriteShort(player.Ping) ||
		    !writer.WriteShort(uint16_t(player.Frags)) ||
		    !writer.WriteShort(uint16_t(player.Kills)) ||
		    !writer.WriteShort(uint16_t(player.Deaths)))
		{
			return false;
		}
	}

	if (sendto(MySocket, reinterpret_cast<const char*>(writer.Buffer.data()), static_cast<int>(writer.Offset), 0,
	           reinterpret_cast<const sockaddr*>(&to), sizeof(to)) == SOCKET_ERROR)
	{
		Printf("Failed to send launcher response: %s\n", neterror());
		return false;
	}
	return true;
}

static bool WriteSessionToken(size_t offset, uint32_t token)
{
	if (offset + 4u > MaxTransmitSize)
		return false;

	WriteBE32(&NetBuffer[offset], token);
	return true;
}

static uint32_t ReadSessionToken(const uint8_t* data, size_t offset)
{
	return ReadBE32(&data[offset]);
}

static bool TryHandleServerQuery(const sockaddr_in& from, const uint8_t* request, int msgSize)
{
	if (msgSize < 4)
		return false;

	const uint32_t challenge = ReadBE32(request);
	if (challenge == uint32_t(LAUNCHER_CHALLENGE) || challenge == uint32_t(PROTO_CHALLENGE))
	{
		DebugTrace::Markf("net", "server query challenge=%u players=%d", challenge, CountConnectedClients());
		return SendLauncherInfo(from, request, msgSize);
	}

	if (((challenge >> 20) & 0x0FFFu) == ODAMEX_QUERY_TAG_ID)
	{
		DebugTrace::Markf("net", "server query tag=%u players=%d", challenge, CountConnectedClients());
		return SendLauncherInfo(from, request, msgSize);
	}

	return false;
}
}

CUSTOM_CVAR(String, net_password, "", CVAR_IGNORE)
{
	if (strlen(self) + 1 > MaxPasswordSize)
	{
		self = "";
		Printf(TEXTCOLOR_RED "Password cannot be greater than 255 characters\n");
	}
}

// Game-specific API
size_t Net_SetEngineInfo(uint8_t*& stream);
FVerificationError Net_VerifyEngine(uint8_t*& stream, size_t& offset, size_t packetLength);
void Net_SetupUserInfo();
const char* Net_GetClientName(int client, unsigned int charLimit);
void Net_SetUserInfo(int client, TArrayView<uint8_t>& stream);
void Net_ReadUserInfo(int client, TArrayView<uint8_t>& stream);
void Net_ReadGameInfo(TArrayView<uint8_t>& stream);
void Net_SetGameInfo(TArrayView<uint8_t>& stream);
void Net_ReadMapLoadInfo(TArrayView<uint8_t>& stream);
void Net_SetMapLoadInfo(TArrayView<uint8_t>& stream);
void Net_ReadServerInfo(TArrayView<uint8_t>& stream);
void Net_SetServerInfo(TArrayView<uint8_t>& stream);

static SOCKET CreateUDPSocket()
{
	SOCKET s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
		I_FatalError("Couldn't create socket: %s", neterror());

	return s;
}

static void BindToLocalPort(SOCKET s, u_short port)
{
	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	int v = bind(s, (sockaddr *)&address, sizeof(address));
	if (v == SOCKET_ERROR)
		I_FatalError("Couldn't bind to port: %s", neterror());
}

static bool TryBuildAddress(sockaddr_in& address, const char* addrName, FString* error)
{
	FString target = {};
	u_short port = GamePort;
	const char* portName = strchr(addrName, ':');
	if (portName != nullptr)
	{
		target = FString(addrName, portName - addrName);
		u_short portConversion = atoi(portName + 1);
		if (!portConversion)
		{
			if (error != nullptr)
				error->Format("Malformed port: %s", portName + 1);
			return false;
		}
		else
			port = portConversion;
	}
	else
	{
		target = addrName;
	}

	bool isNamed = false;
	for (size_t curChar = 0u; curChar < target.Len(); ++curChar)
	{
		char c = target[curChar];
		if ((c < '0' || c > '9') && c != '.')
		{
			isNamed = true;
			break;
		}
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	if (!isNamed)
	{
		address.sin_addr.s_addr = inet_addr(target.GetChars());
		if (address.sin_addr.s_addr == INADDR_NONE && strcmp(target.GetChars(), "255.255.255.255") != 0)
		{
			if (error != nullptr)
				error->Format("Couldn't parse address: %s", target.GetChars());
			return false;
		}
	}
	else
	{
		hostent* hostEntry = gethostbyname(target.GetChars());
		if (hostEntry == nullptr)
		{
			if (error != nullptr)
				error->Format("gethostbyname: Couldn't find %s (%s)", target.GetChars(), neterror());
			return false;
		}

		address.sin_addr.s_addr = *(int*)hostEntry->h_addr_list[0];
	}

	return true;
}

static void BuildAddress(sockaddr_in& address, const char* addrName)
{
	FString error;
	if (!TryBuildAddress(address, addrName, &error))
		I_FatalError("%s", error.GetChars());
}

static bool ReadQueryByte(const uint8_t* data, size_t& offset, size_t limit, uint8_t& value)
{
	if (offset + 1u > limit)
		return false;

	value = data[offset++];
	return true;
}

static bool ReadQueryShort(const uint8_t* data, size_t& offset, size_t limit, uint16_t& value)
{
	if (offset + 2u > limit)
		return false;

	value = (uint16_t(data[offset]) << 8) | uint16_t(data[offset + 1u]);
	offset += 2u;
	return true;
}

static bool ReadQueryString(const uint8_t* data, size_t& offset, size_t limit, FString& value)
{
	if (offset >= limit)
		return false;

	const size_t start = offset;
	while (offset < limit && data[offset] != 0u)
		++offset;

	if (offset >= limit)
		return false;

	value = FString(reinterpret_cast<const char*>(&data[start]));
	++offset;
	return true;
}

static bool TryReadServerQuerySnapshot(const uint8_t* data, size_t length, FServerQuerySnapshot& snapshot, FString* error)
{
	if (length < 8u)
	{
		if (error != nullptr)
			error->Format("Query reply was too short");
		return false;
	}

	const uint32_t challenge = ReadBE32(data);
	if (challenge != uint32_t(MSG_CHALLENGE))
	{
		if (error != nullptr)
			error->Format("Unexpected query reply header: %u", challenge);
		return false;
	}

	size_t offset = 8u;
	uint8_t deathmatch = 0u;
	uint8_t skill = 0u;
	uint8_t teamplay = 0u;
	if (!ReadQueryString(data, offset, length, snapshot.HostName) ||
	    !ReadQueryByte(data, offset, length, snapshot.PlayerCount) ||
	    !ReadQueryByte(data, offset, length, snapshot.MaxPlayers) ||
	    !ReadQueryString(data, offset, length, snapshot.MapName) ||
	    !ReadQueryString(data, offset, length, snapshot.SessionState) ||
	    !ReadQueryByte(data, offset, length, deathmatch) ||
	    !ReadQueryByte(data, offset, length, skill) ||
	    !ReadQueryByte(data, offset, length, teamplay) ||
	    !ReadQueryShort(data, offset, length, snapshot.TimeLeft) ||
	    !ReadQueryShort(data, offset, length, snapshot.FragLimit) ||
	    !ReadQueryString(data, offset, length, snapshot.Version) ||
	    !ReadQueryString(data, offset, length, snapshot.GitHash))
	{
		if (error != nullptr)
			error->Format("Query reply was truncated");
		return false;
	}
	snapshot.Deathmatch = deathmatch != 0u;
	snapshot.Skill = skill;
	snapshot.Teamplay = teamplay != 0u;

	uint8_t playerCount = 0u;
	if (!ReadQueryByte(data, offset, length, playerCount))
	{
		if (error != nullptr)
			error->Format("Query reply was truncated");
		return false;
	}
	snapshot.PlayerCount = playerCount;

	snapshot.Players.Clear();
	snapshot.Players.Reserve(playerCount);
	for (uint8_t i = 0u; i < playerCount; ++i)
	{
		FServerQueryPlayer player = {};
		uint16_t ping = 0u;
		uint16_t frags = 0u;
		uint16_t kills = 0u;
		uint16_t deaths = 0u;
		if (!ReadQueryString(data, offset, length, player.Name) ||
		    !ReadQueryShort(data, offset, length, ping) ||
		    !ReadQueryShort(data, offset, length, frags) ||
		    !ReadQueryShort(data, offset, length, kills) ||
		    !ReadQueryShort(data, offset, length, deaths))
		{
			if (error != nullptr)
				error->Format("Query player list was truncated");
			return false;
		}

		player.Ping = ping;
		player.Frags = int16_t(frags);
		player.Kills = int16_t(kills);
		player.Deaths = int16_t(deaths);
		snapshot.Players.Push(player);
	}

	return true;
}

bool I_QueryServerInfo(const char* addrName, FServerQuerySnapshot& snapshot, FString* error)
{
	snapshot = {};
	if (error != nullptr)
		*error = "";

	sockaddr_in address = {};
	if (!TryBuildAddress(address, addrName, error))
		return false;

#ifdef _WIN32
	WSADATA data;
	if (WSAStartup(0x0101, &data) != 0)
	{
		if (error != nullptr)
			error->Format("Couldn't initialize Windows sockets");
		return false;
	}
#endif

	bool success = false;
	SOCKET socketHandle = INVALID_SOCKET;
	do
	{
		socketHandle = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (socketHandle == INVALID_SOCKET)
		{
			if (error != nullptr)
				error->Format("Couldn't create socket: %s", neterror());
			break;
		}

		std::array<uint8_t, 4> request = {};
		WriteBE32(request.data(), uint32_t(LAUNCHER_CHALLENGE));
		if (sendto(socketHandle, reinterpret_cast<const char*>(request.data()), static_cast<int>(request.size()), 0,
		           reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
		{
			if (error != nullptr)
				error->Format("Failed to send query: %s", neterror());
			break;
		}

		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(socketHandle, &readset);
		timeval timeout = {};
		timeout.tv_sec = 2;
		const int selectResult = select(static_cast<int>(socketHandle + 1), &readset, nullptr, nullptr, &timeout);
		if (selectResult <= 0)
		{
			if (error != nullptr)
				error->Format(selectResult == 0 ? "Query timed out" : "Query wait failed: %s", neterror());
			break;
		}

		std::array<uint8_t, MAX_MSGLEN> reply = {};
		sockaddr_in from = {};
		socklen_t fromSize = sizeof(from);
		const int replySize = recvfrom(socketHandle, reinterpret_cast<char*>(reply.data()), static_cast<int>(reply.size()), 0,
		                               reinterpret_cast<sockaddr*>(&from), &fromSize);
		if (replySize <= 0)
		{
			if (error != nullptr)
				error->Format("Failed to read query reply: %s", neterror());
			break;
		}

		success = TryReadServerQuerySnapshot(reply.data(), static_cast<size_t>(replySize), snapshot, error);
	} while (false);

	if (socketHandle != INVALID_SOCKET)
		closesocket(socketHandle);
#ifdef _WIN32
	WSACleanup();
#endif
	return success;
}

static void StartNetwork(bool autoPort)
{
#ifdef _WIN32
	WSADATA data;
	if (!DebugServer::RuntimeEvents::IsDebugServerRunning()) {
		if (WSAStartup(0x0101, &data))
			I_FatalError("Couldn't initialize Windows sockets");
	}
#endif

	netgame = true;
	multiplayer = true;
	MySocket = CreateUDPSocket();
	BindToLocalPort(MySocket, autoPort ? 0 : GamePort);

	u_long trueVal = 1u;
#ifndef __sun
	ioctlsocket(MySocket, FIONBIO, &trueVal);
#else
	fcntl(MySocket, F_SETFL, trueVal | O_NONBLOCK);
#endif
}

void CloseNetwork()
{
	DebugTrace::Mark("net", "close network");
	SV_ShutdownMasters();
	if (MySocket != INVALID_SOCKET)
	{
		closesocket(MySocket);
		MySocket = INVALID_SOCKET;
		netgame = false;
	}
#ifdef _WIN32
	if (!DebugServer::RuntimeEvents::IsDebugServerRunning()){
		WSACleanup();
	}
#endif
}

static void GenerateGameID()
{
	const uint64_t val = GameIDGen.GenRand64();
	memcpy(GameID, &val, sizeof(val));
}

// Print a network-related message to the console. This doesn't print to the window so should
// not be used for that and is mainly for logging.
static void I_NetLog(const char* text, ...)
{
	// todo: use better abstraction once everything is migrated to in-game start screens.
#if defined _WIN32 || defined __APPLE__
	va_list ap;
	va_start(ap, text);
	VPrintf(PRINT_HIGH, text, ap);
	Printf("\n");
	va_end(ap);
#else
	FString str;
	va_list argptr;

	va_start(argptr, text);
	str.VFormat(text, argptr);
	va_end(argptr);
	fprintf(stderr, "\r%-40s\n", str.GetChars());
#endif
}

static void SetConnectFlow(ENetConnectFlow flow)
{
	if (NetConnectFlowState != flow)
	{
		NetConnectFlowState = flow;
		DebugTrace::Markf("net", "connect flow=%s", ConnectFlowName(flow));
	}
}

// Gracefully closes the net window so that any error messaging can be properly displayed.
static void I_NetError(const char* error)
{
	if (!DedicatedServerMode && !SilentNetStartMode)
		NetStartWindow::NetClose();
	I_FatalError("%s", error);
}

static void I_NetInit(const char* msg, bool host)
{
	Printf("%s:: %s\n", DedicatedServerMode ? "NetServer" : "NetSession", msg);
#if defined(_WIN32) && defined(HCDE_DEDICATED_SERVER)
	if (DedicatedServerMode)
	{
		I_SetDedicatedServerConsoleStatus(msg);
	}
#endif
	if (!DedicatedServerMode && !SilentNetStartMode)
	{
		HCDE_ServerMode_GuardClientSubsystem("network session window");
		NetStartWindow::NetInit(msg, host);
	}
}

// todo: later these must be dispatched by the main menu, not the start screen.
// Updates the general status of the session flow.
static void I_NetMessage(const char* msg)
{
	Printf("%s:: %s\n", DedicatedServerMode ? "NetServer" : "NetSession", msg);
#if defined(_WIN32) && defined(HCDE_DEDICATED_SERVER)
	if (DedicatedServerMode)
	{
		I_SetDedicatedServerConsoleStatus(msg);
	}
#endif
	if (!DedicatedServerMode && !SilentNetStartMode)
	{
		HCDE_ServerMode_GuardClientSubsystem("network session message");
		NetStartWindow::NetMessage(msg);
	}
}

// Listen for incoming connections while the pregame server flow is active. The main thread needs to be locked up
// here to prevent the engine from continuing to start the game until everyone is ready.
static bool I_NetLoop(bool (*loopCallback)(void*), void* data)
{
	if (DedicatedServerMode || SilentNetStartMode)
	{
		while (!loopCallback(data))
		{
#if defined(_WIN32) && defined(HCDE_DEDICATED_SERVER)
			if (DedicatedServerMode)
			{
				I_PumpDedicatedServerConsoleWindow();
				if (DedicatedServerAbortRequested)
					return false;
			}
#endif
			Sleep(1);
		}
#if defined(_WIN32) && defined(HCDE_DEDICATED_SERVER)
		if (DedicatedServerMode)
		{
			I_PumpDedicatedServerConsoleWindow();
		}
#endif
		return !DedicatedServerAbortRequested;
	}
	return NetStartWindow::NetLoop(loopCallback, data);
}

// A new client has just entered the game, so add them to the player list.
static void I_NetClientConnected(int client, unsigned int charLimit = 0u)
{
	if (I_IsServerReservedSlot(client))
	{
		Printf("%s:: Dedicated server authority slot ready.\n", DedicatedServerMode ? "NetServer" : "NetSession");
		return;
	}

	Printf("%s:: Client '%s' connected.\n", DedicatedServerMode ? "NetServer" : "NetSession", Net_GetClientName(client, 0u));

	const char* name = Net_GetClientName(client, charLimit);
	unsigned int flags = CFL_NONE;
	if (I_IsHCDEServiceAuthoritySlot(client) && !I_IsServerReservedSlot(client))
		flags |= CFL_HOST;
	if (client == consoleplayer)
		flags |= CFL_CONSOLEPLAYER;

	NetStartWindow::NetConnect(client, name, flags, Connected[client].Status);
}

// A client changed ready state.
static void I_NetClientUpdated(int client)
{
	NetStartWindow::NetUpdate(client, Connected[client].Status);
}

static void I_NetClientDisconnected(int client)
{
	Printf("%s:: Client '%s' disconnected.\n", DedicatedServerMode ? "NetServer" : "NetSession", Net_GetClientName(client, 0u));
	NetStartWindow::NetDisconnect(client);
}

static void I_NetUpdatePlayers(int current, int limit)
{
	if (I_UsesDedicatedServerSlot())
	{
		current = max(current - 1, 0);
		limit = max(limit - 1, 0);
	}
	NetStartWindow::NetProgress(current, limit);
}

static bool I_ShouldStartNetGame()
{
	if (DedicatedServerMode)
		return DedicatedServerStartRequested;
	return NetStartWindow::ShouldStartNet();
}

static void I_GetKickClients(TArray<int>& clients)
{
	clients.Clear();

	int c = -1;
	while ((c = NetStartWindow::GetNetKickClient()) != -1)
		clients.Push(c);
}

static void I_GetBanClients(TArray<int>& clients)
{
	clients.Clear();

	int c = -1;
	while ((c = NetStartWindow::GetNetBanClient()) != -1)
		clients.Push(c);
}

void I_NetDone()
{
	NetStartWindow::NetDone();
}

void I_ClearClient(size_t client)
{
	Connected[client].Clear();
}

static int FindClient(const sockaddr_in& address)
{
	int i = 0;
	for (; i < MaxClients; ++i)
	{
		if (Connected[i].Status == CSTAT_NONE)
			continue;

		if (address.sin_addr.s_addr == Connected[i].Address.sin_addr.s_addr
			&& address.sin_port == Connected[i].Address.sin_port)
		{
			break;
		}
	}

	return i >= MaxClients ? -1 : i;
}

static void SendPacket(const sockaddr_in& to)
{
	// Huge packets should be sent out as sequences, not as one big packet, otherwise it's prone
	// to high amounts of congestion and reordering needed.
	if (NetBufferLength > MAX_MSGLEN)
		I_FatalError("Netbuffer overflow: Tried to send %lu bytes of data", NetBufferLength);

	assert(!(NetBuffer[0] & NCMD_COMPRESSED));

	uint8_t* dataStart = &TransmitBuffer[4];
	uLong size = MaxTransmitSize - 5u;
	if (NetBufferLength >= MinCompressionSize)
	{
		*dataStart = NetBuffer[0] | NCMD_COMPRESSED;
		const int res = compress2(dataStart + 1, &size, NetBuffer + 1, NetBufferLength - 1u, 9);
		if (res != Z_OK)
			I_Error("Net compression failed (zlib error %d)", res);

		++size;
	}
	else
	{
		memcpy(dataStart, NetBuffer, NetBufferLength);
		size = NetBufferLength;
	}

	if (size + 4 > MaxTransmitSize)
		I_Error("Failed to compress data down to acceptable transmission size");

	// If a connection packet, don't check the game id since they might not have it yet.
	const uint32_t crc = (NetBuffer[0] & NCMD_SETUP) ? CalcCRC32(dataStart, size) : AddCRC32(CalcCRC32(dataStart, size), GameID, std::extent_v<decltype(GameID)>);
	TransmitBuffer[0] = crc >> 24;
	TransmitBuffer[1] = crc >> 16;
	TransmitBuffer[2] = crc >> 8;
	TransmitBuffer[3] = crc;

	sendto(MySocket, (const char*)TransmitBuffer, size + 4, 0, (const sockaddr*)&to, sizeof(to));
}

static bool FlushHCDEReliableServices(const sockaddr_in& to, FConnection& connection, bool force = false)
{
	ClearAckedHCDEReliableServices(connection);
	auto* pending = FindOldestHCDEReliableService(connection);
	if (pending == nullptr)
		return false;

	const uint64_t now = I_msTime();
	if (!force && pending->SendCount > 0u && now - pending->LastSendTime < HCDEServiceResendMS)
		return true;

	if (pending->Packet.Size() < HCDEServiceHeaderSize)
	{
		DebugTrace::Markf("net", "dropping malformed retained service %s key=%u seq=%u len=%u", HCDEServiceName(pending->Service), pending->Key, pending->Sequence, pending->Packet.Size());
		pending->Clear();
		return false;
	}

	NetBufferLength = pending->Packet.Size();
	memcpy(NetBuffer, pending->Packet.Data(), NetBufferLength);
	WriteBE32(&NetBuffer[HCDEServiceAckOffset], connection.HCDEServiceRxSeq);
	WriteBE32(&pending->Packet[HCDEServiceAckOffset], connection.HCDEServiceRxSeq);
	SendPacket(to);

	if (pending->SendCount == 0u)
		pending->FirstSendTime = now;
	pending->LastSendTime = now;
	++pending->SendCount;
	DebugTrace::Markf("net", "sent reliable service %s key=%u seq=%u ack=%u count=%u", HCDEServiceName(pending->Service), pending->Key, pending->Sequence, connection.HCDEServiceRxSeq, pending->SendCount);
	return true;
}

static bool BeginReliableHCDEPregameService(EHCDEPregameService service, FConnection& connection, uint8_t key)
{
	if (FindHCDEReliableService(connection, service, key) != nullptr)
	{
		DebugTrace::Markf("net", "reusing pending reliable service %s key=%u", HCDEServiceName(service), key);
		return false;
	}
	if (FindFreeHCDEReliableService(connection) == nullptr)
	{
		DebugTrace::Markf("net", "reliable service queue full while adding %s key=%u", HCDEServiceName(service), key);
		return false;
	}

	BeginHCDEPregameService(service, connection);
	return true;
}

static bool CommitReliableHCDEPregameService(const sockaddr_in& to, FConnection& connection, EHCDEPregameService service, uint8_t key)
{
	auto* pending = FindFreeHCDEReliableService(connection);
	if (pending == nullptr)
	{
		DebugTrace::Markf("net", "reliable service queue full while committing %s key=%u", HCDEServiceName(service), key);
		return false;
	}

	pending->Active = true;
	pending->Service = service;
	pending->Key = key;
	pending->Sequence = ReadBE32(&NetBuffer[HCDEServiceSequenceOffset]);
	pending->FirstSendTime = 0u;
	pending->LastSendTime = 0u;
	pending->SendCount = 0u;
	pending->Packet.Resize(NetBufferLength);
	memcpy(pending->Packet.Data(), NetBuffer, NetBufferLength);

	DebugTrace::Markf("net", "queued reliable service %s key=%u seq=%u len=%zu", HCDEServiceName(service), key, pending->Sequence, NetBufferLength);
	FlushHCDEReliableServices(to, connection);
	return true;
}

static void GetPacket(sockaddr_in* const from = nullptr)
{
	sockaddr_in fromAddress;
	socklen_t fromSize = sizeof(fromAddress);

	int msgSize = recvfrom(MySocket, (char *)TransmitBuffer, MaxTransmitSize, 0,
				  (sockaddr *)&fromAddress, &fromSize);

	int client = FindClient(fromAddress);
	if (client >= 0 && msgSize == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAECONNRESET)
		{
			if (consoleplayer == -1)
			{
				client = -1;
				msgSize = 0;
			}
			else
			{
				// The remote node aborted unexpectedly, so pretend it sent an exit packet. If it was the
				// authority, the game is too bricked to continue because authority migration owns recovery.
				if (I_IsHCDEServiceAuthoritySlot(client))
					I_NetError("Authority unexpectedly disconnected");

				NetBuffer[0] = NCMD_EXIT;
				msgSize = 1;
			}
		}
		else if (err != WSAEWOULDBLOCK)
		{
			I_Error("Failed to get packet: %s", neterror());
		}
		else
		{
			client = -1;
			msgSize = 0;
		}
	}
	else if (msgSize > 0)
	{
		if (TryHandleServerQuery(fromAddress, TransmitBuffer, msgSize))
		{
			RemoteClient = -1;
			NetBufferLength = 0u;
			if (from != nullptr)
				*from = fromAddress;
			return;
		}

		const uint8_t* dataStart = &TransmitBuffer[4];
		if (client == -1 && !(*dataStart & NCMD_SETUP))
		{
			msgSize = 0;
		}
		else if (client == -1 && bGameStarted)
		{
			NetBuffer[0] = NCMD_SETUP;
			NetBuffer[1] = PRE_IN_PROGRESS;
			NetBufferLength = 2u;
			SendPacket(fromAddress);
			msgSize = 0;
		}
		else
		{
			const uint32_t check = (*dataStart & NCMD_SETUP) ? CalcCRC32(dataStart, msgSize - 4) : AddCRC32(CalcCRC32(dataStart, msgSize - 4), GameID, std::extent_v<decltype(GameID)>);
			const uint32_t crc = (TransmitBuffer[0] << 24) | (TransmitBuffer[1] << 16) | (TransmitBuffer[2] << 8) | TransmitBuffer[3];
			if (check != crc)
			{
				DPrintf(DMSG_NOTIFY, "Checksum on packet failed: expected %u, got %u", check, crc);
				client = -1;
				msgSize = 0;
			}
			else
			{
				NetBuffer[0] = (*dataStart & ~NCMD_COMPRESSED);
				if (*dataStart & NCMD_COMPRESSED)
				{
					uLongf size = MAX_MSGLEN - 1;
					int err = uncompress(NetBuffer + 1, &size, dataStart + 1, msgSize - 5);
					if (err != Z_OK)
					{
						Printf("Net decompression failed (zlib error %s)\n", M_ZLibError(err).GetChars());
						client = -1;
						msgSize = 0;
					}
					else
					{
						msgSize = size + 1;
					}
				}
				else
				{
					msgSize -= 4;
					memcpy(NetBuffer + 1, dataStart + 1, msgSize - 1);
				}
			}
		}
	}
	else
	{
		client = -1;
	}

	RemoteClient = client;
	NetBufferLength = max<int>(msgSize, 0);
	if (from != nullptr)
		*from = fromAddress;
}

void I_NetCmd(ENetCommand cmd)
{
	if (cmd == CMD_SEND)
	{
		if (RemoteClient >= 0)
			SendPacket(Connected[RemoteClient].Address);
	}
	else if (cmd == CMD_GET)
	{
		GetPacket();
	}
}

static void SetClientAck(size_t client, size_t from, bool add)
{
	const uint64_t bit = (uint64_t)1u << from;
	if (add)
		Connected[client].InfoAck |= bit;
	else
		Connected[client].InfoAck &= ~bit;
}

static bool ClientGotAck(size_t client, size_t from)
{
	return (Connected[client].InfoAck & ((uint64_t)1u << from));
}

static bool GetConnection(sockaddr_in& from)
{
	GetPacket(&from);
	return NetBufferLength > 0;
}

static void RejectConnection(const sockaddr_in& to, ENetConnectType reason)
{
	NetBuffer[0] = NCMD_SETUP;
	NetBuffer[1] = reason;
	NetBufferLength = 2u;

	SendPacket(to);
}

static void SendVerificationError(const sockaddr_in& to, const FVerificationError& error)
{
	NetBuffer[0] = NCMD_SETUP;
	NetBuffer[1] = PRE_VERIFICATION_ERROR;
	NetBuffer[2] = error.Error;
	if (error.Error == FVerificationError::VE_ENGINE)
	{
		NetBuffer[3] = error.Major;
		NetBuffer[4] = error.Minor;
		NetBuffer[5] = error.Revision;
		NetBuffer[6] = error.NetMajor;
		NetBuffer[7] = error.NetMinor;
		NetBuffer[8] = error.NetRevision;
		NetBufferLength = 9u;
	}
	else
	{
		const TArray<FString>* ar = nullptr;
		if (error.Error == FVerificationError::VE_FILE_UNKNOWN)
			ar = &error.UnknownFiles;
		else if (error.Error == FVerificationError::VE_FILE_ORDER)
			ar = &error.ExpectedOrder;
		else if (error.Error == FVerificationError::VE_FILE_MISSING)
			ar = &error.MissingFiles;

		if (ar == nullptr)
		{
			DebugTrace::Markf("net", "verification error payload type %u has no list", error.Error);
			NetBuffer[3] = NetBuffer[4] = NetBuffer[5] = NetBuffer[6] = 0u;
			NetBufferLength = 7u;
			SendPacket(to);
			return;
		}

		size_t count = 0u;
		size_t i = 7u;
		for (auto& file : *ar)
		{
			const size_t len = static_cast<size_t>(file.Len()) + 1u;
			if (i + len > MAX_MSGLEN)
			{
				DebugTrace::Markf("net", "verification error truncated after %zu entries", count);
				Printf("Verification error reply truncated to fit packet size\n");
				break;
			}

			memcpy(&NetBuffer[i], file.GetChars(), file.Len() + 1u);
			i += len;
			++count;
		}
		NetBuffer[3] = (count >> 24);
		NetBuffer[4] = (count >> 16);
		NetBuffer[5] = (count >> 8);
		NetBuffer[6] = count;
		NetBufferLength = i;
	}

	SendPacket(to);
}

static void AddClientConnection(const sockaddr_in& from, int client, const FHCDEConnectInfo& connectInfo)
{
	Net_ResetClientState(client);
	Connected[client].Status = CSTAT_CONNECTING;
	Connected[client].Address = from;
	Connected[client].SessionToken = MakeSessionToken(from, client);
	Connected[client].bHCDEConnect = connectInfo.Present;
	Connected[client].HCDEConnectVersion = connectInfo.Version;
	Connected[client].HCDEConnectFlags = connectInfo.Flags;
	NetworkClients += client;
	if (connectInfo.Present)
	{
		I_NetLog("Client %u connected to server with HCDE service connect v%u flags=0x%02x",
			client, connectInfo.Version, connectInfo.Flags);
	}
	else
	{
		I_NetLog("Client %u %s", client, DedicatedServerMode ? "connected to server using legacy setup" : "joined the host");
	}
	I_NetClientUpdated(client);

	// Make sure any ready clients are marked as needing the new client's info.
	for (int i = 1; i < MaxClients; ++i)
	{
		if (Connected[i].Status == CSTAT_READY)
		{
			Connected[i].Status = CSTAT_WAITING;
			I_NetClientUpdated(i);
		}
	}
}

static void RemoveClientConnection(int client)
{
	I_NetClientDisconnected(client);
	players[client].settings_controller = false;
	I_ClearClient(client);
	NetworkClients -= client;
	I_NetLog("Client %u %s", client, DedicatedServerMode ? "disconnected from server" : "left the host");

	// Let everyone else know the user left as well.
	NetBuffer[0] = NCMD_SETUP;
	NetBuffer[1] = PRE_DISCONNECT;
	NetBuffer[2] = client;

	for (int i = 1; i < MaxClients; ++i)
	{
		if (Connected[i].Status == CSTAT_NONE)
			continue;

		SetClientAck(i, client, false);
		WriteBE32(&NetBuffer[3], Connected[i].SessionToken);
		NetBufferLength = 7u;
		SendPacket(Connected[i].Address);
	}
}

static bool DropClientForHCDETimeout(int client, int* connectedPlayers, const char* context)
{
	if (client <= 0 || client >= MaxClients || Connected[client].Status == CSTAT_NONE || !Connected[client].bHCDEConnect)
		return false;

	auto* pending = FindTimedOutHCDEReliableService(Connected[client], I_msTime());
	if (pending == nullptr)
		return false;

	const auto service = pending->Service;
	const uint8_t key = pending->Key;
	const uint32_t sequence = pending->Sequence;
	const uint32_t sends = pending->SendCount;
	const uint64_t elapsed = I_msTime() - pending->FirstSendTime;
	const sockaddr_in timedOutAddress = Connected[client].Address;

	I_NetLog("Client %d timed out during %s on HCDE service %s key=%u seq=%u after %llu ms (%u sends)",
		client, context, HCDEServiceName(service), key, sequence, (unsigned long long)elapsed, sends);
	DebugTrace::Markf("net", "dropping client %d after HCDE service timeout context=%s service=%s key=%u seq=%u elapsed=%llu sends=%u",
		client, context, HCDEServiceName(service), key, sequence, (unsigned long long)elapsed, sends);

	RejectConnection(timedOutAddress, PRE_SETUP_TIMEOUT);
	RemoveClientConnection(client);
	if (connectedPlayers != nullptr && *connectedPlayers > 0)
	{
		--*connectedPlayers;
		I_NetUpdatePlayers(*connectedPlayers, MaxClients);
	}
	return true;
}

void HandleIncomingConnection()
{
	if (!I_IsLocalHCDEServiceAuthority() || RemoteClient == -1)
		return;

	if (Connected[RemoteClient].Status == CSTAT_READY)
	{
		if (Connected[RemoteClient].bHCDEConnect)
		{
			if (BeginReliableHCDEPregameService(HPS_START_GAME, Connected[RemoteClient], 0u))
				CommitReliableHCDEPregameService(Connected[RemoteClient].Address, Connected[RemoteClient], HPS_START_GAME, 0u);
		}
		else
		{
			NetBuffer[0] = NCMD_SETUP;
			NetBuffer[1] = PRE_GO;
			WriteBE32(&NetBuffer[2], Connected[RemoteClient].SessionToken);
			NetBufferLength = 6u;
			SendPacket(Connected[RemoteClient].Address);
		}
	}
}

static bool Host_CheckForConnections(void* connected)
{
	const bool hasPassword = strlen(net_password) > 0;
	int* connectedPlayers = (int*)connected;
	bool forceStarting = I_ShouldStartNetGame();
	if (DedicatedServerMode && forceStarting && *connectedPlayers <= 1)
	{
		DedicatedServerStartRequested = false;
		forceStarting = false;
		Printf("NetServer:: Start requested, but no playable clients are connected yet.\n");
	}

	TArray<int> toBoot = {};
	I_GetKickClients(toBoot);
	for (auto client : toBoot)
	{
		if (client <= 0 || Connected[client].Status == CSTAT_NONE)
			continue;

		sockaddr_in booted = Connected[client].Address;

		RemoveClientConnection(client);
		--*connectedPlayers;
		I_NetUpdatePlayers(*connectedPlayers, MaxClients);

		RejectConnection(booted, PRE_KICKED);
	}

	I_GetBanClients(toBoot);
	for (auto client : toBoot)
	{
		if (client <= 0 || Connected[client].Status == CSTAT_NONE)
			continue;

		sockaddr_in booted = Connected[client].Address;
		BannedConnections.Push(booted);

		RemoveClientConnection(client);
		--*connectedPlayers;
		I_NetUpdatePlayers(*connectedPlayers, MaxClients);

		RejectConnection(booted, PRE_BANNED);
	}

	sockaddr_in from;
	while (GetConnection(from))
	{
		if (NetBuffer[0] == NCMD_EXIT)
		{
			if (RemoteClient >= 0)
			{
				RemoveClientConnection(RemoteClient);
				--*connectedPlayers;
				I_NetUpdatePlayers(*connectedPlayers, MaxClients);
			}

			continue;
		}

		if (NetBuffer[0] != NCMD_SETUP)
			continue;

		if (NetBuffer[1] == PRE_CONNECT)
		{
			if (RemoteClient >= 0)
				continue;

			uint8_t* engineInfo = &NetBuffer[2];
			if (NetBufferLength < 9u)
			{
				DebugTrace::Markf("net", "malformed connect packet from %s (len=%zu)", inet_ntoa(from.sin_addr), NetBufferLength);
				RejectConnection(from, PRE_WRONG_PASSWORD);
				continue;
			}

			size_t passwordOffset = 0u;
			size_t banned = 0u;
			FVerificationError error = {};
			for (; banned < BannedConnections.Size(); ++banned)
			{
				if (BannedConnections[banned].sin_addr.s_addr == from.sin_addr.s_addr)
					break;
			}

			if (banned < BannedConnections.Size())
			{
				RejectConnection(from, PRE_BANNED);
				continue;
			}
			if ((error = Net_VerifyEngine(engineInfo, passwordOffset, NetBufferLength - 2u)).Error != FVerificationError::VE_NONE)
			{
				SendVerificationError(from, error);
				continue;
			}
			if (2u + passwordOffset >= NetBufferLength)
			{
				DebugTrace::Markf("net", "malformed connect password from %s (offset=%zu len=%zu)", inet_ntoa(from.sin_addr), passwordOffset, NetBufferLength);
				RejectConnection(from, PRE_WRONG_PASSWORD);
				continue;
			}

			const size_t passwordStart = 2u + passwordOffset;
			size_t passwordEnd = 0u;
			if (!FindStringEnd(passwordStart, NetBufferLength, passwordEnd))
			{
				DebugTrace::Markf("net", "unterminated connect password from %s (offset=%zu len=%zu)", inet_ntoa(from.sin_addr), passwordOffset, NetBufferLength);
				RejectConnection(from, PRE_WRONG_PASSWORD);
				continue;
			}

			FHCDEConnectInfo connectInfo = {};
			ReadHCDEConnectInfo(passwordEnd, connectInfo);
			if (connectInfo.Present && connectInfo.Version != HCDEConnectProtocolVersion)
			{
				DebugTrace::Markf("net", "unsupported HCDE service connect version %u from %s", connectInfo.Version, inet_ntoa(from.sin_addr));
				RejectConnection(from, PRE_PROTOCOL_ERROR);
				continue;
			}
			if (DedicatedServerMode && !connectInfo.Present)
			{
				Printf("NetServer:: Legacy setup connect from %s; launcher should use -dedicatedjoin for HCDE service connect.\n", inet_ntoa(from.sin_addr));
			}

			if (hasPassword && strcmp(net_password, (const char*)&NetBuffer[passwordStart]))
			{
				RejectConnection(from, PRE_WRONG_PASSWORD);
				continue;
			}
			if (*connectedPlayers >= MaxClients)
			{
				RejectConnection(from, PRE_FULL);
				continue;
			}
			if (forceStarting)
			{
				RejectConnection(from, PRE_IN_PROGRESS);
				continue;
			}

			int free = 1;
			for (; free < MaxClients; ++free)
			{
				if (Connected[free].Status == CSTAT_NONE)
					break;
			}

			AddClientConnection(from, free, connectInfo);
			++*connectedPlayers;
			I_NetUpdatePlayers(*connectedPlayers, MaxClients);
		}
		else if (NetBuffer[1] == PRE_USER_INFO)
		{
			if (Connected[RemoteClient].Status == CSTAT_CONNECTING)
			{
				if (NetBufferLength < 6u || !CheckSessionToken(Connected[RemoteClient], ReadSessionToken(NetBuffer, 2u), "host userinfo"))
					continue;

				TArrayView<uint8_t> stream = TArrayView(&NetBuffer[6], MAX_MSGLEN - 6);
				Net_ReadUserInfo(RemoteClient, stream);
				Connected[RemoteClient].Status = CSTAT_WAITING;
				I_NetClientConnected(RemoteClient, 16u);
			}
		}
		else if (NetBuffer[1] == PRE_USER_INFO_ACK)
		{
			if (NetBufferLength < 7u || !CheckSessionToken(Connected[RemoteClient], ReadSessionToken(NetBuffer, 2u), "host userinfo ack"))
				continue;

			SetClientAck(RemoteClient, NetBuffer[6], true);
		}
		else if (NetBuffer[1] == PRE_GAME_INFO_ACK)
		{
			if (NetBufferLength < 6u || !CheckSessionToken(Connected[RemoteClient], ReadSessionToken(NetBuffer, 2u), "host gameinfo ack"))
				continue;

			Connected[RemoteClient].bHasGameInfo = true;
		}
		else if (NetBuffer[1] == PRE_HCDE_SERVICE)
		{
			if (RemoteClient <= 0 || !Connected[RemoteClient].bHCDEConnect)
			{
				DebugTrace::Markf("net", "ignored HCDE service packet from unnegotiated client %d", RemoteClient);
				continue;
			}
			if (NetBufferLength < HCDEServiceHeaderSize)
			{
				DebugTrace::Markf("net", "host HCDE service packet too short len=%zu", NetBufferLength);
				continue;
			}

			const auto service = EHCDEPregameService(NetBuffer[2]);
			switch (service)
			{
			case HPS_CLIENT_USER_INFO:
			{
				if (Connected[RemoteClient].Status != CSTAT_CONNECTING)
					break;
				if (!CheckHCDEPregameService(RemoteClient, HCDEServiceHeaderSize, "host service userinfo"))
					break;

				TArrayView<uint8_t> stream = TArrayView(&NetBuffer[HCDEServiceHeaderSize], MAX_MSGLEN - HCDEServiceHeaderSize);
				Net_ReadUserInfo(RemoteClient, stream);
				Connected[RemoteClient].Status = CSTAT_WAITING;
				I_NetClientConnected(RemoteClient, 16u);
				break;
			}
			case HPS_USER_INFO_ACK:
				if (!CheckHCDEPregameService(RemoteClient, HCDEServiceHeaderSize + 1u, "host service userinfo ack"))
					break;
				SetClientAck(RemoteClient, NetBuffer[HCDEServiceHeaderSize], true);
				break;
			case HPS_GAME_INFO_ACK:
				if (!CheckHCDEPregameService(RemoteClient, HCDEServiceHeaderSize, "host service gameinfo ack"))
					break;
				Connected[RemoteClient].bHasGameInfo = true;
				break;
			case HPS_MAP_LOAD_ACK:
				if (!CheckHCDEPregameService(RemoteClient, HCDEServiceHeaderSize, "host service mapload ack"))
					break;
				Connected[RemoteClient].bHasMapLoadInfo = true;
				break;
			case HPS_START_GAME_ACK:
				if (!CheckHCDEPregameService(RemoteClient, HCDEServiceHeaderSize, "host service start ack"))
					break;
				if (!Connected[RemoteClient].bHasStartGameAck)
					I_NetLog("Client %d acknowledged HCDE service start", RemoteClient);
				Connected[RemoteClient].bHasStartGameAck = true;
				break;
			case HPS_HEARTBEAT:
				CheckHCDEPregameService(RemoteClient, HCDEServiceHeaderSize, "host service heartbeat");
				break;
			default:
				DebugTrace::Markf("net", "ignored unsupported host HCDE service %u", unsigned(NetBuffer[2]));
				break;
			}
		}
	}

	SV_UpdateMaster();
	const size_t addrSize = sizeof(sockaddr_in);
	bool ready = true;
	NetBuffer[0] = NCMD_SETUP;
	for (int client = 1; client < MaxClients; ++client)
	{
		auto& con = Connected[client];
		if (DropClientForHCDETimeout(client, connectedPlayers, "pregame setup"))
		{
			ready = false;
			continue;
		}
		if (con.bHCDEConnect)
			FlushHCDEReliableServices(con.Address, con);

		// If we're starting before the server is full, only check against connected clients.
		if (con.Status != CSTAT_READY && (!forceStarting || con.Status != CSTAT_NONE))
			ready = false;

		if (con.Status == CSTAT_CONNECTING)
		{
			BeginSetupPacket(PRE_CONNECT_ACK, con.SessionToken, 5u);
			NetBuffer[2] = client;
			NetBuffer[3] = *connectedPlayers;
			NetBuffer[4] = MaxClients;
			uint8_t ackFlags = DedicatedServerMode ? PRE_CONNECT_ACK_DEDICATED : 0u;
			if (con.bHCDEConnect)
				ackFlags |= PRE_CONNECT_ACK_HCDE_SERVICE;
			if (DedicatedServerMode)
				ackFlags |= PRE_CONNECT_ACK_SERVER_AUTHORITY;
			NetBuffer[9] = ackFlags;
			NetBufferLength = 10u;
			if ((ackFlags & PRE_CONNECT_ACK_HCDE_SERVICE) != 0)
			{
				NetBuffer[NetBufferLength++] = HCDEConnectProtocolVersion;
				NetBuffer[NetBufferLength++] = HCDE_CONNECT_SERVER_AUTHORITY;
			}
			SendPacket(con.Address);
			if (con.bHCDEConnect)
			{
				if (BeginReliableHCDEPregameService(HPS_CONSOLE_PLAYER, con, uint8_t(client)))
				{
					NetBuffer[HCDEServiceHeaderSize] = uint8_t(client);
					NetBuffer[HCDEServiceHeaderSize + 1u] = uint8_t(*connectedPlayers);
					NetBuffer[HCDEServiceHeaderSize + 2u] = uint8_t(MaxClients);
					NetBuffer[HCDEServiceHeaderSize + 3u] = HCDE_CONNECT_SERVER_AUTHORITY;
					NetBufferLength = HCDEServiceHeaderSize + 4u;
					CommitReliableHCDEPregameService(con.Address, con, HPS_CONSOLE_PLAYER, uint8_t(client));
				}
			}
		}
		else if (con.Status == CSTAT_WAITING)
		{
			bool clientReady = true;
			if (!ClientGotAck(client, client))
			{
				if (con.bHCDEConnect)
				{
					if (BeginReliableHCDEPregameService(HPS_USER_INFO_ACK, con, uint8_t(client)))
					{
						NetBuffer[HCDEServiceHeaderSize] = uint8_t(client);
						NetBufferLength = HCDEServiceHeaderSize + 1u;
						CommitReliableHCDEPregameService(con.Address, con, HPS_USER_INFO_ACK, uint8_t(client));
					}
				}
				else
				{
					BeginSetupPacket(PRE_USER_INFO_ACK, con.SessionToken);
					NetBufferLength = 6u;
					SendPacket(con.Address);
				}
				clientReady = false;
			}

			if (con.bHCDEConnect && !con.bHasMapLoadInfo)
			{
				if (BeginReliableHCDEPregameService(HPS_MAP_LOAD, con, 0u))
				{
					TArrayView<uint8_t> stream = TArrayView(&NetBuffer[NetBufferLength], MAX_MSGLEN - NetBufferLength);
					Net_SetMapLoadInfo(stream);
					NetBufferLength += stream.Data() - &NetBuffer[NetBufferLength];
					CommitReliableHCDEPregameService(con.Address, con, HPS_MAP_LOAD, 0u);
				}
				clientReady = false;
			}

			if (!con.bHasGameInfo)
			{
				if (con.bHCDEConnect)
				{
					if (BeginReliableHCDEPregameService(HPS_GAME_INFO, con, 0u))
					{
						NetBuffer[HCDEServiceHeaderSize] = TicDup;
						memcpy(&NetBuffer[HCDEServiceHeaderSize + 1u], GameID, 8);
						NetBufferLength = HCDEServiceHeaderSize + 9u;

						TArrayView<uint8_t> stream = TArrayView(&NetBuffer[NetBufferLength], MAX_MSGLEN - NetBufferLength);
						Net_SetServerInfo(stream);
						NetBufferLength += stream.Data() - &NetBuffer[NetBufferLength];
						CommitReliableHCDEPregameService(con.Address, con, HPS_GAME_INFO, 0u);
					}
				}
				else
				{
					BeginSetupPacket(PRE_GAME_INFO, con.SessionToken);
					NetBuffer[6] = TicDup;
					memcpy(&NetBuffer[7], GameID, 8);
					NetBufferLength = 15u;

					TArrayView<uint8_t> stream = TArrayView(&NetBuffer[NetBufferLength], MAX_MSGLEN - NetBufferLength);
					Net_SetGameInfo(stream);
					NetBufferLength += stream.Data() - &NetBuffer[NetBufferLength];
					SendPacket(con.Address);
				}
				clientReady = false;
			}

			for (int i = 0; i < MaxClients; ++i)
			{
				if (i == client || Connected[i].Status == CSTAT_NONE)
					continue;

				if (!ClientGotAck(client, i))
				{
					if (Connected[i].Status >= CSTAT_WAITING)
					{
						if (con.bHCDEConnect)
						{
							if (!BeginReliableHCDEPregameService(HPS_PEER_USER_INFO, con, uint8_t(i)))
							{
								clientReady = false;
								continue;
							}
							NetBuffer[HCDEServiceHeaderSize] = uint8_t(i);
							NetBufferLength = HCDEServiceHeaderSize + 1u;
						}
						else
						{
							BeginSetupPacket(PRE_USER_INFO, con.SessionToken, 3u);
							NetBuffer[2] = uint8_t(i);
							WriteBE32(&NetBuffer[3], con.SessionToken);
							NetBufferLength = 7u;
						}
						// Client will already have the host connection information.
						if (i > 0)
						{
							memcpy(&NetBuffer[NetBufferLength], &Connected[i].Address, addrSize);
							NetBufferLength += addrSize;
						}

						TArrayView<uint8_t> stream = TArrayView(&NetBuffer[NetBufferLength], MAX_MSGLEN - NetBufferLength);
						Net_SetUserInfo(i, stream);
						NetBufferLength += stream.Data() - &NetBuffer[NetBufferLength];
						if (con.bHCDEConnect)
							CommitReliableHCDEPregameService(con.Address, con, HPS_PEER_USER_INFO, uint8_t(i));
						else
							SendPacket(con.Address);
					}
					clientReady = false;
				}
			}

			if (clientReady)
			{
				con.Status = CSTAT_READY;
				I_NetClientUpdated(client);
			}
		}
		else if (con.Status == CSTAT_READY)
		{
			if (con.bHCDEConnect)
			{
				if (!HasPendingHCDEReliableService(con))
				{
					BeginHCDEPregameService(HPS_HEARTBEAT, con);
					NetBuffer[HCDEServiceHeaderSize] = *connectedPlayers;
					NetBuffer[HCDEServiceHeaderSize + 1u] = MaxClients;
					NetBufferLength = HCDEServiceHeaderSize + 2u;
					SendPacket(con.Address);
				}
			}
			else
			{
				BeginSetupPacket(PRE_HEARTBEAT, con.SessionToken);
				NetBuffer[6] = *connectedPlayers;
				NetBuffer[7] = MaxClients;
				NetBufferLength = 8u;
				SendPacket(con.Address);
			}
		}
	}

	const bool shouldStart = ready && (*connectedPlayers >= MaxClients || forceStarting);
	if (shouldStart && forceStarting)
	{
		DedicatedServerStartRequested = false;
	}
	return shouldStart;
}

static bool Host_CheckStartGameAcks(void* connected)
{
	int* connectedPlayers = (int*)connected;

	sockaddr_in from;
	while (GetConnection(from))
	{
		if (NetBuffer[0] == NCMD_EXIT)
		{
			if (RemoteClient > 0)
			{
				RemoveClientConnection(RemoteClient);
				--*connectedPlayers;
				I_NetUpdatePlayers(*connectedPlayers, MaxClients);
			}
			continue;
		}

		if (NetBuffer[0] != NCMD_SETUP || NetBuffer[1] != PRE_HCDE_SERVICE)
			continue;
		if (RemoteClient <= 0 || !Connected[RemoteClient].bHCDEConnect)
		{
			DebugTrace::Markf("net", "ignored HCDE start-ack packet from unnegotiated client %d", RemoteClient);
			continue;
		}

		const auto service = EHCDEPregameService(NetBuffer[2]);
		switch (service)
		{
		case HPS_START_GAME_ACK:
			if (!CheckHCDEPregameService(RemoteClient, HCDEServiceHeaderSize, "host service start ack"))
				break;
			if (!Connected[RemoteClient].bHasStartGameAck)
				I_NetLog("Client %d acknowledged HCDE service start", RemoteClient);
			Connected[RemoteClient].bHasStartGameAck = true;
			break;
		case HPS_HEARTBEAT:
			CheckHCDEPregameService(RemoteClient, HCDEServiceHeaderSize, "host service start heartbeat");
			break;
		default:
			DebugTrace::Markf("net", "ignored HCDE service %u while waiting for start ack", unsigned(NetBuffer[2]));
			break;
		}
	}

	bool allAcked = true;
	int acknowledged = 1;
	for (int client = 1; client < MaxClients; ++client)
	{
		auto& con = Connected[client];
		if (con.Status == CSTAT_NONE)
			continue;
		if (DropClientForHCDETimeout(client, connectedPlayers, "start acknowledgement"))
			continue;

		if (con.bHCDEConnect)
		{
			FlushHCDEReliableServices(con.Address, con);
			if (!con.bHasStartGameAck)
			{
				allAcked = false;
				continue;
			}
		}
		++acknowledged;
	}
	I_NetUpdatePlayers(acknowledged, *connectedPlayers);
	return allAcked;
}

static void SendAbort()
{
	NetBuffer[0] = NCMD_EXIT;
	NetBufferLength = 1u;

	if (consoleplayer == 0)
	{
		for (int client = 1; client < MaxClients; ++client)
		{
			if (Connected[client].Status != CSTAT_NONE)
				SendPacket(Connected[client].Address);
		}
	}
	else
	{
		SendPacket(Connected[0].Address);
	}
}

static bool HostGame(int arg)
{
	DebugTrace::Markf("net", "host request arg=%d", arg);
	DedicatedServerStartRequested = false;
	if (DedicatedServerAbortRequested)
	{
		DebugTrace::Mark("net", "host request cancelled before network start");
		throw CExitEvent(0);
	}
	int requestedClients = (arg < Args->NumArgs()) ? atoi(Args->GetArg(arg)) : 0;
	if (DedicatedServerMode)
	{
		if (requestedClients <= 0)
			requestedClients = 1;
		if ((unsigned)requestedClients >= MAXPLAYERS)
			I_FatalError("Cannot host a dedicated game with %u client slots. The limit is currently %lu", requestedClients, MAXPLAYERS - 1u);
		MaxClients = requestedClients + 1;
	}
	else if (!(MaxClients = requestedClients))
	{	// No player count specified, assume 2
		MaxClients = 2u;
	}

	if ((unsigned)MaxClients > MAXPLAYERS)
		I_FatalError("Cannot host a game with %u players. The limit is currently %lu", MaxClients, MAXPLAYERS);

	HCDE_ServerMode_SetNetworkDetails(I_GetVisibleMaxClients(), MaxClients, GamePort, DedicatedServerMode, DedicatedServerMode ? "server-init" : "host-init");

	GenerateGameID();
	NetworkClients += 0;
	Connected[consoleplayer].Status = CSTAT_READY;
	Net_SetupUserInfo();

	// If only 1 player, don't bother starting the network
	if (MaxClients == 1)
	{
		TicDup = 1u;
		multiplayer = true;
		return true;
	}

	StartNetwork(false);
	DebugTrace::Markf("net", "host network ready port=%u", static_cast<unsigned>(GamePort));
	SV_InitMasters();
	HCDE_ServerMode_SetMasterState(SV_IsMasterAdvertisingEnabled(), SV_GetMasterCount());
	HCDE_ServerMode_SetNetworkDetails(I_GetVisibleMaxClients(), MaxClients, GamePort, DedicatedServerMode, DedicatedServerMode ? "server-waiting" : "host-waiting");
	HCDE_ServerMode_PrintDiagnostics(DedicatedServerMode ? "dedicated-host" : "host");
	SetConnectFlow(NCF_SERVER_WAITING);
	I_NetInit(DedicatedServerMode ? "Starting dedicated server..." : "Hosting game...", true);
	I_NetUpdatePlayers(1, MaxClients);
	I_NetClientConnected(0u, 16u);
	I_NetMessage(DedicatedServerMode ? "Dedicated server accepting clients" : "Waiting for players");

	// Wait for the session to be full.
	int connectedPlayers = 1;
	if (!I_NetLoop(Host_CheckForConnections, (void*)&connectedPlayers))
	{
		SendAbort();
		SV_ShutdownMasters();
		DebugTrace::Mark("net", "host session aborted");
		throw CExitEvent(0);
	}

	// Now go
	SetConnectFlow(NCF_SYNCING);
	HCDE_ServerMode_SetNetworkDetails(I_GetVisibleMaxClients(), MaxClients, GamePort, DedicatedServerMode, DedicatedServerMode ? "server-syncing" : "syncing");
	I_NetMessage(DedicatedServerMode ? "Starting dedicated game" : "Starting game");

	// If the player force started with only themselves in the session, start the session
	// immediately.
	if (connectedPlayers == 1)
	{
		I_NetDone();
		CloseNetwork();
		MaxClients = 1;
		TicDup = 1u;
		return true;
	}

	I_NetLog("Go");

	for (size_t client = 1u; client < (size_t)MaxClients; ++client)
	{
		if (Connected[client].Status != CSTAT_NONE)
		{
			if (Connected[client].bHCDEConnect)
			{
				if (BeginReliableHCDEPregameService(HPS_START_GAME, Connected[client], 0u))
					CommitReliableHCDEPregameService(Connected[client].Address, Connected[client], HPS_START_GAME, 0u);
			}
			else
			{
				NetBuffer[0] = NCMD_SETUP;
				NetBuffer[1] = PRE_GO;
				WriteBE32(&NetBuffer[2], Connected[client].SessionToken);
				NetBufferLength = 6u;
				SendPacket(Connected[client].Address);
			}
		}
	}

	if (!I_NetLoop(Host_CheckStartGameAcks, (void*)&connectedPlayers))
	{
		SendAbort();
		SV_ShutdownMasters();
		DebugTrace::Mark("net", "host start acknowledgement wait aborted");
		throw CExitEvent(0);
	}

	I_NetDone();
	I_NetLog("Total players: %d", I_UsesDedicatedServerSlot() ? max(connectedPlayers - 1, 0) : connectedPlayers);

	return true;
}

uint16_t I_GetGamePort()
{
	return GamePort;
}

static FString ReadVerificationError(TArrayView<uint8_t> stream)
{
	if (stream.Size() < 5u)
		return "Unknown error";

	if (stream[0] == FVerificationError::VE_ENGINE)
	{
		if (stream.Size() < 7u)
			return "Unknown error";

		return FStringf("Engine mismatch: host expected %d.%d.%d, got %d.%d.%d",
						stream[1], stream[2], stream[3], stream[4], stream[5], stream[6]);
	}

	TMap<FString, FString> files = {};
	for (size_t i = 0u; i < fileSystem.GetNumWads(); ++i)
	{
		if (!fileSystem.IsOptionalResource(i))
		{
			const FString crc = fileSystem.GetResourceHash(i);
			FString name = fileSystem.GetResourceFileName(i);
			FixPathSeperator(name);
			auto a = name.Split('/', FString::TOK_SKIPEMPTY);
			files[crc] = a.Last();
		}
	}

	const size_t size = (stream[1] << 24) | (stream[2] << 16) | (stream[3] << 8) | stream[4];
	size_t offset = 5;
	if (stream[0] == FVerificationError::VE_FILE_UNKNOWN)
	{
		FString er = "Host found unknown files:";
		for (size_t i = 0; i < size; ++i)
		{
			FString crc = {};
			if (!ReadQueryString(stream.Data(), offset, stream.Size(), crc))
				return "Unknown error";

			auto file = files.CheckKey(crc);
			if (file != nullptr)
				er.AppendFormat("\n* %s", file->GetChars());
			else
				er.AppendFormat("\n* <? Unknown file ?>");
		}
		return er;
	}
	else if (stream[0] == FVerificationError::VE_FILE_ORDER)
	{
		FString er = "Wrong file order. Expected:";
		for (size_t i = 0; i < size; ++i)
		{
			FString crc = {};
			if (!ReadQueryString(stream.Data(), offset, stream.Size(), crc))
				return "Unknown error";

			auto file = files.CheckKey(crc);
			if (file != nullptr)
				er.AppendFormat("\n* %s", file->GetChars());
			else
				er.AppendFormat("\n* <? Unknown file ?>");
		}
		return er;
	}
	else if (stream[0] == FVerificationError::VE_FILE_MISSING)
	{
		FString er = "Host was expecting missing files:";
		for (size_t i = 0; i < size; ++i)
		{
			FString file = {};
			if (!ReadQueryString(stream.Data(), offset, stream.Size(), file))
				return "Unknown error";

			er.AppendFormat("\n* %s", file.GetChars());
		}
		return er;
	}

	return "Unknown error";
}

static bool Guest_ContactHost(void* unused)
{
	// Listen for a reply.
	const size_t addrSize = sizeof(sockaddr_in);
	sockaddr_in from;
	while (GetConnection(from))
	{
		const size_t msgSize = NetBufferLength;

		if (RemoteClient != 0)
			continue;

		if (NetBuffer[0] == NCMD_EXIT)
			I_NetError("The host cancelled the game");

		if (NetBuffer[0] != NCMD_SETUP)
			continue;

		if (NetBuffer[1] == PRE_HEARTBEAT)
		{
			if (NetBufferLength < 8u || !CheckSessionToken(Connected[0], ReadSessionToken(NetBuffer, 2u), "host heartbeat"))
				continue;
			MaxClients = NetBuffer[7];
			I_NetUpdatePlayers(NetBuffer[6], MaxClients);
		}
		else if (NetBuffer[1] == PRE_DISCONNECT)
		{
			if (NetBufferLength < 7u || !CheckSessionToken(Connected[0], ReadSessionToken(NetBuffer, 3u), "host disconnect"))
				continue;

			I_ClearClient(NetBuffer[2]);
			NetworkClients -= NetBuffer[2];
			SetClientAck(consoleplayer, NetBuffer[2], false);
			I_NetClientDisconnected(NetBuffer[2]);
		}
		else if (NetBuffer[1] == PRE_FULL)
		{
			I_NetError("The game is full");
		}
		else if (NetBuffer[1] == PRE_IN_PROGRESS)
		{
			I_NetError("The game has already started");
		}
		else if (NetBuffer[1] == PRE_WRONG_PASSWORD)
		{
			I_NetError("Invalid password");
		}
		else if (NetBuffer[1] == PRE_VERIFICATION_ERROR)
		{
			if (NetBufferLength < 3u)
			{
				I_NetError("Malformed verification error response");
				continue;
			}

			I_NetError(ReadVerificationError(TArrayView{ &NetBuffer[2], static_cast<unsigned>(NetBufferLength - 2u) }).GetChars());
		}
		else if (NetBuffer[1] == PRE_KICKED)
		{
			I_NetError("You have been kicked from the game");
		}
		else if (NetBuffer[1] == PRE_BANNED)
		{
			I_NetError("You have been banned from the game");
		}
		else if (NetBuffer[1] == PRE_PROTOCOL_ERROR)
		{
			I_NetError("The server rejected HCDE service protocol negotiation");
		}
		else if (NetBuffer[1] == PRE_SETUP_TIMEOUT)
		{
			I_NetError("The server timed out during HCDE setup");
		}
		else if (NetBuffer[1] == PRE_CONNECT_ACK)
		{
			if (consoleplayer == -1)
			{
				if (msgSize < 9)
					continue;

				Connected[0].SessionToken = ReadSessionToken(NetBuffer, 5u);
				const uint8_t ackFlags = msgSize >= 10 ? NetBuffer[9] : 0u;
				if ((ackFlags & PRE_CONNECT_ACK_DEDICATED) != 0)
				{
					DedicatedJoinMode = true;
				}
				const bool serviceConnect = (ackFlags & PRE_CONNECT_ACK_HCDE_SERVICE) != 0;
				if (serviceConnect)
				{
					if (msgSize < 12)
						I_NetError("Malformed HCDE service connect acknowledgement");
					if (NetBuffer[10] != HCDEConnectProtocolVersion)
						I_NetError("Unsupported HCDE service connect version from server");

					Connected[0].bHCDEConnect = true;
					Connected[0].HCDEConnectVersion = NetBuffer[10];
					Connected[0].HCDEConnectFlags = NetBuffer[11];
					Printf("NetSession:: HCDE service connect negotiated v%u flags=0x%02x\n", NetBuffer[10], NetBuffer[11]);
				}

				MaxClients = NetBuffer[4];
				if (Connected[0].Status != CSTAT_WAITING)
				{
					NetworkClients += 0;
					Connected[0].Status = CSTAT_WAITING;
					I_NetClientUpdated(0);
				}
				I_NetUpdatePlayers(NetBuffer[3], MaxClients);
				if (serviceConnect)
				{
					I_NetMessage("Waiting for server assignment");
				}
				else
				{
					consoleplayer = NetBuffer[2];
					NetworkClients += consoleplayer;
					Connected[consoleplayer].Status = CSTAT_CONNECTING;
					Connected[consoleplayer].SessionToken = Connected[0].SessionToken;
					Net_SetupUserInfo();

					I_NetMessage("Sending player information");
					I_NetClientConnected(consoleplayer, 16u);
				}
			}
		}
		else if (NetBuffer[1] == PRE_HCDE_SERVICE)
		{
			if (!Connected[0].bHCDEConnect)
			{
				DebugTrace::Markf("net", "ignored HCDE service packet before negotiation");
				continue;
			}
			if (NetBufferLength < HCDEServiceHeaderSize)
			{
				DebugTrace::Markf("net", "guest HCDE service packet too short len=%zu", NetBufferLength);
				continue;
			}

			const auto service = EHCDEPregameService(NetBuffer[2]);
			switch (service)
			{
			case HPS_CONSOLE_PLAYER:
			{
				if (!CheckHCDEPregameService(0u, HCDEServiceHeaderSize + 4u, "guest service console player"))
					break;

				const int assigned = NetBuffer[HCDEServiceHeaderSize];
				const int connectedPlayers = NetBuffer[HCDEServiceHeaderSize + 1u];
				const int announcedMaxClients = NetBuffer[HCDEServiceHeaderSize + 2u];
				const int firstPlayable = I_GetFirstPlayableClientSlot();
				if (assigned < firstPlayable || assigned >= announcedMaxClients || announcedMaxClients > int(MAXPLAYERS))
				{
					DebugTrace::Markf("net", "ignored invalid HCDE console player assignment slot=%d max=%d", assigned, announcedMaxClients);
					break;
				}

				MaxClients = announcedMaxClients;
				if (consoleplayer == -1)
				{
					consoleplayer = assigned;
					NetworkClients += consoleplayer;
					Connected[consoleplayer].Status = CSTAT_CONNECTING;
					Connected[consoleplayer].SessionToken = Connected[0].SessionToken;
					Connected[consoleplayer].bHCDEConnect = true;
					Connected[consoleplayer].HCDEConnectVersion = Connected[0].HCDEConnectVersion;
					Connected[consoleplayer].HCDEConnectFlags = Connected[0].HCDEConnectFlags | NetBuffer[HCDEServiceHeaderSize + 3u];
					Net_SetupUserInfo();

					I_NetMessage("Sending player information");
					I_NetLog("Received HCDE console player %d", consoleplayer);
					I_NetClientConnected(consoleplayer, 16u);
				}
				I_NetUpdatePlayers(connectedPlayers, MaxClients);
				break;
			}
			case HPS_HEARTBEAT:
				if (consoleplayer < 0)
					break;
				if (!CheckHCDEPregameService(0u, HCDEServiceHeaderSize + 2u, "guest service heartbeat"))
					break;
				MaxClients = NetBuffer[HCDEServiceHeaderSize + 1u];
				I_NetUpdatePlayers(NetBuffer[HCDEServiceHeaderSize], MaxClients);
				break;
			case HPS_USER_INFO_ACK:
				if (consoleplayer < 0)
					break;
				if (!CheckHCDEPregameService(0u, HCDEServiceHeaderSize + 1u, "guest service userinfo ack"))
					break;

				if (NetBuffer[HCDEServiceHeaderSize] == consoleplayer)
					SetClientAck(consoleplayer, consoleplayer, true);
				if (Connected[consoleplayer].Status == CSTAT_CONNECTING)
				{
					Connected[consoleplayer].Status = CSTAT_WAITING;
					I_NetClientUpdated(consoleplayer);
					I_NetMessage("Waiting for server start");
				}

				if (BeginReliableHCDEPregameService(HPS_USER_INFO_ACK, Connected[0], uint8_t(consoleplayer)))
				{
					NetBuffer[HCDEServiceHeaderSize] = uint8_t(consoleplayer);
					NetBufferLength = HCDEServiceHeaderSize + 1u;
					CommitReliableHCDEPregameService(from, Connected[0], HPS_USER_INFO_ACK, uint8_t(consoleplayer));
				}
				break;
			case HPS_MAP_LOAD:
				if (consoleplayer < 0)
					break;
				if (!CheckHCDEPregameService(0u, HCDEServiceHeaderSize, "guest service mapload"))
					break;
				if (!Connected[consoleplayer].bHasMapLoadInfo)
				{
					TArrayView<uint8_t> stream = TArrayView(&NetBuffer[HCDEServiceHeaderSize], MAX_MSGLEN - HCDEServiceHeaderSize);
					Net_ReadMapLoadInfo(stream);
					Connected[consoleplayer].bHasMapLoadInfo = true;
					I_NetLog("Received HCDE map load");
				}

				if (BeginReliableHCDEPregameService(HPS_MAP_LOAD_ACK, Connected[0], 0u))
					CommitReliableHCDEPregameService(from, Connected[0], HPS_MAP_LOAD_ACK, 0u);
				break;
			case HPS_GAME_INFO:
				if (consoleplayer < 0)
					break;
				if (!CheckHCDEPregameService(0u, HCDEServiceHeaderSize + 9u, "guest service gameinfo"))
					break;
				if (!Connected[consoleplayer].bHasGameInfo)
				{
					TicDup = clamp<int>(NetBuffer[HCDEServiceHeaderSize], 1, MAXTICDUP);
					memcpy(GameID, &NetBuffer[HCDEServiceHeaderSize + 1u], 8);
					TArrayView<uint8_t> stream = TArrayView(&NetBuffer[HCDEServiceHeaderSize + 9u], MAX_MSGLEN - (HCDEServiceHeaderSize + 9u));
					Net_ReadServerInfo(stream);
					Connected[consoleplayer].bHasGameInfo = true;
					I_NetLog("Received HCDE server info");
				}

				if (BeginReliableHCDEPregameService(HPS_GAME_INFO_ACK, Connected[0], 0u))
					CommitReliableHCDEPregameService(from, Connected[0], HPS_GAME_INFO_ACK, 0u);
				break;
			case HPS_PEER_USER_INFO:
			{
				if (consoleplayer < 0)
					break;
				if (!CheckHCDEPregameService(0u, HCDEServiceHeaderSize + 1u, "guest service peer userinfo"))
					break;

				const int c = NetBuffer[HCDEServiceHeaderSize];
				if (c < 0 || c >= MaxClients)
				{
					DebugTrace::Markf("net", "ignored HCDE peer userinfo for invalid client %d", c);
					break;
				}

				if (!ClientGotAck(consoleplayer, c))
				{
					NetworkClients += c;
					size_t byte = HCDEServiceHeaderSize + 1u;
					if (c > 0)
					{
						if (NetBufferLength < byte + addrSize)
						{
							DebugTrace::Markf("net", "HCDE peer userinfo missing address for client %d", c);
							break;
						}
						Connected[c].Status = CSTAT_WAITING;
						memcpy(&Connected[c].Address, &NetBuffer[byte], addrSize);
						byte += addrSize;
					}
					else
					{
						Connected[c].Status = CSTAT_READY;
					}
					TArrayView<uint8_t> stream = TArrayView(&NetBuffer[byte], MAX_MSGLEN - byte);
					Net_ReadUserInfo(c, stream);
					SetClientAck(consoleplayer, c, true);

					I_NetClientConnected(c, 16u);
				}

				if (BeginReliableHCDEPregameService(HPS_USER_INFO_ACK, Connected[0], uint8_t(c)))
				{
					NetBuffer[HCDEServiceHeaderSize] = uint8_t(c);
					NetBufferLength = HCDEServiceHeaderSize + 1u;
					CommitReliableHCDEPregameService(from, Connected[0], HPS_USER_INFO_ACK, uint8_t(c));
				}
				break;
			}
			case HPS_START_GAME:
				if (consoleplayer < 0)
					break;
				if (!CheckHCDEPregameService(0u, HCDEServiceHeaderSize, "guest service start"))
					break;

				if (BeginReliableHCDEPregameService(HPS_START_GAME_ACK, Connected[0], 0u))
				{
					CommitReliableHCDEPregameService(from, Connected[0], HPS_START_GAME_ACK, 0u);
					FlushHCDEReliableServices(from, Connected[0], true);
				}
				I_NetMessage("Starting game");
				I_NetLog("Received HCDE service start");
				return true;
			default:
				DebugTrace::Markf("net", "ignored unsupported guest HCDE service %u", unsigned(NetBuffer[2]));
				break;
			}
		}
		else if (NetBuffer[1] == PRE_USER_INFO_ACK)
		{
			if (!CheckSetupPacket(consoleplayer, 6u, 2u, "guest userinfo ack"))
				continue;

			// The host will only ever send us this to confirm they've gotten our data.
			if (msgSize >= 7 && NetBuffer[6] == consoleplayer)
				SetClientAck(consoleplayer, consoleplayer, true);
			if (Connected[consoleplayer].Status == CSTAT_CONNECTING)
			{
				Connected[consoleplayer].Status = CSTAT_WAITING;
				I_NetClientUpdated(consoleplayer);
				I_NetMessage("Waiting for server start");
			}

			BeginSetupPacket(PRE_USER_INFO_ACK, Connected[0].SessionToken);
			NetBuffer[6] = consoleplayer;
			NetBufferLength = 7u;
			SendPacket(from);
		}
		else if (NetBuffer[1] == PRE_GAME_INFO)
		{
			if (!Connected[consoleplayer].bHasGameInfo)
			{
				if (!CheckSetupPacket(consoleplayer, 15u, 2u, "guest gameinfo"))
					continue;

				TicDup = clamp<int>(NetBuffer[6], 1, MAXTICDUP);
				memcpy(GameID, &NetBuffer[7], 8);
				TArrayView<uint8_t> stream = TArrayView(&NetBuffer[15], MAX_MSGLEN - 15);
				Net_ReadGameInfo(stream);
				Connected[consoleplayer].bHasGameInfo = true;
			}

			BeginSetupPacket(PRE_GAME_INFO_ACK, Connected[0].SessionToken);
			NetBufferLength = 6u;
			SendPacket(from);
		}
		else if (NetBuffer[1] == PRE_USER_INFO)
		{
			if (msgSize < 7)
				continue;

			if (!CheckSessionToken(Connected[0], ReadSessionToken(NetBuffer, 3u), "guest userinfo"))
				continue;

			const int c = NetBuffer[2];
			if (!ClientGotAck(consoleplayer, c))
			{
				NetworkClients += c;
				size_t byte = 7u;
				if (c > 0)
				{
					Connected[c].Status = CSTAT_WAITING;
					memcpy(&Connected[c].Address, &NetBuffer[byte], addrSize);
					byte += addrSize;
				}
				else
				{
					Connected[c].Status = CSTAT_READY;
				}
				TArrayView<uint8_t> stream = TArrayView(&NetBuffer[byte], MAX_MSGLEN - byte);
				Net_ReadUserInfo(c, stream);
				SetClientAck(consoleplayer, c, true);

				I_NetClientConnected(c, 16u);
			}

			BeginSetupPacket(PRE_USER_INFO_ACK, Connected[0].SessionToken);
			NetBuffer[6] = c;
			NetBufferLength = 7u;
			SendPacket(from);
		}
		else if (NetBuffer[1] == PRE_GO)
		{
			if (!CheckSetupPacket(consoleplayer, 6u, 2u, "guest go"))
				continue;

			I_NetMessage("Starting game");
			I_NetLog("Received GO");
			return true;
		}
	}

	if (Connected[0].bHCDEConnect)
		FlushHCDEReliableServices(Connected[0].Address, Connected[0]);

	NetBuffer[0] = NCMD_SETUP;
	if (consoleplayer == -1)
	{
		if (Connected[0].bHCDEConnect && Connected[0].SessionToken != 0u)
		{
			if (!HasPendingHCDEReliableService(Connected[0]))
			{
				BeginHCDEPregameService(HPS_HEARTBEAT, Connected[0]);
				SendPacket(Connected[0].Address);
			}
		}
		else
		{
			NetBuffer[1] = PRE_CONNECT;
			uint8_t* engineInfo = &NetBuffer[2];
			const size_t end = 2u + Net_SetEngineInfo(engineInfo);
			const size_t passSize = strlen(net_password) + 1;
			memcpy(&NetBuffer[end], net_password, passSize);
			NetBufferLength = end + passSize;
			if (DedicatedJoinMode || SilentNetStartMode)
				AppendHCDEConnectInfo(BuildLocalHCDEConnectFlags());
			SendPacket(Connected[0].Address);
		}
	}
	else
	{
		auto& con = Connected[consoleplayer];
		if (con.Status == CSTAT_CONNECTING)
		{
			if (Connected[0].bHCDEConnect)
			{
				if (BeginReliableHCDEPregameService(HPS_CLIENT_USER_INFO, Connected[0], uint8_t(consoleplayer)))
				{
					TArrayView<uint8_t> stream = TArrayView(&NetBuffer[NetBufferLength], MAX_MSGLEN - NetBufferLength);
					Net_SetUserInfo(consoleplayer, stream);
					NetBufferLength += stream.Data() - &NetBuffer[NetBufferLength];
					CommitReliableHCDEPregameService(Connected[0].Address, Connected[0], HPS_CLIENT_USER_INFO, uint8_t(consoleplayer));
				}
			}
			else
			{
				BeginSetupPacket(PRE_USER_INFO, Connected[0].SessionToken);
				NetBufferLength = 6u;

				TArrayView<uint8_t> stream = TArrayView(&NetBuffer[NetBufferLength], MAX_MSGLEN - NetBufferLength);
				Net_SetUserInfo(consoleplayer, stream);
				NetBufferLength += stream.Data() - &NetBuffer[NetBufferLength];
				SendPacket(Connected[0].Address);
			}
		}
		else if (con.Status == CSTAT_WAITING)
		{
			if (Connected[0].bHCDEConnect)
			{
				if (!HasPendingHCDEReliableService(Connected[0]))
				{
					BeginHCDEPregameService(HPS_HEARTBEAT, Connected[0]);
					SendPacket(Connected[0].Address);
				}
			}
			else
			{
				BeginSetupPacket(PRE_HEARTBEAT, Connected[0].SessionToken);
				NetBufferLength = 6u;
				SendPacket(Connected[0].Address);
			}
		}
	}

	return false;
}

static bool JoinGame(int arg)
{
	DebugTrace::Markf("net", "join request arg=%d", arg);
	if (arg >= Args->NumArgs()
		|| Args->GetArg(arg)[0] == '-' || Args->GetArg(arg)[0] == '+')
	{
		I_FatalError("You need to specify the host machine's address");
	}

	consoleplayer = -1;
	StartNetwork(true);
	DebugTrace::Markf("net", "join network ready port=%u", static_cast<unsigned>(GamePort));

	// Host is always client 0.
	BuildAddress(Connected[0].Address, Args->GetArg(arg));
	Connected[0].Status = CSTAT_CONNECTING;

	SetConnectFlow(NCF_CLIENT_AUTH);
	I_NetInit("Joining server...", false);
	I_NetUpdatePlayers(0u, MaxClients);
	I_NetClientUpdated(0);

	if (!I_NetLoop(Guest_ContactHost, nullptr))
	{
		SendAbort();
		throw CExitEvent(0);
	}

	for (int i = 1u; i < MaxClients; ++i)
	{
		if (Connected[i].Status != CSTAT_NONE)
			Connected[i].Status = CSTAT_READY;
	}

	SetConnectFlow(NCF_SYNCING);
	HCDE_ServerMode_SetNetworkDetails(I_GetVisibleMaxClients(), MaxClients, GamePort, DedicatedJoinMode, DedicatedJoinMode ? "client-syncing" : "syncing");
	HCDE_ServerMode_PrintDiagnostics(DedicatedJoinMode ? "dedicated-join" : "join");
	I_NetLog("Total players: %d", I_GetVisibleMaxClients());
	I_NetDone();

	return true;
}

//
// I_InitNetwork
//
bool I_InitNetwork()
{
	HCDE_ServerMode_InitFromArgs();
	DedicatedServerMode = HCDE_ServerMode_IsDedicatedServer();
	DedicatedJoinMode = HCDE_ServerMode_IsDedicatedJoin();
	SilentNetStartMode = HCDE_ServerMode_ShouldSuppressRoomUI();

	// set up for network
	const char* v = Args->CheckValue(FArg_dup);
	if (v != nullptr)
		TicDup = clamp<int>(atoi(v), 1, MAXTICDUP);

	v = Args->CheckValue(FArg_port);
	if (v != nullptr)
	{
		GamePort = atoi(v);
		Printf("Using alternate port %d\n", GamePort);
	}

	net_password = Args->CheckValue(FArg_password);

	// parse network game options,
	//		player 1: -host <numplayers>
	//		player 1: -server <numplayers> (dedicated server mode)
	//		player x: -join <player 1's address>
	int arg = -1;
	if (DedicatedServerMode && (arg = Args->CheckParm(FArg_server)))
	{
		if (!HostGame(arg + 1))
			return false;
	}
	else if ((arg = Args->CheckParm(FArg_host)))
	{
		if (!HostGame(arg + 1))
			return false;
	}
	else if ((arg = Args->CheckParm(FArg_join)))
	{
		if (!JoinGame(arg + 1))
			return false;
	}
	else
	{
		// single player game
		GenerateGameID();
		TicDup = 1;
		NetworkClients += 0;
		Connected[0].Status = CSTAT_READY;
		Net_SetupUserInfo();
	}

	bGameStarted = true;
	return true;
}

bool I_IsDedicatedServerMode()
{
	return DedicatedServerMode;
}

void I_DedicatedServerRequestStart()
{
	if (!DedicatedServerMode)
	{
		Printf("NetServer:: Start request ignored because this is not a dedicated server.\n");
		return;
	}
	DedicatedServerStartRequested = true;
}

void I_DedicatedServerRequestAbort()
{
	if (!DedicatedServerMode)
	{
		DedicatedServerAbortRequested = true;
		Printf("NetServer:: Stop request queued while dedicated server mode initializes.\n");
		return;
	}
	DedicatedServerAbortRequested = true;
}

bool I_UsesDedicatedServerSlot()
{
	return DedicatedServerMode || DedicatedJoinMode;
}

bool I_IsServerReservedSlot(int client)
{
	return I_UsesDedicatedServerSlot() && client == I_GetHCDEServiceAuthoritySlot();
}

int I_GetFirstPlayableClientSlot()
{
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	return I_UsesDedicatedServerSlot() ? authoritySlot + 1 : 0;
}

int I_GetVisibleMaxClients()
{
	return I_UsesDedicatedServerSlot() ? max(MaxClients - 1, 0) : MaxClients;
}

int I_ToVisibleClientSlot(int client)
{
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	if (I_UsesDedicatedServerSlot() && client > authoritySlot)
		return client - 1;
	return client;
}

int I_ToInternalClientSlot(int visibleClient)
{
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	if (I_UsesDedicatedServerSlot() && visibleClient >= authoritySlot)
		return visibleClient + 1;
	return visibleClient;
}

bool I_ClientUsesHCDEService(int client)
{
	return client >= 0 && client < MaxClients && Connected[client].bHCDEConnect;
}

int I_GetHCDEServiceAuthoritySlot()
{
	return I_UsesDedicatedServerSlot() && HCDE_ServerMode_HasAuthorityState()
		? HCDE_ServerMode_GetAuthoritySlot()
		: Net_Arbitrator;
}

bool I_IsLocalHCDEServiceAuthority()
{
	if (DedicatedServerMode && HCDE_ServerMode_HasAuthorityState() && !HCDE_ServerMode_IsAuthorityPlayerBacked())
		return true;

	return consoleplayer == I_GetHCDEServiceAuthoritySlot();
}

bool I_IsHCDEServiceAuthoritySlot(int client)
{
	return client >= 0 && client < MaxClients && client == I_GetHCDEServiceAuthoritySlot();
}

bool I_IsRemoteHCDEServiceAuthority(int client)
{
	return !I_IsLocalHCDEServiceAuthority() && I_IsHCDEServiceAuthoritySlot(client);
}

int I_GetHCDELiveAuthoritySlot()
{
	return I_GetHCDEServiceAuthoritySlot();
}

bool I_IsLocalHCDELiveAuthority()
{
	return I_IsLocalHCDEServiceAuthority();
}

bool I_IsHCDELiveAuthoritySlot(int client)
{
	return I_IsHCDEServiceAuthoritySlot(client);
}

static bool I_IsHCDELiveRoutablePeer(int client)
{
	return client >= 0
		&& client < MaxClients
		&& client != consoleplayer
		&& I_ClientUsesHCDEService(client);
}

bool I_ShouldSendHCDELiveControlTo(int client)
{
	if (!I_IsHCDELiveRoutablePeer(client))
		return false;

	return I_IsLocalHCDELiveAuthority()
		? !I_IsHCDELiveAuthoritySlot(client)
		: I_IsHCDELiveAuthoritySlot(client);
}

bool I_ShouldSendHCDELiveClientInputTo(int client)
{
	return I_IsHCDELiveRoutablePeer(client)
		&& !I_IsLocalHCDELiveAuthority()
		&& I_IsHCDELiveAuthoritySlot(client);
}

bool I_ShouldSendHCDELiveServerSnapshotTo(int client)
{
	return I_IsHCDELiveRoutablePeer(client)
		&& I_IsLocalHCDELiveAuthority()
		&& !I_IsHCDELiveAuthoritySlot(client);
}

bool I_ShouldAcceptHCDELiveClientInputFrom(int client)
{
	return I_IsHCDELiveRoutablePeer(client)
		&& I_IsLocalHCDELiveAuthority()
		&& !I_IsHCDELiveAuthoritySlot(client);
}

bool I_ShouldAcceptHCDELiveServerSnapshotFrom(int client)
{
	return I_IsHCDELiveRoutablePeer(client)
		&& !I_IsLocalHCDELiveAuthority()
		&& I_IsHCDELiveAuthoritySlot(client);
}

#ifdef _WIN32
const char* neterror()
{
	static char neterr[16];
	int			code;

	switch (code = WSAGetLastError()) {
		case WSAEACCES:				return "EACCES";
		case WSAEADDRINUSE:			return "EADDRINUSE";
		case WSAEADDRNOTAVAIL:		return "EADDRNOTAVAIL";
		case WSAEAFNOSUPPORT:		return "EAFNOSUPPORT";
		case WSAEALREADY:			return "EALREADY";
		case WSAECONNABORTED:		return "ECONNABORTED";
		case WSAECONNREFUSED:		return "ECONNREFUSED";
		case WSAECONNRESET:			return "ECONNRESET";
		case WSAEDESTADDRREQ:		return "EDESTADDRREQ";
		case WSAEFAULT:				return "EFAULT";
		case WSAEHOSTDOWN:			return "EHOSTDOWN";
		case WSAEHOSTUNREACH:		return "EHOSTUNREACH";
		case WSAEINPROGRESS:		return "EINPROGRESS";
		case WSAEINTR:				return "EINTR";
		case WSAEINVAL:				return "EINVAL";
		case WSAEISCONN:			return "EISCONN";
		case WSAEMFILE:				return "EMFILE";
		case WSAEMSGSIZE:			return "EMSGSIZE";
		case WSAENETDOWN:			return "ENETDOWN";
		case WSAENETRESET:			return "ENETRESET";
		case WSAENETUNREACH:		return "ENETUNREACH";
		case WSAENOBUFS:			return "ENOBUFS";
		case WSAENOPROTOOPT:		return "ENOPROTOOPT";
		case WSAENOTCONN:			return "ENOTCONN";
		case WSAENOTSOCK:			return "ENOTSOCK";
		case WSAEOPNOTSUPP:			return "EOPNOTSUPP";
		case WSAEPFNOSUPPORT:		return "EPFNOSUPPORT";
		case WSAEPROCLIM:			return "EPROCLIM";
		case WSAEPROTONOSUPPORT:	return "EPROTONOSUPPORT";
		case WSAEPROTOTYPE:			return "EPROTOTYPE";
		case WSAESHUTDOWN:			return "ESHUTDOWN";
		case WSAESOCKTNOSUPPORT:	return "ESOCKTNOSUPPORT";
		case WSAETIMEDOUT:			return "ETIMEDOUT";
		case WSAEWOULDBLOCK:		return "EWOULDBLOCK";
		case WSAHOST_NOT_FOUND:		return "HOST_NOT_FOUND";
		case WSANOTINITIALISED:		return "NOTINITIALISED";
		case WSANO_DATA:			return "NO_DATA";
		case WSANO_RECOVERY:		return "NO_RECOVERY";
		case WSASYSNOTREADY:		return "SYSNOTREADY";
		case WSATRY_AGAIN:			return "TRY_AGAIN";
		case WSAVERNOTSUPPORTED:	return "VERNOTSUPPORTED";
		case WSAEDISCON:			return "EDISCON";

		default:
			mysnprintf(neterr, countof(neterr), "%d", code);
			return neterr;
	}
}
#endif
