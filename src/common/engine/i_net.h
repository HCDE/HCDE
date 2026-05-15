/*
** i_net.h
**
**
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

#ifndef __I_NET_H__
#define __I_NET_H__

#include <stdint.h>
#include "m_argv.h"
#include "tarray.h"

inline constexpr size_t MAXPLAYERS = 64u;

EXTERN_FARG(host);
EXTERN_FARG(join);
EXTERN_FARG(server);
EXTERN_FARG(netwaitsilent);
EXTERN_FARG(dedicatedjoin);

enum ENetConstants
{
	BACKUPTICS = 35 * 5,	// Remember up to 5 seconds of data.
	MAXTICDUP = 3,
	MAXSENDTICS = 35 * 1,	// Only send up to 1 second of data at a time.
	STABILITYTICS = 17,
	LOCALCMDTICS = (BACKUPTICS * MAXTICDUP),
	MAX_MSGLEN = 14000,
};

enum ENetCommand
{
	CMD_NONE,
	CMD_SEND,
	CMD_GET,
};

enum ENetFlags
{
	NCMD_EXIT = 0x80,		// Client has left the game
	NCMD_RETRANSMIT = 0x40,		//
	NCMD_SETUP = 0x20,		// Guest is letting the host know who it is
	NCMD_LEVELREADY = 0x10,		// After loading a level, guests send this over to the host who then sends it back after all are received
	NCMD_QUITTERS = 0x08,		// Client is getting info about one or more players quitting
	NCMD_COMPRESSED = 0x04,		// Remainder of packet is compressed
	NCMD_LATENCYACK = 0x02,		// A latency packet was just read, so let the sender know.
	NCMD_LATENCY = 0x01,		// Latency packet, used for measuring RTT.
};

struct FVerificationError
{
	enum EVerifyError : uint8_t
	{
		VE_NONE,
		VE_ENGINE,
		VE_FILE_UNKNOWN,
		VE_FILE_MISSING,
		VE_FILE_ORDER,
	};

	EVerifyError Error = VE_NONE;
	uint8_t Major = 0u, Minor = 0u, Revision = 0u;
	uint8_t NetMajor = 0u, NetMinor = 0u, NetRevision = 0u;
	TArray<FString> UnknownFiles = {};
	TArray<FString> ExpectedOrder = {};
	// Since the guest didn't load these, we have no checksum to fetch the proper name, so send over our own.
	TArray<FString> MissingFiles = {};
};

struct FServerQueryPlayer
{
	FString Name = {};
	uint16_t Ping = 0u;
	int16_t Frags = 0;
	int16_t Kills = 0;
	int16_t Deaths = 0;
};

struct FServerQuerySnapshot
{
	FString HostName = {};
	FString MapName = {};
	FString GameName = {};
	FString SessionState = {};
	FString Version = {};
	FString GitHash = {};
	uint8_t PlayerCount = 0u;
	uint8_t MaxPlayers = 0u;
	uint8_t Skill = 0u;
	bool Deathmatch = false;
	bool Teamplay = false;
	uint16_t TimeLeft = 0u;
	uint16_t FragLimit = 0u;
	TArray<FServerQueryPlayer> Players = {};
};

struct FClientStack : public TArray<int>
{
	inline bool InGame(int i) const { return Find(i) < Size(); }

	void operator+=(const int i)
	{
		if (!InGame(i))
			SortedInsert(i);
	}

	void operator-=(const int i)
	{
		Delete(Find(i));
	}
};

extern bool netgame, multiplayer;
extern int consoleplayer;
extern int Net_Arbitrator;
extern FClientStack NetworkClients;
extern uint8_t NetBuffer[MAX_MSGLEN];
extern size_t NetBufferLength;
extern uint8_t TicDup;
extern int RemoteClient;
extern int MaxClients;

constexpr int32_t PROTO_CHALLENGE = -5560020;   // Signals challenger wants protobufs.
constexpr int32_t MSG_CHALLENGE = 5560020;      // Signals challenger wants MSG protocol.
constexpr int32_t LAUNCHER_CHALLENGE = 777123;   // Launcher/server-info challenge.
constexpr uint16_t ODAMEX_QUERY_TAG_ID = 0x0AD0;

bool I_InitNetwork();
bool I_IsDedicatedServerMode();
bool I_UsesDedicatedServerSlot();
void I_DedicatedServerRequestStart();
void I_DedicatedServerRequestAbort();
// Dedicated sessions still reserve an internal arbitrator slot; these helpers
// keep that slot out of public player counts and UI-facing client numbering.
bool I_IsServerReservedSlot(int client);
int I_GetFirstPlayableClientSlot();
int I_GetVisibleMaxClients();
int I_ToVisibleClientSlot(int client);
int I_ToInternalClientSlot(int visibleClient);
bool I_ClientUsesHCDEService(int client);
int I_GetHCDEServiceAuthoritySlot();
bool I_IsLocalHCDEServiceAuthority();
bool I_IsHCDEServiceAuthoritySlot(int client);
bool I_IsRemoteHCDEServiceAuthority(int client);
int I_GetHCDELiveAuthoritySlot();
bool I_IsLocalHCDELiveAuthority();
bool I_IsHCDELiveAuthoritySlot(int client);
bool I_ShouldSendHCDELiveControlTo(int client);
bool I_ShouldSendHCDELiveClientInputTo(int client);
bool I_ShouldSendHCDELiveServerSnapshotTo(int client);
bool I_ShouldAcceptHCDELiveClientInputFrom(int client);
bool I_ShouldAcceptHCDELiveServerSnapshotFrom(int client);
void I_GetLocalServerSnapshot(FServerQuerySnapshot& snapshot);
bool I_QueryServerInfo(const char* addrName, FServerQuerySnapshot& snapshot, FString* error = nullptr);
void I_ClearClient(size_t client);
void I_NetCmd(ENetCommand cmd);
void I_NetDone();
void HandleIncomingConnection();
void CloseNetwork();
uint16_t I_GetGamePort();

#endif
