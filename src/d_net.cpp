/*
** d_net.cpp
**
** DOOM Network game communication and protocol, all OS independent parts.
**
**---------------------------------------------------------------------------
**
** Copyright 1993-1996 id Software
** Copyright 1999-2016 Marisa Heit
** Copyright 2002-2016 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <chrono>
#include <climits>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "a_keys.h"
#include "a_sharedglobal.h"
#include "actorinlines.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "d_eventbase.h"
#include "d_main.h"
#include "d_net.h"
#include "d_net_diag.h"
#include "d_net_diagnostics.h"
#include "d_net_blackbox.h"
#include "d_net_checksum.h"
#include "d_net_movement_diag.h"
#include "d_net_rewind.h"
#include "playsim/playerstate_trace.h"
#include "d_netinf.h"
#include "events.h"
#include "g_game.h"
#include "g_levellocals.h"
#include "gameconfigfile.h"
#include "gi.h"
#include "gstrings.h"
#include "hcde_servermode.h"
#include "debugtrace.h"
#include "i_interface.h"
#include "i_net.h"
#include "i_specialpaths.h"
#include "i_system.h"
#include "i_time.h"
#include "m_argv.h"
#include "m_cheat.h"
#include "menu.h"
#include "p_conversation.h"
#include "p_enemy.h"
#include "p_lnspec.h"
#include "p_local.h"
#include "p_pspr.h"
#include "p_spec.h"
#include "p_trace.h"
#include "r_utility.h"
#include "s_music.h"
#include "savegamemanager.h"
#include "sbar.h"
#include "screenjob.h"
#include "stats.h"
#include "statnums.h"
#include "sv_master.h"
#include "version.h"
#include "vm.h"

void P_RunClientSideLogic();

EXTERN_CVAR (Int, disableautosave)
EXTERN_CVAR (Int, autosavecount)
EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, vid_vsync)
EXTERN_CVAR (Int, vid_maxfps)
EXTERN_CVAR (Int, sv_gametype)

EXTERN_FARG(loadgame);

FARG(extratic, "Multiplayer", "Sends backup commands over the network", "",
	"Causes " GAMENAME " to send a backup copy of every movement command across the network.");

extern uint8_t		*demo_p;		// [RH] Special "ticcmds" get recorded in demos
extern FString	savedescription;
extern FString	savegamefile;

extern bool AppActive;

void P_ClearLevelInterpolation();

enum ELevelStartStatus
{
	LST_READY,
	LST_HOST,
	LST_WAITING,
};

enum EReadyType
{
	RT_VOTE,
	RT_ANYONE,
	RT_HOST_ONLY,
};

enum ELagType
{
	LAG_NONE,
	LAG_PREDICTING,
	LAG_WAITING,
	LAG_SKIPPING,
};

static const char* Net_LevelStartStatusName(ELevelStartStatus status)
{
	switch (status)
	{
	case LST_READY: return "ready";
	case LST_HOST: return "host";
	case LST_WAITING: return "waiting";
	default: return "unknown";
	}
}

static const char* Net_LagStateName(ELagType state)
{
	switch (state)
	{
	case LAG_NONE: return "none";
	case LAG_PREDICTING: return "predicting";
	case LAG_WAITING: return "waiting";
	case LAG_SKIPPING: return "skipping";
	default: return "unknown";
	}
}

static const char* Net_InvasionStateName(EInvasionState state)
{
	switch (state)
	{
	case INVS_DISABLED: return "disabled";
	case INVS_WAITING: return "waiting";
	case INVS_COUNTDOWN: return "countdown";
	case INVS_SPAWNING: return "spawning";
	case INVS_CLEANUP: return "cleanup";
	case INVS_INTERMISSION: return "intermission";
	case INVS_VICTORY: return "victory";
	case INVS_FAILURE: return "failure";
	default: return "unknown";
	}
}

static bool Net_IsInvasionModeEnabled()
{
	return sv_gametype == 4;
}

static bool Net_IsLocalInvasionAuthority()
{
	return !netgame || I_IsLocalHCDEServiceAuthority();
}

static int Net_InvasionSecondsToTics(float seconds)
{
	if (seconds <= 0.0f)
		return 0;

	const int tics = static_cast<int>(ceil(seconds * TICRATE));
	return max(tics, 0);
}

// When true, mirror selected HCDE net/invasion diagnostics to the HUD console so
// local testers can capture issues without a DebugTrace-aware host/session.
CVAR(Bool, hcde_hud_debug, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
// Dedicated invasion debugger. Level 1 mirrors important state/spawn/result
// events to console, level 2 adds timing safety warnings, and level 3 is for
// noisy per-phase traces while chasing bad map/mod metadata.
CUSTOM_CVAR(Int, sv_invasiondebug, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
	else if (self > 3)
		self = 3;
}
// Persistent on-screen lag/invasion overlay (top-left). Also enable with `stat hcde_lag`.
CVAR(Bool, hcde_lag_hud, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
// Prediction-loss debugger. The existing fault logger only fires when the tic
// gate fully stalls (availableTics==0 with backlog + stale world). Most actual
// drift sits below that bar - SequenceAck running 1-3 behind, mirrors off by
// one, a passive resend every tic, occasional health/state repair. This cvar
// turns on a continuous probe that samples gate + mirror + repair health each
// frame and persists it. Levels:
//   0 = off
//   1 = CSV sample every `net_predict_debug_interval` tics to hcde_predict_metrics.csv
//   2 = + soft-warning lines to hcde_prediction_softwarn.log when a threshold trips
//   3 = + full DebugTrace snapshot dumped at most once per 5s when a warning trips
CUSTOM_CVAR(Int, net_predict_debug, 3, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 3)
		self = 3;
}
// Sample period for the prediction debugger, in tics. Default = TICRATE (1s).
CUSTOM_CVAR(Int, net_predict_debug_interval, 15, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	else if (self > 350)
		self = 350;
}
CUSTOM_CVAR(Int, net_echo_debug, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
}
// Reconcile decision tracing for local pose repair. 0=off, 1=log apply/skip at
// baseline threshold, 2=also log lead/allowance breakdown.
CUSTOM_CVAR(Int, net_reconcile_debug, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 2)
		self = 2;
}
CUSTOM_CVAR(Int, net_self_test_run_client, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
}
// HCDE: How many tics the local input/render side should run AHEAD of the
// server-confirmed gametic on a non-authority netgame client. The world tic
// gate inside TryRunTics holds back world simulation until ClientTic leads
// gametic by this many tics, which guarantees P_PredictClient always has
// unconfirmed commands to replay on top of the latest authoritative pose.
// Without a lead (lead == 0) ClientTic and gametic march in lockstep on a
// healthy connection and the local view only updates after each full server
// round-trip, which feels like input lag. 2 ~= 57ms at 35Hz and matches the
// jitter window we already plan for. 0 disables and restores legacy lockstep.
CUSTOM_CVAR(Int, cl_net_prediction_lead, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 8)
		self = 8;
}
// SequenceAck lag (newestTic - SequenceAck) at which the soft-warn fires.
CUSTOM_CVAR(Int, net_predict_softwarn_ack_lag, 3, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	else if (self > 60)
		self = 60;
}
// HCDE: how far around the local player to scan for nearby monsters when
// dumping the "what hit me?" diagnostic. 0 disables the dump entirely. Larger
// values (in Doom map units) widen the scan radius - 512 is comfortable for
// most arena fights, 1024 catches snipers from across a room, 4096 is the
// hard cap so we never have to walk the whole actor list.
// The dump is fired only when the snapshot reports the local player took
// damage AND prints at most once per second, so leaving this on does not
// burn meaningful CPU. The trace lands in the "net.desync" channel.
//
// Default is 768 units: large enough to catch hitscan snipers and stray
// projectiles in typical Doom map geometry, small enough to keep the trace
// readable. Set to 0 to disable.
CUSTOM_CVAR(Int, cl_debug_monster_proximity, 768, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 4096)
		self = 4096;
}
// |blocking-mirror-count - server-active-monster-count| at which the soft-warn fires.
CUSTOM_CVAR(Int, net_predict_softwarn_mirror_delta, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	else if (self > 64)
		self = 64;
}
// Number of passive-resend events within one window before the soft-warn fires.
CUSTOM_CVAR(Int, net_predict_softwarn_passive_storm, 5, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	else if (self > 1000)
		self = 1000;
}
// Native-only gameplay policy. Pregame/setup/control traffic is still accepted,
// but live gameplay must use HCDE `HCIN`/`HCSN` once the session is in netgame.
CVAR(Bool, net_hcde_native_only, true, CVAR_SERVERINFO | CVAR_NOSAVE);

static bool Net_InvasionDebugEnabled(int level = 1)
{
	return *hcde_hud_debug || *sv_invasiondebug >= level;
}

// NETWORKING
//
// gametic is the tic about to (or currently being) run.
// ClientTic is the tick the client is currently on and building a command for.
//
// A world tick cannot be ran until CurrentSequence >= gametic for all clients.

int 				ClientTic = 0;
usercmd_t			LocalCmds[LOCALCMDTICS] = {};
int					LastSentConsistency = 0;		// Last consistency we sent out. If < CurrentConsistency, send them out.
int					CurrentConsistency = 0;			// Last consistency we generated.
FClientNetState		ClientStates[MAXPLAYERS] = {};

// Try and stabilize uneven connections by checking for spikes in available
// sequences. If they're found, try and average out a buffer to prioritize
// making the experience smoother over very stop and go heavy.
static int			StabilityBuffer = 0;
static int			PrevAvailableDiff = 0;
static size_t		CurStabilityTic = 0u;
static int			StabilityTics[STABILITYTICS] = {};

// If we're sending a packet to ourselves, store it here instead. This is the simplest way to execute
// playback as it means in the world running code itself all player commands are built the exact same way
// instead of having to rely on pulling from the correct local buffers. It also ensures all commands are
// executed over the net at the exact same tick.
static size_t	LocalNetBufferSize = 0;
static uint8_t	LocalNetBuffer[MAX_MSGLEN] = {};

constexpr size_t HCDELiveMagicOffset = 1u;
constexpr size_t HCDELiveVersionOffset = 5u;
constexpr size_t HCDELiveTypeOffset = 6u;
constexpr size_t HCDELiveTxSequenceOffset = 7u;
constexpr size_t HCDELiveAckOffset = 11u;
constexpr size_t HCDELiveHeaderSize = 15u;
constexpr uint64_t HCDELiveControlIntervalMS = 1000u;
constexpr uint8_t HCDELiveProtocolVersion = 1u;
constexpr uint8_t HCDELiveMagic[4] = { 'H', 'L', 'I', 'V' };
// HCDE live control packet payload layout (after the 15-byte HCDELive header):
//   [0..5]   base 6 bytes (legacy v0 keepalive payload)
//   [6..9]   HCAP magic
//   [10]     capability extension version
//   [11]     capability extension flags (HCDELiveControlCapFlagSessionId)
//   [12..19] big-endian 64-bit capability mask
//   [20..23] big-endian 32-bit local session ID (only when SessionId flag is set)
// The capability extension is variable-length: peers built before the session
// ID extension was added still send 14 bytes (no session ID), so the parser
// must accept the shorter form to preserve cross-version compatibility.
constexpr size_t HCDELiveControlBasePayloadSize = 6u;
constexpr size_t HCDELiveControlCapabilitiesOffset = HCDELiveControlBasePayloadSize;
constexpr size_t HCDELiveControlCapabilitiesMagicOffset = HCDELiveControlCapabilitiesOffset;
constexpr size_t HCDELiveControlCapabilitiesVersionOffset = HCDELiveControlCapabilitiesOffset + 4u;
constexpr size_t HCDELiveControlCapabilitiesFlagsOffset = HCDELiveControlCapabilitiesOffset + 5u;
constexpr size_t HCDELiveControlCapabilitiesMaskOffset = HCDELiveControlCapabilitiesOffset + 6u;
// Minimum capability block (no session ID extension): 4 magic + 1 ver + 1 flags + 8 mask = 14 bytes.
constexpr size_t HCDELiveControlCapabilitiesMinSize = 14u;
// Full capability block (session ID extension present): 14 + 4 = 18 bytes.
constexpr size_t HCDELiveControlCapabilitiesSize = HCDELiveControlCapabilitiesMinSize + 4u;
constexpr size_t HCDELiveControlMinPayloadSize = HCDELiveControlBasePayloadSize + HCDELiveControlCapabilitiesMinSize;
constexpr size_t HCDELiveControlPayloadSize = HCDELiveControlBasePayloadSize + HCDELiveControlCapabilitiesSize;
constexpr uint8_t HCDELiveControlCapabilitiesVersion = 1u;
constexpr uint8_t HCDELiveControlCapFlagSessionId = 1u << 0;
constexpr uint8_t HCDELiveControlCapabilitiesMagic[4] = { 'H', 'C', 'A', 'P' };
constexpr uint64_t HCDELiveCapControlV1 = 1ull << 0;
constexpr uint64_t HCDELiveCapClientInputV5 = 1ull << 1;
constexpr uint64_t HCDELiveCapServerSnapshotV4 = 1ull << 2;
constexpr uint64_t HCDELiveCapServerWorldDeltaV2 = 1ull << 3;
constexpr uint64_t HCDELiveCapInvasionSnapshotV2 = 1ull << 4;
constexpr uint64_t HCDELiveCapActorRegistryV1 = 1ull << 16;
constexpr uint64_t HCDELiveCapActorDeltaV2 = 1ull << 17;
constexpr uint64_t HCDELiveCapLaneBudgetsV1 = 1ull << 18;
constexpr uint64_t HCDELiveCapAuthorityEventsV1 = 1ull << 19;
constexpr uint64_t HCDELiveKnownCapabilityMask =
	HCDELiveCapControlV1
	| HCDELiveCapClientInputV5
	| HCDELiveCapServerSnapshotV4
	| HCDELiveCapServerWorldDeltaV2
	| HCDELiveCapInvasionSnapshotV2
	| HCDELiveCapActorRegistryV1
	| HCDELiveCapActorDeltaV2
	| HCDELiveCapLaneBudgetsV1
	| HCDELiveCapAuthorityEventsV1;
constexpr size_t HCDEGameplayMagicOffset = 0u;
constexpr size_t HCDEGameplayVersionOffset = 4u;
constexpr size_t HCDEGameplayKindOffset = 5u;
constexpr size_t HCDEGameplayRoomOffset = 6u;
constexpr size_t HCDEGameplayFlagsOffset = 7u;
constexpr size_t HCDEGameplayTicOffset = 8u;
constexpr size_t HCDEGameplayHeaderSize = 12u;
constexpr uint8_t HCDEGameplayProtocolVersion = 1u;
constexpr uint8_t HCDEGameplayMagic[4] = { 'H', 'G', 'P', 'L' };

constexpr size_t HCDEClientInputMagicOffset = 0u;
constexpr size_t HCDEClientInputVersionOffset = 4u;
constexpr size_t HCDEClientInputFlagsOffset = 5u;
constexpr size_t HCDEClientInputRoutingOffset = 6u;
constexpr size_t HCDEClientInputPlayerCountOffset = 7u;
constexpr size_t HCDEClientInputSequenceAckOffset = 8u;
constexpr size_t HCDEClientInputConsistencyAckOffset = 12u;
constexpr size_t HCDEClientInputBaseSequenceOffset = 16u;
constexpr size_t HCDEClientInputBaseConsistencyOffset = 20u;
constexpr size_t HCDEClientInputCommandTicsOffset = 24u;
constexpr size_t HCDEClientInputConsistencyTicsOffset = 25u;
constexpr size_t HCDEClientInputStabilityOffset = 26u;
constexpr size_t HCDEClientInputBodyBytesOffset = 27u;
constexpr size_t HCDEClientInputHeaderSize = 29u;
constexpr uint8_t HCDEClientInputProtocolVersion = 5u;
constexpr uint8_t HCDEClientInputMagic[4] = { 'H', 'C', 'I', 'N' };
constexpr size_t HCDEClientInputRecordsMagicOffset = 0u;
constexpr size_t HCDEClientInputRecordsVersionOffset = 4u;
constexpr size_t HCDEClientInputRecordsPlayerCountOffset = 5u;
constexpr size_t HCDEClientInputRecordsHeaderSize = 6u;
constexpr uint8_t HCDEClientInputRecordsProtocolVersion = 4u;
constexpr uint8_t HCDEClientInputRecordsMagic[4] = { 'H', 'C', 'I', 'R' };
constexpr size_t HCDEExplicitUserCmdBytes = 16u;

constexpr size_t HCDEServerSnapshotMagicOffset = 0u;
constexpr size_t HCDEServerSnapshotVersionOffset = 4u;
constexpr size_t HCDEServerSnapshotFlagsOffset = 5u;
constexpr size_t HCDEServerSnapshotRoutingOffset = 6u;
constexpr size_t HCDEServerSnapshotPlayerCountOffset = 7u;
constexpr size_t HCDEServerSnapshotSequenceAckOffset = 8u;
constexpr size_t HCDEServerSnapshotConsistencyAckOffset = 12u;
constexpr size_t HCDEServerSnapshotQuitterBytesOffset = 16u;
constexpr size_t HCDEServerSnapshotBaseSequenceOffset = 18u;
constexpr size_t HCDEServerSnapshotBaseConsistencyOffset = 22u;
constexpr size_t HCDEServerSnapshotCommandTicsOffset = 26u;
constexpr size_t HCDEServerSnapshotConsistencyTicsOffset = 27u;
constexpr size_t HCDEServerSnapshotStabilityOffset = 28u;
constexpr size_t HCDEServerSnapshotBodyBytesOffset = 29u;
constexpr size_t HCDEServerSnapshotHeaderSize = 31u;
constexpr uint8_t HCDEServerSnapshotProtocolVersion = 4u;
constexpr uint8_t HCDEServerSnapshotMagic[4] = { 'H', 'C', 'S', 'N' };
constexpr size_t HCDEServerSnapshotRecordsMagicOffset = 0u;
constexpr size_t HCDEServerSnapshotRecordsVersionOffset = 4u;
constexpr size_t HCDEServerSnapshotRecordsPlayerCountOffset = 5u;
constexpr size_t HCDEServerSnapshotRecordsHeaderSize = 6u;
constexpr uint8_t HCDEServerSnapshotRecordsProtocolVersion = 2u;
constexpr uint8_t HCDEServerSnapshotRecordsMagic[4] = { 'H', 'C', 'S', 'R' };
// HCDW is a validated, server-authored pose stream that rides behind the
// inherited snapshot records until live replication owns world state directly.
constexpr size_t HCDEServerWorldDeltaMagicOffset = 0u;
constexpr size_t HCDEServerWorldDeltaVersionOffset = 4u;
constexpr size_t HCDEServerWorldDeltaFlagsOffset = 5u;
constexpr size_t HCDEServerWorldDeltaTicOffset = 6u;
constexpr size_t HCDEServerWorldDeltaCountOffset = 10u;
constexpr size_t HCDEServerWorldDeltaHeaderSize = 11u;
constexpr size_t HCDEServerWorldDeltaRecordV1Size = 60u;
constexpr size_t HCDEServerWorldDeltaRecordV2Size = 36u;
constexpr size_t HCDEServerWorldDeltaRecordSize = HCDEServerWorldDeltaRecordV2Size;
constexpr uint8_t HCDEServerWorldDeltaProtocolVersion = 2u;
constexpr uint8_t HCDEServerWorldDeltaMagic[4] = { 'H', 'C', 'D', 'W' };
constexpr uint8_t HCDEServerWorldDeltaPoseHasActor = 1u << 0;
constexpr uint8_t HCDEServerWorldDeltaPoseLive = 1u << 1;
constexpr uint8_t HCDEServerWorldDeltaPoseOnGround = 1u << 2;
constexpr size_t HCDEAuthorityEventsMagicOffset = 0u;
constexpr size_t HCDEAuthorityEventsVersionOffset = 4u;
constexpr size_t HCDEAuthorityEventsFlagsOffset = 5u;
constexpr size_t HCDEAuthorityEventsCountOffset = 6u;
constexpr size_t HCDEAuthorityEventsReservedOffset = 7u;
constexpr size_t HCDEAuthorityEventsHeaderSize = 8u;
constexpr uint8_t HCDEAuthorityEventsProtocolVersion = 1u;
constexpr uint8_t HCDEAuthorityEventsMagic[4] = { 'H', 'C', 'A', 'V' };
constexpr size_t HCDEAuthorityEventPacketLimit = 8u;
constexpr uint8_t HCDEAuthorityEventSpawn = 1u;
constexpr uint8_t HCDEAuthorityEventDespawn = 2u;
constexpr uint8_t HCDEAuthorityEventDamage = 3u;
constexpr int HCDEAuthorityDamageMinIntervalTics = TICRATE / 7;
constexpr int HCDEAuthorityDamageImmediateDelta = 16;
constexpr size_t HCDEInvasionSnapshotMagicOffset = 0u;
constexpr size_t HCDEInvasionSnapshotVersionOffset = 4u;
constexpr size_t HCDEInvasionSnapshotFlagsOffset = 5u;
constexpr size_t HCDEInvasionSnapshotStateOffset = 6u;
constexpr size_t HCDEInvasionSnapshotReservedOffset = 7u;
constexpr size_t HCDEInvasionSnapshotStateTicsOffset = 8u;
constexpr size_t HCDEInvasionSnapshotWaveOffset = 12u;
constexpr size_t HCDEInvasionSnapshotMaxWavesOffset = 16u;
constexpr size_t HCDEInvasionSnapshotWaveBudgetOffset = 20u;
constexpr size_t HCDEInvasionSnapshotWaveSpawnedOffset = 24u;
constexpr size_t HCDEInvasionSnapshotWaveClearedOffset = 28u;
constexpr size_t HCDEInvasionSnapshotActiveMonstersOffset = 32u;
constexpr size_t HCDEInvasionSnapshotSpawnSpotCountOffset = 36u;
constexpr size_t HCDEInvasionSnapshotActiveSpawnSpotCountOffset = 38u;
constexpr size_t HCDEInvasionSnapshotSpawnPlanBudgetOffset = 40u;
constexpr size_t HCDEInvasionSnapshotSpawnActiveTagOffset = 44u;
constexpr size_t HCDEInvasionSnapshotSpawnFlagsOffset = 48u;
constexpr size_t HCDEInvasionSnapshotSpawnFallbackSourceOffset = 49u;
constexpr size_t HCDEInvasionSnapshotHeaderV1Size = 36u;
constexpr size_t HCDEInvasionSnapshotHeaderV2Size = 52u;
constexpr uint8_t HCDEInvasionSnapshotProtocolVersion = 2u;
constexpr uint8_t HCDEInvasionSnapshotMagic[4] = { 'H', 'C', 'I', 'V' };
constexpr uint8_t HCDEInvasionSnapshotFlagBossWave = 1u << 0;
constexpr uint8_t HCDEInvasionSnapshotSpawnFlagUsingFallback = 1u << 0;
constexpr size_t HCDEAuthorityEventReplayLimit = 64u;
constexpr size_t HCDEAuthorityEventHistoryLimit = 128u;
constexpr size_t HCDEInvasionSpawnEventHistoryLimit = 128u;
constexpr size_t HCDEAuthorityEventActorDeltaReserveBytes = 900u;
constexpr uint8_t HCDEInvasionActorActionNone = 0u;
constexpr uint8_t HCDEInvasionActorActionSpawn = 1u;
constexpr uint8_t HCDEInvasionActorActionSee = 2u;
constexpr uint8_t HCDEInvasionActorActionMelee = 3u;
constexpr uint8_t HCDEInvasionActorActionMissile = 4u;
constexpr uint8_t HCDEInvasionActorActionPain = 5u;
constexpr uint8_t HCDEInvasionActorActionMax = HCDEInvasionActorActionPain;
constexpr int HCDEInvasionActorActionHoldTics = TICRATE / 2;
constexpr size_t HCDEActorDeltasMagicOffset = 0u;
constexpr size_t HCDEActorDeltasVersionOffset = 4u;
constexpr size_t HCDEActorDeltasFlagsOffset = 5u;
constexpr size_t HCDEActorDeltasCountOffset = 6u;
constexpr size_t HCDEActorDeltasReservedOffset = 7u;
constexpr size_t HCDEActorDeltasHeaderSize = 8u;
constexpr uint8_t HCDEActorDeltasProtocolVersion = 2u;
constexpr uint8_t HCDEActorDeltasMagic[4] = { 'H', 'C', 'D', 'A' };
constexpr uint8_t HCDEActorDeltasFlagComplete = 1u << 0;
constexpr uint8_t HCDEActorDeltaFlagLive = 1u << 0;
constexpr uint16_t HCDEActorDeltaFieldCategory = 1u << 0;
constexpr uint16_t HCDEActorDeltaFieldFlags = 1u << 1;
constexpr uint16_t HCDEActorDeltaFieldAction = 1u << 2;
constexpr uint16_t HCDEActorDeltaFieldHealth = 1u << 3;
constexpr uint16_t HCDEActorDeltaFieldPos = 1u << 4;
constexpr uint16_t HCDEActorDeltaFieldVel = 1u << 5;
constexpr uint16_t HCDEActorDeltaFieldAngles = 1u << 6;
constexpr uint16_t HCDEActorDeltaFieldAll =
	HCDEActorDeltaFieldCategory
	| HCDEActorDeltaFieldFlags
	| HCDEActorDeltaFieldAction
	| HCDEActorDeltaFieldHealth
	| HCDEActorDeltaFieldPos
	| HCDEActorDeltaFieldVel
	| HCDEActorDeltaFieldAngles;
constexpr double HCDEActorDeltaPosScale = 16.0;
constexpr double HCDEActorDeltaVelScale = 32.0;
constexpr size_t HCDEInvasionSnapshotPayloadBudgetBytes = 1200u;
constexpr size_t HCDELaneBudgetControlBytes = 96u;
constexpr size_t HCDELaneBudgetCommandBytes = 4096u;
constexpr size_t HCDELaneBudgetAuthorityBytes = 384u;
constexpr size_t HCDELaneBudgetPlayerSnapshotBytes = 4096u;
constexpr size_t HCDELaneBudgetActorDeltaBytes = 900u;
constexpr size_t HCDELaneBudgetQueryRegistryBytes = 512u;
constexpr size_t HCDELaneBudgetPresentationEchoBytes = 512u;
constexpr int HCDEActorBaselineRepairWindowTics = TICRATE * 2;
constexpr double HCDEInvasionMirrorVisualFallbackStepPerTic = 8.0;
constexpr double HCDEInvasionMirrorVisualSpeedMultiplier = 1.10;
constexpr double HCDEInvasionMirrorVisualMaxStepPerTic = 12.0;
constexpr double HCDEInvasionMirrorVisualSnapDistance = 384.0;
constexpr int HCDEInvasionProjectileMirrorMaxAgeTics = TICRATE * 3;
constexpr int HCDEInvasionMaxWavesLimit = 3000;
constexpr double HCDEServerBaselineRepairDistance = 128.0;
constexpr int HCDEPassiveClientResendSequenceSlack = 4;
// Minimum number of tics an ack must remain at the same value before the
// passive-resend gate considers it actually stuck. On a healthy connection an
// ack still trails by 1-5 tics every frame; only a sustained stall warrants a
// soft snapshot/command replay. TICRATE/4 ~= 230 ms at 35 hz which is well
// above normal localhost RTT jitter and below the threshold that hurts gameplay.
constexpr int HCDEPassiveResendAckStaleTics = TICRATE / 4;
// Cooldown between consecutive passive-resend triggers for the same client to
// keep a still-stuck ack from re-firing every single tic and re-creating the
// resend storm we are trying to fix.
constexpr int HCDEPassiveResendRetryTics = TICRATE / 4;
constexpr double HCDEServerReconcileDistance = 20.0;
constexpr double HCDEServerReconcileHardDistance = 384.0;
// Pose repair on damage only kicks in once drift exceeds this squared distance.
// Keeps imp chip damage from constantly snapping the local pawn while still
// correcting meaningful melee desync.
constexpr double HCDEServerReconcilePoseDamageDistance = 128.0;
constexpr int HCDEServerReconcilePoseMinDamage = 10;

enum EHCDELiveMessage : uint8_t
{
	HLIVE_CONTROL = 1,
	HLIVE_CLIENT_COMMANDS,
	HLIVE_SERVER_SNAPSHOT,
};

enum EHCDEGameplayPayload : uint8_t
{
	HGP_RESERVED_LEGACY_CLIENT_COMMANDS = 1,
	HGP_RESERVED_LEGACY_SERVER_SNAPSHOT,
	HGP_CLIENT_INPUTS,
	HGP_SERVER_SNAPSHOT,
};

enum EHCDEActorInterestTier : uint8_t
{
	HINTEREST_CRITICAL = 0,
	HINTEREST_HIGH,
	HINTEREST_MEDIUM,
	HINTEREST_LOW,
	HINTEREST_DORMANT,
	HINTEREST_COUNT,
};

enum EHCDEInvasionSimulationLOD : uint8_t
{
	HSIMLOD_FULL = 0,
	HSIMLOD_REDUCED,
	HSIMLOD_DORMANT,
	HSIMLOD_COUNT,
};

struct FHCDELiveLaneStats
{
	uint64_t TxPackets = 0u;
	uint64_t TxBytes = 0u;
	uint64_t RxPackets = 0u;
	uint64_t RxBytes = 0u;
	uint64_t Deferred = 0u;
	uint64_t BudgetClamps = 0u;
};

struct FHCDEActorPriorityCandidate
{
	size_t ActorIndex = 0u;
	int Score = 0;
	bool Priority = false;
	bool KeepAlive = false;
	uint8_t InterestTier = HINTEREST_DORMANT;
};

struct FHCDEActorInterestResult
{
	bool Relevant = false;
	bool Priority = false;
	bool KeepAlive = false;
	bool Protected = false;
	bool HasBaseline = false;
	uint8_t Tier = HINTEREST_DORMANT;
	int LastRelevantTic = 0;
	int KeepAliveTics = TICRATE * 4;
	int Score = 0;
	double DistanceSquared = -1.0;
};

struct FHCDEProjectilePolicyResult
{
	bool IsProjectile = false;
	bool Relevant = false;
	bool Protected = false;
	bool KeepAlive = false;
	bool PlayerOwned = false;
	bool TargetingViewer = false;
	bool InboundToViewer = false;
	bool HasBaseline = false;
	uint8_t Tier = HINTEREST_DORMANT;
	int KeepAliveTics = TICRATE * 3;
	int ScoreBonus = 0;
	double DistanceSquared = -1.0;
};

struct FHCDELivePeerState
{
	uint32_t TxSequence = 0u;
	// Highest accepted envelope sequence across all live message types. This is
	// only used for peer ACK/telemetry; gameplay ordering is tracked per lane.
	uint32_t RxSequence = 0u;
	uint32_t RxControlSequence = 0u;
	uint32_t RxClientCommandSequence = 0u;
	uint32_t RxServerSnapshotSequence = 0u;
	uint32_t PeerAck = 0u;
	uint32_t DuplicateCount = 0u;
	uint32_t ControlSent = 0u;
	uint32_t ControlReceived = 0u;
	uint32_t ClientCommandSent = 0u;
	uint32_t ClientCommandReceived = 0u;
	uint32_t SnapshotSent = 0u;
	uint32_t SnapshotReceived = 0u;
	uint32_t LastAppliedSnapshotGameTic = 0u;
	uint32_t UnsupportedReceived = 0u;
	uint32_t AuthorityRejected = 0u;
	uint64_t RemoteCapabilities = 0u;
	uint64_t NegotiatedCapabilities = 0u;
	uint64_t UnsupportedCapabilities = 0u;
	uint32_t CapabilityControlReceived = 0u;
	uint32_t LegacyControlReceived = 0u;
	uint32_t WorldDeltaReceived = 0u;
	uint32_t BaselineRepairs = 0u;
	uint32_t BaselineLocalDrift = 0u;
	uint32_t Reconciliations = 0u;
	uint32_t HardReconciliations = 0u;
	FHCDELiveLaneStats Lanes[HLANE_COUNT] = {};
	uint32_t ActorQueueDepth = 0u;
	uint32_t ActorQueuePriorityDepth = 0u;
	uint32_t ActorQueueDeferredDepth = 0u;
	int ActorQueueTopScore = 0;
	uint32_t ActorInterestSkipped = 0u;
	uint32_t ActorInterestKeepAlive = 0u;
	uint32_t ActorInterestTiers[HINTEREST_COUNT] = {};
	uint32_t ProjectilePolicyTiers[HINTEREST_COUNT] = {};
	uint32_t ProjectilePolicySkipped = 0u;
	uint32_t ProjectilePolicyKeepAlive = 0u;
	uint32_t ProjectilePolicyProtected = 0u;
	uint32_t ActorBaselineRepairWindows = 0u;
	uint32_t ActorBaselineRepairResets = 0u;
	uint32_t AuthorityEventCatchupRecords = 0u;
	uint32_t PlayerSnapshotBudgetPressure = 0u;
	uint32_t PlayerSnapshotMaxBytes = 0u;
	uint32_t PlayerSnapshotMaxRecords = 0u;
	uint32_t SharedActorPlayerRecordsSuppressed = 0u;
	uint32_t ReplayRequestStrikes = 0u;
	uint64_t ReplayRequestWindowStartMS = 0u;
	uint64_t ReplayRequestSuppressedUntilMS = 0u;

	void Clear()
	{
		*this = {};
	}
};

struct FHCDELiveNativeSendScratch
{
	usercmd_t SnapshotCommands[MAXPLAYERS][MAXSENDTICS] = {};
	const uint8_t* SnapshotEventStreams[MAXPLAYERS][MAXSENDTICS] = {};
	size_t SnapshotEventSizes[MAXPLAYERS][MAXSENDTICS] = {};
	usercmd_t ClientCommands[MAXSENDTICS] = {};
	const uint8_t* ClientEventStreams[MAXSENDTICS] = {};
	size_t ClientEventSizes[MAXSENDTICS] = {};
};

static uint8_t	CurrentRoomID = 0u;	// Ignore commands not from this room (useful when transitioning levels).

void Net_TraceSetSvGametype(int value, const char* reason)
{
	const int oldValue = int(sv_gametype);
	if (oldValue == value)
		return;

	DebugTrace::Infof("mode", "sv_gametype %d -> %d reason=%s gametic=%d room=%u map=%s",
		oldValue, value, reason != nullptr ? reason : "?",
		gametic, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	sv_gametype = value;
}

void Net_TraceSetDeathmatch(int value, const char* reason)
{
	const int oldValue = int(deathmatch);
	if (oldValue == value)
		return;

	DebugTrace::Infof("mode", "deathmatch %d -> %d reason=%s gametic=%d",
		oldValue, value, reason != nullptr ? reason : "?", gametic);
	deathmatch = value;
}

void Net_TraceSetTeamplay(int value, const char* reason)
{
	const int oldValue = int(teamplay);
	if (oldValue == value)
		return;

	DebugTrace::Infof("mode", "teamplay %d -> %d reason=%s gametic=%d",
		oldValue, value, reason != nullptr ? reason : "?", gametic);
	teamplay = value;
}
static int		LastGameUpdate = 0;		// Track the last time the game actually ran the world.
static uint64_t	MutedClients = 0u;		// Ignore messages from these clients.
static_assert(MAXPLAYERS <= 64, "MAXPLAYERS must remain <=64 while using fixed-sized late-join bitmask state.");
static uint64_t LateJoinSyncPending = 0u; // Clients admitted during an active match but not yet live-synced.
static int LateJoinSyncTargetSequence[MAXPLAYERS] = {};
static int LateJoinSyncTargetConsistency[MAXPLAYERS] = {};
static int LateJoinSyncStartTic[MAXPLAYERS] = {};
// Per-client repair windows force fresh actor baselines and walk authority-event history after join or reset.
static int HCDEActorBaselineRepairUntilTic[MAXPLAYERS] = {};
static uint32_t HCDEAuthorityEventReplayNextId[MAXPLAYERS] = {};
static int ConsistencyGraceUntilTic[MAXPLAYERS] = {};
// Per-client state used by HCDEPassiveResendShouldFire() to decide whether a
// lagging SequenceAck is structurally stuck. LastSequenceAck records the most
// recently observed ack value, AckChangedTic records the gametic when that
// value last advanced, and LastRequestTic enforces the retry cooldown.
// Net_ClearBuffers and Net_ResetClientState own resetting all three.
static int HCDEPassiveResendLastSequenceAck[MAXPLAYERS] = {};
static int HCDEPassiveResendAckChangedTic[MAXPLAYERS] = {};
static int HCDEPassiveResendLastRequestTic[MAXPLAYERS] = {};
// Per-client gametic deadline for the next non-authority receive-side compaction
// pass on HCDEReplicatedActors. Authorities compact during their send/migration
// passes; pure receivers need their own cadence or stale baseline-only refs
// pile up indefinitely on actor-heavy maps.
static int HCDEActorDeltaV2ReceiveCompactNextTic[MAXPLAYERS] = {};
static uint64_t	LastHCDELiveControlMS = 0u;
static uint64_t LastHCDELiveSequenceRejectReportMS = 0u;
static uint64_t LastHCDELiveSnapshotRejectReportMS = 0u;
#include "d_net_predict_state.cpp"

// Profile for an invasion monster type.
struct FInvasionMonsterProfile
{
	const char* ClassName; // Doom actor class name (e.g. "DoomImp").
	int Cost;             // Spawn cost (unused in Stage 10 director).
	int MinWave;          // Earliest wave this monster can appear in pool-based spawning.
};

static const FInvasionMonsterProfile InvasionMonsterProfiles[] =
{
	{ "ZombieMan", 1, 1 },
	{ "ShotgunGuy", 2, 1 },
	{ "DoomImp", 2, 1 },
	{ "ChaingunGuy", 3, 2 },
	{ "Demon", 4, 1 },
	{ "Spectre", 4, 1 },
	{ "LostSoul", 2, 1 },
	{ "Cacodemon", 10, 2 },
	{ "HellKnight", 15, 3 },
	{ "Revenant", 15, 3 },
	{ "Arachnotron", 18, 4 },
	{ "Fatso", 20, 4 },
	{ "PainElemental", 25, 4 },
	{ "BaronOfHell", 40, 5 },
	{ "Archvile", 60, 6 },
	{ "WolfensteinSS", 5, 2 },
	{ "Cyberdemon", 200, 8 },
	{ "SpiderMastermind", 180, 8 }
};

static int CutsceneCountdown = 0;	// If enough people are ready, count down the timer. This won't reset between unreadies, only on intermission entrance.
static uint64_t CutsceneReady = 0u; // If in a cutscene, check if we're ready to move to move past it.
static int CutsceneReadyLastToggle[MAXPLAYERS] = {};
static EInvasionState InvasionState = INVS_DISABLED; // Authoritative game phase.
static int InvasionStateTics = 0;                  // Time remaining in current phase (if applicable).
static int InvasionPendingWave = 0;                // Wave queued during countdown; committed on wave start.
static int InvasionNoParticipantTics = 0;          // Grace window for dedicated server reconnects with no participants.
static EInvasionState InvasionAnnouncementState = INVS_DISABLED; // Local last-seen state for HUD/console announcement dedupe.
static int InvasionAnnouncementWave = 0;              // Local last-seen wave for announcement transitions.
static int InvasionAnnouncementLastCountdownSecond = -1; // Last second announced for "Prepare for invasion".
static int InvasionReplicatedActiveMonsterCount = 0; // Cached count for client UI.
static int InvasionWipeGraceTics = 0;              // Grace period before declaring failure after all players die.
static FRandom pr_invasion("Invasion");            // Seeded RNG for invasion spawning/selection.
constexpr uint8_t INV_WAVEF_BOSS = 1u << 0;

struct FInvasionWaveDirector
{
	int Wave = 0;
	int MaxWaves = 0;
	int WaveBudget = 0;
	int WaveSpawned = 0;
	int WaveCleared = 0;
	uint8_t WaveFlags = 0u;
	int SpawnIntervalTics = 0;
	int SpawnIntervalCountdown = 0;
	TArray<TObjPtr<AActor*>> ActiveMonsters;

	void Reset()
	{
		Wave = 0;
		MaxWaves = 0;
		WaveBudget = 0;
		WaveSpawned = 0;
		WaveCleared = 0;
		WaveFlags = 0u;
		SpawnIntervalTics = 0;
		SpawnIntervalCountdown = 0;
		ActiveMonsters.Clear();
	}
};

enum EInvasionSpawnSource : uint8_t
{
	INVSPAWN_NONE = 0,
	INVSPAWN_CLASSIC = 1,
	INVSPAWN_MAPSPOT = 2,
	INVSPAWN_DEATHMATCH = 3,
	INVSPAWN_PLAYERSTART = 4,
};

struct FInvasionSpawnSpotRecord
{
	DVector3 Pos = {};
	DAngle Yaw = nullAngle;
	int Tag = 0;
	int StartSpawnNumber = 1;
	int SpawnDelayTics = 0;
	int RoundSpawnDelayTics = 0;
	int FirstWave = 1;
	int MaxSpawnNumber = 0;
	int PlannedSpawnCount = 0;
	int SpawnedCount = 0;
	int NextSpawnGameTic = 0;
	EInvasionSpawnSource Source = INVSPAWN_CLASSIC;
	PClassActor* SpotClass = nullptr;
};

struct FInvasionSpawnDirectory
{
	TArray<FInvasionSpawnSpotRecord> Spots;
	int ActiveTag = 0;
	int TotalSpotCount = 0;
	int ActiveSpotCount = 0;
	int TotalSpawnBudget = 0;
	int SpawnedThisWave = 0;
	bool UsingFallback = false;
	EInvasionSpawnSource FallbackSource = INVSPAWN_NONE;

	void Reset()
	{
		Spots.Clear();
		ActiveTag = 0;
		TotalSpotCount = 0;
		ActiveSpotCount = 0;
		TotalSpawnBudget = 0;
		SpawnedThisWave = 0;
		UsingFallback = false;
		FallbackSource = INVSPAWN_NONE;
	}
};

struct FHCDEAuthorityEvent
{
	uint8_t EventType = HCDEAuthorityEventSpawn;
	uint8_t Source = 0u;
	uint8_t Category = 0u;
	uint8_t ActorFlags = 0u;
	uint16_t ClassId = 0u;
	uint32_t EventSeq = 0u;
	uint32_t Id = 0u;
	int Tic = 0;
	int Wave = 0;
	FString ClassName;
	DVector3 Pos = {};
	DVector3 Vel = {};
	DAngle Yaw = nullAngle;
	int Health = 0;
};

struct FInvasionReplicatedActorRef
{
	uint32_t Id = 0u;
	TObjPtr<AActor*> Actor = MakeObjPtr<AActor*>(nullptr);
	bool DeathDeltaSent = false;
	bool IsProjectile = false;
	bool ForceDeathDelta = false;
	bool MirrorVisualArmed = false;
	uint8_t ServerForcedActionState = HCDEInvasionActorActionNone;
	int ServerForcedActionTic = 0;
	bool HasVisualTarget = false;
	DVector3 VisualTargetPos = {};
	DVector3 VisualTargetVel = {};
	DAngle VisualTargetYaw = nullAngle;
	DAngle VisualTargetPitch = nullAngle;
	int VisualTargetHealth = 0;
	int VisualTargetTic = 0;
	int SpawnTic = 0;
	// Authority-event sampling is kept separate from visual smoothing and sim LOD.
	int LastAuthorityHealth = 0;
	int LastAuthorityEventHealth = 0;
	int LastAuthorityHealthEventTic = 0;
	uint8_t VisualActionState = HCDEInvasionActorActionNone;
	int VisualActionTic = 0;
	uint8_t SimulationLOD = HSIMLOD_FULL;
	bool SimulationSuspended = false;
	int SimulationOriginalStatNum = STAT_DEFAULT;
	int SimulationNextThinkTic = 0;
	int SimulationLastDecisionTic = 0;
	int SimulationLastHealth = 0;
	uint64_t SimulationSkippedTics = 0u;
	uint64_t SimulationAllowedTics = 0u;
};

enum EHCDEReplicatedActorCategory : uint8_t
{
	HREP_ACTOR_UNKNOWN = 0,
	HREP_ACTOR_PLAYER,
	HREP_ACTOR_MONSTER,
	HREP_ACTOR_PROJECTILE,
	HREP_ACTOR_PICKUP,
	HREP_ACTOR_MAP,
	HREP_ACTOR_SCRIPT,
	HREP_ACTOR_VISUAL,
};

enum EHCDEReplicatedActorSource : uint8_t
{
	HREP_SOURCE_SHARED = 0,
	HREP_SOURCE_INVASION,
	HREP_SOURCE_COOP,
	HREP_SOURCE_DM,
};

struct FHCDEReplicatedActorClientState
{
	bool BaselineValid = false;
	int LastBaselineTic = 0;
	int LastSentTic = 0;
	uint16_t ClassId = 0u;
	uint8_t Category = HREP_ACTOR_UNKNOWN;
	uint8_t Flags = 0u;
	uint8_t ActionState = HCDEInvasionActorActionNone;
	int Health = 0;
	DVector3 Pos = {};
	DVector3 Vel = {};
	uint32_t Yaw = 0u;
	uint32_t Pitch = 0u;
};

struct FHCDEReplicatedActorRef
{
	uint32_t Id = 0u;
	TObjPtr<AActor*> Actor = MakeObjPtr<AActor*>(nullptr);
	uint16_t ClassId = 0u;
	uint8_t Category = HREP_ACTOR_UNKNOWN;
	uint8_t Source = HREP_SOURCE_SHARED;
	uint8_t Flags = 0u;
	bool Active = false;
	bool Retired = false;
	int SpawnTic = 0;
	int RetireTic = 0;
	int LastTouchedTic = 0;
	FHCDEReplicatedActorClientState ClientState[MAXPLAYERS] = {};
};

struct FInvasionPendingMirrorSpawn
{
	uint32_t Id = 0u;
	int Wave = 0;
	FString ClassName;
	DVector3 Pos = {};
	DVector3 Vel = {};
	DAngle Yaw = nullAngle;
	DAngle Pitch = nullAngle;
	int Health = 0;
	bool MarkApplied = false;
	int QueuedTic = 0;
};

static FInvasionWaveDirector InvasionWaveDirector = {};
static FInvasionSpawnDirectory InvasionSpawnDirectory = {};
static TArray<FInvasionSpawnSpotRecord> InvasionRegisteredSpawnSpots = {};
static FLevelLocals* InvasionRegisteredSpawnSpotLevel = nullptr;
// Retained HCAV facts are replayed to late joiners and repair windows; keep the log bounded.
static TArray<FHCDEAuthorityEvent> HCDERecentAuthorityEvents = {};
static TArray<FHCDEAuthorityEvent> InvasionPendingSpawnEvents = {};
static TArray<FInvasionPendingMirrorSpawn> InvasionPendingMirrorSpawns = {};
static TArray<FInvasionReplicatedActorRef> InvasionReplicatedActors = {};
static TMap<uint32_t, unsigned int> InvasionReplicatedActorIdIndex = {};
static TMap<const AActor*, unsigned int> InvasionReplicatedActorPtrIndex = {};
static TArray<FHCDEReplicatedActorRef> HCDEReplicatedActors = {};
static TMap<uint32_t, unsigned int> HCDEReplicatedActorIdIndex = {};
static TMap<const AActor*, unsigned int> HCDEReplicatedActorPtrIndex = {};
static TArray<const PClassActor*> HCDEReplicatedActorClasses = {};
static TMap<const PClassActor*, unsigned int> HCDEReplicatedActorClassIndex = {};
static uint32_t HCDEModeNextActorId = 0x80000000u;
static int HCDEModeMigrationNextScanTic = 0;
static uint32_t HCDEModeMigrationLastConsidered = 0u;
static uint32_t HCDEModeMigrationLastRegistered = 0u;
static uint32_t HCDEModeMigrationLastInvasion = 0u;
static uint32_t HCDEModeMigrationLastCoop = 0u;
static uint32_t HCDEModeMigrationLastDM = 0u;
static uint32_t InvasionNextAuthorityEventSeq = 1u;
static uint32_t InvasionNextSpawnEventId = 1u;
static uint32_t InvasionLastAppliedSpawnEventId = 0u;
static size_t HCDEInvasionActorDeltaV2SendCursor[MAXPLAYERS] = {};
static size_t HCDEActorDeltaV2SendCursor[MAXPLAYERS] = {};
static TArray<FHCDEActorPriorityCandidate> HCDEActorPriorityQueues[MAXPLAYERS] = {};
static uint32_t InvasionSimulationLODCurrent[HSIMLOD_COUNT] = {};
static uint32_t InvasionSimulationLODSuspendedCurrent = 0u;
static uint32_t InvasionSimulationLODAllowedCurrent = 0u;
static uint32_t InvasionSimulationLODSkippedCurrent = 0u;
static uint32_t InvasionSimulationLODWakeHealthCurrent = 0u;
static uint32_t InvasionSimulationLODWakeDistanceCurrent = 0u;
static int InvasionMirrorNextVisualDiagnosticTic = 0;
static int InvasionMirrorNextPurgeTic = 0;
static uint64_t LastMirrorVisualPassUS = 0u;
static FHCDELagHUDMetrics HCDELagHUDLast = {};

static void Net_RefreshLagHUDMetrics(int availableTics, int runTics, int totalTics, int worldSteps, bool ticGateStalled);
static int InvasionMirrorVisualTickBudget = 0;
// Client mirror actors only get one visual pass per rendered frame while TryRunTics may
// advance multiple gametics; scale projectile deltas by gametics consumed so catch-up stays believable.
static int InvasionMirrorVisualWorldSteps = 1;

static void Net_RebuildInvasionSpawnSpots(int wave);
static void Net_PrepareInvasionMirrorFromSnapshot(EInvasionState previousState, int previousWave);
static FInvasionReplicatedActorRef* Net_FindInvasionReplicatedActor(uint32_t id);
static FInvasionReplicatedActorRef* Net_FindInvasionReplicatedActorByActor(const AActor* actor);
static void Net_RegisterInvasionReplicatedActor(uint32_t id, AActor* actor);
static void Net_ClearInvasionReplicatedActorIndexes();
static void Net_IndexInvasionReplicatedActor(size_t index);
static void Net_RebuildInvasionReplicatedActorIndexes();
static bool Net_GetInvasionReplicatedActorIndex(uint32_t id, size_t& index);
static bool Net_GetInvasionReplicatedActorIndexByActor(const AActor* actor, size_t& index);
static void Net_SetInvasionReplicatedActorPtr(FInvasionReplicatedActorRef& ref, AActor* actor);
static const char* HCDEReplicatedActorCategoryName(uint8_t category);
static uint8_t Net_ClassifyHCDEReplicatedActor(const AActor* actor, bool invasionProjectile);
static uint16_t Net_GetHCDEReplicatedActorClassId(const PClassActor* actorClass);
static FHCDEReplicatedActorRef* Net_FindHCDEReplicatedActor(uint32_t id);
static FHCDEReplicatedActorRef* Net_FindHCDEReplicatedActorByActor(const AActor* actor);
static void Net_RegisterHCDEReplicatedActor(uint32_t id, AActor* actor, uint8_t category, uint8_t source);
static void Net_RetireHCDEReplicatedActor(uint32_t id);
static int Net_CompactHCDEReplicatedActors();
static void Net_ClearHCDEReplicatedActors();
static void Net_DrainPendingInvasionSpawnEvents();
static void Net_DrainPendingInvasionMirrorSpawns();
static AActor* Net_SelectInvasionCombatTarget(AActor* actor);
static int Net_GetInvasionSkillWakeDelayTics();
static void Net_AwakenInvasionMonster(AActor* actor);
static void Net_SpawnInvasionTeleportFog(AActor* actor);
static bool Net_IsInvasionReplicatedProjectile(const AActor* actor);
static bool Net_ClassDefaultsSuggestProjectile(PClassActor* cls);
static bool Net_IsInvasionActorCorpseLike(const AActor* actor);
static void Net_SetInvasionMirrorVisualOnly(uint32_t id, AActor* actor);
static void Net_TickInvasionMirrorVisualActors();
static void Net_LogInvasionMirrorVisualDiagnostic();
static void Net_DetachInvasionMirrorCorpse(FInvasionReplicatedActorRef& ref);
static void Net_RetireInvasionMirrorProjectile(FInvasionReplicatedActorRef& ref);
static void Net_PurgeStaleInvasionMirrorActorsOnClient();

static bool Net_IsInvasionRoundActiveState(EInvasionState state)
{
	return state == INVS_SPAWNING || state == INVS_CLEANUP || state == INVS_INTERMISSION;
}

static void Net_ResetInvasionAnnouncements()
{
	InvasionAnnouncementState = INVS_DISABLED;
	InvasionAnnouncementWave = 0;
	InvasionAnnouncementLastCountdownSecond = -1;
}

static void Net_ShowInvasionStatusMessage(const char* text)
{
	if (text == nullptr || text[0] == '\0')
		return;

	// In client/listen sessions, use a centered HUD message so the wave flow
	// is visible during combat. Dedicated servers fall back to console output.
	if (!I_IsDedicatedServerMode() && gamestate == GS_LEVEL)
	{
		C_MidPrint(BigFont, text, true);
	}
	else
	{
		Printf(PRINT_HIGH, "%s\n", text);
	}
}

static void Net_TickInvasionAnnouncements()
{
	if (demoplayback || !Net_IsInvasionModeEnabled())
	{
		Net_ResetInvasionAnnouncements();
		return;
	}

	const EInvasionState state = InvasionState;
	const int wave = max(InvasionWaveDirector.Wave, 0);
	const EInvasionState prevState = InvasionAnnouncementState;

	if (state != prevState || wave != InvasionAnnouncementWave)
	{
		// Emit a completion message when a wave exits active combat and enters
		// intermission or victory.
		if ((state == INVS_INTERMISSION || state == INVS_VICTORY)
			&& wave > 0
			&& prevState != INVS_DISABLED
			&& prevState != INVS_WAITING
			&& prevState != INVS_COUNTDOWN)
		{
			FString msg;
			msg.Format("Wave %d completed", wave);
			Net_ShowInvasionStatusMessage(msg.GetChars());
		}

		if (state == INVS_SPAWNING
			&& wave > 0
			&& prevState != INVS_SPAWNING)
		{
			FString msg;
			if (wave == 1)
				msg = "FIGHT";
			else
				msg.Format("Wave %d begins", wave);
			Net_ShowInvasionStatusMessage(msg.GetChars());
		}

		if (state != INVS_COUNTDOWN)
			InvasionAnnouncementLastCountdownSecond = -1;

		InvasionAnnouncementState = state;
		InvasionAnnouncementWave = wave;
	}

	if (state == INVS_COUNTDOWN)
	{
		const int tics = max(CutsceneCountdown, max(InvasionStateTics, 0));
		const int seconds = (tics + TICRATE - 1) / TICRATE;
		if (seconds > 0 && seconds != InvasionAnnouncementLastCountdownSecond)
		{
			FString msg;
			const int pendingWave = max(InvasionPendingWave, 0);
			const int maxWaves = max(InvasionWaveDirector.MaxWaves, 0);
			if (pendingWave > 0 && pendingWave == maxWaves)
				msg.Format("Final wave in: %d", seconds);
			else
				msg.Format("Prepare for invasion: %d", seconds);
			Net_ShowInvasionStatusMessage(msg.GetChars());
			InvasionAnnouncementLastCountdownSecond = seconds;
		}
	}
	else if (state == INVS_INTERMISSION)
	{
		const int seconds = (max(InvasionStateTics, 0) + TICRATE - 1) / TICRATE;
		if (seconds > 0 && seconds != InvasionAnnouncementLastCountdownSecond)
		{
			FString msg;
			msg.Format("Next wave in: %d", seconds);
			Net_ShowInvasionStatusMessage(msg.GetChars());
			InvasionAnnouncementLastCountdownSecond = seconds;
		}
	}
}

static int  LevelStartDebug = 0;
static int	LevelStartDelay = 0; // While this is > 0, don't start generating packets yet.
static ELevelStartStatus LevelStartStatus = LST_READY; // Listen for when to actually start making tics.

static void Net_SetLevelStartStatus(ELevelStartStatus status, const char* reason)
{
	if (LevelStartStatus == status)
		return;

	DebugTrace::Infof("net.levelstart", "%s -> %s reason=%s gametic=%d clienttic=%d room=%u map=%s",
		Net_LevelStartStatusName(LevelStartStatus), Net_LevelStartStatusName(status),
		reason != nullptr ? reason : "?", gametic, ClientTic, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	LevelStartStatus = status;
	if (status == LST_READY && netgame)
		Net_DiagSessionNetReady();
}

static uint64_t	LevelStartAck = 0u; // Used by the host to determine if everyone has loaded in.

static int FullLatencyCycle = MAXSENDTICS * 3;	// Give ~3 seconds to gather latency info about clients on boot up.
static int LastLatencyUpdate = 0;				// Update average latency every ~1 second.

static ELagType	LagState = LAG_NONE;	// What kind of lag the game is currently getting.
static int 	EnterTic = 0;
static int	LastEnterTic = 0;
static bool bCommandsReset = false;		// If true, commands were recently cleared. Don't generate any more tics.
static int	AuthorityWaitGraceUntil = 0;
static int	LastTicGateStallTrace = 0;

static void Net_RefreshLagHUDMetrics(int availableTics, int runTics, int totalTics, int worldSteps, bool ticGateStalled)
{
	HCDELagHUDLast.Gametic = gametic;
	HCDELagHUDLast.ClientTic = ClientTic;
	HCDELagHUDLast.CommandBacklog = ClientTic - gametic;
	HCDELagHUDLast.AvailableTics = availableTics;
	HCDELagHUDLast.RunTics = runTics;
	HCDELagHUDLast.TotalTics = totalTics;
	HCDELagHUDLast.WorldSteps = worldSteps;
	HCDELagHUDLast.StabilityBuffer = StabilityBuffer;
	HCDELagHUDLast.SimStaleTics = max<int>(EnterTic - LastGameUpdate, 0);
	HCDELagHUDLast.TicGateStalled = ticGateStalled;
	HCDELagHUDLast.DedicatedClient = I_UsesDedicatedServerSlot() && !I_IsLocalHCDEServiceAuthority();
	HCDELagHUDLast.InvasionAuthority = Net_IsLocalInvasionAuthority();
	HCDELagHUDLast.LagState = Net_LagStateName(LagState);
	HCDELagHUDLast.InvasionState = Net_InvasionStateName(InvasionState);
	HCDELagHUDLast.InvasionWave = InvasionWaveDirector.Wave;
	HCDELagHUDLast.TrackedMirrors = int(InvasionReplicatedActors.Size());
	HCDELagHUDLast.PendingSpawns = int(InvasionPendingMirrorSpawns.Size());
	HCDELagHUDLast.PendingEvents = int(InvasionPendingSpawnEvents.Size());
	HCDELagHUDLast.LastMirrorMS = double(LastMirrorVisualPassUS) / 1000.0;
	HCDELagHUDLast.AvgMirrorMS = HCDEProfileAverage(HCDELiveProfile.MirrorVisualMicros, HCDELiveProfile.MirrorVisualPasses) / 1000.0;
	HCDELagHUDLast.MaxMirrorMS = double(HCDELiveProfile.MirrorVisualMaxMicros) / 1000.0;
	HCDELagHUDLast.AvgWorldMS = HCDEProfileAverage(HCDELiveProfile.WorldTicMicros, HCDELiveProfile.WorldTics) / 1000.0;
	HCDELagHUDLast.MaxWorldMS = double(HCDELiveProfile.WorldTicMaxMicros) / 1000.0;
}

static int	CommandsAhead = 0;		// If too far ahead of the host, slow down to remove built-up latency.
static int	SkipCommandTimer = 0;	// Tracker for when to check for skipping commands. ~0.5 seconds in a row of being ahead will start skipping.
static int	SkipCommandAmount = 0;	// Amount of commands to skip. Try and batch skip them all at once since we won't be able to get an update until the full RTT.

static int Net_CountInvasionParticipants();
static int Net_CountInvasionAliveParticipants();
static double Net_GetInvasionSkillMonsterSpeedScale();
static void Net_ApplyInvasionMonsterSkillTuning(AActor* actor);
static int Net_GetInvasionActiveMonsterCap();

static void Net_SetInvasionState(EInvasionState state, int tics, const char* reason = nullptr)
{
	tics = max(tics, 0);
	if (InvasionState == state && InvasionStateTics == tics)
		return;

	const EInvasionState prevState = InvasionState;
	InvasionState = state;
	InvasionStateTics = tics;
	if (!Net_IsInvasionRoundActiveState(state))
		InvasionWipeGraceTics = 0;

	DebugTrace::Markf("invasion", "state %s -> %s tics=%d reason=%s gametic=%d room=%u map=%s",
		Net_InvasionStateName(prevState), Net_InvasionStateName(InvasionState), InvasionStateTics,
		reason != nullptr ? reason : "none", gametic, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	if (Net_InvasionDebugEnabled(1))
	{
		Printf(PRINT_HIGH, "HCDE invasion state: %s -> %s tics=%d reason=%s wave=%d/%d spawned=%d/%d cleared=%d active=%d participants=%d alive=%d gametic=%d map=%s\n",
			Net_InvasionStateName(prevState), Net_InvasionStateName(InvasionState), InvasionStateTics,
			reason != nullptr ? reason : "none",
			InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves,
			InvasionWaveDirector.WaveSpawned, InvasionWaveDirector.WaveBudget,
			InvasionWaveDirector.WaveCleared, int(InvasionWaveDirector.ActiveMonsters.Size()),
			Net_CountInvasionParticipants(), Net_CountInvasionAliveParticipants(), gametic,
			primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	}
}

void D_ProcessEvents(void);
void G_BuildTiccmd(usercmd_t *cmd);
void D_DoAdvanceDemo(void);
void P_ClearPredictionData();

static void RunScript(TArrayView<uint8_t>& stream, AActor *pawn, int snum, int argn, int always);

extern	bool	 advancedemo;

static size_t GetNetBufferSize();
static unsigned Net_PlayableNetworkClientCount();
static bool Net_HasPlayableNetworkClients();
static bool Net_TrySkipCommand(int cmd, TArrayView<uint8_t>& stream);
static bool Net_TrySkipUserCmdMessage(TArrayView<uint8_t>& stream, bool allowImplicitEmptyAtEnd = false);
static bool Net_TrySkipUserCmdMessageWithBoundary(const uint8_t* data, size_t dataSize,
	bool allowImplicitEmptyAtEnd, uint8_t commandTics, uint64_t commandOffsetsSeen,
	uint8_t remainingRecords, size_t& recordBytes);
static void HSendPacket(int client, size_t size);

namespace
{
struct FNetPacketTraceWindow
{
	uint64_t LastLogMS = 0u;
	uint32_t InCount = 0u;
	uint32_t OutCount = 0u;
};

static FNetPacketTraceWindow NetPacketTraceWindow;

static bool Net_ShouldAggregatePacketTrace(uint8_t header)
{
	return (header & (NCMD_LATENCY | NCMD_LATENCYACK)) != 0u
		&& (header & ~(NCMD_LATENCY | NCMD_LATENCYACK | NCMD_COMPRESSED)) == 0u;
}

static void Net_MaybeFlushPacketTraceWindow()
{
	const uint64_t nowMS = I_msTime();
	if (nowMS - NetPacketTraceWindow.LastLogMS < 1000u
		&& NetPacketTraceWindow.InCount + NetPacketTraceWindow.OutCount < 32u)
		return;

	if (NetPacketTraceWindow.InCount > 0u)
	{
		DebugTrace::Debugf("net.in", "aggregate packets=%u gametic=%d room=%u",
			unsigned(NetPacketTraceWindow.InCount), gametic, unsigned(CurrentRoomID));
		NetPacketTraceWindow.InCount = 0u;
	}
	if (NetPacketTraceWindow.OutCount > 0u)
	{
		DebugTrace::Debugf("net.out", "aggregate packets=%u gametic=%d room=%u",
			unsigned(NetPacketTraceWindow.OutCount), gametic, unsigned(CurrentRoomID));
		NetPacketTraceWindow.OutCount = 0u;
	}
	NetPacketTraceWindow.LastLogMS = nowMS;
}

static void Net_TraceIncomingPacket(int clientNum, uint8_t header, size_t size)
{
	if (Net_ShouldAggregatePacketTrace(header))
	{
		++NetPacketTraceWindow.InCount;
		Net_MaybeFlushPacketTraceWindow();
		return;
	}

	DebugTrace::Infof("net.in", "client=%d flags=0x%02x size=%zu gametic=%d clienttic=%d room=%u",
		clientNum, unsigned(header), size, gametic, ClientTic, unsigned(CurrentRoomID));
}

static void Net_TraceOutgoingPacket(int client, uint8_t header, size_t size)
{
	if (client == consoleplayer)
		return;

	if (Net_ShouldAggregatePacketTrace(header))
	{
		++NetPacketTraceWindow.OutCount;
		Net_MaybeFlushPacketTraceWindow();
		return;
	}

	DebugTrace::Infof("net.out", "client=%d flags=0x%02x size=%zu gametic=%d room=%u",
		client, unsigned(header), size, gametic, unsigned(CurrentRoomID));
}
}

static uint32_t HCDELiveReadBE32(const uint8_t* data)
{
	return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

static uint16_t HCDELiveReadBE16(const uint8_t* data)
{
	return (uint16_t(data[0]) << 8) | uint16_t(data[1]);
}

static uint64_t HCDELiveReadBE64(const uint8_t* data)
{
	return (uint64_t(data[0]) << 56) | (uint64_t(data[1]) << 48) | (uint64_t(data[2]) << 40) | (uint64_t(data[3]) << 32)
		| (uint64_t(data[4]) << 24) | (uint64_t(data[5]) << 16) | (uint64_t(data[6]) << 8) | uint64_t(data[7]);
}

static void HCDELiveWriteBE16(uint8_t* data, uint16_t value)
{
	data[0] = uint8_t(value >> 8);
	data[1] = uint8_t(value);
}

static void HCDELiveWriteBE32(uint8_t* data, uint32_t value)
{
	data[0] = uint8_t(value >> 24);
	data[1] = uint8_t(value >> 16);
	data[2] = uint8_t(value >> 8);
	data[3] = uint8_t(value);
}

static void HCDELiveWriteBE64(uint8_t* data, uint64_t value)
{
	data[0] = uint8_t(value >> 56);
	data[1] = uint8_t(value >> 48);
	data[2] = uint8_t(value >> 40);
	data[3] = uint8_t(value >> 32);
	data[4] = uint8_t(value >> 24);
	data[5] = uint8_t(value >> 16);
	data[6] = uint8_t(value >> 8);
	data[7] = uint8_t(value);
}

static const char* HCDELiveMessageName(uint8_t type)
{
	switch (type)
	{
	case HLIVE_CONTROL:
		return "control";
	case HLIVE_CLIENT_COMMANDS:
		return "client commands";
	case HLIVE_SERVER_SNAPSHOT:
		return "server snapshot";
	default:
		return "unknown";
	}
}

struct FHCDELiveCapabilityName
{
	uint64_t Mask;
	const char* Name;
};

static const FHCDELiveCapabilityName HCDELiveCapabilityNames[] =
{
	{ HCDELiveCapControlV1, "control-v1" },
	{ HCDELiveCapClientInputV5, "client-input-v5" },
	{ HCDELiveCapServerSnapshotV4, "server-snapshot-v4" },
	{ HCDELiveCapServerWorldDeltaV2, "server-world-delta-v2" },
	{ HCDELiveCapInvasionSnapshotV2, "invasion-snapshot-v2" },
	{ HCDELiveCapActorRegistryV1, "actor-registry-v1" },
	{ HCDELiveCapActorDeltaV2, "actor-delta-v2" },
	{ HCDELiveCapLaneBudgetsV1, "lane-budgets-v1" },
	{ HCDELiveCapAuthorityEventsV1, "authority-events-v1" },
};

static uint64_t HCDELiveLocalCapabilities()
{
	// Only advertise features that are safe to use today. Future protocol bits
	// are named above, but stay clear until their implementation lands.
	return HCDELiveCapControlV1
		| HCDELiveCapClientInputV5
		| HCDELiveCapServerSnapshotV4
		| HCDELiveCapServerWorldDeltaV2
		| HCDELiveCapInvasionSnapshotV2
		| HCDELiveCapActorRegistryV1
		| HCDELiveCapActorDeltaV2
		| HCDELiveCapLaneBudgetsV1
		| HCDELiveCapAuthorityEventsV1;
}

static uint64_t HCDELiveNegotiatedCapabilities(int client)
{
	if (client < 0 || client >= MAXPLAYERS)
		return 0u;

	return HCDELivePeers[client].NegotiatedCapabilities;
}

static bool HCDELivePeerHasCapability(int client, uint64_t capability)
{
	return (HCDELiveNegotiatedCapabilities(client) & capability) == capability;
}

static uint64_t HCDELiveRequiredGameplayCapabilities()
{
	return HCDELiveLocalCapabilities();
}

static bool HCDELivePeerHasRequiredGameplayCapabilities(int client)
{
	const uint64_t required = HCDELiveRequiredGameplayCapabilities();
	return (HCDELiveNegotiatedCapabilities(client) & required) == required;
}

static bool HCDEEnforcesNativeGameplayForPeer(int client)
{
	return netgame
		&& !demoplayback
		&& net_hcde_native_only
		&& client >= 0
		&& client < MAXPLAYERS
		&& client != consoleplayer;
}

// Reject classic NCMD gameplay traffic when the session is configured to require
// native HCDE gameplay. This intentionally does not touch setup/latency/level-start
// traffic; callers invoke it only at live gameplay send/receive boundaries.
static void HCDERejectLegacyGameplayPeer(int client, const char* direction, const char* reason)
{
	if (client < 0 || client >= MAXPLAYERS)
		return;

	auto& peer = HCDELivePeers[client];
	auto& state = ClientStates[client];
	++peer.UnsupportedReceived;
	++HCDELiveProfile.LiveLegacyGameplayRejected;
	DebugTrace::Warningf("net",
		"rejected legacy gameplay %s client=%d reason=%s hcde=%d negotiated=0x%llx required=0x%llx seq=%d ack=%d room=%u native-only",
		direction != nullptr ? direction : "gameplay",
		client,
		reason != nullptr ? reason : "unknown",
		I_ClientUsesHCDEService(client) ? 1 : 0,
		static_cast<unsigned long long>(peer.NegotiatedCapabilities),
		static_cast<unsigned long long>(HCDELiveRequiredGameplayCapabilities()),
		state.CurrentSequence,
		state.SequenceAck,
		unsigned(CurrentRoomID));

	if (I_IsLocalHCDEServiceAuthority() && !I_IsHCDEServiceAuthoritySlot(client))
	{
		Printf(PRINT_HIGH,
			"NetGame:: Disconnecting client %d '%s' for legacy gameplay while HCDE native-only mode is enabled (reason=%s)\n",
			client, players[client].userinfo.GetName(), reason != nullptr ? reason : "unknown");
		state.Flags |= CF_QUIT;
	}
}

// Record + trace a capability-gated rejection of a native HCDE gameplay packet.
// Used by both the receive (HandleHCDELivePacket) and send (HSendNative*Packet,
// HSendLiveGameplayPacket) paths so a peer that hasn't completed the HCDE live
// capability handshake never speaks native gameplay traffic. `inbound` increments
// the per-peer UnsupportedReceived diagnostic so capability flaps show up next to
// other malformed-packet counters.
static void HCDERejectLiveGameplayForCapabilities(int client, EHCDELiveMessage type, const char* direction, bool inbound)
{
	if (client < 0 || client >= MAXPLAYERS)
		return;

	auto& peer = HCDELivePeers[client];
	if (inbound)
		++peer.UnsupportedReceived;
	++HCDELiveProfile.LiveNativeCapabilityRejects;
	DebugTrace::Warningf("net",
		"rejected HCDE live %s %s client=%d negotiated=0x%llx required=0x%llx native-only",
		HCDELiveMessageName(uint8_t(type)), direction != nullptr ? direction : "gameplay",
		client,
		static_cast<unsigned long long>(peer.NegotiatedCapabilities),
		static_cast<unsigned long long>(HCDELiveRequiredGameplayCapabilities()));
}

static size_t HCDELiveLaneDefaultBudgetBytes(uint8_t lane)
{
	switch (EHCDELiveLane(lane))
	{
	case HLANE_CONTROL:
		return HCDELaneBudgetControlBytes;
	case HLANE_COMMAND:
		return HCDELaneBudgetCommandBytes;
	case HLANE_AUTHORITY:
		return HCDELaneBudgetAuthorityBytes;
	case HLANE_PLAYER_SNAPSHOT:
		return HCDELaneBudgetPlayerSnapshotBytes;
	case HLANE_ACTOR_DELTA:
		return HCDELaneBudgetActorDeltaBytes;
	case HLANE_QUERY_REGISTRY:
		return HCDELaneBudgetQueryRegistryBytes;
	case HLANE_PRESENTATION_ECHO:
		return HCDELaneBudgetPresentationEchoBytes;
	default:
		return 0u;
	}
}

static size_t HCDELiveLaneBudgetBytes(int client, uint8_t lane)
{
	if (!HCDELivePeerHasCapability(client, HCDELiveCapLaneBudgetsV1))
		return 0u;
	return HCDELiveLaneDefaultBudgetBytes(lane);
}

static size_t HCDELiveLaneBudgetEnd(int client, uint8_t lane, size_t cursor, size_t outputCapacity)
{
	const size_t budget = HCDELiveLaneBudgetBytes(client, lane);
	if (budget == 0u || cursor >= outputCapacity)
		return outputCapacity;

	const size_t available = outputCapacity - cursor;
	const size_t cappedAvailable = min(available, budget);
	const size_t budgetEnd = cursor + cappedAvailable;
	if (budgetEnd < outputCapacity)
		HCDERecordLiveLaneBudgetClamp(lane, client);
	return budgetEnd;
}

static void HCDEPrintLiveCapabilityNames(uint64_t capabilities, uint64_t unsupportedBits)
{
	bool any = false;
	for (const auto& capability : HCDELiveCapabilityNames)
	{
		if ((capabilities & capability.Mask) != 0u)
		{
			Printf(PRINT_HIGH, "%s%s", any ? " " : "", capability.Name);
			any = true;
		}
	}
	if (unsupportedBits != 0u)
	{
		Printf(PRINT_HIGH, "%sunknown-0x%llx", any ? " " : "", static_cast<unsigned long long>(unsupportedBits));
		any = true;
	}
	if (!any)
		Printf(PRINT_HIGH, "none");
	Printf(PRINT_HIGH, "\n");
}

// Serialize the local capability advertisement and (optionally) a 32-bit
// session ID into the live control packet payload. Layout:
//   [0..3]  HCAP magic
//   [4]     version byte (must equal HCDELiveControlCapabilitiesVersion)
//   [5]     flags byte (HCDELiveControlCapFlagSessionId when session ID follows)
//   [6..13] big-endian 64-bit capability mask
//   [14..17] big-endian 32-bit local DebugTrace session ID (when flag is set)
// Older HCDE peers omit bytes [14..17] entirely; the parser MUST stay tolerant
// of that legacy layout, so we only emit the session ID when our diagnostic
// session has actually allocated one.
static void HCDEAppendLiveControlCapabilities(uint8_t* output, size_t payloadOffset)
{
	memcpy(&output[payloadOffset + HCDELiveControlCapabilitiesMagicOffset], HCDELiveControlCapabilitiesMagic, sizeof(HCDELiveControlCapabilitiesMagic));
	output[payloadOffset + HCDELiveControlCapabilitiesVersionOffset] = HCDELiveControlCapabilitiesVersion;
	output[payloadOffset + HCDELiveControlCapabilitiesFlagsOffset] = HCDELiveControlCapFlagSessionId;
	HCDELiveWriteBE64(&output[payloadOffset + HCDELiveControlCapabilitiesMaskOffset], HCDELiveLocalCapabilities());
	HCDELiveWriteBE32(&output[payloadOffset + HCDELiveControlCapabilitiesMaskOffset + 8u], DebugTrace::GetSessionId());
}

// Parse the capability extension that arrived inside a live control packet
// and update peer-local mirrors. Two outcomes are possible:
//   1. Payload is missing or too short -> treat as a legacy v0 control packet
//      (no negotiated capabilities).
//   2. Payload is present but the capability magic, version, or flags byte is
//      unrecognized -> drop the entire capability block defensively.
static void HCDEApplyLiveControlCapabilities(int client, size_t payloadSize)
{
	auto& peer = HCDELivePeers[client];
	// Accept either the minimum 14-byte capability block (legacy diag-less peers)
	// or the extended 18-byte form that includes the session ID. Anything shorter
	// or with a missing/garbled HCAP magic is treated as an old v0 keepalive.
	if (payloadSize < HCDELiveControlMinPayloadSize
		|| memcmp(&NetBuffer[HCDELiveHeaderSize + HCDELiveControlCapabilitiesMagicOffset], HCDELiveControlCapabilitiesMagic, sizeof(HCDELiveControlCapabilitiesMagic)) != 0)
	{
		peer.RemoteCapabilities = 0u;
		peer.NegotiatedCapabilities = 0u;
		peer.UnsupportedCapabilities = 0u;
		++peer.LegacyControlReceived;
		++HCDELiveProfile.LegacyControlsReceived;
		return;
	}

	const uint8_t capabilityVersion = NetBuffer[HCDELiveHeaderSize + HCDELiveControlCapabilitiesVersionOffset];
	const uint8_t capabilityFlags = NetBuffer[HCDELiveHeaderSize + HCDELiveControlCapabilitiesFlagsOffset];
	if (capabilityVersion == 0u || capabilityVersion > HCDELiveControlCapabilitiesVersion
		|| (capabilityFlags & ~HCDELiveControlCapFlagSessionId) != 0u)
	{
		peer.RemoteCapabilities = 0u;
		peer.NegotiatedCapabilities = 0u;
		peer.UnsupportedCapabilities = 0u;
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE live capabilities from client=%d version=%u flags=0x%x",
			client, unsigned(capabilityVersion), unsigned(capabilityFlags));
		return;
	}

	const uint64_t previousNegotiated = peer.NegotiatedCapabilities;
	peer.RemoteCapabilities = HCDELiveReadBE64(&NetBuffer[HCDELiveHeaderSize + HCDELiveControlCapabilitiesMaskOffset]);
	peer.UnsupportedCapabilities = peer.RemoteCapabilities & ~HCDELiveKnownCapabilityMask;
	peer.NegotiatedCapabilities = peer.RemoteCapabilities & HCDELiveLocalCapabilities();
	++peer.CapabilityControlReceived;
	++HCDELiveProfile.CapabilityControlsReceived;
	if (previousNegotiated != peer.NegotiatedCapabilities)
	{
		DebugTrace::Markf("net.cap", "HCDE live capabilities client=%d remote=0x%llx negotiated=0x%llx",
			client,
			static_cast<unsigned long long>(peer.RemoteCapabilities),
			static_cast<unsigned long long>(peer.NegotiatedCapabilities));
	}
	if ((capabilityFlags & HCDELiveControlCapFlagSessionId) != 0u
		&& payloadSize >= HCDELiveControlPayloadSize)
	{
		const uint32_t remoteSessionId = HCDELiveReadBE32(&NetBuffer[HCDELiveHeaderSize + HCDELiveControlCapabilitiesMaskOffset + 8u]);
		Net_DiagPeerCorrelate(client, remoteSessionId, peer.NegotiatedCapabilities);
	}
	if (peer.UnsupportedCapabilities != 0u)
	{
		DebugTrace::Markf("net", "HCDE live control from client=%d advertised unknown capabilities=0x%llx",
			client, static_cast<unsigned long long>(peer.UnsupportedCapabilities));
	}
}

static bool HCDELiveBufferLooksLikePacket(const uint8_t* data, size_t length)
{
	return length >= HCDELiveHeaderSize
		&& data[0] == 0u
		&& memcmp(&data[HCDELiveMagicOffset], HCDELiveMagic, sizeof(HCDELiveMagic)) == 0;
}

static bool HCDELiveLooksLikePacket()
{
	return HCDELiveBufferLooksLikePacket(NetBuffer, NetBufferLength);
}

static bool HCDELiveReportIntervalElapsed(uint64_t& lastMS, uint64_t intervalMS)
{
	const uint64_t now = I_msTime();
	if (lastMS != 0u && now - lastMS < intervalMS)
		return false;

	lastMS = now;
	return true;
}

// HCDEPassiveResendShouldFire decides whether to issue a passive (soft) resend
// for the given client based on whether their SequenceAck has actually
// stalled. The function is *not* pure: it mutates the per-client trackers
// every time it is called, and on a true return it stamps the firing gametic
// into HCDEPassiveResendLastRequestTic so the retry cooldown kicks in. Callers
// must therefore avoid invoking it in conditions that get short-circuited by
// the surrounding logic - any call site is treated as "we are about to fire a
// resend iff this returns true".
//
// A dedicated client with prediction lead can report an ack that is
// structurally several tics behind the producer even on localhost. That is
// healthy as long as the ack keeps moving. Passive resend should therefore
// detect a *stuck* ack, not merely a trailing one, or large maps/mods with
// heavier snapshot payloads will resend every few tics forever.
static bool HCDEPassiveResendShouldFire(int clientNum, int sequenceAck)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS || sequenceAck < 0)
		return false;

	// Ack advanced (or we have never observed it before) - reset the staleness
	// timer and the retry cooldown, and report not-stuck for this tic.
	if (HCDEPassiveResendLastSequenceAck[clientNum] != sequenceAck
		|| HCDEPassiveResendAckChangedTic[clientNum] == 0)
	{
		HCDEPassiveResendLastSequenceAck[clientNum] = sequenceAck;
		HCDEPassiveResendAckChangedTic[clientNum] = gametic;
		HCDEPassiveResendLastRequestTic[clientNum] = 0;
		return false;
	}

	// Same ack as last call. Wait for it to stay parked for the staleness
	// window before considering this a real stall.
	if (gametic - HCDEPassiveResendAckChangedTic[clientNum] < HCDEPassiveResendAckStaleTics)
		return false;
	// Already fired a resend recently; respect the cooldown.
	if (HCDEPassiveResendLastRequestTic[clientNum] != 0
		&& gametic - HCDEPassiveResendLastRequestTic[clientNum] < HCDEPassiveResendRetryTics)
	{
		return false;
	}

	HCDEPassiveResendLastRequestTic[clientNum] = gametic;
	return true;
}

static uint32_t& HCDELiveReceiveSequenceForType(FHCDELivePeerState& peer, uint8_t type)
{
	// Control pings, client commands, and server snapshots can arrive out of
	// order relative to one another. Keep receive ordering per lane so a harmless
	// control ping can never make the client discard an older gameplay snapshot.
	switch (EHCDELiveMessage(type))
	{
	case HLIVE_CONTROL:
		return peer.RxControlSequence;
	case HLIVE_CLIENT_COMMANDS:
		return peer.RxClientCommandSequence;
	case HLIVE_SERVER_SNAPSHOT:
		return peer.RxServerSnapshotSequence;
	default:
		return peer.RxSequence;
	}
}

static size_t BeginHCDELivePacket(int client, EHCDELiveMessage type)
{
	auto& peer = HCDELivePeers[client];
	++peer.TxSequence;
	if (peer.TxSequence == 0u)
		++peer.TxSequence;

	NetBuffer[0] = 0u;
	memcpy(&NetBuffer[HCDELiveMagicOffset], HCDELiveMagic, sizeof(HCDELiveMagic));
	NetBuffer[HCDELiveVersionOffset] = HCDELiveProtocolVersion;
	NetBuffer[HCDELiveTypeOffset] = uint8_t(type);
	HCDELiveWriteBE32(&NetBuffer[HCDELiveTxSequenceOffset], peer.TxSequence);
	HCDELiveWriteBE32(&NetBuffer[HCDELiveAckOffset], peer.RxSequence);
	return HCDELiveHeaderSize;
}

static bool HCDELiveSequenceIsFresh(int clientNum, uint8_t type, uint32_t sequence)
{
	auto& peer = HCDELivePeers[clientNum];
	uint32_t& typedSequence = HCDELiveReceiveSequenceForType(peer, type);
	if (sequence == 0u)
	{
		DebugTrace::Markf("net", "ignored HCDE live %s from client=%d with zero sequence",
			HCDELiveMessageName(type), clientNum);
		return false;
	}

	if (sequence <= typedSequence)
	{
		++peer.DuplicateCount;
		DebugTrace::Markf("net", "ignored duplicate HCDE live %s client=%d seq=%u last-type=%u last-any=%u duplicates=%u",
			HCDELiveMessageName(type), clientNum, sequence, typedSequence, peer.RxSequence, peer.DuplicateCount);
		if ((type == HLIVE_CLIENT_COMMANDS || type == HLIVE_SERVER_SNAPSHOT)
			&& HCDELiveReportIntervalElapsed(LastHCDELiveSequenceRejectReportMS, 2000u))
		{
			Printf(PRINT_HIGH,
				"HCDE live packet stale: type=%s from=%d seq=%u last-type=%u last-any=%u duplicates=%u gametic=%d clienttic=%d\n",
				HCDELiveMessageName(type), clientNum, sequence, typedSequence, peer.RxSequence,
				peer.DuplicateCount, gametic, ClientTic);
		}
		return false;
	}

	return true;
}

static void AcceptHCDELiveSequence(int clientNum, uint8_t type, uint32_t sequence)
{
	auto& peer = HCDELivePeers[clientNum];
	uint32_t& typedSequence = HCDELiveReceiveSequenceForType(peer, type);
	typedSequence = sequence;
	if (sequence > peer.RxSequence)
		peer.RxSequence = sequence;
}

static void ClearHCDELiveReplayPressure(int clientNum, const char* reason)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;

	auto& peer = HCDELivePeers[clientNum];
	if (peer.ReplayRequestStrikes == 0u
		&& peer.ReplayRequestWindowStartMS == 0u
		&& peer.ReplayRequestSuppressedUntilMS == 0u)
	{
		return;
	}

	DebugTrace::Markf("net", "cleared HCDE live replay pressure client=%d reason=%s strikes=%u suppressed-until=%llu room=%u",
		clientNum, reason != nullptr ? reason : "unknown",
		unsigned(peer.ReplayRequestStrikes),
		static_cast<unsigned long long>(peer.ReplayRequestSuppressedUntilMS),
		unsigned(CurrentRoomID));
	peer.ReplayRequestStrikes = 0u;
	peer.ReplayRequestWindowStartMS = 0u;
	peer.ReplayRequestSuppressedUntilMS = 0u;
}

// Native gameplay decode/build failures request retransmit instead of falling back
// to classic NCMD. This guard prevents a persistent native failure from becoming
// an infinite replay loop: repeated requests inside a short window either mark a
// locally-owned remote client for disconnect or, for authority/not-locally-kickable
// peers, temporarily suppress further replay pressure so diagnostics can surface
// the real failure without flooding the peer.
static void RequestHCDELiveReplay(int clientNum, const char* reason)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;

	auto& state = ClientStates[clientNum];
	auto& peer = HCDELivePeers[clientNum];
	const uint64_t now = I_msTime();
	constexpr uint64_t ReplayStrikeWindowMS = 5000u;
	constexpr uint64_t ReplaySuppressionMS = 5000u;
	constexpr uint32_t ReplayStrikeLimit = 6u;

	if (peer.ReplayRequestSuppressedUntilMS != 0u && now < peer.ReplayRequestSuppressedUntilMS)
	{
		++HCDELiveProfile.LiveReplaySuppressions;
		DebugTrace::Warningf("net",
			"suppressed HCDE live replay request client=%d reason=%s strikes=%u suppress-ms-left=%llu seq=%d ack=%d room=%u",
			clientNum, reason != nullptr ? reason : "unknown",
			unsigned(peer.ReplayRequestStrikes),
			static_cast<unsigned long long>(peer.ReplayRequestSuppressedUntilMS - now),
			state.CurrentSequence, state.SequenceAck, unsigned(CurrentRoomID));
		return;
	}

	if (peer.ReplayRequestWindowStartMS == 0u || now < peer.ReplayRequestWindowStartMS
		|| now - peer.ReplayRequestWindowStartMS > ReplayStrikeWindowMS)
	{
		peer.ReplayRequestWindowStartMS = now;
		peer.ReplayRequestStrikes = 0u;
		peer.ReplayRequestSuppressedUntilMS = 0u;
	}

	++peer.ReplayRequestStrikes;
	++HCDELiveProfile.LiveReplayRequests;
	if (peer.ReplayRequestStrikes >= ReplayStrikeLimit)
	{
		++HCDELiveProfile.LiveReplayEscalations;
		if (I_IsLocalHCDEServiceAuthority() && !I_IsHCDEServiceAuthoritySlot(clientNum))
		{
			Printf(PRINT_HIGH,
				"NetGame:: Disconnecting client %d '%s' for repeated HCDE live replay requests reason=%s strikes=%u at gametic=%d clienttic=%d room=%u\n",
				clientNum, players[clientNum].userinfo.GetName(), reason != nullptr ? reason : "unknown",
				unsigned(peer.ReplayRequestStrikes), gametic, ClientTic, unsigned(CurrentRoomID));
			DebugTrace::Warningf("net",
				"disconnecting replay-loop source client=%d name=%s reason=%s strikes=%u seq=%d ack=%d rx-snapshot=%u rx-command=%u room=%u",
				clientNum, players[clientNum].userinfo.GetName(), reason != nullptr ? reason : "unknown",
				unsigned(peer.ReplayRequestStrikes), state.CurrentSequence, state.SequenceAck,
				peer.RxServerSnapshotSequence, peer.RxClientCommandSequence, unsigned(CurrentRoomID));
			state.Flags |= CF_QUIT;
			peer.ReplayRequestStrikes = 0u;
			peer.ReplayRequestWindowStartMS = now;
			peer.ReplayRequestSuppressedUntilMS = 0u;
			return;
		}

		peer.ReplayRequestSuppressedUntilMS = now + ReplaySuppressionMS;
		DebugTrace::Warningf("net",
			"HCDE live replay strike limit reached but peer is authority/not locally kickable client=%d reason=%s strikes=%u suppress-ms=%llu seq=%d ack=%d rx-snapshot=%u rx-command=%u room=%u",
			clientNum, reason != nullptr ? reason : "unknown", unsigned(peer.ReplayRequestStrikes),
			static_cast<unsigned long long>(ReplaySuppressionMS), state.CurrentSequence, state.SequenceAck,
			peer.RxServerSnapshotSequence, peer.RxClientCommandSequence, unsigned(CurrentRoomID));
		return;
	}

	state.Flags |= CF_MISSING_SEQ;
	DebugTrace::Warningf("net", "requested HCDE live replay client=%d reason=%s strikes=%u window-ms=%llu seq=%d ack=%d rx-snapshot=%u rx-command=%u room=%u",
		clientNum,
		reason != nullptr ? reason : "unknown",
		unsigned(peer.ReplayRequestStrikes),
		static_cast<unsigned long long>(now - peer.ReplayRequestWindowStartMS),
		state.CurrentSequence,
		state.SequenceAck,
		peer.RxServerSnapshotSequence,
		peer.RxClientCommandSequence,
		unsigned(CurrentRoomID));
}

static bool ShouldWrapHCDEClientCommandPacket(int client)
{
	return netgame
		&& !demoplayback
		&& I_ShouldSendHCDELiveClientInputTo(client);
}

static bool ShouldWrapHCDEServerSnapshotPacket(int client)
{
	return netgame
		&& !demoplayback
		&& I_ShouldSendHCDELiveServerSnapshotTo(client);
}

static bool HCDEEnforcesDedicatedInputAuthority()
{
	return netgame
		&& !demoplayback
		&& I_IsLocalHCDELiveAuthority()
		&& I_UsesDedicatedServerSlot();
}

static bool HCDEInputRecordAuthorized(int senderClient, int playerNum, uint8_t playerCount)
{
	if (!HCDEEnforcesDedicatedInputAuthority())
		return true;

	const bool authorized = playerCount <= 1u && playerNum == senderClient;
	if (!authorized)
	{
		Net_DiagTraceInputAuthority(senderClient, "reject-unauthorized-player",
			FStringf("player=%d count=%u", playerNum, unsigned(playerCount)).GetChars());
	}
	return authorized;
}

static bool HCDEWorldDeltasCanMutatePlaysim()
{
	// Dedicated-slot sessions are server-authoritative. Let clients accept the
	// validated pose stream so small prediction drift is repaired before the
	// consistency checker evaluates the next tic.
	return I_UsesDedicatedServerSlot();
}

static bool HCDEUsesNonPlayerServerAuthority()
{
	return I_IsDedicatedServerMode() && I_IsServerReservedSlot(I_GetHCDEServiceAuthoritySlot());
}

static bool HCDEUsesNonPlayerServerAuthoritySlot(int authoritySlot)
{
	return I_IsDedicatedServerMode() && I_IsServerReservedSlot(authoritySlot);
}

static const char* HCDEAuthorityDisplayName()
{
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	return HCDEUsesNonPlayerServerAuthority()
		? HCDE_ServerMode_GetAuthorityName()
		: players[authoritySlot].userinfo.GetName();
}

static void HCDESetAuthoritySettingsController(bool enabled)
{
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	if (HCDEUsesNonPlayerServerAuthority())
	{
		HCDE_ServerMode_SetAuthoritySettingsController(enabled);
		players[authoritySlot].settings_controller = false;
		return;
	}

	players[authoritySlot].settings_controller = enabled;
}

static void HCDESetAuthorityWaiting(bool waiting)
{
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	if (HCDEUsesNonPlayerServerAuthority())
	{
		HCDE_ServerMode_SetAuthorityWaiting(waiting);
		players[authoritySlot].waiting = false;
		return;
	}

	players[authoritySlot].waiting = waiting;
}

static void HCDEInitializeAuthoritySession()
{
	const int authoritySlot = Net_Arbitrator;
	const bool nonPlayerAuthority = HCDEUsesNonPlayerServerAuthoritySlot(authoritySlot);
	const char* authorityName = nonPlayerAuthority ? "HCDE dedicated server" : players[authoritySlot].userinfo.GetName();
	HCDE_ServerMode_SetAuthorityState(authoritySlot, !nonPlayerAuthority, true, false, authorityName);
	HCDESetAuthoritySettingsController(true);
}

bool Net_LocalCanControlSettings()
{
	if (!netgame)
		return true;

	if (I_IsDedicatedServerMode() && I_IsLocalHCDEServiceAuthority())
		return HCDE_ServerMode_AuthorityCanControlSettings();

	if (consoleplayer < 0 || consoleplayer >= MAXPLAYERS)
		return false;

	return players[consoleplayer].settings_controller;
}

static const char* HCDEGameplayPayloadName(uint8_t kind)
{
	switch (kind)
	{
	case HGP_RESERVED_LEGACY_CLIENT_COMMANDS:
		return "reserved legacy client commands";
	case HGP_RESERVED_LEGACY_SERVER_SNAPSHOT:
		return "reserved legacy server snapshot";
	case HGP_CLIENT_INPUTS:
		return "client inputs";
	case HGP_SERVER_SNAPSHOT:
		return "server snapshot";
	default:
		return "unknown";
	}
}

static void WriteHCDEGameplayEnvelope(uint8_t* data, EHCDEGameplayPayload kind)
{
	memcpy(&data[HCDEGameplayMagicOffset], HCDEGameplayMagic, sizeof(HCDEGameplayMagic));
	data[HCDEGameplayVersionOffset] = HCDEGameplayProtocolVersion;
	data[HCDEGameplayKindOffset] = uint8_t(kind);
	data[HCDEGameplayRoomOffset] = CurrentRoomID;
	data[HCDEGameplayFlagsOffset] = 0u;
	HCDELiveWriteBE32(&data[HCDEGameplayTicOffset], uint32_t(max<int>(gametic, 0)));
}

static bool UnwrapHCDEGameplayEnvelope(int clientNum, size_t payloadSize, const char* label, EHCDEGameplayPayload expectedKind, const uint8_t*& payload, size_t& gameplayPayloadSize, uint32_t& remoteGameTic)
{
	auto& peer = HCDELivePeers[clientNum];
	payload = nullptr;
	gameplayPayloadSize = 0u;
	remoteGameTic = 0u;

	if (payloadSize < HCDEGameplayHeaderSize || payloadSize > MAX_MSGLEN)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "malformed HCDE live %s payload from client=%d size=%zu",
			label, clientNum, payloadSize);
		return false;
	}

	const uint8_t* envelope = &NetBuffer[HCDELiveHeaderSize];
	if (memcmp(&envelope[HCDEGameplayMagicOffset], HCDEGameplayMagic, sizeof(HCDEGameplayMagic)) != 0)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE live %s without gameplay envelope from client=%d",
			label, clientNum);
		return false;
	}

	const uint8_t gameplayVersion = envelope[HCDEGameplayVersionOffset];
	const uint8_t gameplayKind = envelope[HCDEGameplayKindOffset];
	const uint8_t room = envelope[HCDEGameplayRoomOffset];
	const uint8_t flags = envelope[HCDEGameplayFlagsOffset];
	remoteGameTic = HCDELiveReadBE32(&envelope[HCDEGameplayTicOffset]);
	if (gameplayVersion != HCDEGameplayProtocolVersion || gameplayKind != uint8_t(expectedKind) || flags != 0u)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE live %s envelope from client=%d version=%u kind=%s flags=%u",
			label, clientNum, unsigned(gameplayVersion), HCDEGameplayPayloadName(gameplayKind), unsigned(flags));
		return false;
	}

	if (room != CurrentRoomID)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored stale HCDE live %s envelope from client=%d room=%u current=%u tic=%u",
			label, clientNum, unsigned(room), unsigned(CurrentRoomID), remoteGameTic);

		// Cutscene/intermission deadlock recovery for non-authority peers.
		//
		// In coop, the cutscene-end transition is driven by the authority writing
		// DEM_ENDSCREENJOB into its NetEvents stream during the same gametic that
		// `Net_AdvanceCutscene` fires. Each peer is supposed to deserialize that
		// DEM byte from the next snapshot and run `EndScreenJob` locally so all
		// peers walk through `G_DoWorldDone` together (which is what bumps
		// `CurrentRoomID`).
		//
		// What we have observed in the field is that the authority can advance
		// CurrentRoomID on its own (via its local Net_RunCommand pass) before the
		// snapshot that contains the DEM byte actually lands on the client - the
		// snapshot send window can lag the playsim by a tic, and the per-tic
		// NetEvents bucket the byte ends up in might never be replayed to the
		// client because the next packet from the authority is already tagged
		// with room=N+1 and gets rejected here as "stale". The result is a
		// mutual deadlock:
		//   * authority is in room=N+1 (MAP02), waiting for the client's
		//     NCMD_LEVELREADY to release the level start;
		//   * client is in room=N (cutscene), rejecting every authority envelope
		//     it sees, so it never receives DEM_ENDSCREENJOB and never moves on;
		//   * client never reaches `IsMapLoaded() == true`, so it never sends
		//     NCMD_LEVELREADY back; the server's `TryReleaseLevelStart` watchdog
		//     stays armed for many seconds before falling back to a forced
		//     advance, well past the point the user has given up and quit.
		//
		// When the authority is exactly one room ahead and we are still parked
		// on a cutscene, treat it as a definitive signal that the authority has
		// already crossed the world-done boundary and run `EndScreenJob`
		// locally. The completion lambda queues `ga_worlddone`, the next
		// G_Ticker pass calls G_DoWorldDone -> Net_ResetCommands -> CurrentRoomID
		// catches up to the authority, and the normal NCMD_LEVELREADY
		// handshake takes over. Throttle by wall clock (not gametic) so a
		// recovery attempt still fires promptly after a map transition even
		// when gametic resets to a small value while LastForceCutsceneEndTic
		// still holds a large tic from the previous map.
		const uint8_t expectedNext = static_cast<uint8_t>(CurrentRoomID + 1u);
		const bool authorityIsAhead = (room == expectedNext) && I_IsHCDEServiceAuthoritySlot(clientNum);
		const bool stuckInCutscene = (gamestate == GS_CUTSCENE || gamestate == GS_INTRO)
			&& cutscene.runner != nullptr;
		if (authorityIsAhead && stuckInCutscene && !I_IsLocalHCDEServiceAuthority())
		{
			static uint64_t LastForceCutsceneEndMS = 0u;
			const uint64_t nowMS = I_msTime();
			if (LastForceCutsceneEndMS == 0u || nowMS - LastForceCutsceneEndMS >= 1000u)
			{
				LastForceCutsceneEndMS = nowMS;
				DebugTrace::Warningf("net",
					"client room desync recovery: authority (client=%d) is one room ahead "
					"(room=%u local=%u) while we're still on a cutscene (gamestate=%d gametic=%d clienttic=%d). "
					"Forcing local EndScreenJob so the world-done handshake can complete.",
					clientNum, unsigned(room), unsigned(CurrentRoomID),
					int(gamestate), gametic, ClientTic);
				EndScreenJob();
				G_TickStalledCutscene();
			}
		}

		return false;
	}

	payload = &envelope[HCDEGameplayHeaderSize];
	gameplayPayloadSize = payloadSize - HCDEGameplayHeaderSize;
	return true;
}

bool HCDEAppendByte(uint8_t* output, size_t outputCapacity, size_t& cursor, uint8_t value)
{
	if (cursor >= outputCapacity)
	{
		DebugTrace::Markf("net", "HCDE append byte failed: cursor=%zu outputCapacity=%zu reason=cursor_ge_capacity", cursor, outputCapacity);
		return false;
	}
	output[cursor++] = value;
	return true;
}

bool HCDEAppendBE16(uint8_t* output, size_t outputCapacity, size_t& cursor, uint16_t value)
{
	if (cursor >= outputCapacity)
	{
		DebugTrace::Markf("net", "HCDE append BE16 failed: cursor=%zu outputCapacity=%zu required=2 reason=cursor_ge_capacity", cursor, outputCapacity);
		return false;
	}
	if (outputCapacity - cursor < 2u)
	{
		DebugTrace::Markf("net", "HCDE append BE16 failed: cursor=%zu outputCapacity=%zu required=2 reason=insufficient_capacity", cursor, outputCapacity);
		return false;
	}
	HCDELiveWriteBE16(&output[cursor], value);
	cursor += 2u;
	return true;
}

bool HCDEAppendBE32(uint8_t* output, size_t outputCapacity, size_t& cursor, uint32_t value)
{
	if (cursor >= outputCapacity)
	{
		DebugTrace::Markf("net", "HCDE append BE32 failed: cursor=%zu outputCapacity=%zu required=4 reason=cursor_ge_capacity", cursor, outputCapacity);
		return false;
	}
	if (outputCapacity - cursor < 4u)
	{
		DebugTrace::Markf("net", "HCDE append BE32 failed: cursor=%zu outputCapacity=%zu required=4 reason=insufficient_capacity", cursor, outputCapacity);
		return false;
	}
	HCDELiveWriteBE32(&output[cursor], value);
	cursor += 4u;
	return true;
}

static bool HCDEAppendBE64(uint8_t* output, size_t outputCapacity, size_t& cursor, uint64_t value)
{
	if (cursor >= outputCapacity)
	{
		DebugTrace::Markf("net", "HCDE append BE64 failed: cursor=%zu outputCapacity=%zu required=8 reason=cursor_ge_capacity", cursor, outputCapacity);
		return false;
	}
	if (outputCapacity - cursor < 8u)
	{
		DebugTrace::Markf("net", "HCDE append BE64 failed: cursor=%zu outputCapacity=%zu required=8 reason=insufficient_capacity", cursor, outputCapacity);
		return false;
	}
	HCDELiveWriteBE64(&output[cursor], value);
	cursor += 8u;
	return true;
}

static bool HCDEAppendDouble(uint8_t* output, size_t outputCapacity, size_t& cursor, double value)
{
	if (isfinite(value) == 0)
	{
		DebugTrace::Markf("net", "HCDE append double failed: cursor=%zu reason=not_finite_value", cursor);
		return false;
	}
	uint64_t bits = 0u;
	memcpy(&bits, &value, sizeof(bits));
	return HCDEAppendBE64(output, outputCapacity, cursor, bits);
}

static bool HCDEAppendFloat(uint8_t* output, size_t outputCapacity, size_t& cursor, double value)
{
	const float compact = static_cast<float>(value);
	if (isfinite(compact) == 0)
	{
		DebugTrace::Markf("net", "HCDE append float failed: cursor=%zu reason=not_finite_value", cursor);
		return false;
	}
	uint32_t bits = 0u;
	memcpy(&bits, &compact, sizeof(bits));
	return HCDEAppendBE32(output, outputCapacity, cursor, bits);
}

bool HCDEAppendBytes(uint8_t* output, size_t outputCapacity, size_t& cursor, const uint8_t* data, size_t size)
{
	if (cursor >= outputCapacity)
	{
		DebugTrace::Markf("net", "HCDE append bytes failed: cursor=%zu outputCapacity=%zu reason=cursor_ge_capacity", cursor, outputCapacity);
		return false;
	}
	if (size > outputCapacity - cursor)
	{
		DebugTrace::Markf("net", "HCDE append bytes failed: cursor=%zu outputCapacity=%zu size=%zu reason=insufficient_capacity", cursor, outputCapacity, size);
		return false;
	}
	if (size != 0u)
		memcpy(&output[cursor], data, size);
	cursor += size;
	return true;
}

bool HCDEReadByteField(const uint8_t* data, size_t dataSize, size_t& cursor, uint8_t& value)
{
	if (cursor >= dataSize)
	{
		DebugTrace::Markf("net", "HCDE read byte failed: cursor=%zu dataSize=%zu", cursor, dataSize);
		return false;
	}
	value = data[cursor++];
	return true;
}

bool HCDEReadBE16Field(const uint8_t* data, size_t dataSize, size_t& cursor, uint16_t& value)
{
	if (cursor >= dataSize)
	{
		DebugTrace::Markf("net", "HCDE read BE16 failed: cursor=%zu dataSize=%zu required=2 reason=cursor_ge_size", cursor, dataSize);
		return false;
	}
	if (dataSize - cursor < 2u)
	{
		DebugTrace::Markf("net", "HCDE read BE16 failed: cursor=%zu dataSize=%zu required=2 reason=insufficient_data", cursor, dataSize);
		return false;
	}
	value = HCDELiveReadBE16(&data[cursor]);
	cursor += 2u;
	return true;
}

bool HCDEReadBE32Field(const uint8_t* data, size_t dataSize, size_t& cursor, uint32_t& value)
{
	if (cursor >= dataSize)
	{
		DebugTrace::Markf("net", "HCDE read BE32 failed: cursor=%zu dataSize=%zu required=4 reason=cursor_ge_size", cursor, dataSize);
		return false;
	}
	if (dataSize - cursor < 4u)
	{
		DebugTrace::Markf("net", "HCDE read BE32 failed: cursor=%zu dataSize=%zu required=4 reason=insufficient_data", cursor, dataSize);
		return false;
	}
	value = HCDELiveReadBE32(&data[cursor]);
	cursor += 4u;
	return true;
}

static bool HCDEReadDoubleField(const uint8_t* data, size_t dataSize, size_t& cursor, double& value)
{
	if (cursor >= dataSize)
	{
		DebugTrace::Markf("net", "HCDE read double failed: cursor=%zu dataSize=%zu required=8 reason=cursor_ge_size", cursor, dataSize);
		return false;
	}
	if (dataSize - cursor < 8u)
	{
		DebugTrace::Markf("net", "HCDE read double failed: cursor=%zu dataSize=%zu required=8 reason=insufficient_data", cursor, dataSize);
		return false;
	}
	const uint64_t bits = HCDELiveReadBE64(&data[cursor]);
	memcpy(&value, &bits, sizeof(value));
	cursor += 8u;
	if (isfinite(value) == 0)
	{
		DebugTrace::Markf("net", "HCDE read double failed: cursor=%zu reason=not_finite_value", cursor);
		return false;
	}
	return true;
}

static bool HCDEReadFloatField(const uint8_t* data, size_t dataSize, size_t& cursor, double& value)
{
	uint32_t bits = 0u;
	if (!HCDEReadBE32Field(data, dataSize, cursor, bits))
		return false;

	float compact = 0.f;
	memcpy(&compact, &bits, sizeof(compact));
	if (isfinite(compact) == 0)
	{
		DebugTrace::Markf("net", "HCDE read float failed: cursor=%zu reason=not_finite_value", cursor);
		return false;
	}
	value = double(compact);
	return true;
}

static int32_t HCDEQuantizeActorDeltaPos(double value)
{
	if (isfinite(value) == 0)
		return 0;

	const int64_t quantized = int64_t(llround(value * HCDEActorDeltaPosScale));
	return int32_t(clamp<int64_t>(quantized, INT32_MIN, INT32_MAX));
}

static int16_t HCDEQuantizeActorDeltaVel(double value)
{
	if (isfinite(value) == 0)
		return 0;

	const int64_t quantized = int64_t(llround(value * HCDEActorDeltaVelScale));
	return int16_t(clamp<int64_t>(quantized, INT16_MIN, INT16_MAX));
}

static double HCDEDequantizeActorDeltaPos(int32_t value)
{
	return double(value) / HCDEActorDeltaPosScale;
}

static double HCDEDequantizeActorDeltaVel(int16_t value)
{
	return double(value) / HCDEActorDeltaVelScale;
}

static uint16_t HCDECompactAngle(uint32_t bam)
{
	return uint16_t(bam >> 16);
}

static uint32_t HCDEExpandCompactAngle(uint16_t compact)
{
	return uint32_t(compact) << 16;
}

static bool HCDEAppendQuantizedPos(uint8_t* output, size_t outputCapacity, size_t& cursor, double value)
{
	return HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(HCDEQuantizeActorDeltaPos(value)));
}

static bool HCDEAppendQuantizedVel(uint8_t* output, size_t outputCapacity, size_t& cursor, double value)
{
	return HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(HCDEQuantizeActorDeltaVel(value)));
}

static bool HCDEReadQuantizedPosField(const uint8_t* data, size_t dataSize, size_t& cursor, double& value)
{
	uint32_t raw = 0u;
	if (!HCDEReadBE32Field(data, dataSize, cursor, raw))
		return false;
	value = HCDEDequantizeActorDeltaPos(int32_t(raw));
	return true;
}

static bool HCDEReadQuantizedVelField(const uint8_t* data, size_t dataSize, size_t& cursor, double& value)
{
	uint16_t raw = 0u;
	if (!HCDEReadBE16Field(data, dataSize, cursor, raw))
		return false;
	value = HCDEDequantizeActorDeltaVel(int16_t(raw));
	return true;
}

static bool HCDEAppendFieldBytes(uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor, size_t size)
{
	if (inputCursor >= dataSize)
	{
		DebugTrace::Markf("net", "HCDE append field failed: inputCursor=%zu dataSize=%zu reason=input_ge_size", inputCursor, dataSize);
		return false;
	}
	if (size > dataSize - inputCursor)
	{
		DebugTrace::Markf("net", "HCDE append field failed: inputCursor=%zu dataSize=%zu size=%zu reason=insufficient_data", inputCursor, dataSize, size);
		return false;
	}
	if (!HCDEAppendBytes(output, outputCapacity, outputCursor, &data[inputCursor], size))
	{
		DebugTrace::Markf("net", "HCDE append field failed: outputCursor=%zu outputCapacity=%zu size=%zu reason=append_failed", outputCursor, outputCapacity, size);
		return false;
	}
	inputCursor += size;
	return true;
}

static bool HCDEIsAllowedTicEventType(uint8_t type);
static bool HCDEAppendCanonicalEventPayload(uint8_t eventType, uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor);

static const usercmd_t* HCDEBuildUserCmdBasis(int playerNum, uint32_t sequence)
{
	if (sequence == 0u)
		return nullptr;

	const uint32_t lastTic = sequence - 1u;
	if (playerNum == consoleplayer)
	{
		const size_t ticDup = TicDup > 0 ? size_t(TicDup) : 0u;
		const size_t realLastTic = (size_t(lastTic) * ticDup) % LOCALCMDTICS;
		return &LocalCmds[realLastTic];
	}

	return playerNum >= 0 && playerNum < MAXPLAYERS ? &ClientStates[playerNum].Tics[lastTic % BACKUPTICS].Command : nullptr;
}

static const usercmd_t* HCDEReceiveUserCmdBasis(int playerNum, uint32_t sequence)
{
	if (sequence == 0u || playerNum < 0 || playerNum >= MAXPLAYERS)
		return nullptr;

	const uint32_t lastTic = sequence - 1u;
	return &ClientStates[playerNum].Tics[lastTic % BACKUPTICS].Command;
}

static bool HCDEAppendUserCmdFields(uint8_t* output, size_t outputCapacity, size_t& cursor, const usercmd_t& command)
{
	return HCDEAppendBE32(output, outputCapacity, cursor, command.buttons)
		&& HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(command.pitch))
		&& HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(command.yaw))
		&& HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(command.roll))
		&& HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(command.forwardmove))
		&& HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(command.sidemove))
		&& HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(command.upmove));
}

static bool HCDEReadUserCmdFields(const uint8_t* data, size_t dataSize, size_t& cursor, usercmd_t& command)
{
	if (cursor >= dataSize)
	{
		DebugTrace::Markf("net", "HCDE read usercmd failed: cursor=%zu dataSize=%zu required=%zu reason=cursor_ge_size", cursor, dataSize, size_t(HCDEExplicitUserCmdBytes));
		return false;
	}
	if (dataSize - cursor < HCDEExplicitUserCmdBytes)
	{
		DebugTrace::Markf("net", "HCDE read usercmd failed: cursor=%zu dataSize=%zu required=%zu reason=insufficient_data", cursor, dataSize, size_t(HCDEExplicitUserCmdBytes));
		return false;
	}

	command.buttons = HCDELiveReadBE32(&data[cursor]);
	cursor += 4u;
	command.pitch = int16_t(HCDELiveReadBE16(&data[cursor]));
	cursor += 2u;
	command.yaw = int16_t(HCDELiveReadBE16(&data[cursor]));
	cursor += 2u;
	command.roll = int16_t(HCDELiveReadBE16(&data[cursor]));
#include "d_net_snapshot_part1.cpp"
static bool HCDEAppendInvasionSnapshot(int clientNum, uint8_t* output, size_t outputCapacity, size_t& cursor)
{
	if (!HCDEIsValidLiveClient(clientNum))
		return false;
	if (!Net_IsInvasionModeEnabled())
		return true;
	if (!HCDELivePeerHasCapability(clientNum, HCDELiveCapInvasionSnapshotV2))
		return true;

	const bool includeActorDeltas = HCDELivePeerHasCapability(clientNum, HCDELiveCapActorDeltaV2)
		&& HCDELivePeerHasCapability(clientNum, HCDELiveCapActorRegistryV1);
	const bool includeAuthorityEvents = HCDELivePeerHasCapability(clientNum, HCDELiveCapAuthorityEventsV1);

	const size_t startCursor = cursor;
	uint8_t flags = 0u;
	if (Net_IsInvasionBossWave())
		flags |= HCDEInvasionSnapshotFlagBossWave;

	uint8_t spawnFlags = 0u;
	if (Net_IsInvasionSpawnUsingFallback())
		spawnFlags |= HCDEInvasionSnapshotSpawnFlagUsingFallback;

	if (!HCDEAppendBytes(output, outputCapacity, cursor, HCDEInvasionSnapshotMagic, sizeof(HCDEInvasionSnapshotMagic))
		|| !HCDEAppendByte(output, outputCapacity, cursor, HCDEInvasionSnapshotProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, cursor, flags)
		|| !HCDEAppendByte(output, outputCapacity, cursor, uint8_t(clamp<int>(Net_GetInvasionState(), INVS_DISABLED, INVS_FAILURE)))
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max(Net_GetInvasionStateTics(), 0)))
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max(Net_GetInvasionWave(), 0)))
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max(Net_GetInvasionMaxWaves(), 0)))
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max(Net_GetInvasionWaveBudget(), 0)))
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max(Net_GetInvasionWaveSpawned(), 0)))
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max(Net_GetInvasionWaveCleared(), 0)))
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max(Net_GetInvasionActiveMonsterCount(), 0)))
		|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(Net_GetInvasionSpawnSpotCount(), 0, 0xffff)))
		|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(Net_GetInvasionActiveSpawnSpotCount(), 0, 0xffff)))
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max(Net_GetInvasionSpawnPlanBudget(), 0)))
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max(Net_GetInvasionSpawnActiveTag(), 0)))
		|| !HCDEAppendByte(output, outputCapacity, cursor, spawnFlags)
		|| !HCDEAppendByte(output, outputCapacity, cursor, uint8_t(clamp<int>(Net_GetInvasionSpawnFallbackSource(), INVSPAWN_NONE, INVSPAWN_PLAYERSTART)))
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u))
	{
		return false;
	}
	HCDERecordLiveLaneTx(HLANE_AUTHORITY, clientNum, cursor - startCursor);
	const size_t invasionPayloadEnd = min(outputCapacity, cursor + HCDEInvasionSnapshotPayloadBudgetBytes);
	if (includeAuthorityEvents)
	{
		const size_t authorityBudgetEnd = HCDELiveLaneBudgetEnd(clientNum, HLANE_AUTHORITY, cursor, invasionPayloadEnd);
		if (!HCDEAppendAuthorityEvents(clientNum, output, authorityBudgetEnd, cursor))
			return false;
	}

	if (includeActorDeltas)
	{
		const size_t actorDeltaBudgetEnd = HCDELiveLaneBudgetEnd(clientNum, HLANE_ACTOR_DELTA, cursor, invasionPayloadEnd);
		if (!HCDEAppendActorDeltasV2(clientNum, output, actorDeltaBudgetEnd, cursor))
			return false;
	}

	++HCDELiveProfile.InvasionSnapshotPacketsBuilt;
	HCDELiveProfile.InvasionSnapshotBytesBuilt += cursor - startCursor;
	return true;
}

static bool HCDEApplyInvasionSnapshot(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& bodyCursor)
{
	if (!HCDEIsValidLiveClient(clientNum))
		return false;
	if (!HCDELivePeerHasCapability(clientNum, HCDELiveCapInvasionSnapshotV2))
		return false;
	const bool expectActorDeltas = HCDELivePeerHasCapability(clientNum, HCDELiveCapActorDeltaV2)
		&& HCDELivePeerHasCapability(clientNum, HCDELiveCapActorRegistryV1);
	const bool expectAuthorityEvents = HCDELivePeerHasCapability(clientNum, HCDELiveCapAuthorityEventsV1);

	if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < HCDEInvasionSnapshotHeaderV1Size)
		return false;
	if (memcmp(&body[bodyCursor + HCDEInvasionSnapshotMagicOffset], HCDEInvasionSnapshotMagic, sizeof(HCDEInvasionSnapshotMagic)) != 0)
		return false;

	const size_t startCursor = bodyCursor;
	const uint8_t version = body[bodyCursor + HCDEInvasionSnapshotVersionOffset];
	const uint8_t flags = body[bodyCursor + HCDEInvasionSnapshotFlagsOffset];
	const uint8_t stateRaw = body[bodyCursor + HCDEInvasionSnapshotStateOffset];
	const uint8_t reserved = body[bodyCursor + HCDEInvasionSnapshotReservedOffset];
	if (!Net_IsInvasionModeEnabled())
	{
		// Dedicated clients can still have sv_gametype=0 locally even while the
		// authority is running invasion. A non-disabled invasion snapshot proves
		// the server mode and must be accepted so mirror sync can proceed.
		if (stateRaw == INVS_DISABLED || stateRaw > INVS_FAILURE)
			return false;
		Net_TraceSetSvGametype(4, "invasion-snapshot-authority");
	}
	const bool hasSpawnMetadata = (version >= 2u);
	const size_t snapshotBytes = hasSpawnMetadata ? HCDEInvasionSnapshotHeaderV2Size : HCDEInvasionSnapshotHeaderV1Size;
	if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < snapshotBytes)
		return false;
	if ((version != 1u && version != HCDEInvasionSnapshotProtocolVersion)
		|| reserved != 0u
		|| (flags & ~HCDEInvasionSnapshotFlagBossWave) != 0u
		|| stateRaw > INVS_FAILURE)
	{
		return false;
	}

	size_t cursor = bodyCursor + HCDEInvasionSnapshotStateTicsOffset;
	uint32_t stateTics = 0u;
	uint32_t wave = 0u;
	uint32_t maxWaves = 0u;
	uint32_t waveBudget = 0u;
	uint32_t waveSpawned = 0u;
	uint32_t waveCleared = 0u;
	uint32_t activeMonsters = 0u;
	uint16_t spawnSpotCount = 0u;
	uint16_t activeSpawnSpotCount = 0u;
	uint32_t spawnPlanBudget = 0u;
	uint32_t spawnActiveTag = 0u;
	uint8_t spawnFlags = 0u;
	uint8_t spawnFallbackSource = uint8_t(INVSPAWN_NONE);
	if (!HCDEReadBE32Field(body, bodyBytes, cursor, stateTics)
		|| !HCDEReadBE32Field(body, bodyBytes, cursor, wave)
		|| !HCDEReadBE32Field(body, bodyBytes, cursor, maxWaves)
		|| !HCDEReadBE32Field(body, bodyBytes, cursor, waveBudget)
		|| !HCDEReadBE32Field(body, bodyBytes, cursor, waveSpawned)
		|| !HCDEReadBE32Field(body, bodyBytes, cursor, waveCleared)
		|| !HCDEReadBE32Field(body, bodyBytes, cursor, activeMonsters))
	{
		return false;
	}

	if (hasSpawnMetadata)
	{
		uint8_t reserved2 = 0u;
		uint8_t reserved3 = 0u;
		if (!HCDEReadBE16Field(body, bodyBytes, cursor, spawnSpotCount)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, activeSpawnSpotCount)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, spawnPlanBudget)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, spawnActiveTag)
			|| !HCDEReadByteField(body, bodyBytes, cursor, spawnFlags)
			|| !HCDEReadByteField(body, bodyBytes, cursor, spawnFallbackSource)
			|| !HCDEReadByteField(body, bodyBytes, cursor, reserved2)
			|| !HCDEReadByteField(body, bodyBytes, cursor, reserved3)
			|| (spawnFlags & ~HCDEInvasionSnapshotSpawnFlagUsingFallback) != 0u
			|| spawnFallbackSource > INVSPAWN_PLAYERSTART
			|| reserved2 != 0u
			|| reserved3 != 0u)
		{
			return false;
		}
	}

	const EInvasionState previousState = InvasionState;
	const int previousWave = InvasionWaveDirector.Wave;
	const EInvasionState newState = EInvasionState(stateRaw);

	if (previousState != newState)
	{
		DebugTrace::Infof("invasion", "snapshot state %s -> %s wave=%u tics=%u client=%d room=%u",
			Net_InvasionStateName(previousState), Net_InvasionStateName(newState),
			unsigned(wave), unsigned(stateTics), clientNum, unsigned(CurrentRoomID));
	}

	InvasionState = newState;
	InvasionStateTics = max<int>(int(stateTics), 0);
	InvasionWaveDirector.Wave = max<int>(int(wave), 0);
	InvasionWaveDirector.MaxWaves = max<int>(int(maxWaves), 0);
	InvasionWaveDirector.WaveBudget = max<int>(int(waveBudget), 0);
	const int incomingSpawned = max<int>(int(waveSpawned), 0);
	const int incomingCleared = max<int>(int(waveCleared), 0);
	if (!I_IsLocalHCDEServiceAuthority()
		&& previousWave == InvasionWaveDirector.Wave
		&& Net_IsInvasionRoundActiveState(InvasionState))
	{
		InvasionWaveDirector.WaveSpawned = max(InvasionWaveDirector.WaveSpawned, incomingSpawned);
		InvasionWaveDirector.WaveCleared = max(InvasionWaveDirector.WaveCleared, incomingCleared);
	}
	else
	{
		InvasionWaveDirector.WaveSpawned = incomingSpawned;
		InvasionWaveDirector.WaveCleared = incomingCleared;
	}
	InvasionWaveDirector.WaveFlags = (flags & HCDEInvasionSnapshotFlagBossWave) != 0u ? INV_WAVEF_BOSS : 0u;
	InvasionReplicatedActiveMonsterCount = max<int>(int(activeMonsters), 0);
	InvasionSpawnDirectory.TotalSpotCount = int(spawnSpotCount);
	InvasionSpawnDirectory.ActiveSpotCount = int(activeSpawnSpotCount);
	InvasionSpawnDirectory.TotalSpawnBudget = max<int>(int(spawnPlanBudget), 0);
	InvasionSpawnDirectory.ActiveTag = max<int>(int(spawnActiveTag), 0);
	InvasionSpawnDirectory.UsingFallback = (spawnFlags & HCDEInvasionSnapshotSpawnFlagUsingFallback) != 0u;
	InvasionSpawnDirectory.FallbackSource = EInvasionSpawnSource(spawnFallbackSource);
	InvasionSpawnDirectory.SpawnedThisWave = InvasionWaveDirector.WaveSpawned;
	Net_PrepareInvasionMirrorFromSnapshot(previousState, previousWave);

	bodyCursor = cursor;
	HCDERecordLiveLaneRx(HLANE_AUTHORITY, clientNum, snapshotBytes);
	if (bodyCursor < bodyBytes
		&& expectAuthorityEvents
		&& bodyBytes - bodyCursor >= HCDEAuthorityEventsHeaderSize
		&& memcmp(&body[bodyCursor + HCDEAuthorityEventsMagicOffset], HCDEAuthorityEventsMagic, sizeof(HCDEAuthorityEventsMagic)) == 0)
	{
		if (!HCDEApplyAuthorityEvents(clientNum, body, bodyBytes, bodyCursor))
			return false;
	}
	if (bodyCursor < bodyBytes
		&& expectActorDeltas
		&& bodyBytes - bodyCursor >= HCDEActorDeltasHeaderSize
		&& memcmp(&body[bodyCursor + HCDEActorDeltasMagicOffset], HCDEActorDeltasMagic, sizeof(HCDEActorDeltasMagic)) == 0)
	{
		if (!HCDEApplyActorDeltasV2(clientNum, body, bodyBytes, bodyCursor))
			return false;
	}

	++HCDELiveProfile.InvasionSnapshotPacketsReceived;
	HCDELiveProfile.InvasionSnapshotBytesReceived += bodyCursor - startCursor;
	DebugTrace::Markf("net", "HCDE invasion snapshot recv client=%d version=%u state=%s tics=%d wave=%d/%d budget=%d spawned=%d cleared=%d active=%d boss=%d spots=%d/%d spot-budget=%d tag=%d fallback=%d source=%d",
		clientNum, unsigned(version), Net_InvasionStateName(InvasionState), InvasionStateTics,
		InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves,
		InvasionWaveDirector.WaveBudget, InvasionWaveDirector.WaveSpawned,
		InvasionWaveDirector.WaveCleared, InvasionReplicatedActiveMonsterCount,
		(InvasionWaveDirector.WaveFlags & INV_WAVEF_BOSS) != 0u ? 1 : 0,
		InvasionSpawnDirectory.TotalSpotCount, InvasionSpawnDirectory.ActiveSpotCount,
		InvasionSpawnDirectory.TotalSpawnBudget, InvasionSpawnDirectory.ActiveTag,
		InvasionSpawnDirectory.UsingFallback ? 1 : 0,
		int(InvasionSpawnDirectory.FallbackSource));

	// Remote clients receive invasion phase transitions through snapshots.
	// Tick announcements after applying to keep countdown and wave-complete
	// messages in sync with the authoritative state.
	Net_TickInvasionAnnouncements();
	return true;
}

static bool HCDEIsAllowedTicEventType(uint8_t type)
{
	switch (type)
	{
	case DEM_MUSICCHANGE:
	case DEM_PRINT:
	case DEM_CENTERPRINT:
	case DEM_UINFCHANGED:
	case DEM_SINFCHANGED:
	case DEM_GENERICCHEAT:
	case DEM_GIVECHEAT:
	case DEM_SAY:
	case DEM_CHANGEMAP:
	case DEM_SUICIDE:
	case DEM_ADDBOT:
	case DEM_KILLBOTS:
	case DEM_INVUSEALL:
	case DEM_INVUSE:
	case DEM_PAUSE:
	case DEM_SAVEGAME:
	case DEM_SUMMON:
	case DEM_FOV:
	case DEM_MYFOV:
	case DEM_CHANGEMAP2:
	case DEM_RUNSCRIPT:
	case DEM_SINFCHANGEDXOR:
	case DEM_INVDROP:
	case DEM_WARPCHEAT:
	case DEM_CENTERVIEW:
	case DEM_SUMMONFRIEND:
	case DEM_SPRAY:
	case DEM_CROUCH:
	case DEM_RUNSCRIPT2:
	case DEM_CHECKAUTOSAVE:
	case DEM_DOAUTOSAVE:
	case DEM_MORPHEX:
	case DEM_SUMMONFOE:
	case DEM_TAKECHEAT:
	case DEM_ADDCONTROLLER:
	case DEM_DELCONTROLLER:
	case DEM_KILLCLASSCHEAT:
	case DEM_SUMMON2:
	case DEM_SUMMONFRIEND2:
	case DEM_SUMMONFOE2:
	case DEM_ADDSLOTDEFAULT:
	case DEM_ADDSLOT:
	case DEM_SETSLOT:
	case DEM_SUMMONMBF:
	case DEM_CONVREPLY:
	case DEM_CONVCLOSE:
	case DEM_CONVNULL:
	case DEM_RUNSPECIAL:
	case DEM_SETPITCHLIMIT:
	case DEM_RUNNAMEDSCRIPT:
	case DEM_REVERTCAMERA:
	case DEM_SETSLOTPNUM:
	case DEM_REMOVE:
	case DEM_FINISHGAME:
	case DEM_NETEVENT:
	case DEM_MDK:
	case DEM_SETINV:
	case DEM_ENDSCREENJOB:
	case DEM_ZSC_CMD:
	case DEM_CHANGESKILL:
	case DEM_KICK:
	case DEM_READIED:
	case DEM_WEAPSELECT:
	case DEM_USEFLECHETTE:
		return true;
	default:
		return false;
	}
}

static bool HCDEIsAllowedClientInputEventType(uint8_t type)
{
	// Client inputs are a narrower trust boundary than server snapshots. Keep
	// player/mod-authored requests here and leave authoritative world changes
	// to the server snapshot path.
	switch (type)
	{
	case DEM_UINFCHANGED:
	case DEM_SAY:
	case DEM_SUICIDE:
	case DEM_INVUSEALL:
	case DEM_INVUSE:
	case DEM_PAUSE:
	case DEM_MYFOV:
	case DEM_RUNSCRIPT:
	case DEM_INVDROP:
	case DEM_CENTERVIEW:
	case DEM_SPRAY:
	case DEM_CROUCH:
	case DEM_RUNSCRIPT2:
	case DEM_CONVREPLY:
	case DEM_CONVCLOSE:
	case DEM_CONVNULL:
	case DEM_RUNSPECIAL:
	case DEM_SETPITCHLIMIT:
	case DEM_RUNNAMEDSCRIPT:
	case DEM_REVERTCAMERA:
	case DEM_SETSLOT:
	case DEM_ADDSLOT:
	case DEM_ADDSLOTDEFAULT:
	case DEM_NETEVENT:
	case DEM_ZSC_CMD:
	case DEM_READIED:
	case DEM_WEAPSELECT:
	case DEM_USEFLECHETTE:
		return true;
	default:
		return false;
	}
}

static bool HCDEAppendCanonicalString(uint8_t* output, size_t outputCapacity, size_t& cursor, const uint8_t* stringBytes, size_t stringBytesSize)
{
	if (stringBytesSize > UINT16_MAX)
		return false;
	return HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(stringBytesSize))
		&& HCDEAppendBytes(output, outputCapacity, cursor, stringBytes, stringBytesSize);
}

// Store tic event payloads in a length-prefixed HCDE form so replaying them never depends on raw stream alignment.
static bool HCDEAppendCanonicalNullString(uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor)
{
	if (inputCursor >= dataSize)
		return false;

	const size_t remaining = dataSize - inputCursor;
	const void* terminator = memchr(&data[inputCursor], 0, remaining);
	if (terminator == nullptr)
		return false;

	const size_t stringBytes = size_t(static_cast<const uint8_t*>(terminator) - &data[inputCursor]);
	if (!HCDEAppendCanonicalString(output, outputCapacity, outputCursor, &data[inputCursor], stringBytes))
		return false;
	inputCursor += stringBytes + 1u;
	return true;
}

static bool HCDEAppendCanonicalWeaponIndex(uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor)
{
	uint8_t first = 0u;
	if (!HCDEReadByteField(data, dataSize, inputCursor, first))
		return false;

	uint16_t index = first & 0x7fu;
	if (first & 0x80u)
	{
		uint8_t high = 0u;
		if (!HCDEReadByteField(data, dataSize, inputCursor, high))
			return false;
		index |= uint16_t(high) << 7;
	}

	return HCDEAppendBE16(output, outputCapacity, outputCursor, index);
}

static bool HCDEAppendCanonicalCVarChange(uint8_t eventType, uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor)
{
	uint8_t descriptor = 0u;
	if (!HCDEReadByteField(data, dataSize, inputCursor, descriptor))
		return false;

	const uint8_t type = descriptor >> 6;
	const size_t nameBytes = descriptor & 0x3fu;
	if (type > CVAR_String || nameBytes == 0u || inputCursor > dataSize || nameBytes > dataSize - inputCursor)
		return false;

	if (!HCDEAppendByte(output, outputCapacity, outputCursor, type)
		|| !HCDEAppendCanonicalString(output, outputCapacity, outputCursor, &data[inputCursor], nameBytes))
	{
		return false;
	}
	inputCursor += nameBytes;

	if (eventType == DEM_SINFCHANGEDXOR)
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u);

	switch (type)
	{
	case CVAR_Bool:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u);
	case CVAR_Int:
	case CVAR_Float:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 4u);
	case CVAR_String:
		return HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor);
	default:
		return false;
	}
}

static bool HCDEAppendCanonicalRunArgs(uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor, bool named)
{
	uint8_t argCount = 0u;
	if (named)
	{
		if (!HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			|| !HCDEReadByteField(data, dataSize, inputCursor, argCount)
			|| !HCDEAppendByte(output, outputCapacity, outputCursor, argCount))
		{
			return false;
		}
		argCount &= 127u;
	}
	else
	{
		if (!HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 2u)
			|| !HCDEReadByteField(data, dataSize, inputCursor, argCount)
			|| !HCDEAppendByte(output, outputCapacity, outputCursor, argCount))
		{
			return false;
		}
	}

	return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, size_t(argCount) * 4u);
}

static bool HCDEAppendCanonicalEventPayload(uint8_t eventType, uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor)
{
	switch (eventType)
	{
	case DEM_SUICIDE:
	case DEM_KILLBOTS:
	case DEM_INVUSEALL:
	case DEM_PAUSE:
	case DEM_CENTERVIEW:
	case DEM_CROUCH:
	case DEM_CHECKAUTOSAVE:
	case DEM_DOAUTOSAVE:
	case DEM_CONVCLOSE:
	case DEM_CONVNULL:
	case DEM_REVERTCAMERA:
	case DEM_FINISHGAME:
	case DEM_ENDSCREENJOB:
	case DEM_READIED:
	case DEM_USEFLECHETTE:
		return true;

	case DEM_SAY:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u)
			&& HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor);

	case DEM_ADDBOT:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u)
			&& HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 4u);

	case DEM_GIVECHEAT:
	case DEM_TAKECHEAT:
		return HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 4u);

	case DEM_SETINV:
		return HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 5u);

	case DEM_NETEVENT:
		return HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 14u);

	case DEM_ZSC_CMD:
		{
			uint16_t commandBytes = 0u;
			return HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
				&& HCDEReadBE16Field(data, dataSize, inputCursor, commandBytes)
				&& HCDEAppendBE16(output, outputCapacity, outputCursor, commandBytes)
				&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, commandBytes);
		}

	case DEM_SUMMON2:
	case DEM_SUMMONFRIEND2:
	case DEM_SUMMONFOE2:
		return HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 25u);

	case DEM_CHANGEMAP2:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u)
			&& HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor);

	case DEM_MUSICCHANGE:
	case DEM_PRINT:
	case DEM_CENTERPRINT:
	case DEM_UINFCHANGED:
	case DEM_CHANGEMAP:
	case DEM_SUMMON:
	case DEM_SUMMONFRIEND:
	case DEM_SUMMONFOE:
	case DEM_SUMMONMBF:
	case DEM_REMOVE:
	case DEM_SPRAY:
	case DEM_MORPHEX:
	case DEM_KILLCLASSCHEAT:
	case DEM_MDK:
		return HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor);

	case DEM_WARPCHEAT:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 6u);

	case DEM_INVUSE:
	case DEM_FOV:
	case DEM_MYFOV:
	case DEM_CHANGESKILL:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 4u);

	case DEM_INVDROP:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 8u);

	case DEM_GENERICCHEAT:
	case DEM_ADDCONTROLLER:
	case DEM_DELCONTROLLER:
	case DEM_KICK:
	case DEM_WEAPSELECT:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u);

	case DEM_SAVEGAME:
		return HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendCanonicalNullString(output, outputCapacity, outputCursor, data, dataSize, inputCursor);

	case DEM_SINFCHANGED:
	case DEM_SINFCHANGEDXOR:
		return HCDEAppendCanonicalCVarChange(eventType, output, outputCapacity, outputCursor, data, dataSize, inputCursor);

	case DEM_RUNSCRIPT:
	case DEM_RUNSCRIPT2:
	case DEM_RUNSPECIAL:
		return HCDEAppendCanonicalRunArgs(output, outputCapacity, outputCursor, data, dataSize, inputCursor, false);

	case DEM_RUNNAMEDSCRIPT:
		return HCDEAppendCanonicalRunArgs(output, outputCapacity, outputCursor, data, dataSize, inputCursor, true);

	case DEM_CONVREPLY:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 3u);

	case DEM_SETSLOT:
	case DEM_SETSLOTPNUM:
		{
			if (eventType == DEM_SETSLOTPNUM
				&& !HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u))
			{
				return false;
			}
			uint8_t slot = 0u;
			uint8_t count = 0u;
			if (!HCDEReadByteField(data, dataSize, inputCursor, slot)
				|| !HCDEReadByteField(data, dataSize, inputCursor, count)
				|| !HCDEAppendByte(output, outputCapacity, outputCursor, slot)
				|| !HCDEAppendByte(output, outputCapacity, outputCursor, count))
			{
				return false;
			}
			for (uint8_t i = 0u; i < count; ++i)
			{
				if (!HCDEAppendCanonicalWeaponIndex(output, outputCapacity, outputCursor, data, dataSize, inputCursor))
					return false;
			}
			return true;
		}

	case DEM_ADDSLOT:
	case DEM_ADDSLOTDEFAULT:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u)
			&& HCDEAppendCanonicalWeaponIndex(output, outputCapacity, outputCursor, data, dataSize, inputCursor);

	case DEM_SETPITCHLIMIT:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 2u);

	default:
		return false;
	}
}

// Convert a raw NCMD-style DEM_* event stream into the canonical HCDE event-record
// format. When `clientInput` is true, the narrower HCDEIsAllowedClientInputEventType
// allow-list is enforced and disallowed events are silently dropped (parsed into a
// throw-away region of the output and then rewound) so the receiver -- which only
// accepts client-input events from that same narrow list -- never has to reject the
// whole packet on account of a stray locally-queued admin/cheat event. Server
// snapshot builds (`clientInput == false`) keep the strict-fail behaviour.
static bool HCDEAppendEventRecords(uint8_t* output, size_t outputCapacity, size_t& cursor, const uint8_t* data, size_t dataSize, bool clientInput)
{
	if (cursor >= outputCapacity)
	{
		DebugTrace::Markf("net", "HCDE append event records failed: cursor=%zu outputCapacity=%zu reason=cursor_ge_capacity", cursor, outputCapacity);
		return false;
	}
	if (outputCapacity - cursor < 2u)
	{
		DebugTrace::Markf("net", "HCDE append event records failed: cursor=%zu outputCapacity=%zu required=2 reason=insufficient_capacity", cursor, outputCapacity);
		return false;
	}

	const size_t countOffset = cursor;
	cursor += 2u;
	uint16_t eventCount = 0u;
	size_t inputCursor = 0u;
	while (inputCursor < dataSize)
	{
		uint8_t eventType = 0u;
		if (!HCDEReadByteField(data, dataSize, inputCursor, eventType))
		{
			DebugTrace::Markf("net", "HCDE append event records failed: inputCursor=%zu dataSize=%zu reason= EventType_read_failed", inputCursor, dataSize);
			return false;
		}
		if (!HCDEIsAllowedTicEventType(eventType))
		{
			DebugTrace::Markf("net", "HCDE append event records failed: eventType=%u reason=disallowed_event_type", eventType);
			return false;
		}
		if (eventCount == UINT16_MAX)
		{
			DebugTrace::Markf("net", "HCDE append event records failed: eventCount=%u reason=event_count_overflow", unsigned(eventCount));
			return false;
		}
		// Snapshot cursor so we can rewind if the event is filtered out below.
		const size_t eventRecordStart = cursor;
		if (!HCDEAppendByte(output, outputCapacity, cursor, eventType))
		{
			DebugTrace::Markf("net", "HCDE append event records failed: cursor=%zu outputCapacity=%zu eventType=%u reason=EventType_append_failed", cursor, outputCapacity, eventType);
			return false;
		}
		if (cursor >= outputCapacity)
		{
			DebugTrace::Markf("net", "HCDE append event records failed: cursor=%zu outputCapacity=%zu reason=cursor_ge_capacity", cursor, outputCapacity);
			return false;
		}
		if (outputCapacity - cursor < 2u)
		{
			DebugTrace::Markf("net", "HCDE append event records failed: cursor=%zu outputCapacity=%zu required=2 reason=insufficient_capacity", cursor, outputCapacity);
			return false;
		}

		const size_t payloadSizeOffset = cursor;
		cursor += 2u;
		const size_t payloadStart = cursor;
		if (!HCDEAppendCanonicalEventPayload(eventType, output, outputCapacity, cursor, data, dataSize, inputCursor))
		{
			DebugTrace::Markf("net", "HCDE append event records failed: eventType=%u reason=payload_append_failed", eventType);
			return false;
		}

		const size_t payloadBytes = cursor - payloadStart;
		if (payloadBytes > UINT16_MAX)
		{
			DebugTrace::Markf("net", "HCDE append event records failed: payloadBytes=%zu reason=payload_too_large", payloadBytes);
			return false;
		}

		// Drop client-input events that fall outside the narrow allow-list: parsing
		// already advanced inputCursor past the legacy form so the next iteration
		// stays in sync with the input stream, but we rewind `cursor` so the wire
		// payload omits the disallowed record entirely.
		if (clientInput && !HCDEIsAllowedClientInputEventType(eventType))
		{
			cursor = eventRecordStart;
			DebugTrace::Markf("net", "HCDE append event records: dropped client-input event type=%u (not in client-input allow-list)", eventType);
			continue;
		}

		HCDELiveWriteBE16(&output[payloadSizeOffset], uint16_t(payloadBytes));
		++eventCount;
	}

	HCDELiveWriteBE16(&output[countOffset], eventCount);
	return true;
}

static bool HCDEReadCanonicalString(const uint8_t* data, size_t dataSize, size_t& cursor, const uint8_t*& stringBytes, size_t& stringBytesSize)
{
	uint16_t length = 0u;
	if (!HCDEReadBE16Field(data, dataSize, cursor, length))
	{
		DebugTrace::Markf("net", "HCDE read canonical string failed: cursor=%zu reason=length_read_failed", cursor);
		return false;
	}
	if (cursor >= dataSize)
	{
		DebugTrace::Markf("net", "HCDE read canonical string failed: cursor=%zu dataSize=%zu length=%zu reason=cursor_ge_size", cursor, dataSize, length);
		return false;
	}
	if (length > dataSize - cursor)
	{
		DebugTrace::Markf("net", "HCDE read canonical string failed: cursor=%zu dataSize=%zu length=%zu reason=insufficient_data", cursor, dataSize, length);
		return false;
	}

	stringBytes = &data[cursor];
	stringBytesSize = length;
	cursor += length;
	return true;
}

static bool HCDEAppendLegacyStringFromCanonical(uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor)
{
	const uint8_t* stringBytes = nullptr;
	size_t stringBytesSize = 0u;
	return HCDEReadCanonicalString(data, dataSize, inputCursor, stringBytes, stringBytesSize)
		&& HCDEAppendBytes(output, outputCapacity, outputCursor, stringBytes, stringBytesSize)
		&& HCDEAppendByte(output, outputCapacity, outputCursor, 0u);
}

static bool HCDEAppendLegacyWeaponIndex(uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor)
{
	uint16_t index = 0u;
	if (!HCDEReadBE16Field(data, dataSize, inputCursor, index) || index > 32767u)
		return false;

	if (index < 128u)
		return HCDEAppendByte(output, outputCapacity, outputCursor, uint8_t(index));
	return HCDEAppendByte(output, outputCapacity, outputCursor, uint8_t(0x80u | (index & 0x7fu)))
		&& HCDEAppendByte(output, outputCapacity, outputCursor, uint8_t(index >> 7));
}

static bool HCDEAppendLegacyCVarChange(uint8_t eventType, uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor)
{
	uint8_t type = 0u;
	const uint8_t* nameBytes = nullptr;
	size_t nameBytesSize = 0u;
	if (!HCDEReadByteField(data, dataSize, inputCursor, type)
		|| type > CVAR_String
		|| !HCDEReadCanonicalString(data, dataSize, inputCursor, nameBytes, nameBytesSize)
		|| nameBytesSize == 0u
		|| nameBytesSize > 63u)
	{
		return false;
	}

	if (!HCDEAppendByte(output, outputCapacity, outputCursor, uint8_t(nameBytesSize | (size_t(type) << 6)))
		|| !HCDEAppendBytes(output, outputCapacity, outputCursor, nameBytes, nameBytesSize))
	{
		return false;
	}

	if (eventType == DEM_SINFCHANGEDXOR)
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u);

	switch (type)
	{
	case CVAR_Bool:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u);
	case CVAR_Int:
	case CVAR_Float:
		return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 4u);
	case CVAR_String:
		return HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor);
	default:
		return false;
	}
}

static bool HCDEAppendLegacyRunArgs(uint8_t* output, size_t outputCapacity, size_t& outputCursor, const uint8_t* data, size_t dataSize, size_t& inputCursor, bool named)
{
	uint8_t argCount = 0u;
	if (named)
	{
		if (!HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			|| !HCDEReadByteField(data, dataSize, inputCursor, argCount)
			|| !HCDEAppendByte(output, outputCapacity, outputCursor, argCount))
		{
			return false;
		}
		argCount &= 127u;
	}
	else
	{
		if (!HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 2u)
			|| !HCDEReadByteField(data, dataSize, inputCursor, argCount)
			|| !HCDEAppendByte(output, outputCapacity, outputCursor, argCount))
		{
			return false;
		}
	}

	return HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, size_t(argCount) * 4u);
}

static bool HCDEAppendLegacyEventPayload(uint8_t eventType, const uint8_t* data, size_t dataSize, uint8_t* output, size_t outputCapacity, size_t& outputCursor)
{
	size_t inputCursor = 0u;
	bool ok = false;

	switch (eventType)
	{
	case DEM_SUICIDE:
	case DEM_KILLBOTS:
	case DEM_INVUSEALL:
	case DEM_PAUSE:
	case DEM_CENTERVIEW:
	case DEM_CROUCH:
	case DEM_CHECKAUTOSAVE:
	case DEM_DOAUTOSAVE:
	case DEM_CONVCLOSE:
	case DEM_CONVNULL:
	case DEM_REVERTCAMERA:
	case DEM_FINISHGAME:
	case DEM_ENDSCREENJOB:
	case DEM_READIED:
	case DEM_USEFLECHETTE:
		ok = true;
		break;

	case DEM_SAY:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u)
			&& HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor);
		break;

	case DEM_ADDBOT:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u)
			&& HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 4u);
		break;

	case DEM_GIVECHEAT:
	case DEM_TAKECHEAT:
		ok = HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 4u);
		break;

	case DEM_SETINV:
		ok = HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 5u);
		break;

	case DEM_NETEVENT:
		ok = HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 14u);
		break;

	case DEM_ZSC_CMD:
		{
			uint16_t commandBytes = 0u;
			ok = HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
				&& HCDEReadBE16Field(data, dataSize, inputCursor, commandBytes)
				&& HCDEAppendBE16(output, outputCapacity, outputCursor, commandBytes)
				&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, commandBytes);
			break;
		}

	case DEM_SUMMON2:
	case DEM_SUMMONFRIEND2:
	case DEM_SUMMONFOE2:
		ok = HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 25u);
		break;

	case DEM_CHANGEMAP2:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u)
			&& HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor);
		break;

	case DEM_MUSICCHANGE:
	case DEM_PRINT:
	case DEM_CENTERPRINT:
	case DEM_UINFCHANGED:
	case DEM_CHANGEMAP:
	case DEM_SUMMON:
	case DEM_SUMMONFRIEND:
	case DEM_SUMMONFOE:
	case DEM_SUMMONMBF:
	case DEM_REMOVE:
	case DEM_SPRAY:
	case DEM_MORPHEX:
	case DEM_KILLCLASSCHEAT:
	case DEM_MDK:
		ok = HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor);
		break;

	case DEM_WARPCHEAT:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 6u);
		break;

	case DEM_INVUSE:
	case DEM_FOV:
	case DEM_MYFOV:
	case DEM_CHANGESKILL:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 4u);
		break;

	case DEM_INVDROP:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 8u);
		break;

	case DEM_GENERICCHEAT:
	case DEM_ADDCONTROLLER:
	case DEM_DELCONTROLLER:
	case DEM_KICK:
	case DEM_WEAPSELECT:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u);
		break;

	case DEM_SAVEGAME:
		ok = HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor)
			&& HCDEAppendLegacyStringFromCanonical(output, outputCapacity, outputCursor, data, dataSize, inputCursor);
		break;

	case DEM_SINFCHANGED:
	case DEM_SINFCHANGEDXOR:
		ok = HCDEAppendLegacyCVarChange(eventType, output, outputCapacity, outputCursor, data, dataSize, inputCursor);
		break;

	case DEM_RUNSCRIPT:
	case DEM_RUNSCRIPT2:
	case DEM_RUNSPECIAL:
		ok = HCDEAppendLegacyRunArgs(output, outputCapacity, outputCursor, data, dataSize, inputCursor, false);
		break;

	case DEM_RUNNAMEDSCRIPT:
		ok = HCDEAppendLegacyRunArgs(output, outputCapacity, outputCursor, data, dataSize, inputCursor, true);
		break;

	case DEM_CONVREPLY:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 3u);
		break;

	case DEM_SETSLOT:
	case DEM_SETSLOTPNUM:
		{
			if (eventType == DEM_SETSLOTPNUM
				&& !HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u))
			{
				return false;
			}
			uint8_t slot = 0u;
			uint8_t count = 0u;
			if (!HCDEReadByteField(data, dataSize, inputCursor, slot)
				|| !HCDEReadByteField(data, dataSize, inputCursor, count)
				|| !HCDEAppendByte(output, outputCapacity, outputCursor, slot)
				|| !HCDEAppendByte(output, outputCapacity, outputCursor, count))
			{
				return false;
			}
			for (uint8_t i = 0u; i < count; ++i)
			{
				if (!HCDEAppendLegacyWeaponIndex(output, outputCapacity, outputCursor, data, dataSize, inputCursor))
					return false;
			}
			ok = true;
			break;
		}

	case DEM_ADDSLOT:
#include "d_net_snapshot_part2.cpp"
#include "d_net_invasion.cpp"
		if (*hcde_hud_debug && HCDELiveProfile.MirrorVisualPasses % 35u == 0u)
		{
			const double avgMS = HCDELiveProfile.MirrorVisualPasses > 0u
				? double(HCDELiveProfile.MirrorVisualMicros) / double(HCDELiveProfile.MirrorVisualPasses) / 1000.0
				: 0.0;
			const double maxMS = double(HCDELiveProfile.MirrorVisualMaxMicros) / 1000.0;
			Printf(PRINT_HIGH,
				"HCDE mirror perf tracked=%u updated=%u skipped=%u pending-spawns=%u pending-events=%u world-steps=%d avg=%.2fms max=%.2fms backlog=%d lag=%s\n",
				unsigned(InvasionReplicatedActors.Size()),
				updated,
				skipped,
				unsigned(InvasionPendingMirrorSpawns.Size()),
				unsigned(InvasionPendingSpawnEvents.Size()),
				InvasionMirrorVisualWorldSteps,
				avgMS,
				maxMS,
				ClientTic - gametic,
				Net_LagStateName(LagState));
		}
	}
}

static void Net_TickInvasionMirrorState()
{
	if (Net_IsLocalInvasionAuthority() || InvasionState == INVS_DISABLED)
		return;

	if (InvasionStateTics > 0)
		--InvasionStateTics;
}

void Net_ClearBuffers()
{
	CloseNetwork();

	for (unsigned int i = 0; i < MAXPLAYERS; ++i)
	{
		playeringame[i] = false;
		players[i].waiting = players[i].inconsistant = false;

		auto& state = ClientStates[i];
		state.AverageLatency = state.CurrentLatency = 0u;
		memset(state.SentTime, 0, sizeof(state.SentTime));
		memset(state.RecvTime, 0, sizeof(state.RecvTime));
		state.bNewLatency = true;

		state.ResendID = state.StabilityBuffer = 0u;
		state.CurrentNetConsistency = state.LastVerifiedConsistency = state.ConsistencyAck = state.ResendConsistencyFrom = -1;
		state.CurrentSequence = state.SequenceAck = state.ResendSequenceFrom = -1;
		state.Flags = 0;

		for (int j = 0; j < BACKUPTICS; ++j)
			state.Tics[j].Data.SetData(nullptr, 0);

		LateJoinSyncTargetSequence[i] = -1;
		LateJoinSyncTargetConsistency[i] = -1;
		LateJoinSyncStartTic[i] = -1;
		HCDEActorBaselineRepairUntilTic[i] = 0;
		HCDEAuthorityEventReplayNextId[i] = 0u;
		HCDEPassiveResendLastSequenceAck[i] = -1;
		HCDEPassiveResendAckChangedTic[i] = 0;
		HCDEPassiveResendLastRequestTic[i] = 0;
		HCDEActorDeltaV2ReceiveCompactNextTic[i] = 0;
		HCDELivePeers[i].Clear();
	}

	P_ClearPredictionData();
	NetBufferLength = 0u;
	RemoteClient = -1;
	MaxClients = TicDup = 1u;
	consoleplayer = 0;
	LocalNetBufferSize = 0u;
	Net_Arbitrator = 0;
	LastHCDELiveControlMS = 0u;
	LastHCDELiveSequenceRejectReportMS = 0u;
	LastHCDELiveSnapshotRejectReportMS = 0u;
	LastHCDELiveTicGateReportMS = 0u;
	LastHCDEPredictionLeadCapReportMS = 0u;
	LastHCDEMonsterProximityDumpMS = 0u;
	LastHCDEPredictionFaultReportMS = 0u;
	LastPredictionFaultBundleMS = 0u;
	// Rate-limit timestamps for the prediction-debug warn/dump and the mirror
	// visual pass time also need to drop back to zero on netstate reset, or
	// the *first* event of the next netgame is silently suppressed by an
	// interval check against a stale timestamp from the previous session
	// (and the lag HUD shows a stale "last mirror pass took N ms" reading).
	HCDEPredictionDebugLastSoftWarnMS = 0u;
	HCDEPredictionDebugLastTraceDumpMS = 0u;
	LastMirrorVisualPassUS = 0u;
	// PendingLocalHealthRepair holds the next health/pose snap that the
	// playsim will apply on its next tic boundary. If a previous netgame
	// disconnected with one queued (e.g. server confirmed damage right as
	// the user quit), the residual .Valid=true would otherwise leak into
	// the fresh session and slam the new pawn at the next world delta. We
	// only need to clear Valid - the rest of the fields are write-then-read
	// gated on it.
	PendingLocalHealthRepair.Valid = false;

	LagState = LAG_NONE;
	MutedClients = 0u;
	LateJoinSyncPending = 0u;
	CurrentRoomID = 0u;
	NetworkClients.Clear();
	netgame = multiplayer = false;
	LastSentConsistency = CurrentConsistency = 0;
	LastEnterTic = LastGameUpdate = EnterTic;
	gametic = ClientTic = 0;
	SkipCommandTimer = SkipCommandAmount = CommandsAhead = 0;
	StabilityBuffer = PrevAvailableDiff = 0;
	CurStabilityTic = 0u;
	memset(StabilityTics, 0, sizeof(StabilityTics));
	NetEvents.ResetStream();

	CutsceneReady = 0u;
	CutsceneCountdown = 0;
	Net_ResetInvasionState("clear-buffers");
	bCommandsReset = false;
	AuthorityWaitGraceUntil = 0;

	LevelStartAck = 0u;
	LevelStartDelay = LevelStartDebug = 0;
	Net_SetLevelStartStatus(LST_READY, "net-reset");
	LastTicGateStallTrace = 0;

	FullLatencyCycle = MAXSENDTICS * 3;
	LastLatencyUpdate = 0;

	// On a dedicated server (-server) slot 0 is the reserved transport-only
	// authority slot, NOT a player. Marking playeringame[0]=true here causes
	// FLevelLocals::SpawnPlayer to give the dedicated server a real pawn at
	// player start #1, so a single connected client ends up looking like two
	// players in the game (the ghost server pawn + the actual joiner). Skip
	// the playable-slot bookkeeping for the reserved authority slot.
	//
	// NetworkClients += 0 is preserved unconditionally because the dedicated
	// server still needs slot 0 in NetworkClients as its transport identity
	// (HCDE service authority); the engine just must not treat it as a pawn.
	if (!I_IsServerReservedSlot(0))
		playeringame[0] = true;
	NetworkClients += 0;
}

static bool Net_IsLateJoinSyncPending(int client);
static bool Net_UsesServerAuthoritativeConsistency();

static bool Net_IsCutsceneReadyParticipant(int player)
{
	return player >= 0 && player < MAXPLAYERS
		&& playeringame[player]
		&& !I_IsServerReservedSlot(player)
		&& !Net_IsLateJoinSyncPending(player);
}

static bool Net_IsGameplayConsistencyParticipant(int player)
{
	if (Net_UsesServerAuthoritativeConsistency())
		return false;

	return player >= 0 && player < MAXPLAYERS
		&& playeringame[player]
		&& !I_IsServerReservedSlot(player)
		&& !Net_IsLateJoinSyncPending(player);
}

static bool Net_IsTicGateClient(int player)
{
	if (player < 0 || player >= MAXPLAYERS || !NetworkClients.InGame(player))
		return false;

	// A dedicated server's reserved authority slot is only a transport endpoint.
	// It does not generate gameplay commands on any peer, so gating the playsim
	// on its sequence can freeze clients after a map load or respawn.
	if (I_IsServerReservedSlot(player))
		return false;

	// Runtime late-join clients need a warmup window so the authority can keep
	// running while they bootstrap from live snapshots.
	if ((LateJoinSyncPending & ((uint64_t)1u << player)) != 0u)
		return false;

	return true;
}

static bool Net_IsLateJoinSyncPending(int client)
{
	return client >= 0 && client < MAXPLAYERS
		&& (LateJoinSyncPending & ((uint64_t)1u << client)) != 0u;
}

static void Net_SetLateJoinSyncPending(int client, int targetSequence, int targetConsistency, const char* reason)
{
	if (client < 0 || client >= MAXPLAYERS)
		return;

	LateJoinSyncPending |= (uint64_t)1u << client;
	LateJoinSyncTargetSequence[client] = max<int>(targetSequence, -1);
	LateJoinSyncTargetConsistency[client] = max<int>(targetConsistency, -1);
	LateJoinSyncStartTic[client] = EnterTic;
	HCDEBeginActorBaselineRepair(client, reason != nullptr ? reason : "late-join");
	DebugTrace::Markf("net", "late-join sync pending client=%d room=%u gametic=%d clienttic=%d target-seq=%d target-con=%d reason=%s",
		client, unsigned(CurrentRoomID), gametic, ClientTic, LateJoinSyncTargetSequence[client], LateJoinSyncTargetConsistency[client],
		reason != nullptr ? reason : "unknown");
}

static void Net_ClearLateJoinSyncPending(int client, const char* reason)
{
	if (client < 0 || client >= MAXPLAYERS)
		return;

	if ((LateJoinSyncPending & ((uint64_t)1u << client)) == 0u)
		return;

	LateJoinSyncPending &= ~((uint64_t)1u << client);
	DebugTrace::Markf("net", "late-join sync complete client=%d room=%u gametic=%d clienttic=%d target-seq=%d target-con=%d reason=%s",
		client, unsigned(CurrentRoomID), gametic, ClientTic, LateJoinSyncTargetSequence[client], LateJoinSyncTargetConsistency[client],
		reason != nullptr ? reason : "unknown");
	LateJoinSyncTargetSequence[client] = -1;
	LateJoinSyncTargetConsistency[client] = -1;
	LateJoinSyncStartTic[client] = -1;
	ConsistencyGraceUntilTic[client] = max<int>(ConsistencyGraceUntilTic[client], gametic + TICRATE);
}

static int Net_GetCutsceneReadyHost()
{
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	if (Net_IsCutsceneReadyParticipant(authoritySlot))
		return authoritySlot;

	for (auto client : NetworkClients)
	{
		if (Net_IsCutsceneReadyParticipant(client))
			return client;
	}

	return -1;
}

bool Net_IsPlayerReady(int player)
{
	if (demoplayback || net_cutscenereadytype != RT_VOTE)
		return false;

	if (!Net_IsCutsceneReadyParticipant(player))
		return false;

	if (cutscene.runner)
	{
		int type = ST_VOTE;
		IFVM(ScreenJobRunner, GetSkipType)
			type = VMCallSingle<int>(func, cutscene.runner);

		if (type == ST_UNSKIPPABLE)
			return false;
	}

	return players[player].Bot != nullptr || (CutsceneReady & ((uint64_t)1u << player));
}

// Check if every client is ready to move on from the current cutscene.
void Net_PlayerReadiedUp(int player)
{
	if (!netgame || demoplayback)
		return;

	if (player < 0 || player >= MAXPLAYERS)
		return;

	// Ready is a toggle, so ignore quick duplicates from held keys or resent input packets.
	constexpr int readyToggleDebounce = TICRATE / 4;
	if (gametic - CutsceneReadyLastToggle[player] < readyToggleDebounce)
		return;

	CutsceneReadyLastToggle[player] = gametic;

	// Allow unreadying in case a player needs to leave momentarily.
	if (Net_IsPlayerReady(player))
		CutsceneReady &= ~((uint64_t)1u << player);
	else
		CutsceneReady |= (uint64_t)1u << player;

	DebugTrace::Markf("net", "cutscene ready toggle player=%d ready=%d room=%u map=%s gametic=%d clienttic=%d mask=0x%llx",
		player, Net_IsPlayerReady(player) ? 1 : 0, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>", gametic, ClientTic,
		static_cast<unsigned long long>(CutsceneReady));
}

void Net_StartCutscene()
{
	CutsceneReady = 0u;
	for (int i = 0; i < MAXPLAYERS; ++i)
		CutsceneReadyLastToggle[i] = gametic - TICRATE;

	float countdownSeconds = net_cutscenecountdown;
	const bool invasionMode = Net_IsInvasionModeEnabled();
	const bool shouldUseCountdown = !demoplayback
		&& (invasionMode ? sv_invasioncountdowntime > 0.0f : countdownSeconds > 0.0f)
		&& (netgame || invasionMode);
	CutsceneCountdown = shouldUseCountdown
		? (invasionMode ? Net_GetEffectiveInvasionCountdownTics() : static_cast<int>(ceil(countdownSeconds * TICRATE)))
		: 0;
	if (Net_IsInvasionModeEnabled())
		Net_SetInvasionState(INVS_COUNTDOWN, CutsceneCountdown, "cutscene-start");
}

// Allow the game to automatically start after a set amount of time.
bool Net_CheckCutsceneReady()
{
	if (!cutscene.runner)
		return false;

	int type = ST_VOTE;
	IFVM(ScreenJobRunner, GetSkipType)
		type = VMCallSingle<int>(func, cutscene.runner);

	if (type == ST_UNSKIPPABLE)
		return false;

	// Keep the countdown progressing for every skippable cutscene mode. Without
	// this, RT_ANYONE / RT_HOST_ONLY can deadlock forever when ready input never
	// survives a tic-gate stall or DEM_READIED round-trip during map transitions.
	if (CutsceneCountdown > 0)
	{
		--CutsceneCountdown;
		if (CutsceneCountdown <= 0)
		{
			DebugTrace::Markf("net", "cutscene countdown auto-advance room=%u map=%s readytype=%d",
				unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
				int(net_cutscenereadytype));
			return true;
		}
		DebugTrace::Markf("net", "cutscene countdown ticking seconds=%.3f room=%u map=%s readytype=%d",
			double(CutsceneCountdown) / TICRATE, unsigned(CurrentRoomID),
			primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
			int(net_cutscenereadytype));
		return false;
	}

	if (net_cutscenereadytype == RT_ANYONE)
	{
		for (auto client : NetworkClients)
		{
			if (Net_IsCutsceneReadyParticipant(client) && (CutsceneReady & ((uint64_t)1u << client)))
			{
				DebugTrace::Markf("net", "cutscene advance ready-anyone client=%d room=%u map=%s",
					client, unsigned(CurrentRoomID),
					primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
				return true;
			}
		}
		DebugTrace::Markf("net", "cutscene advance blocked ready-anyone room=%u map=%s",
			unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
		return false;
	}

	if (net_cutscenereadytype == RT_HOST_ONLY)
	{
		const int readyHost = Net_GetCutsceneReadyHost();
		const bool ready = readyHost >= 0 && (CutsceneReady & ((uint64_t)1u << readyHost));
		DebugTrace::Markf("net", "cutscene advance host-only host=%d ready=%d room=%u map=%s",
			readyHost, ready ? 1 : 0, unsigned(CurrentRoomID),
			primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
		return ready;
	}

	uint64_t mask = 0u;
	uint64_t readyMask = 0u;
	int totalReady = 0;
	int totalClients = 0;
	int humanReady = 0;
	int humanClients = 0;
	for (auto client : NetworkClients)
	{
		if (!Net_IsCutsceneReadyParticipant(client))
			continue;

		mask |= (uint64_t)1u << client;
		++totalClients;
		const bool ready = Net_IsPlayerReady(client);
		if (ready)
		{
			readyMask |= (uint64_t)1u << client;
			++totalReady;
		}
		if (players[client].Bot == nullptr)
		{
			++humanClients;
			if (ready)
				++humanReady;
		}
	}

	if (humanClients <= 1)
	{
		const bool ready = humanReady >= humanClients;
		DebugTrace::Markf("net", "cutscene human fast-path ready=%d/%d total=%d/%d room=%u map=%s",
			humanReady, humanClients, totalReady, totalClients, unsigned(CurrentRoomID),
			primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
		return ready;
	}

	if (totalClients <= 0)
		return true;

	if ((readyMask & mask) == mask)
		return true;

	const float readyRatio = (float)totalReady / totalClients;
	const bool thresholdReached = readyRatio >= net_cutscenereadypercent;
	DebugTrace::Markf("net", "cutscene threshold check ratio=%.3f ready=%d/%d threshold=%.3f result=%d room=%u map=%s",
		double(readyRatio), totalReady, totalClients, double(net_cutscenereadypercent), thresholdReached ? 1 : 0,
		unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");

	return thresholdReached;
}

void Net_AdvanceCutscene()
{
	CutsceneReady = 0u;
	CutsceneCountdown = 0;
	if (Net_IsInvasionModeEnabled())
		Net_StartInvasionWave("cutscene-advance");
	if (I_IsLocalHCDEServiceAuthority())
		Net_WriteInt8(DEM_ENDSCREENJOB);
}

bool Net_IsWaiting()
{
	return LagState == LAG_WAITING;
}

static void Net_ClearStaleWaitingFlags()
{
	// Waiting flags describe the current tic stream only; carrying them across a
	// room/map reset can deadlock the first command of the next map.
	for (auto client : NetworkClients)
		players[client].waiting = false;
	HCDESetAuthorityWaiting(false);
}

static void Net_ResetAuthorityWaitWatchdog(const char* reason, bool trace = true)
{
	LastGameUpdate = EnterTic;
	AuthorityWaitGraceUntil = EnterTic + MAXSENDTICS * TicDup * 3;
	Net_ClearStaleWaitingFlags();
	if (trace)
	{
		DebugTrace::Markf("net", "authority wait watchdog reset reason=%s enter=%d grace-until=%d room=%u map=%s",
			reason != nullptr ? reason : "unknown", EnterTic, AuthorityWaitGraceUntil, unsigned(CurrentRoomID),
			primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	}
}

// This is needed for handling PSprite bobbing specifically since it's predicted.
double Net_ModifyFrac(double ticFrac)
{
	return LagState < LAG_WAITING ? ticFrac : 1.0;
}

double Net_ModifyObjectFrac(DObject* obj, double ticFrac)
{
	return LagState == LAG_NONE || LagState == LAG_SKIPPING || obj->IsClientSide() ? ticFrac : 1.0;
}

double Net_ModifyParticleFrac(particle_t* part, double ticFrac)
{
	return LagState == LAG_NONE || LagState == LAG_SKIPPING ? ticFrac : 0.0;
}

void Net_ResetCommands(bool midTic)
{
	bCommandsReset = midTic;
	++CurrentRoomID;
	// A room/map change invalidates old waiting state. Leaving it set can stop
	// the first command for the next map from being generated.
	Net_ClearStaleWaitingFlags();
	LateJoinSyncPending = 0u;
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		LateJoinSyncTargetSequence[i] = -1;
		LateJoinSyncTargetConsistency[i] = -1;
		LateJoinSyncStartTic[i] = -1;
		HCDEActorBaselineRepairUntilTic[i] = 0;
		HCDEAuthorityEventReplayNextId[i] = 0u;
	}
	LastTicGateStallTrace = 0;
	SkipCommandTimer = SkipCommandAmount = CommandsAhead = 0;
	StabilityBuffer = PrevAvailableDiff = 0;
	CurStabilityTic = 0u;
	memset(StabilityTics, 0, sizeof(StabilityTics));

	int tic = gametic / TicDup;
	if (midTic)
	{
		// If we're mid ticdup cycle, make sure we immediately enter the next one after
		// the current tic we're in finishes.
		ClientTic = (tic + 1) * TicDup;
		gametic = (tic * TicDup) + (TicDup - 1);
	}
	else
	{
		ClientTic = gametic = tic * TicDup;
		--tic;
	}

	for (auto client : NetworkClients)
	{
		auto& state = ClientStates[client];
		state.Flags &= CF_QUIT;
		state.StabilityBuffer = 0u;
		state.ResendID = CurrentRoomID;
		// After a level reset every peer must agree on a single anchor sequence
		// to begin streaming from again. Previously this clamped with min(),
		// which left the authority's view of a lagging client below the new
		// map's anchor; the LST_HOST tic gate then waited for a sequence the
		// client would never resend (it only sends `>= tic + 1` going forward),
		// freezing the world on the new map's first tic. Snap both fields to
		// `tic` so the next tic can be produced as soon as the client delivers
		// its first post-reset command.
		state.CurrentSequence = tic;
		state.SequenceAck = tic;
		// Any in-flight resend window targeted the previous map's sequence
		// space. Carrying it across the room reset would either re-send stale
		// pre-reset commands (which the peer treats as duplicates and ignores)
		// or block fresh sequences behind a phantom retransmit. Clear both
		// resend cursors unconditionally so the next NetUpdate streams the
		// post-reset tics starting at `tic + 1`.
		state.ResendSequenceFrom = -1;
		state.ResendConsistencyFrom = -1;

		// Make sure not to run its current command either.
		auto& curTic = state.Tics[tic % BACKUPTICS];
		const int running = (curTic.Command.buttons & BT_RUN); // This isn't delta'd so needs to be kept.
		memset(&curTic.Command, 0, sizeof(curTic.Command));
		curTic.Command.buttons |= running;
	}

	NetEvents.ResetStream();
	Net_ResetInvasionState("reset-commands");
	HCDEMovementResetJitter();
	HCDERewind_Reset("reset-commands");
}

void Net_SetWaiting()
{
	if (netgame && !demoplayback && Net_HasPlayableNetworkClients())
	{
		LevelStartAck = 0u;
		LevelStartDelay = LevelStartDebug = 0;
		FullLatencyCycle = 0;
		Net_ResetAuthorityWaitWatchdog("level-wait");
		Net_SetLevelStartStatus(LST_WAITING, "map-load-wait");
		DebugTrace::Markf("net.levelstart", "level start waiting room=%u map=%s authority=%d transport-clients=%u playable-clients=%u",
			unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
			I_IsLocalHCDEServiceAuthority() ? 1 : 0, unsigned(NetworkClients.Size()), Net_PlayableNetworkClientCount());
	}
}

// [RH] Rewritten to properly calculate the packet size
//		with our variable length Command.
static size_t GetNetBufferSize()
{
	if (NetBufferLength == 0u)
		return 0u;

	if (NetBuffer[0] & NCMD_EXIT)
		return 1 + I_IsHCDEServiceAuthoritySlot(RemoteClient);

	// Setup/control messages are variable-sized. We still validate the common
	// minimum header here so obviously truncated packets are rejected.
	if (NetBuffer[0] & NCMD_SETUP)
	{
		if (NetBufferLength < 2u)
			return 2u;
		return NetBufferLength;
	}
	if (NetBuffer[0] & (NCMD_LATENCY | NCMD_LATENCYACK))
		return 2;
	if (HCDELiveLooksLikePacket())
		return NetBufferLength;

	if (NetBuffer[0] & NCMD_LEVELREADY)
	{
		// Level start carries the authority's starting gametic (4 bytes) so clients
		// can align their local gametic and sequence anchors exactly, preventing
		// the post-load "availableTics == 0" stall caused by local init ticks.
		int bytes = 2;
		if (I_IsHCDEServiceAuthoritySlot(RemoteClient))
			bytes += 6;

		return bytes;
	}

	// Header info
	size_t totalBytes = 10u;
	if (NetBufferLength < totalBytes)
		return totalBytes;

	if (NetBuffer[0] & NCMD_QUITTERS)
	{
		const size_t quitterCountBytes = size_t(NetBuffer[totalBytes]) + 1u;
		totalBytes += quitterCountBytes;
		if (NetBufferLength < totalBytes)
			return totalBytes;
	}

	if (NetBufferLength < totalBytes + 1u)
		return totalBytes + 1u;
	const int playerCount = NetBuffer[totalBytes++];

	if (NetBufferLength < totalBytes + 1u)
		return totalBytes + 1u;
	const int numTics = NetBuffer[totalBytes++];
	if (numTics > 0)
	{
		totalBytes += 4u;
		if (NetBufferLength < totalBytes)
			return totalBytes;
	}

	if (NetBufferLength < totalBytes + 1u)
		return totalBytes + 1u;
	const int ranTics = NetBuffer[totalBytes++];
	if (ranTics > 0)
	{
		totalBytes += 4u;
		if (NetBufferLength < totalBytes)
			return totalBytes;
	}
	// Stability buffer/commands ahead
	++totalBytes;
	if (NetBufferLength < totalBytes)
		return totalBytes;

	// Minimum additional packet size per player:
	// 1 byte for player number
	// If from the host, 2 bytes for the latency to the host
	size_t padding = 1u;
	if (I_IsHCDEServiceAuthoritySlot(RemoteClient))
		padding += 2u;
	if (NetBufferLength < totalBytes + playerCount * padding)
		return totalBytes + playerCount * padding;

	TArrayView<uint8_t> skipper = TArrayView(&NetBuffer[totalBytes], NetBufferLength - totalBytes);
	for (int p = 0; p < playerCount; ++p)
	{
		if (skipper.Size() < 1u)
			return NetBufferLength + 1u;
		AdvanceStream(skipper, 1u); // player number
		if (I_IsHCDEServiceAuthoritySlot(RemoteClient))
		{
			if (skipper.Size() < 2u)
				return NetBufferLength + 1u;
			AdvanceStream(skipper, 2u);
		}

		for (int i = 0; i < ranTics; ++i)
		{
			if (skipper.Size() < 3u)
				return NetBufferLength + 1u;
			AdvanceStream(skipper, 3u);
		}

		uint64_t commandOffsetsSeen = 0u;
		for (int i = 0; i < numTics; ++i)
		{
			if (skipper.Size() < 1u)
				return NetBufferLength + 1u;
			const uint8_t commandOffset = skipper[0];
			if (commandOffset >= numTics || commandOffset >= 64u)
				return NetBufferLength + 1u;
			const uint64_t commandMask = uint64_t(1u) << commandOffset;
			if ((commandOffsetsSeen & commandMask) != 0u)
				return NetBufferLength + 1u;
			commandOffsetsSeen |= commandMask;
			AdvanceStream(skipper, 1u); // sequence offset byte

			size_t commandPayloadBytes = 0u;
			if (!Net_TrySkipUserCmdMessageWithBoundary(skipper.Data(), skipper.Size(),
				i + 1 == numTics, uint8_t(numTics), commandOffsetsSeen,
				uint8_t(numTics - i - 1), commandPayloadBytes)
				|| commandPayloadBytes > skipper.Size())
			{
				return NetBufferLength + 1u;
			}
			AdvanceStream(skipper, commandPayloadBytes);
		}
	}

	return int(skipper.Data() - NetBuffer);
}

//
// HSendPacket
//
static void HSendPacket(int client, size_t size)
{
	// This data already exists locally in the demo file, so don't write it out.
	if (demoplayback)
		return;

	if (netgame && size > 0u)
		Net_TraceOutgoingPacket(client, NetBuffer[0], size);

	RemoteClient = client;
	NetBufferLength = size;
	if (client == consoleplayer)
	{
		memcpy(LocalNetBuffer, NetBuffer, size);
		LocalNetBufferSize = size;
		return;
	}

	if (!netgame)
		I_Error("Tried to send a packet to a client while offline");

	I_NetCmd(CMD_SEND);
}

// HGetPacket
// Returns false if no packet is waiting
static bool HGetPacket()
{
	if (demoplayback)
		return false;

	if (LocalNetBufferSize)
	{
		memcpy(NetBuffer, LocalNetBuffer, LocalNetBufferSize);
		NetBufferLength = LocalNetBufferSize;
		RemoteClient = consoleplayer;
		LocalNetBufferSize = 0u;
		return true;
	}

	if (!netgame)
		return false;

	I_NetCmd(CMD_GET);
	if (RemoteClient == -1)
		return false;

	size_t sizeCheck = GetNetBufferSize();
	if (NetBufferLength != sizeCheck)
	{
		Printf("Incorrect packet size %zu (expected %zu)\n", NetBufferLength, sizeCheck);
		return false;
	}

	return true;
}

static void ClientConnecting(int client)
{
	if (!I_IsLocalHCDEServiceAuthority())
		return;

	if (client < 0 || client >= int(MAXPLAYERS))
		return;

	if (!NetworkClients.InGame(client) || I_IsHCDEServiceAuthoritySlot(client))
		return;

	auto& state = ClientStates[client];
	const int currentSequence = max<int>(ClientTic / max<int>(TicDup, 1), 0);
	const int currentConsistency = max<int>(CurrentConsistency, 0);
	const int replayWindow = max<int>(MAXSENDTICS / 2, 1);
	if (Net_IsLateJoinSyncPending(client))
	{
		// Keep replay pressure active while this client is still catching up.
		state.Flags |= CF_RETRANSMIT;
		return;
	}

	// Stage 2 late-join sync: seed a bounded replay window and hold this client
	// in a non-gating warmup state until first live gameplay packets arrive.
	state.Flags |= CF_RETRANSMIT;
	state.ResendSequenceFrom = min<int>(state.ResendSequenceFrom >= 0 ? state.ResendSequenceFrom : currentSequence,
		max<int>(currentSequence - replayWindow, 0));
	state.ResendConsistencyFrom = min<int>(state.ResendConsistencyFrom >= 0 ? state.ResendConsistencyFrom : currentConsistency,
		max<int>(currentConsistency - replayWindow, 0));

	// A newly admitted runtime client is now an active participant.
	if (!I_IsServerReservedSlot(client))
	{
		playeringame[client] = true;
		if (players[client].playerstate == PST_GONE || players[client].playerstate == PST_DEAD)
			SET_PLAYER_STATE(&players[client], client, PST_ENTER, "runtime_connect_admitted");
	}

	Net_SetLateJoinSyncPending(client, max<int>(currentSequence - 1, 0), max<int>(currentConsistency - 1, 0), "runtime-connect");
	if (!players[client].waiting)
	{
		players[client].waiting = true;
		Net_ResetAuthorityWaitWatchdog("late-join-connect");
		DebugTrace::Infof("player", "late-join connect client=%d room=%u gametic=%d clienttic=%d replay-seq=%d replay-con=%d",
			client, unsigned(CurrentRoomID), gametic, ClientTic, state.ResendSequenceFrom, state.ResendConsistencyFrom);
	}
}

static void Net_EnsureRuntimeClientSlot(int client, int sourceClient)
{
	if (client < 0 || client >= MAXPLAYERS)
		return;

	const bool wasKnown = NetworkClients.InGame(client);
	if (!wasKnown)
	{
		NetworkClients += client;
		DebugTrace::Infof("player", "runtime slot activated client=%d source=%d room=%u gametic=%d clienttic=%d",
			client, sourceClient, unsigned(CurrentRoomID), gametic, ClientTic);
	}

	const bool reserved = I_IsServerReservedSlot(client);
	playeringame[client] = !reserved;
	if (!reserved && (players[client].playerstate == PST_GONE || players[client].playerstate == PST_DEAD))
	{
		SET_PLAYER_STATE(&players[client], client, PST_ENTER, "runtime_slot_activated");
	}
}

void Net_ResetClientState(int clientNum)
{
	auto& state = ClientStates[clientNum];
	state.CurrentLatency = 0u;
	state.bNewLatency = true;
	state.AverageLatency = 0u;
	memset(state.SentTime, 0, sizeof(state.SentTime));
	memset(state.RecvTime, 0, sizeof(state.RecvTime));

	state.Flags = 0;
	state.StabilityBuffer = 0u;
	state.ResendID = 0u;
	state.ResendSequenceFrom = -1;
	state.SequenceAck = -1;
	state.CurrentSequence = -1;
	state.ResendConsistencyFrom = -1;
	state.ConsistencyAck = -1;
	state.LastVerifiedConsistency = -1;
	state.CurrentNetConsistency = -1;
	memset(state.NetConsistency, 0, sizeof(state.NetConsistency));
	memset(state.LocalConsistency, 0, sizeof(state.LocalConsistency));
	state.MalformedPacketStrikes = 0u;
	state.MalformedWindowStartMS = 0u;
	ConsistencyGraceUntilTic[clientNum] = 0;
	HCDEPassiveResendLastSequenceAck[clientNum] = -1;
	HCDEPassiveResendAckChangedTic[clientNum] = 0;
	HCDEPassiveResendLastRequestTic[clientNum] = 0;
	HCDEActorDeltaV2ReceiveCompactNextTic[clientNum] = 0;

	for (auto& tic : state.Tics)
		tic.Data.SetData(nullptr, 0);

	HCDELivePeers[clientNum].Clear();
	Net_ResetHCDEReplicatedActorBaseline(clientNum);
	HCDEClearActorBaselineRepair(clientNum, "reset-client-state");
}

static void DisconnectClient(int clientNum)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
	{
		Printf(PRINT_HIGH, "NetGame:: Ignored disconnect for invalid client %d at gametic=%d clienttic=%d room=%u\n",
			clientNum, gametic, ClientTic, unsigned(CurrentRoomID));
		DebugTrace::Warningf("net", "ignored invalid disconnect client=%d gametic=%d clienttic=%d room=%u",
			clientNum, gametic, ClientTic, unsigned(CurrentRoomID));
		return;
	}

	const auto& state = ClientStates[clientNum];
	Printf(PRINT_HIGH, "NetGame:: Disconnecting client %d '%s' at gametic=%d clienttic=%d room=%u map=%s seq=%d ack=%d consistency=%d remoteConsistency=%d\n",
		clientNum, players[clientNum].userinfo.GetName(), gametic, ClientTic, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		state.CurrentSequence, state.SequenceAck, CurrentConsistency, state.CurrentNetConsistency);
	DebugTrace::Warningf("net", "disconnect client=%d name=%s gametic=%d clienttic=%d room=%u map=%s seq=%d ack=%d consistency=%d remoteConsistency=%d",
		clientNum, players[clientNum].userinfo.GetName(), gametic, ClientTic, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		state.CurrentSequence, state.SequenceAck, CurrentConsistency, state.CurrentNetConsistency);

	NetworkClients -= clientNum;
	LateJoinSyncPending &= ~((uint64_t)1u << clientNum);
	LateJoinSyncTargetSequence[clientNum] = -1;
	LateJoinSyncTargetConsistency[clientNum] = -1;
	LateJoinSyncStartTic[clientNum] = -1;
	HCDEClearActorBaselineRepair(clientNum, "disconnect");
	const uint64_t mask = ~((uint64_t)1u << clientNum);
	MutedClients &= mask;
	CutsceneReady &= mask;
	LevelStartAck &= mask;
	playeringame[clientNum] = false;
	players[clientNum].waiting = false;
	players[clientNum].inconsistant = false;
	players[clientNum].settings_controller = false;
	I_ClearClient(clientNum);
	Net_ResetClientState(clientNum);
	// Capture the pawn leaving in the next world tick.
	SET_PLAYER_STATE(&players[clientNum], clientNum, PST_GONE, "Net_DisconnectClient");
}

static void SetArbitrator(int clientNum)
{
	Net_Arbitrator = clientNum;
	HCDEInitializeAuthoritySession();
	Printf("%s is the new authority\n", HCDEAuthorityDisplayName());

	for (auto client : NetworkClients)
		ClientStates[client].AverageLatency = 0u;
	Net_ResetCommands(false);
	Net_SetWaiting();
}

static int HCDESelectNextServiceAuthoritySlot(int leavingAuthority)
{
	for (auto client : NetworkClients)
	{
		if (client == leavingAuthority || I_IsServerReservedSlot(client))
			continue;

		return client;
	}

	for (auto client : NetworkClients)
	{
		if (client != leavingAuthority)
			return client;
	}

	return leavingAuthority;
}

static void ClientQuit(int clientNum, int newHost)
{
	if (!NetworkClients.InGame(clientNum))
		return;

	// This will get caught in the main loop and send it out to everyone as one big packet. The only
	// exception is the host who will leave instantly and send out any needed data.
	if (!I_IsHCDEServiceAuthoritySlot(clientNum))
	{
		if (!I_IsLocalHCDEServiceAuthority())
		{
			Printf(PRINT_HIGH, "NetGame:: Ignored unexpected disconnect packet from non-authority client %d at gametic=%d clienttic=%d room=%u\n",
				clientNum, gametic, ClientTic, unsigned(CurrentRoomID));
			DebugTrace::Warningf("net", "unexpected disconnect packet client=%d gametic=%d clienttic=%d room=%u",
				clientNum, gametic, ClientTic, unsigned(CurrentRoomID));
			DPrintf(DMSG_WARNING, "Received disconnect packet from client %d erroneously\n", clientNum);
		}
		else
		{
			Printf(PRINT_HIGH, "NetGame:: Client %d '%s' sent exit; queueing quit broadcast at gametic=%d clienttic=%d room=%u\n",
				clientNum, players[clientNum].userinfo.GetName(), gametic, ClientTic, unsigned(CurrentRoomID));
			DebugTrace::Infof("player", "exit queued client=%d name=%s gametic=%d room=%u",
				clientNum, players[clientNum].userinfo.GetName(), gametic, ClientTic, unsigned(CurrentRoomID));
			ClientStates[clientNum].Flags |= CF_QUIT;
		}

		return;
	}

	DisconnectClient(clientNum);
	if (I_IsHCDEServiceAuthoritySlot(clientNum))
	{
		const bool validReplacement = NetworkClients.InGame(newHost)
			&& newHost != clientNum
			&& !I_IsServerReservedSlot(newHost);
		SetArbitrator(validReplacement ? newHost : HCDESelectNextServiceAuthoritySlot(clientNum));
	}

	if (demorecording)
		G_CheckDemoStatus();
}

static bool IsMapLoaded()
{
	return gamestate == GS_LEVEL;
}

static uint64_t LevelStartPlayableMask()
{
	uint64_t mask = 0u;
	for (auto pNum : NetworkClients)
	{
		if (!I_IsHCDEServiceAuthoritySlot(pNum) && !Net_IsLateJoinSyncPending(pNum))
			mask |= (uint64_t)1u << pNum;
	}
	return mask;
}

static ELevelStartStatus Net_AuthorityLevelStartStatusAfterRelease()
{
	// Server-authoritative model: no UZDoom listen-host lockstep sequence gate.
	(void)I_IsLocalHCDEServiceAuthority();
	return LST_READY;
}

static unsigned Net_PlayableNetworkClientCount()
{
	unsigned count = 0u;
	for (auto pNum : NetworkClients)
	{
		if (!I_IsHCDEServiceAuthoritySlot(pNum) && !Net_IsLateJoinSyncPending(pNum))
			++count;
	}
	return count;
}

static bool Net_HasPlayableNetworkClients()
{
	return Net_PlayableNetworkClientCount() > 0u;
}

static bool TryReleaseLevelStart()
{
	const uint64_t mask = LevelStartPlayableMask();
	if (mask == 0u || (LevelStartAck & mask) != mask || !IsMapLoaded())
		return false;

	// Beyond this point a player is likely lagging out anyway.
	constexpr uint16_t LatencyCap = 350u;
	uint16_t highestAvg = 0u;
	for (auto client : NetworkClients)
	{
		if (I_IsHCDEServiceAuthoritySlot(client))
			continue;

		const uint16_t latency = min<uint16_t>(ClientStates[client].AverageLatency, LatencyCap);
		if (latency > highestAvg)
			highestAvg = latency;
	}

	NetBuffer[0] = NCMD_LEVELREADY;
	NetBuffer[1] = CurrentRoomID;
	constexpr double MS2Sec = 1.0 / 1000.0;
	for (auto client : NetworkClients)
	{
		int delay = 0;
		if (!I_IsHCDEServiceAuthoritySlot(client))
			delay = int(floor((highestAvg - min<uint16_t>(ClientStates[client].AverageLatency, LatencyCap)) * MS2Sec * TICRATE));

		NetBuffer[2] = delay >> 8;
		NetBuffer[3] = delay;
		NetBuffer[4] = gametic >> 24;
		NetBuffer[5] = gametic >> 16;
		NetBuffer[6] = gametic >> 8;
		NetBuffer[7] = gametic;

		HSendPacket(client, 8);
	}

	LevelStartAck = 0u;
	Net_SetLevelStartStatus(Net_AuthorityLevelStartStatusAfterRelease(), "try-release-level-start");
	LevelStartDelay = LevelStartDebug = 0;

	// NOTE: Do not re-anchor ClientStates[*].CurrentSequence here. The authority's
	// view of each client's last delivered sequence is maintained authoritatively
	// by Net_ResetCommands (transitions) or by the default -1 (cold start). Setting
	// it to `gametic/TicDup` here makes the authority "ahead of" what the client
	// will actually send next, which causes every fresh client packet to be
	// rejected as a duplicate -- producing the "net stall on launch" deadlock
	// where the authority never advances `lowestSeq` and the client never
	// receives any sequence updates back.
	Net_ResetAuthorityWaitWatchdog("authority-release");
	Printf(PRINT_HIGH, "NetGame:: Authority released level start at gametic=%d clienttic=%d room=%u map=%s transport-clients=%u playable-clients=%u\n",
		gametic, ClientTic, unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		unsigned(NetworkClients.Size()), Net_PlayableNetworkClientCount());
	DebugTrace::Markf("net.levelstart", "authority released level start gametic=%d clienttic=%d room=%u map=%s transport-clients=%u playable-clients=%u",
		gametic, ClientTic, unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		unsigned(NetworkClients.Size()), Net_PlayableNetworkClientCount());
	return true;
}

static void CheckLevelStart(int client, int delayTics)
{
	if (LevelStartStatus != LST_WAITING)
	{
		if (I_IsLocalHCDEServiceAuthority() && client != consoleplayer)
		{
			// Someone might've missed the previous packet, so resend it just in case.
			NetBuffer[0] = NCMD_LEVELREADY;
			NetBuffer[1] = CurrentRoomID;
			NetBuffer[2] = 0;
			NetBuffer[3] = 0;
			NetBuffer[4] = gametic >> 24;
			NetBuffer[5] = gametic >> 16;
			NetBuffer[6] = gametic >> 8;
			NetBuffer[7] = gametic;

			HSendPacket(client, 8);
		}

		return;
	}

	if (I_IsHCDEServiceAuthoritySlot(client))
	{
		LevelStartAck = 0u;
		Net_SetLevelStartStatus(Net_AuthorityLevelStartStatusAfterRelease(), "check-level-start-authority");
		LevelStartDelay = LevelStartDebug = delayTics;

		const int serverGametic = (NetBuffer[4] << 24) | (NetBuffer[5] << 16) | (NetBuffer[6] << 8) | NetBuffer[7];
		if (gametic != serverGametic)
		{
			// Only realign the playsim clock; leave ClientTic and the per-peer
			// CurrentSequence/SequenceAck alone. Between the local
			// Net_ResetCommands(true) and the authority's NCMD_LEVELREADY the
			// client has been allowed to keep building and shipping commands
			// (NetUpdate runs G_BuildTiccmd and the send loop unconditionally),
			// so the authority's view of this client's CurrentSequence has
			// almost certainly advanced past the post-reset anchor. Rewinding
			// ClientTic (or stamping CurrentSequence back to `tic`) would make
			// the next outbound command duplicate sequences the authority has
			// already accepted, which then deadlocks the world on the new map.
			// gametic, on the other hand, only advances inside the LST_HOST/READY
			// playsim gate and is safe to snap to the authority's value here.
			Printf(PRINT_HIGH, "NetGame:: Aligning local gametic=%d to server gametic=%d\n", gametic, serverGametic);
			gametic = serverGametic;
		}

		Net_ResetAuthorityWaitWatchdog("authority-start");
		Printf(PRINT_HIGH, "NetGame:: Authority started level for client at gametic=%d clienttic=%d room=%u map=%s delay=%d\n",
			gametic, ClientTic, unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>", delayTics);
		DebugTrace::Markf("net.levelstart", "client accepted authority level start gametic=%d clienttic=%d room=%u map=%s delay=%d",
			gametic, ClientTic, unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>", delayTics);
		return;
	}

	LevelStartAck |= (uint64_t)1u << client;
	DebugTrace::Markf("net.levelstart", "level start ack client=%d ack=%llu mask=%llu loaded=%d room=%u",
		client, (unsigned long long)LevelStartAck, (unsigned long long)LevelStartPlayableMask(),
		IsMapLoaded() ? 1 : 0, unsigned(CurrentRoomID));
	TryReleaseLevelStart();
}

// Repeated malformed gameplay packets from the same peer are grouped in a short
// window so hostile/broken traffic cannot keep forcing parser retries forever.
static void NoteMalformedGameplayPacket(int clientNum, const char* reason)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;

	auto& state = ClientStates[clientNum];
	const uint64_t now = I_msTime();
	constexpr uint64_t MalformedStrikeWindowMS = 5000u;
	constexpr uint32_t MalformedStrikeLimit = 6u;
	if (state.MalformedWindowStartMS == 0u || now < state.MalformedWindowStartMS
		|| now - state.MalformedWindowStartMS > MalformedStrikeWindowMS)
	{
		state.MalformedWindowStartMS = now;
		state.MalformedPacketStrikes = 0u;
	}

	++state.MalformedPacketStrikes;
	DebugTrace::Warningf("net.session", "malformed gameplay packet from=%d reason=%s strikes=%u window-ms=%llu room=%u",
		clientNum, reason, unsigned(state.MalformedPacketStrikes),
		static_cast<unsigned long long>(now - state.MalformedWindowStartMS),
		unsigned(CurrentRoomID));

	if (state.MalformedPacketStrikes < MalformedStrikeLimit)
		return;

	state.MalformedPacketStrikes = 0u;
	state.MalformedWindowStartMS = now;
	if (!I_IsLocalHCDEServiceAuthority() || I_IsHCDEServiceAuthoritySlot(clientNum))
	{
		DebugTrace::Warningf("net.session", "malformed packet strike limit reached but peer is authority/not locally kickable client=%d room=%u",
			clientNum, unsigned(CurrentRoomID));
		return;
	}

	Printf(PRINT_HIGH, "NetGame:: Disconnecting client %d '%s' for repeated malformed gameplay packets at gametic=%d clienttic=%d room=%u\n",
		clientNum, players[clientNum].userinfo.GetName(), gametic, ClientTic, unsigned(CurrentRoomID));
	DebugTrace::Warningf("net", "disconnecting malformed-packet source client=%d name=%s gametic=%d clienttic=%d room=%u",
		clientNum, players[clientNum].userinfo.GetName(), gametic, ClientTic, unsigned(CurrentRoomID));
	ClientStates[clientNum].Flags |= CF_QUIT;
}

struct FLatencyAck
{
	int Client;
	uint8_t Seq;

	FLatencyAck(int client, uint8_t seq) : Client(client), Seq(seq) {}
};

//
// GetPackets
//
static void GetPackets()
{
	TArray<FLatencyAck> latencyAcks = {};
	while (HGetPacket())
	{
		const int clientNum =  RemoteClient;
		auto& clientState = ClientStates[clientNum];
		Net_BlackboxRecordPacket(1, clientNum, 0u, 0u, 0u, uint8_t(CurrentRoomID), NetBuffer, NetBufferLength);
		Net_TraceIncomingPacket(clientNum, NetBuffer[0], NetBufferLength);

		if (NetBuffer[0] & NCMD_EXIT)
		{
			Printf(PRINT_HIGH, "NetGame:: Received exit packet from client %d at gametic=%d clienttic=%d room=%u\n",
				clientNum, gametic, ClientTic, unsigned(CurrentRoomID));
			DebugTrace::Warningf("net.session", "received exit packet client=%d gametic=%d clienttic=%d room=%u",
				clientNum, gametic, ClientTic, unsigned(CurrentRoomID));
			ClientQuit(clientNum, I_IsHCDEServiceAuthoritySlot(clientNum) ? NetBuffer[1] : -1);
			continue;
		}

		if (NetBuffer[0] & NCMD_SETUP)
		{
			HandleIncomingConnection();
			ClientConnecting(clientNum);
			continue;
		}

		if (NetBuffer[0] & NCMD_LATENCY)
		{
			size_t i = 0u;
			for (; i < latencyAcks.Size(); ++i)
			{
				if (latencyAcks[i].Client == clientNum)
					break;
			}

			if (i >= latencyAcks.Size())
				latencyAcks.Push({ clientNum, NetBuffer[1] });

			continue;
		}

		if (NetBuffer[0] & NCMD_LATENCYACK)
		{
			if (NetBuffer[1] == clientState.CurrentLatency)
			{
				clientState.RecvTime[clientState.CurrentLatency++ % MAXSENDTICS] = I_msTime();
				clientState.bNewLatency = true;
			}

			continue;
		}

		if (NetBuffer[0] & NCMD_LEVELREADY)
		{
			if (NetBuffer[1] == CurrentRoomID)
			{
				int delay = 0;
				if (I_IsHCDEServiceAuthoritySlot(clientNum))
					delay = (NetBuffer[2] << 8) | NetBuffer[3];

				CheckLevelStart(clientNum, delay);
			}

			continue;
		}

		if (HCDELiveLooksLikePacket())
		{
			if (HandleHCDELivePacket(clientNum))
				continue;
		}

		if (HCDEEnforcesNativeGameplayForPeer(clientNum))
		{
			HCDERejectLegacyGameplayPeer(clientNum, "recv", I_ClientUsesHCDEService(clientNum) ? "raw-ncmd-from-hcde-peer" : "raw-ncmd-from-non-hcde-peer");
			continue;
		}

		if (NetBuffer[0] & NCMD_RETRANSMIT)
		{
			clientState.ResendID = NetBuffer[1];
			clientState.Flags |= CF_RETRANSMIT;
		}

		// Command packets must include room id, sequence ack, and consistency ack.
		// Treat undersized packets as missing/corrupt and request retransmit.
		if (NetBufferLength < 10u)
		{
			NoteMalformedGameplayPacket(clientNum, "header-too-short");
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}

		const int consistencyAck = (NetBuffer[6] << 24) | (NetBuffer[7] << 16) | (NetBuffer[8] << 8) | NetBuffer[9];
		const bool validID = NetBuffer[1] == CurrentRoomID;
		if (validID)
		{
			clientState.Flags |= CF_UPDATED;
			clientState.SequenceAck = (NetBuffer[2] << 24) | (NetBuffer[3] << 16) | (NetBuffer[4] << 8) | NetBuffer[5];
			if (I_IsLocalHCDEServiceAuthority() && Net_IsLateJoinSyncPending(clientNum))
			{
				const int targetSeq = LateJoinSyncTargetSequence[clientNum];
				const int targetCon = LateJoinSyncTargetConsistency[clientNum];
				const bool seqReady = targetSeq < 0 || clientState.SequenceAck >= targetSeq;
				const bool conReady = targetCon < 0 || consistencyAck >= targetCon;
				constexpr int LateJoinPromotionTimeoutTics = MAXSENDTICS * 12;
				const bool timedOut = LateJoinSyncStartTic[clientNum] >= 0
					&& EnterTic - LateJoinSyncStartTic[clientNum] >= LateJoinPromotionTimeoutTics;

				if (!seqReady)
				{
					const int resendFrom = max<int>(clientState.SequenceAck + 1, 0);
					if (clientState.ResendSequenceFrom < 0 || clientState.ResendSequenceFrom > resendFrom)
						clientState.ResendSequenceFrom = resendFrom;
					clientState.Flags |= CF_RETRANSMIT_SEQ;
				}
				if (!conReady)
				{
					const int resendFrom = max<int>(consistencyAck + 1, 0);
					if (clientState.ResendConsistencyFrom < 0 || clientState.ResendConsistencyFrom > resendFrom)
						clientState.ResendConsistencyFrom = resendFrom;
					clientState.Flags |= CF_RETRANSMIT_CON;
				}

				if (seqReady && conReady)
				{
					players[clientNum].waiting = false;
					Net_ClearLateJoinSyncPending(clientNum, "acks-caught-up");
				}
				else if (timedOut)
				{
					DebugTrace::Warningf("net", "late-join sync promotion timed out client=%d seq=%d/%d con=%d/%d room=%u",
						clientNum, clientState.SequenceAck, targetSeq, consistencyAck, targetCon, unsigned(CurrentRoomID));
					players[clientNum].waiting = false;
					Net_ClearLateJoinSyncPending(clientNum, "promotion-timeout");
				}
			}
			if (!I_IsLocalHCDEServiceAuthority() && I_IsHCDEServiceAuthoritySlot(clientNum))
			{
				const bool wasWaiting = players[clientNum].waiting;
				Net_ResetAuthorityWaitWatchdog("authority-packet", wasWaiting);
				if (wasWaiting)
				{
					DebugTrace::Markf("net", "authority wait cleared by current-room packet client=%d seq=%d ack=%d room=%u",
						clientNum, clientState.CurrentSequence, clientState.SequenceAck, unsigned(CurrentRoomID));
				}
			}
		}
		else
		{
			// Room ids isolate map transitions. Parsing stale-room command payloads
			// can incorrectly raise missing-sequence/consistency flags during level
			// changes, which then stalls synchronization in the new room.
			DebugTrace::Markf("net", "ignored stale-room packet client=%d packet-room=%u current-room=%u gametic=%d clienttic=%d",
				clientNum, unsigned(NetBuffer[1]), unsigned(CurrentRoomID), gametic, ClientTic);
			continue;
		}

		int curByte = 10;
		auto hasBytes = [&](int count) -> bool
		{
			return count >= 0 && curByte >= 0 && curByte + count <= int(NetBufferLength);
		};
		bool malformedPacketFields = false;
		if (NetBuffer[0] & NCMD_QUITTERS)
		{
			if (!hasBytes(1))
			{
				NoteMalformedGameplayPacket(clientNum, "quitters-count-missing");
				clientState.Flags |= CF_MISSING_SEQ;
				continue;
			}
			int numPlayers = NetBuffer[curByte++];
			for (int i = 0; i < numPlayers; ++i)
			{
				if (!hasBytes(1))
				{
					malformedPacketFields = true;
					break;
				}
				const int quitter = NetBuffer[curByte++];
				Printf(PRINT_HIGH, "NetGame:: Authority reported client %d quit at gametic=%d clienttic=%d room=%u\n",
					quitter, gametic, ClientTic, unsigned(CurrentRoomID));
				DebugTrace::Warningf("net", "authority quit broadcast client=%d from=%d gametic=%d clienttic=%d room=%u",
					quitter, clientNum, gametic, ClientTic, unsigned(CurrentRoomID));
				DisconnectClient(quitter);
			}
			if (malformedPacketFields)
			{
				NoteMalformedGameplayPacket(clientNum, "quitters-entry-malformed");
				clientState.Flags |= CF_MISSING_SEQ;
				continue;
			}
		}

		if (!hasBytes(1))
		{
			NoteMalformedGameplayPacket(clientNum, "player-count-missing");
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}
		const int playerCount = NetBuffer[curByte++];

		int baseSequence = -1;
		if (!hasBytes(1))
		{
			NoteMalformedGameplayPacket(clientNum, "tic-count-missing");
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}
		const int totalTics = NetBuffer[curByte++];
		if (totalTics > 0)
		{
			if (!hasBytes(4))
			{
				NoteMalformedGameplayPacket(clientNum, "base-sequence-missing");
				clientState.Flags |= CF_MISSING_SEQ;
				continue;
			}
			baseSequence = (NetBuffer[curByte++] << 24) | (NetBuffer[curByte++] << 16) | (NetBuffer[curByte++] << 8) | NetBuffer[curByte++];
		}

		int baseConsistency = -1;
		if (!hasBytes(1))
		{
			NoteMalformedGameplayPacket(clientNum, "consistency-count-missing");
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}
		const int ranTics = NetBuffer[curByte++];
		if (ranTics > 0)
		{
			if (!hasBytes(4))
			{
				NoteMalformedGameplayPacket(clientNum, "base-consistency-missing");
				clientState.Flags |= CF_MISSING_SEQ;
				continue;
			}
			baseConsistency = (NetBuffer[curByte++] << 24) | (NetBuffer[curByte++] << 16) | (NetBuffer[curByte++] << 8) | NetBuffer[curByte++];
		}

		if (!hasBytes(1))
		{
			NoteMalformedGameplayPacket(clientNum, "stability-byte-missing");
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}
		if (I_IsHCDEServiceAuthoritySlot(clientNum))
			CommandsAhead = NetBuffer[curByte];
		else if (I_IsLocalHCDEServiceAuthority())
			clientState.StabilityBuffer = NetBuffer[curByte];
		++curByte;

		for (int p = 0; p < playerCount; ++p)
		{
			if (!hasBytes(1))
			{
				malformedPacketFields = true;
				break;
			}
			const int pNum = NetBuffer[curByte++];
			if (pNum < 0 || pNum >= MAXPLAYERS)
			{
				malformedPacketFields = true;
				DebugTrace::Warningf("net.session", "malformed packet player index=%d max=%d from=%d room=%u",
					pNum, MAXPLAYERS, clientNum, unsigned(CurrentRoomID));
				break;
			}
			// Only trust authority packets to activate runtime player slots.
			// Non-authority peers should never be able to invent live players.
			if (I_IsHCDEServiceAuthoritySlot(clientNum))
				Net_EnsureRuntimeClientSlot(pNum, clientNum);
			auto& pState = ClientStates[pNum];

			// This gets sent over per-player so latencies are correctly displayed.
			if (I_IsHCDEServiceAuthoritySlot(clientNum))
			{
				if (!hasBytes(2))
				{
					malformedPacketFields = true;
					break;
				}
				if (!I_IsLocalHCDEServiceAuthority())
					pState.AverageLatency = (NetBuffer[curByte++] << 8) | NetBuffer[curByte++];
				else
					curByte += 2;
			}

			// Make sure the host doesn't update a player's last consistency ack with their own data.
			if (!I_IsLocalHCDEServiceAuthority()
				|| I_IsHCDEServiceAuthoritySlot(pNum) || !I_IsHCDEServiceAuthoritySlot(clientNum))
			{
				pState.ConsistencyAck = consistencyAck;
			}

			TArray<int16_t> consistencies = {};
			for (int r = 0; r < ranTics; ++r)
			{
				if (!hasBytes(3))
				{
					malformedPacketFields = true;
					break;
				}
				int ofs = NetBuffer[curByte++];
				consistencies.Insert(ofs, (NetBuffer[curByte++] << 8) | NetBuffer[curByte++]);
			}
			if (malformedPacketFields)
			{
				break;
			}

			for (size_t i = 0u; i < consistencies.Size(); ++i)
			{
				const int cTic = baseConsistency + int(i);
				if (cTic <= pState.CurrentNetConsistency)
					continue;

				if (cTic > pState.CurrentNetConsistency + 1 || !consistencies[i])
				{
					clientState.Flags |= CF_MISSING_CON;
					break;
				}

				pState.NetConsistency[cTic % BACKUPTICS] = consistencies[i];
				pState.CurrentNetConsistency = cTic;
			}

			// Each tic within a given packet is given a sequence number to ensure that things were put
			// back together correctly. Normally this wouldn't matter as much but since we need to keep
			// clients in lock step a misordered packet will instantly cause a desync.
			TArray<TArrayView<uint8_t>> data = {}; // each contained TArrayView represents a packet.
			bool malformedCommandRecord = false;
			uint64_t commandOffsetsSeen = 0u;
			for (int t = 0; t < totalTics; ++t)
			{
				if (curByte >= int(NetBufferLength))
				{
					malformedCommandRecord = true;
					break;
				}

				// Try and reorder the tics if they're all there but end up out of order.
				const int ofs = NetBuffer[curByte++];
				if (ofs < 0 || ofs >= totalTics || ofs >= 64)
				{
					malformedCommandRecord = true;
					break;
				}
				const uint64_t commandMask = uint64_t(1u) << ofs;
				if ((commandOffsetsSeen & commandMask) != 0u)
				{
					malformedCommandRecord = true;
					break;
				}
				commandOffsetsSeen |= commandMask;

				if (curByte > int(NetBufferLength))
				{
					malformedCommandRecord = true;
					break;
				}

				size_t commandPayloadBytes = 0u;
				if (!Net_TrySkipUserCmdMessageWithBoundary(&NetBuffer[curByte], NetBufferLength - curByte,
					t + 1 == totalTics, uint8_t(totalTics), commandOffsetsSeen,
					uint8_t(totalTics - t - 1), commandPayloadBytes)
					|| commandPayloadBytes > NetBufferLength - curByte)
				{
					malformedCommandRecord = true;
					break;
				}

				TArrayView<uint8_t> packet = TArrayView(&NetBuffer[curByte], commandPayloadBytes);
				data.Insert(ofs, packet);

				curByte += int(commandPayloadBytes);
			}
			if (malformedCommandRecord)
			{
				NoteMalformedGameplayPacket(clientNum, "command-record-malformed");
				clientState.Flags |= CF_MISSING_SEQ;
				continue;
			}

			for (size_t i = 0u; i < data.Size(); ++i)
			{
				const int seq = baseSequence + int(i);
				// Duplicate command, ignore it.
				if (seq <= pState.CurrentSequence)
					continue;

				// Skipped a command. Packet likely got corrupted while being put back together, so have
				// the client send over the properly ordered commands.
				if (seq > pState.CurrentSequence + 1 || data[i] == nullptr)
				{
					clientState.Flags |= CF_MISSING_SEQ;
					break;
				}

				ReadUserCmdMessage(data[i], pNum, seq);
				// The host and clients are a bit desynced here. We don't want to update the host's latest ack with their own
				// info since they get those from the actual clients, but clients have to get them from the host since they
				// don't commincate with each other.
				if (!I_IsLocalHCDEServiceAuthority()
					|| I_IsHCDEServiceAuthoritySlot(pNum) || !I_IsHCDEServiceAuthoritySlot(clientNum))
				{
					pState.CurrentSequence = seq;
				}
				// Update this so host switching doesn't have any hiccups.
				if (!I_IsLocalHCDEServiceAuthority() && !I_IsHCDEServiceAuthoritySlot(pNum))
					pState.SequenceAck = seq;
			}
		}
		if (malformedPacketFields)
		{
			NoteMalformedGameplayPacket(clientNum, "packet-fields-malformed");
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}

		// Valid gameplay packet path; clear strike pressure.
		clientState.MalformedPacketStrikes = 0u;
		clientState.MalformedWindowStartMS = 0u;
	}

	for (const auto& ack : latencyAcks)
	{
		NetBuffer[0] = NCMD_LATENCYACK;
		NetBuffer[1] = ack.Seq;
		HSendPacket(ack.Client, 2);
	}
}

static void SendHeartbeat()
{
	// TODO: This could probably also be used to determine if there's packets
	// missing and a retransmission is needed.
	const uint64_t time = I_msTime();
	for (auto client : NetworkClients)
	{
		if (client == consoleplayer)
			continue;

		auto& state = ClientStates[client];
		if (LastLatencyUpdate >= MAXSENDTICS)
		{
			int delta = 0;
			const uint8_t startTic = state.CurrentLatency - MAXSENDTICS;
			for (int i = 0; i < MAXSENDTICS; ++i)
			{
				const int tic = (startTic + i) % MAXSENDTICS;
				const uint64_t high = state.RecvTime[tic] < state.SentTime[tic] ? time : state.RecvTime[tic];
				const uint64_t rawDelta = high - state.SentTime[tic];
					// Clamp latency to a reasonable maximum (5 seconds) to prevent overflow
					// from corrupted or out-of-order timestamps
				delta += (rawDelta > 5000) ? 5000 : rawDelta;
			}

			state.AverageLatency = delta / MAXSENDTICS;
		}

		if (state.bNewLatency)
		{
			// Use the most up-to-date time here for better accuracy.
			state.SentTime[state.CurrentLatency % MAXSENDTICS] = I_msTime();
			state.bNewLatency = false;
		}

		NetBuffer[0] = NCMD_LATENCY;
		NetBuffer[1] = state.CurrentLatency;
		HSendPacket(client, 2);
	}
}

static void Net_DumpSyncDiagnostics(int client, int consistency, int16_t localConsistency, int16_t netConsistency);

static void CheckConsistencies()
{
	// Phase 4 (UZDoom legacy removal): HCDE checksum is the canonical desync
	// signal in netgame. Skip the legacy hash comparison loop entirely when
	// the session uses server-authoritative consistency (which is now every
	// netgame). The bookkeeping below is still needed for single-player /
	// demo playback paths that touch this function via shared callsites,
	// even though MakeConsistencies short-circuits earlier in those modes -
	// keep the participant-filter loop running so LastVerifiedConsistency
	// stays consistent with CurrentNetConsistency for any code that reads
	// those fields.
	if (Net_UsesServerAuthoritativeConsistency())
	{
		for (auto client : NetworkClients)
			ClientStates[client].LastVerifiedConsistency = ClientStates[client].CurrentNetConsistency;
		return;
	}

	// Check consistencies retroactively to see if there was a desync at some point. We still
	// check the local client here because these could realistically desync
	// if the client's current position doesn't agree with the host.
	for (auto client : NetworkClients)
	{
		auto& clientState = ClientStates[client];
		if (!Net_IsGameplayConsistencyParticipant(client))
		{
			clientState.LastVerifiedConsistency = clientState.CurrentNetConsistency;
			continue;
		}

		if (gametic < ConsistencyGraceUntilTic[client])
		{
			// A newly promoted client can still be settling its local state for a
			// few tics after the late-join sync completes. Skip consistency locking
			// until that short grace window expires.
			clientState.LastVerifiedConsistency = clientState.CurrentNetConsistency;
			continue;
		}

		// If previously inconsistent, always mark it as such going forward. We don't want this to
		// accidentally go away at some point since the game state is already completely broken.
		if (players[client].inconsistant)
		{
			clientState.LastVerifiedConsistency = clientState.CurrentNetConsistency;
		}
		else
		{
			// Make sure we don't check past tics we haven't even ran yet.
			const int limit = min<int>(CurrentConsistency - 1, clientState.CurrentNetConsistency);
			while (clientState.LastVerifiedConsistency < limit)
			{
				++clientState.LastVerifiedConsistency;
				const int tic = clientState.LastVerifiedConsistency % BACKUPTICS;
				if (clientState.LocalConsistency[tic] != clientState.NetConsistency[tic])
				{
					Printf(PRINT_BOLD, "Net consistency mismatch for player %d '%s' at consistency %d (local=%d net=%d gametic=%d clienttic=%d room=%u map=%s current=%d remote=%d)\n",
						client, players[client].userinfo.GetName(), clientState.LastVerifiedConsistency,
						clientState.LocalConsistency[tic], clientState.NetConsistency[tic], gametic, ClientTic, unsigned(CurrentRoomID),
						primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
						CurrentConsistency, clientState.CurrentNetConsistency);
					DebugTrace::Warningf("net", "consistency mismatch player=%d name=%s consistency=%d local=%d net=%d gametic=%d clienttic=%d room=%u map=%s current=%d remote=%d",
						client, players[client].userinfo.GetName(), clientState.LastVerifiedConsistency, clientState.LocalConsistency[tic], clientState.NetConsistency[tic],
						gametic, ClientTic, unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
						CurrentConsistency, clientState.CurrentNetConsistency);
					DebugTrace::Warningf("net", "out-of-sync latencies player=%d seq=%d seqAck=%d consistencyAck=%d lastVerified=%d averageLatency=%d",
						client, clientState.CurrentSequence, clientState.SequenceAck, clientState.ConsistencyAck,
						clientState.LastVerifiedConsistency, clientState.AverageLatency);
					Net_DumpSyncDiagnostics(client, clientState.LastVerifiedConsistency,
						clientState.LocalConsistency[tic], clientState.NetConsistency[tic]);
					players[client].inconsistant = true;
					clientState.LastVerifiedConsistency = clientState.CurrentNetConsistency;
					break;
				}
			}
		}
	}
}

//==========================================================================
//
// FRandom :: StaticSumSeeds
//
// This function produces a uint32_t that can be used to check the consistancy
// of network games between different machines. Only a select few RNGs are
// used for the sum, because not all RNGs are important to network sync.
//
//==========================================================================

extern FRandom pr_spawnmobj;
extern FRandom pr_acs;
extern FRandom pr_chase;
extern FRandom pr_damagemobj;

static uint32_t StaticSumSeeds()
{
	return
		pr_spawnmobj.Seed() +
		pr_acs.Seed() +
		pr_chase.Seed() +
		pr_damagemobj.Seed();
}

static int16_t CalculateConsistency(int client, uint32_t seed)
{
	if (players[client].mo != nullptr)
	{
		seed += int((players[client].mo->X() + players[client].mo->Y() + players[client].mo->Z()) * 257) + players[client].mo->Angles.Yaw.BAMs() + players[client].mo->Angles.Pitch.BAMs();
		seed ^= players[client].health;
	}

	// Zero value consistencies are seen as invalid, so always have a valid value.
	return (seed & 0xFFFF) ? seed : 1;
}

// Phase 4 (UZDoom legacy removal): HCDE's snapshot checksum chunk
// (`d_net_checksum.cpp`) is the canonical "is the world the same on both
// sides?" signal. The legacy ZDoom consistency hash compared the
// (predicted) client pawn pose/health to the (authoritative) server pose/
// health, which deterministically diverges during the prediction window
// and therefore produced guaranteed false positives in HCDE multiplayer
// regardless of any seed agreement. With HCDE service mandatory at the
// connect gate (Phase 3), every netgame session has the snapshot checksum
// stream available, so we always prefer it.
//
// Returning true here makes Net_IsGameplayConsistencyParticipant return
// false for every player, which short-circuits the hash comparison loop in
// CheckConsistencies() and makes MakeConsistencies() stamp a constant
// nonzero marker (1) into LocalConsistency[]. The marker is still required
// because the resend parser treats a zero entry as missing data; once the
// consistency stream is fully removed from the wire (post-Phase-4 cleanup)
// the marker can go too. Single-player and demo playback short-circuit at
// the top of MakeConsistencies(), so this predicate has no effect there.
static bool Net_UsesServerAuthoritativeConsistency()
{
	return netgame;
}

static void Net_TraceUserCmdSnapshot(const char* label, int client, int tic, const usercmd_t& cmd)
{
	DebugTrace::Warningf("net", "desync %s client=%d tic=%d buttons=0x%08x pitch=%d yaw=%d roll=%d fwd=%d side=%d up=%d",
		label != nullptr ? label : "cmd", client, tic, cmd.buttons, cmd.pitch, cmd.yaw, cmd.roll,
		cmd.forwardmove, cmd.sidemove, cmd.upmove);
}

static void Net_DumpSyncDiagnostics(int client, int consistency, int16_t localConsistency, int16_t netConsistency)
{
	if (!net_desyncdebug)
	{
		DebugTrace::Dump("net");
		return;
	}

	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	DebugTrace::Warningf("net", "DESYNC REPORT BEGIN player=%d name=%s consistency=%d local=%d net=%d",
		client, players[client].userinfo.GetName(), consistency, localConsistency, netConsistency);
	DebugTrace::Warningf("net", "desync session role=%s authority-slot=%d console=%d room=%u map=%s gametic=%d clienttic=%d ticdup=%u enter=%d last-game=%d",
		I_IsLocalHCDEServiceAuthority() ? "authority" : "client", authoritySlot, consoleplayer,
		unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		gametic, ClientTic, unsigned(TicDup), EnterTic, LastGameUpdate);
	DebugTrace::Warningf("net", "desync gates levelstart=%s delay=%d debug=%d lag=%s commands-reset=%d ahead=%d skip-timer=%d skip-amount=%d full-latency-cycle=%d",
		Net_LevelStartStatusName(LevelStartStatus), LevelStartDelay, LevelStartDebug, Net_LagStateName(LagState),
		bCommandsReset ? 1 : 0, CommandsAhead, SkipCommandTimer, SkipCommandAmount, FullLatencyCycle);
	DebugTrace::Warningf("net", "desync rng spawn=%d acs=%d chase=%d damagemobj=%d sum=%u current-consistency=%d last-sent-consistency=%d",
		pr_spawnmobj.Seed(), pr_acs.Seed(), pr_chase.Seed(), pr_damagemobj.Seed(),
		StaticSumSeeds(), CurrentConsistency, LastSentConsistency);

	for (auto pNum : NetworkClients)
	{
		const auto& netState = ClientStates[pNum];
		const auto& peer = HCDELivePeers[pNum];
		const auto& player = players[pNum];
		DebugTrace::Warningf("net", "desync client=%d name=%s in-game=%d waiting=%d inconsistent=%d state=%d flags=0x%x seq=%d ack=%d resend-seq=%d con=%d con-ack=%d resend-con=%d verified=%d avg-lat=%u cur-lat=%u stability=%u",
			pNum, player.userinfo.GetName(), playeringame[pNum] ? 1 : 0, player.waiting ? 1 : 0,
			player.inconsistant ? 1 : 0, int(player.playerstate), netState.Flags, netState.CurrentSequence,
			netState.SequenceAck, netState.ResendSequenceFrom, netState.CurrentNetConsistency,
			netState.ConsistencyAck, netState.ResendConsistencyFrom, netState.LastVerifiedConsistency,
			unsigned(netState.AverageLatency), unsigned(netState.CurrentLatency), unsigned(netState.StabilityBuffer));

		if (player.mo != nullptr)
		{
			DebugTrace::Warningf("net", "desync pawn client=%d pos=(%.3f,%.3f,%.3f) yaw=%d pitch=%d health=%d player-health=%d",
				pNum, player.mo->X(), player.mo->Y(), player.mo->Z(),
				player.mo->Angles.Yaw.BAMs(), player.mo->Angles.Pitch.BAMs(),
				player.mo->health, player.health);
		}
		else
		{
			DebugTrace::Warningf("net", "desync pawn client=%d missing", pNum);
		}

		for (int offset = -2; offset <= 2; ++offset)
		{
			const int con = consistency + offset;
			if (con < 0)
				continue;

			const int index = con % BACKUPTICS;
			DebugTrace::Warningf("net", "desync con-window client=%d con=%d local=%d net=%d marker=%s",
				pNum, con, netState.LocalConsistency[index], netState.NetConsistency[index],
				con == consistency ? "mismatch" : "nearby");
		}

		const int commandStart = max<int>(netState.CurrentSequence - 3, 0);
		for (int seq = commandStart; seq <= netState.CurrentSequence + 1; ++seq)
		{
			if (seq < 0)
				continue;

			Net_TraceUserCmdSnapshot("netcmd", pNum, seq, netState.Tics[seq % BACKUPTICS].Command);
		}

		DebugTrace::Warningf("net", "desync live-peer client=%d tx=%u rx=%u dup=%u control-tx=%u control-rx=%u cap-rx=%u legacy-rx=%u remote-caps=0x%llx negotiated-caps=0x%llx cmd-tx=%u cmd-rx=%u snap-tx=%u snap-rx=%u unsupported=%u rejected=%u deltas=%u repairs=%u drift=%u reconciliations=%u hard=%u",
			pNum, peer.TxSequence, peer.RxSequence, peer.DuplicateCount, peer.ControlSent, peer.ControlReceived,
			peer.CapabilityControlReceived, peer.LegacyControlReceived,
			static_cast<unsigned long long>(peer.RemoteCapabilities),
			static_cast<unsigned long long>(peer.NegotiatedCapabilities),
			peer.ClientCommandSent, peer.ClientCommandReceived, peer.SnapshotSent, peer.SnapshotReceived,
			peer.UnsupportedReceived, peer.AuthorityRejected, peer.WorldDeltaReceived, peer.BaselineRepairs,
			peer.BaselineLocalDrift, peer.Reconciliations, peer.HardReconciliations);
	}

	if (consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
	{
		for (int tic = max<int>(ClientTic - 6, 0); tic <= ClientTic + 1; ++tic)
			Net_TraceUserCmdSnapshot("localcmd", consoleplayer, tic, LocalCmds[tic % LOCALCMDTICS]);
	}

	const FString tracePath = FStringf("%s/hcde-net-desync-%llu.txt",
		M_GetAppDataPath(true).GetChars(), static_cast<unsigned long long>(I_msTime()));
	DebugTrace::Warning("net", "DESYNC REPORT END");
	DebugTrace::SaveToFile(tracePath.GetChars(), "net", DebugTrace::Severity::Debug);
	DebugTrace::Dump("net");
}

// Ran a tick, so prep the next consistencies to send out.
// [RH] Include some random seeds and player stuff in the consistancy
// check, not just the player's x position like BOOM.
static void MakeConsistencies()
{
	if (!netgame || demoplayback || (gametic % TicDup) || !IsMapLoaded())
		return;

	const uint32_t rngSum = Net_UsesServerAuthoritativeConsistency() ? 0u : StaticSumSeeds();
	for (auto client : NetworkClients)
	{
		auto& clientState = ClientStates[client];
		const int consistencyIndex = CurrentConsistency % BACKUPTICS;
		if (!Net_IsGameplayConsistencyParticipant(client))
		{
			// Transport-only slots still need a nonzero marker so resend parsing
			// does not treat the consistency stream as missing.
			clientState.LocalConsistency[consistencyIndex] = 1;
			continue;
		}

		clientState.LocalConsistency[consistencyIndex] = CalculateConsistency(client, rngSum);
	}

	++CurrentConsistency;
}

static bool Net_UpdateStatus()
{
	if (!netgame || demoplayback || NetworkClients.Size() <= 1)
		return true;

	if (LevelStartStatus == LST_WAITING || LevelStartDelay > 0)
		return false;

	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	const bool localAuthority = I_IsLocalHCDEServiceAuthority();

	// Check against the previous tick in case we're recovering from a huge
	// system hiccup. If the game has taken too long to update, it's likely
	// another client is hanging up the game.
	if (LastEnterTic - LastGameUpdate >= MAXSENDTICS * TicDup && LastEnterTic >= AuthorityWaitGraceUntil)
	{
		// Try again in the next MaxDelay tics.
		LastGameUpdate = EnterTic;

		if (localAuthority)
		{
			// Use a missing packet here to tell the other players to retransmit instead of simply retransmitting our
			// own data over instantly. This avoids flooding the network at a time where it's not opportune to do so.
			const int curTic = gametic / TicDup;
			for (auto client : NetworkClients)
			{
				if (I_IsHCDEServiceAuthoritySlot(client))
					continue;
				if (Net_IsLateJoinSyncPending(client))
					continue;

				if (ClientStates[client].CurrentSequence < curTic)
				{
					ClientStates[client].Flags |= CF_MISSING;
					players[client].waiting = true;
				}
				else
				{
					players[client].waiting = false;
				}
			}
		}
		else
		{
			// The client is waiting for data from the host and hasn't recieved it yet. Send
			// our data back over in case the host is waiting for us.
			ClientStates[authoritySlot].Flags |= CF_MISSING;
			HCDESetAuthorityWaiting(true);
			DebugTrace::Warningf("net.levelstart", "authority wait armed client=%d gametic=%d clienttic=%d room=%u map=%s seq=%d ack=%d levelstart=%s lag=%s",
				authoritySlot, gametic, ClientTic, unsigned(CurrentRoomID),
				primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
				ClientStates[authoritySlot].CurrentSequence, ClientStates[authoritySlot].SequenceAck,
				Net_LevelStartStatusName(LevelStartStatus), Net_LagStateName(LagState));
		}
	}

	if (LevelStartStatus == LST_HOST)
	{
		// Legacy status: release immediately; authority no longer waits on peer sequences.
		if (I_IsLocalHCDEServiceAuthority())
			Net_SetLevelStartStatus(LST_READY, "authority-host-gate-skip");
	}

	// Phase 1 (UZDoom lockstep removal): only non-authority peers honor the
	// "any waiting client blocks the world" gate. The authority must keep
	// simulating at TICRATE no matter how far behind a peer is - that's the
	// difference between the legacy lockstep model (everyone advances at the
	// slowest peer) and Odamex/Zandronum-style server authority (server
	// owns the clock; lagging clients lag themselves). The waiting flag is
	// still set above so CF_MISSING / CF_RETRANSMIT can drive resends, but it
	// no longer freezes the host world.
	if (!localAuthority)
	{
		for (auto client : NetworkClients)
		{
			if (Net_IsLateJoinSyncPending(client))
				continue;
			if (players[client].waiting)
				return false;
		}
	}

	// Wait for the game to stabilize a bit after launch before skipping commands.
	bool updated = false;
	int lowestDiff = INT_MAX;
	if (gametic > TICRATE * 2 && !(gametic % TicDup))
	{
		if (localAuthority)
		{
			// If we're consistenty ahead of the highest sequence player, slow down.
			bool allUpdated = true;
			const int curTic = ClientTic / TicDup;
			for (auto client : NetworkClients)
			{
				if (!I_IsHCDEServiceAuthoritySlot(client))
				{
					if (ClientStates[client].Flags & CF_UPDATED)
					{
						updated = true;
						int diff = curTic - ClientStates[client].CurrentSequence;
						if (diff < lowestDiff)
							lowestDiff = diff;
					}
					else
					{
						allUpdated = false;
					}
				}

				ClientStates[client].Flags &= ~CF_UPDATED;
			}

			if (allUpdated)
			{
				// If we're consistently ahead of the world, force a stop here as well. Likely some client
				// has fallen super far behind and needs to be reset.
				const int diff = curTic - gametic / TicDup;
				if (diff > 1)
					lowestDiff = diff;
			}
		}
		else if (ClientStates[authoritySlot].Flags & CF_UPDATED)
		{
			// Check if the host is reporting that we're too far ahead of them.
			updated = true;
			lowestDiff = CommandsAhead;
			ClientStates[authoritySlot].Flags &= ~CF_UPDATED;
		}
	}

	if (updated)
	{
		lowestDiff -= StabilityBuffer;
		if (lowestDiff > 0)
		{
			// Phase 1: SkipCommandAmount throttles local command generation when
			// the session looks "ahead" of peers. That made sense in lockstep,
			// but on authority the world no longer waits for clients, so
			// starving the host's own command stream just adds input delay.
			if (!localAuthority)
			{
				if (SkipCommandTimer++ > TICRATE / 2)
				{
					SkipCommandTimer = 0;
					if (SkipCommandAmount <= 0)
						SkipCommandAmount = lowestDiff * TicDup;
				}
			}
		}
		else
		{
			SkipCommandTimer = 0;
		}
	}

	return true;
}

void NetUpdate(int tics)
{
	GetPackets();
	HandleIncomingConnectionMaintenance();
	if (tics <= 0)
		return;

	if (netgame && !demoplayback)
	{
		// If a tic has passed, always send out a heartbeat packet (also doubles as
		// a latency measurement tool).
		if (I_IsLocalHCDEServiceAuthority())
		{
			DebugTrace::Debugf("net.out", "host heartbeat gametic=%d", gametic);
			LastLatencyUpdate += tics;
			if (FullLatencyCycle > 0)
				FullLatencyCycle = max<int>(FullLatencyCycle - tics, 0);

			SendHeartbeat();
			SendHCDELiveControl();
			SV_UpdateMaster();

			if (LastLatencyUpdate >= MAXSENDTICS)
				LastLatencyUpdate = 0;
		}
		else
		{
			SendHCDELiveControl();
		}

		CheckConsistencies();
	}

	// Sit idle after the level has loaded until everyone is ready to go. This keeps players better
	// in sync with each other than relying on tic balancing to speed up/slow down the game and mirrors
	// how players would wait for a true server to load.
	if (LevelStartStatus != LST_READY)
	{
		if (LevelStartStatus == LST_WAITING)
		{
			if (!Net_HasPlayableNetworkClients())
			{
				// Solo authority (listen host alone, or dedicated server before
				// any joiner). The old `NetworkClients.Size() == 1` check failed
				// on dedicated servers because slot 0 is always in NetworkClients
				// as transport, so Size()==2 with one real client never hit this
				// path and the server could stall in LST_WAITING forever.
				CheckLevelStart(I_GetHCDEServiceAuthoritySlot(), 0);
			}
			else if (I_IsLocalHCDEServiceAuthority())
			{
				TryReleaseLevelStart();
			}
			else
			{
				if (!I_IsLocalHCDEServiceAuthority() && IsMapLoaded())
				{
					NetBuffer[0] = NCMD_LEVELREADY;
					NetBuffer[1] = CurrentRoomID;
					HSendPacket(I_GetHCDEServiceAuthoritySlot(), 2);
				}
			}
		}
		else if (LevelStartStatus == LST_HOST && I_IsLocalHCDEServiceAuthority())
		{
			Net_SetLevelStartStatus(LST_READY, "authority-host-gate-skip");
		}
	}
	else if (LevelStartDelay > 0)
	{
		if (LevelStartDelay < tics)
			tics -= LevelStartDelay;

		LevelStartDelay = max<int>(LevelStartDelay - tics, 0);
	}

	bool netGood = Net_UpdateStatus();
	const int startTic = ClientTic;
	tics = min<int>(tics, MAXSENDTICS * TicDup);
	// Hard ceiling on how many commands the client may generate ahead of the
	// server gametic. The world-tic cap further down is the active lead
	// limiter (it slows down the world loop so ClientTic - gametic stays at
	// desiredLead), so this command cap is just a runaway safety net for the
	// rare case where the world-tic cap is disabled (cl_net_prediction_lead=0).
	// Phase 3 originally tightened this to a single-digit value, but that
	// caused command generation to choke on routine jitter; the user observed
	// rubber banding once the lead naturally crossed that pinhole. Use the
	// legacy BACKUPTICS/2 ceiling so the safety net only fires for actually
	// pathological lead growth.
	const int commandLeadLimit = BACKUPTICS / 2;
	if ((startTic + tics - gametic) / TicDup > commandLeadLimit)
	{
		tics = (gametic + commandLeadLimit * TicDup) - startTic;
		if (tics <= 0)
		{
			// Legacy escape hatch: still enter the loop once so I_StartTic /
			// D_ProcessEvents drain pending input even when over the lead
			// ceiling. The body's `if (!netGood) break;` then prevents an
			// extra command from being generated.
			tics = 1;
			netGood = false;
		}
	}

	for (int i = 0; i < tics; ++i)
	{
		I_StartTic();
		D_ProcessEvents();
		if (pauseext || !netGood)
			break;

		if (SkipCommandAmount > 0)
		{
			--SkipCommandAmount;
			continue;
		}

		G_BuildTiccmd(&LocalCmds[ClientTic++ % LOCALCMDTICS]);
		Net_ApplySelfTestInputs(&LocalCmds[(ClientTic - 1) % LOCALCMDTICS], ClientTic - 1);
		// Listen-host authority: mirror local input into ClientStates so confirmed
		// G_Ticker uses the same cmds as prediction (dedicated clients get echo).
		if (I_IsLocalHCDEServiceAuthority()
			&& consoleplayer >= 0
			&& consoleplayer < MAXPLAYERS
			&& !I_IsServerReservedSlot(consoleplayer)
			&& TicDup == 1)
		{
			const int seq = ClientTic - 1;
			ClientStates[consoleplayer].Tics[seq % BACKUPTICS].Command =
				LocalCmds[seq % LOCALCMDTICS];
		}
		// Movement diagnostics: track rising-edge of attack/use so we can
		// measure tic latency between local issue and authority echo. The
		// function-local static persists across frames; on a Net_ResetCommands
		// the worst case is one missed rising edge if the same button is held
		// across the reset, which is acceptable for diagnostic purposes.
		{
			static uint32_t LastLocalCmdButtons = 0u;
			const uint32_t curButtons = LocalCmds[(ClientTic - 1) % LOCALCMDTICS].buttons;
			const uint32_t rising = curButtons & ~LastLocalCmdButtons;
			if (rising & BT_ATTACK)
				HCDEMovementOnInputButton(BT_ATTACK, ClientTic - 1);
			if (rising & BT_USE)
				HCDEMovementOnInputButton(BT_USE, ClientTic - 1);
			LastLocalCmdButtons = curButtons;
		}
		if (TicDup == 1)
		{
			Net_NewClientTic();
		}
		else
		{
			const int ticDiff = ClientTic % TicDup;
			if (ticDiff)
			{
				const int startTic = ClientTic - ticDiff;

				// Even if we're not sending out inputs, update the local commands so that the TicDup
				// is correctly played back while predicting as best as possible. This will help prevent
				// minor hitches when playing online.
				for (int j = ClientTic - 1; j > startTic; --j)
					LocalCmds[(j - 1) % LOCALCMDTICS].buttons |= LocalCmds[j % LOCALCMDTICS].buttons;
			}
			else
			{
				// Gather up the Command across the last TicDup number of tics
				// and average them out. These are propagated back to the local
				// command so that they'll be predicted correctly.
				const int lastTic = ClientTic - TicDup;
				for (int j = ClientTic - 1; j > lastTic; --j)
					LocalCmds[(j - 1) % LOCALCMDTICS].buttons |= LocalCmds[j % LOCALCMDTICS].buttons;

				int pitch = 0;
				int yaw = 0;
				int roll = 0;
				int forwardmove = 0;
				int sidemove = 0;
				int upmove = 0;

				for (int j = 0; j < TicDup; ++j)
				{
					const int mod = (lastTic + j) % LOCALCMDTICS;
					pitch += LocalCmds[mod].pitch;
					yaw += LocalCmds[mod].yaw;
					roll += LocalCmds[mod].roll;
					forwardmove += LocalCmds[mod].forwardmove;
					sidemove += LocalCmds[mod].sidemove;
					upmove += LocalCmds[mod].upmove;
				}

				pitch /= TicDup;
				yaw /= TicDup;
				roll /= TicDup;
				forwardmove /= TicDup;
				sidemove /= TicDup;
				upmove /= TicDup;

				for (int j = 0; j < TicDup; ++j)
				{
					const int mod = (lastTic + j) % LOCALCMDTICS;
					LocalCmds[mod].pitch = pitch;
					LocalCmds[mod].yaw = yaw;
					LocalCmds[mod].roll = roll;
					LocalCmds[mod].forwardmove = forwardmove;
					LocalCmds[mod].sidemove = sidemove;
					LocalCmds[mod].upmove = upmove;
				}

				Net_NewClientTic();
			}
		}
	}

	const int newestTic = ClientTic / TicDup;
	if (demoplayback)
	{
		// Don't touch net command data while playing a demo, as it'll already exist.
		for (auto client : NetworkClients)
			ClientStates[client].CurrentSequence = newestTic;

		return;
	}

	constexpr int MaxPlayersPerPacket = 16;

	int startSequence = startTic / TicDup;
	int endSequence = newestTic;
	int quitters = 0;
	int quitNums[MAXPLAYERS];
	int players = 1u;
	int maxCommands = MAXSENDTICS;
	const bool localAuthority = I_IsLocalHCDEServiceAuthority();
	if (localAuthority)
	{
		// Ensure the host only sends out available tics when ready instead of constantly shotgunning
		// them out as they're made locally.
		startSequence = gametic / TicDup;
		// Phase 1 (UZDoom lockstep removal): the authority sends each client
		// up to its own newest produced tic. The legacy formula was
		// `endSequence = lowestSeq + 1` which capped EVERY client's window
		// at the slowest client's ack, so one lagging peer would starve all
		// the others' snapshot streams (the visible "rubber banding for the
		// whole match because one player's RTT spiked"). Per-client catchup
		// is naturally bounded below by curState.SequenceAck (or
		// CurrentSequence) and above by MAXSENDTICS in the inner numTics
		// clamp; we no longer need a peer-cross window cap here.
		//
		// We still walk NetworkClients to (a) count players for the
		// fragmentation heuristic and (b) collect quitters. Late-join
		// pending clients are skipped from the player count for packet
		// sizing because their snapshot stream doesn't ride this loop.
		for (auto client : NetworkClients)
		{
			if (I_IsHCDEServiceAuthoritySlot(client))
				continue;

			if (ClientStates[client].Flags & CF_QUIT)
			{
				quitNums[quitters++] = client;
			}
			else
			{
				++players;
			}
		}

		// To avoid fragmenting, split up commands into groups of 16p with only 2 commands per packet.
		// If the average packet size with 16p is ~500b, this gives up to ~1000b per packet of data
		// with some leeway for network events and UDP header data. Most routers have an MTU of 1500b.
		// If player count is < 16, scale the number of commands by 1 per every 4 less players.
		// If player count is < 8, scale the number of commands by 1 per every 1 less player.
		// If player count is < 4, scale the number of commands by 4 per every 1 less player.
		constexpr size_t MaxTicsPerPacket = 2u;
		if (players > 1u)
		{
			maxCommands = MaxTicsPerPacket;
			if (players >= MaxPlayersPerPacket / 2 && players < MaxPlayersPerPacket)
				maxCommands = MaxTicsPerPacket + (MaxPlayersPerPacket - players) / 4;
			else if (players >= MaxPlayersPerPacket / 4 && players < MaxPlayersPerPacket / 2)
				maxCommands = MaxPlayersPerPacket / 4 + MaxPlayersPerPacket / 2 - players;
			else if (players < MaxPlayersPerPacket / 4)
				maxCommands = MaxPlayersPerPacket / 2 + (MaxPlayersPerPacket / 4 - players) * 4;
		}
	}

	const bool resendOnly = startSequence == endSequence && (ClientTic % TicDup);
	const int playerLoops = static_cast<int>(ceil((double)players / MaxPlayersPerPacket));
	for (auto client : NetworkClients)
	{
		// We don't want to send information to anyone but the host. On the other
		// hand, if we're the host we send out everyone's info to everyone else.
		if (!localAuthority && !I_IsHCDEServiceAuthoritySlot(client))
			continue;

		auto& curState = ClientStates[client];
		// If we can only resend, don't send clients any information that they already have. If
		// we couldn't generate any commands because we're at the cap, instead send out a heartbeat.
		if ((curState.Flags & CF_QUIT) || (resendOnly && !(curState.Flags & (CF_RETRANSMIT | CF_MISSING))))
			continue;

		const bool isSelf = client == consoleplayer;
		NetBuffer[0] = (curState.Flags & CF_MISSING) ? NCMD_RETRANSMIT : 0;
		curState.Flags &= ~CF_MISSING;

		NetBuffer[1] = (curState.Flags & CF_RETRANSMIT_SEQ) ? curState.ResendID : CurrentRoomID;
		int lastSeq = curState.CurrentSequence;
		int lastCon = curState.CurrentNetConsistency;
		if (!localAuthority)
		{
			// Make sure to get the lowest sequence of all players
			// since the host themselves might have gotten updated but someone else in the packet
			// did not. That way the host knows to send over the correct tic.
			for (auto cl : NetworkClients)
			{
				if (ClientStates[cl].CurrentSequence < lastSeq)
					lastSeq = ClientStates[cl].CurrentSequence;
				if (ClientStates[cl].CurrentNetConsistency < lastCon)
					lastCon = ClientStates[cl].CurrentNetConsistency;
			}
		}
		// Last sequence we got from this client.
		NetBuffer[2] = (lastSeq >> 24);
		NetBuffer[3] = (lastSeq >> 16);
		NetBuffer[4] = (lastSeq >> 8);
		NetBuffer[5] = lastSeq;
		// Last consistency we got from this client.
		NetBuffer[6] = (lastCon >> 24);
		NetBuffer[7] = (lastCon >> 16);
		NetBuffer[8] = (lastCon >> 8);
		NetBuffer[9] = lastCon;

		if (curState.Flags & CF_RETRANSMIT_SEQ)
		{
			curState.Flags &= ~CF_RETRANSMIT_SEQ;
			if (curState.ResendSequenceFrom < 0)
				curState.ResendSequenceFrom = curState.SequenceAck + 1;
		}

		int passiveResendSequenceFrom = -1;
		if (localAuthority)
		{
			if (!isSelf
				&& curState.SequenceAck >= 0
				&& startSequence - curState.SequenceAck > HCDEPassiveClientResendSequenceSlack
				&& HCDEPassiveResendShouldFire(client, curState.SequenceAck))
			{
				// Dedicated clients can occasionally miss the explicit retransmit edge
				// while still reporting an older sequence ack every input packet. Honor
				// that ack as a soft resend request so one lost authority tic cannot
				// leave the client stuck at the tic gate forever.
				//
				// The old threshold was "ack one tic behind newest" which is so tight
				// it triggers on the natural RTT lag between server snapshot send and
				// client ack arrival - on a 1-2 tic round-trip the client's reported
				// SequenceAck is structurally a couple of tics behind startSequence
				// every single frame, so the previous logic fired a resend snapshot
				// every tic and the server burned uplink bandwidth re-sending packets
				// the client already had. HCDEPassiveClientResendSequenceSlack gives
				// the same breathing room the client-side passive-resend branch below
				// uses, and HCDEPassiveResendShouldFire() adds the important
				// second half of the test: the ack must stop advancing. Doom2
				// Remake-sized actor snapshots can keep a healthy ack about five
				// tics behind forever; resending on lag alone turns that normal
				// trailing ack into a permanent snapshot replay storm.
				passiveResendSequenceFrom = curState.SequenceAck + 1;
				++HCDEPredictionDebugLifetime.PassiveAuthorityResends;
				DebugTrace::Warningf("net", "passive authority resend client=%d from=%d start=%d end=%d seq=%d ack=%d room=%u",
					client, passiveResendSequenceFrom, startSequence, endSequence,
					curState.CurrentSequence, curState.SequenceAck, unsigned(CurrentRoomID));
			}
		}
		else
		{
			// Clients can occasionally stall at the tic gate because they missed the
			// latest server snapshots or because the server duplicated their input, leaving
			// the authority's SequenceAck for our input stream behind. Honor that ack
			// as a soft resend request for our own commands to ensure they stream back
			// to the server.
			const bool validLocalState = consoleplayer >= 0 && consoleplayer < MAXPLAYERS;
			const auto& localState = ClientStates[validLocalState ? consoleplayer : 0];
			// The server's snapshot-confirmed local command stream normally trails
			// the client generator by about 1-2 tics on localhost. Treat only a
			// larger gap as a soft resend request; otherwise this path resends every
			// tic and creates artificial latency pressure.
			const int commandAck = validLocalState ? localState.SequenceAck : -1;
			const int commandAckLag = commandAck >= 0 ? newestTic - commandAck : 0;
			if (validLocalState
				&& commandAck >= 0
				&& commandAckLag > HCDEPassiveClientResendSequenceSlack
				&& HCDEPassiveResendShouldFire(consoleplayer, commandAck))
			{
				passiveResendSequenceFrom = commandAck + 1;
				++HCDEPredictionDebugLifetime.PassiveClientResends;
				DebugTrace::Warningf("net", "passive client resend to authority=%d from=%d newest=%d ack=%d lag=%d room=%u",
					client, passiveResendSequenceFrom, newestTic, commandAck, commandAckLag, unsigned(CurrentRoomID));
			}
		}

		const int sequenceNum = curState.ResendSequenceFrom >= 0
			? curState.ResendSequenceFrom
			: (passiveResendSequenceFrom >= 0 ? passiveResendSequenceFrom : startSequence);
		const int numTics = clamp<int>(endSequence - sequenceNum, 0, MAXSENDTICS);

		if (curState.Flags & CF_RETRANSMIT_CON)
		{
			curState.Flags &= ~CF_RETRANSMIT_CON;
			if (curState.ResendConsistencyFrom < 0)
				curState.ResendConsistencyFrom = curState.ConsistencyAck + 1;
		}

		const int baseConsistency = curState.ResendConsistencyFrom >= 0 ? curState.ResendConsistencyFrom : LastSentConsistency;
		// Don't bother sending over consistencies unless you're the host.
		int ran = 0;
		if (localAuthority)
			ran = clamp<int>(CurrentConsistency - baseConsistency, 0, MAXSENDTICS);

		int ticLoops = static_cast<int>(ceil(max<double>(numTics, ran) / maxCommands));
		if (isSelf || !ticLoops)
			ticLoops = 1;

		const int maxPlayerLoops = isSelf ? 1 : playerLoops;
		int totalQuits = quitters;
		for (int tLoops = 0, curTicOfs = 0; tLoops < ticLoops; ++tLoops, curTicOfs += maxCommands)
		{
			for (int pLoops = 0, curPlayerOfs = 0; pLoops < maxPlayerLoops; ++pLoops, curPlayerOfs += MaxPlayersPerPacket)
			{
				size_t size = 10;
				const int packetQuitters = totalQuits > 0 ? totalQuits : 0;
				if (totalQuits > 0)
				{
					NetBuffer[0] |= NCMD_QUITTERS;
					NetBuffer[size++] = totalQuits;
					for (int i = 0; i < totalQuits; ++i)
						NetBuffer[size++] = quitNums[i];

					totalQuits = 0;
				}
				else
				{
					NetBuffer[0] &= ~NCMD_QUITTERS;
				}

				int playerNums[MAXPLAYERS];
				int playerCount = isSelf ? players : min<int>(players - curPlayerOfs, MaxPlayersPerPacket);
				NetBuffer[size++] = playerCount;
				if (players > 1)
				{
					int i = 0;
					for (auto cl : NetworkClients)
					{
						if (ClientStates[cl].Flags & CF_QUIT)
							continue;

						if (i >= curPlayerOfs)
							playerNums[i - curPlayerOfs] = cl;

						++i;
						if (!isSelf && i >= curPlayerOfs + MaxPlayersPerPacket)
							break;
					}
				}
				else
				{
					playerNums[0] = consoleplayer;
				}

				int sendTics = isSelf ? numTics : clamp<int>(numTics - curTicOfs, 0, maxCommands);
				if (curState.ResendSequenceFrom >= 0)
				{
					curState.ResendSequenceFrom += sendTics;
					if (curState.ResendSequenceFrom >= endSequence)
						curState.ResendSequenceFrom = -1;
				}

				NetBuffer[size++] = sendTics;
				if (sendTics > 0)
				{
					NetBuffer[size++] = (sequenceNum + curTicOfs) >> 24;
					NetBuffer[size++] = (sequenceNum + curTicOfs) >> 16;
					NetBuffer[size++] = (sequenceNum + curTicOfs) >> 8;
					NetBuffer[size++] = sequenceNum + curTicOfs;
				}

				int sendCon = isSelf ? ran : clamp<int>(ran - curTicOfs, 0, maxCommands);
				if (curState.ResendConsistencyFrom >= 0)
				{
					curState.ResendConsistencyFrom += sendCon;
					if (curState.ResendConsistencyFrom >= CurrentConsistency)
						curState.ResendConsistencyFrom = -1;
				}

				NetBuffer[size++] = sendCon;
				if (sendCon > 0)
				{
					NetBuffer[size++] = (baseConsistency + curTicOfs) >> 24;
					NetBuffer[size++] = (baseConsistency + curTicOfs) >> 16;
					NetBuffer[size++] = (baseConsistency + curTicOfs) >> 8;
					NetBuffer[size++] = baseConsistency + curTicOfs;
				}

				if (localAuthority)
					NetBuffer[size++] = I_IsHCDEServiceAuthoritySlot(client) ? 0 : max<int>(curState.CurrentSequence + curState.StabilityBuffer - newestTic, 0);
				else
					NetBuffer[size++] = max<int>(StabilityBuffer, 0);

				// Client commands.

				TArrayView<uint8_t> cmd = TArrayView(&NetBuffer[size], MAX_MSGLEN - size);
				auto& nativeSendScratch = HCDELiveNativeSendScratch;
				for (int i = 0; i < playerCount; ++i)
				{
					WriteInt8(playerNums[i], cmd);

					auto& clientState = ClientStates[playerNums[i]];
					// Measured latency from client to host.
					if (localAuthority)
					{
						WriteInt16(clientState.AverageLatency, cmd);
					}

					for (int r = 0; r < sendCon; ++r)
					{
						WriteInt8(r, cmd);
						const int tic = (baseConsistency + curTicOfs + r) % BACKUPTICS;
						WriteInt16(clientState.LocalConsistency[tic], cmd);
					}

					for (int t = 0; t < sendTics; ++t)
					{
						WriteInt8(t, cmd);

						int curTic = sequenceNum + curTicOfs + t, lastTic = curTic - 1;
						if (playerNums[i] == consoleplayer)
						{
							int realTic = (curTic * TicDup) % LOCALCMDTICS;
							int realLastTic = (lastTic * TicDup) % LOCALCMDTICS;
							// Write out the net events before the user commands so inputs can
							// be used as a marker for when the given command ends.
							auto& stream = NetEvents.Streams[curTic % BACKUPTICS];
							nativeSendScratch.SnapshotCommands[i][t] = LocalCmds[realTic];
							nativeSendScratch.SnapshotEventStreams[i][t] = stream.Stream;
							nativeSendScratch.SnapshotEventSizes[i][t] = stream.Used;
							WriteBytes(TArrayView(stream.Stream, stream.Used), cmd);

							WriteUserCmdMessage(LocalCmds[realTic],
								realLastTic >= 0 ? &LocalCmds[realLastTic] : nullptr, cmd);
						}
						else
						{
							auto& netTic = clientState.Tics[curTic % BACKUPTICS];

							auto data = netTic.Data.GetTArrayView();
							nativeSendScratch.SnapshotCommands[i][t] = netTic.Command;
							int eventLen = 0;
							nativeSendScratch.SnapshotEventStreams[i][t] = netTic.Data.GetData(&eventLen);
							nativeSendScratch.SnapshotEventSizes[i][t] = size_t(max<int>(eventLen, 0));
							WriteBytes(data, cmd);

							WriteUserCmdMessage(netTic.Command,
								lastTic >= 0 ? &clientState.Tics[lastTic % BACKUPTICS].Command : nullptr, cmd);
						}
					}
				}

				const bool canSendNativeClientInput =
					!localAuthority
					&& !isSelf
					&& playerCount == 1
					&& playerNums[0] == consoleplayer;
				if (canSendNativeClientInput)
				{
					for (int t = 0; t < sendTics; ++t)
					{
						const int curTic = sequenceNum + curTicOfs + t;
						const int realTic = (curTic * TicDup) % LOCALCMDTICS;
						auto& stream = NetEvents.Streams[curTic % BACKUPTICS];
						nativeSendScratch.ClientCommands[t] = LocalCmds[realTic];
						nativeSendScratch.ClientEventStreams[t] = stream.Stream;
						nativeSendScratch.ClientEventSizes[t] = stream.Used;
					}
				}
				const bool sentNativeClientInput = canSendNativeClientInput
					&& HSendNativeClientInputPacket(client, NetBuffer[0], CurrentRoomID,
						uint32_t(lastSeq), uint32_t(lastCon),
						uint32_t(sequenceNum), uint32_t(baseConsistency + curTicOfs),
						uint8_t(sendTics), uint8_t(sendCon), uint8_t(max<int>(StabilityBuffer, 0)),
						consoleplayer, curTicOfs,
						nativeSendScratch.ClientCommands,
						nativeSendScratch.ClientEventStreams,
						nativeSendScratch.ClientEventSizes);
				const bool canSendNativeServerSnapshot =
					localAuthority
					&& !isSelf;
				const bool sentNativeServerSnapshot = canSendNativeServerSnapshot
					&& HSendNativeServerSnapshotPacket(client, NetBuffer[0], CurrentRoomID,
						uint32_t(lastSeq), uint32_t(lastCon),
						uint32_t(sequenceNum + curTicOfs), uint32_t(baseConsistency + curTicOfs),
						uint8_t(sendTics), uint8_t(sendCon),
						uint8_t(I_IsHCDEServiceAuthoritySlot(client) ? 0 : max<int>(curState.CurrentSequence + curState.StabilityBuffer - newestTic, 0)),
						playerNums, uint8_t(playerCount), quitNums, uint8_t(packetQuitters),
						nativeSendScratch.SnapshotCommands,
						nativeSendScratch.SnapshotEventStreams,
						nativeSendScratch.SnapshotEventSizes);
				if (!sentNativeClientInput && !sentNativeServerSnapshot)
					HSendLiveGameplayPacket(client, int(cmd.Data() - NetBuffer));
				if (net_extratic && !isSelf)
				{
					if (sentNativeClientInput)
					{
						HSendNativeClientInputPacket(client, NetBuffer[0], CurrentRoomID,
							uint32_t(lastSeq), uint32_t(lastCon),
							uint32_t(sequenceNum), uint32_t(baseConsistency + curTicOfs),
							uint8_t(sendTics), uint8_t(sendCon), uint8_t(max<int>(StabilityBuffer, 0)),
							consoleplayer, curTicOfs,
							nativeSendScratch.ClientCommands,
							nativeSendScratch.ClientEventStreams,
							nativeSendScratch.ClientEventSizes);
					}
					else if (sentNativeServerSnapshot)
					{
						HSendNativeServerSnapshotPacket(client, NetBuffer[0], CurrentRoomID,
							uint32_t(lastSeq), uint32_t(lastCon),
							uint32_t(sequenceNum + curTicOfs), uint32_t(baseConsistency + curTicOfs),
							uint8_t(sendTics), uint8_t(sendCon),
							uint8_t(I_IsHCDEServiceAuthoritySlot(client) ? 0 : max<int>(curState.CurrentSequence + curState.StabilityBuffer - newestTic, 0)),
							playerNums, uint8_t(playerCount), quitNums, uint8_t(packetQuitters),
							nativeSendScratch.SnapshotCommands,
							nativeSendScratch.SnapshotEventStreams,
							nativeSendScratch.SnapshotEventSizes);
					}
					else
					{
						HSendLiveGameplayPacket(client, int(cmd.Data() - NetBuffer));
					}
				}
			}
		}
	}

	// Update this now that all the packets have been sent out.
	if (!resendOnly)
		LastSentConsistency = CurrentConsistency;

	// Listen for other packets. This has to also come after sending so the player that sent
	// data to themselves gets it immediately (important for singleplayer, otherwise there
	// would always be a one-tic delay).
	GetPackets();
}
#include "d_net_diag_commands.cpp"
			memset(StabilityTics, 0, sizeof(StabilityTics));
			return;
		}
	}

	if (diff < 0)
		diff = 0;

	if (!(gametic % TicDup))
	{
		StabilityTics[CurStabilityTic++ % STABILITYTICS] = diff > PrevAvailableDiff ? diff : 0;
		PrevAvailableDiff = diff;
	}

	// If we're not balancing latency, just give an extra tic for padding
	// and nothing else.
	if (!net_ticbalance)
	{
		StabilityBuffer = 1;
		return;
	}

	double total = 0.0;
	int unstableCount = 0;
	for (int t : StabilityTics)
	{
		if (t > 0)
		{
			++unstableCount;
			total += t;
		}
	}

	StabilityBuffer = unstableCount > 0 ? static_cast<int>(ceil(total / unstableCount)) : 0;
}

#include "d_net_predict_impl.cpp"

//
// TryRunTics
//
void TryRunTics()
{
	GC::CheckGC();

	if (ToggleFullscreen)
	{
		ToggleFullscreen = false;
		AddCommandString("toggle vid_fullscreen");
	}

	bool doWait = (cl_capfps || pauseext || (!netgame && r_NoInterpolate && !M_IsAnimated()));
	if (vid_dontdowait && (vid_maxfps > 0 || vid_vsync))
		doWait = false;
	if (!netgame && !AppActive && vid_lowerinbackground)
		doWait = true;

	// Get the full number of tics the client can run.
	if (doWait)
		EnterTic = I_WaitForTic(LastEnterTic);
	else
		EnterTic = I_GetTime();

	HCDEMovementOnFrame(I_msTime());

	const int startCommand = ClientTic;
	int totalTics = EnterTic - LastEnterTic;
	if (totalTics > 1 && singletics)
		totalTics = 1;

	// Listen for other clients and send out data as needed. This is also
	// needed for singleplayer! But is instead handled entirely through local
	// buffers. This has a limit of one seconds worth of commands that can be
	// generated in advanced from the last time the game updated.
	NetUpdate(totalTics);

	LastEnterTic = EnterTic;

	// Paused gameplay should not extrapolate visuals as multi-tic catch-up.
	if (pauseext)
	{
		// Paused frames intentionally do not advance the world. Keep the net
		// watchdog baseline current so pause/unpause does not look like a stale
		// authority gate or a prediction fault.
		LastGameUpdate = EnterTic;
		AuthorityWaitGraceUntil = max<int>(AuthorityWaitGraceUntil, EnterTic + MAXSENDTICS * TicDup);
		HCDEPredictionPauseGraceUntil = EnterTic + TICRATE / 2;
		HCDEPredictionDebugWindowPassiveClient = HCDEPredictionDebugLifetime.PassiveClientResends;
		HCDEPredictionDebugWindowPassiveAuth = HCDEPredictionDebugLifetime.PassiveAuthorityResends;
		HCDEPredictionDebugWindowLocalRepairs = HCDELiveProfile.PredictionLocalHealthRepairs
			+ HCDELiveProfile.PredictionLocalStateRepairs;
		HCDEPredictionDebugWindowHardRepairs = HCDELiveProfile.PredictionHardRespawnRepairs
			+ HCDELiveProfile.PredictionHardDeathRepairs;
		HCDEPredictionDebugWindowFaultReports = HCDELiveProfile.PredictionFaultReports;
		HCDEPredictionDebugMinAvailable = INT_MAX;
		HCDEPredictionDebugMaxAckLag = 0;
		HCDEPredictionDebugMaxStaleVisual = 0;
		HCDEPredictionDebugMaxBacklog = 0;
		Net_RefreshLagHUDMetrics(0, 0, totalTics, 0, false);
		return;
	}

	InvasionMirrorVisualWorldSteps = 1;

	// Phase 1 (UZDoom lockstep removal): the authority simulates on its own
	// wall-clock and never waits on client command sequences. Each client
	// independently caps its world via the prediction-lead block further down.
	//
	// `lowestSequence` is still computed for diagnostics (prediction debug,
	// stall traces, send-window construction) but on the authority it is no
	// longer the gating signal for `availableTics`. Pre-Phase-1 the formula
	// `availableTics = (lowestSequence - gametic) + 1` made the host world
	// freeze whenever any peer fell a few tics behind, which is exactly the
	// "feels like UZDoom" lag the user reported. Odamex/Zandronum-style
	// authority instead simulates at TICRATE regardless of peer state and
	// catches each client up via per-client snapshot deltas.
	int lowestSequence = INT_MAX;
	bool hasTicGateClient = false;
	for (auto client : NetworkClients)
	{
		if (!Net_IsTicGateClient(client))
			continue;

		hasTicGateClient = true;
		if (ClientStates[client].CurrentSequence < lowestSequence)
			lowestSequence = ClientStates[client].CurrentSequence;
	}
	if (!hasTicGateClient)
		lowestSequence = ClientTic / TicDup;

	// Test player prediction code in singleplayer by pretending there is another player
	// that is running exactly x ticks behind us, emulating having a specific amount of ping
	if (cl_debugprediction > 0
		&& !netgame && !demoplayback) // would probably function, but there's no reason to
	{
		if (lowestSequence > cl_debugprediction)
		{
			lowestSequence -= cl_debugprediction;
		}
		else
		{
			lowestSequence = 0;
		}
	}

	int availableTics;
	if (I_IsLocalHCDEServiceAuthority() && netgame && !singletics && !demoplayback)
	{
		// Authority path: drive the world from wall-clock tics. `totalTics`
		// is `EnterTic - LastEnterTic`, i.e. the number of TICRATE-clock
		// tics elapsed since the last frame. The +1 lookahead keeps the
		// stability/runTics math identical to the legacy formula's behavior
		// on a healthy connection, where (lowestSequence - gametic) usually
		// equaled totalTics + 0..1.
		availableTics = totalTics + 1;
	}
	else
	{
		// Client / singleplayer path: the legacy lockstep formula still
		// applies because a non-authority client cannot get ahead of the
		// authority's snapshot stream. Dedicated-mode HCDE clients have an
		// inherent 1-frame roundtrip in the ack pipeline (send → server
		// process → ack back → client read) that keeps availableTics pinned
		// at 0-1 even on localhost; grant them one extra tic of prediction
		// headroom so the playsim doesn't starve while waiting for the ack.
		const int dedicatedPipelineBonus =
			(I_UsesDedicatedServerSlot() && !I_IsLocalHCDEServiceAuthority()) ? 1 : 0;
		availableTics = (lowestSequence - gametic / TicDup) + 1 + dedicatedPipelineBonus;
		// Dedicated clients intentionally hold ClientTic ahead of gametic for
		// prediction lead. When that lead is healthy the legacy sequence formula
		// can pin availableTics at 0 even though snapshots are flowing, which
		// starves the world loop and amplifies position drift/reconcile churn.
		if (I_UsesDedicatedServerSlot() && !I_IsLocalHCDEServiceAuthority() && availableTics <= 0)
		{
			const int desiredLead = clamp<int>(*cl_net_prediction_lead, 0, 8);
			const int currentLead = ClientTic / TicDup - gametic / TicDup;
			if (currentLead > 0 && currentLead <= desiredLead + 1)
				availableTics = 1;
		}
	}

	// Sample sub-fault prediction health every frame. Cheap when off
	// (single compare in Net_PredictionDebugTick); when on it captures the
	// drift that does not cross the binary fault threshold.
	Net_PredictionDebugTick(totalTics, availableTics, lowestSequence);

	// If the amount of tics to run is falling behind the amount of available tics,
	// speed the playsim up a bit to help catch up.
	int runTics = min<int>(totalTics, availableTics);

	// Do not tick the world during level-start handshaking before LST_READY.
	if (netgame && !demoplayback && LevelStartStatus != LST_READY && LevelStartDelay <= 0)
		runTics = 0;

	if (!singletics && totalTics > 0)
	{
		CalculateNetStabilityBuffer(availableTics - totalTics);
		if (totalTics < availableTics - StabilityBuffer)
			++runTics;
		if (I_UsesDedicatedServerSlot() && !I_IsLocalHCDEServiceAuthority())
		{
			// Dedicated HCDE clients can build a sizeable backlog when a heavy mod
			// spends a few frames parsing ACS, spawning monsters, or compiling
			// render resources. Catch up more aggressively before the prediction
			// command window fills and local movement appears frozen.
			const int backlog = availableTics - runTics - StabilityBuffer;
			if (backlog > TICRATE / 2)
				runTics = min<int>(availableTics, max<int>(runTics, totalTics + 6));
			else if (backlog > TICRATE / 4)
				runTics = min<int>(availableTics, max<int>(runTics, totalTics + 3));
		}
	}

	// HCDE: enforce a minimum prediction lead on non-authority netgame clients.
	// NetUpdate has already pushed ClientTic forward by `totalTics`, so the
	// world tic loop must hold back enough that the resulting `ClientTic -
	// gametic` gap stays at least `cl_net_prediction_lead`. With this gap in
	// place P_PredictClient always has a few unconfirmed commands to replay,
	// so the local view reflects input immediately instead of waiting for the
	// next authoritative snapshot. On a clean local connection without this
	// cap availableTics keeps pace with totalTics every frame, the world
	// consumes one cmd per frame, and ClientTic/gametic march in lockstep -
	// which is exactly the "movement is so far behind" input delay clients
	// have been reporting on otherwise-healthy networks.
	//
	// `predictionLeadHeldWorld` is read further below to suppress the
	// "tic gate stalled" warning so we do not slander a healthy connection.
	bool predictionLeadHeldWorld = false;
	if (netgame
		&& !singletics
		&& !demoplayback
		&& !I_IsLocalHCDEServiceAuthority()
		&& gamestate == GS_LEVEL
		&& LevelStartStatus == LST_READY)
	{
		const int desiredLead = HCDEMovementGetAdaptiveDesiredLead(int(*cl_net_prediction_lead));
		if (desiredLead > 0)
		{
			// Both halves of `currentLead` are divided by TicDup to keep the
			// CVar expressed in TicDup-groups (which usually equals raw tics
			// since HCDE defaults TicDup to 1). Multiplying back by TicDup
			// gives the cap in raw tic units to match `runTics`.
			const int currentLead = ClientTic / TicDup - gametic / TicDup;
			const int worldTicCap = max<int>(0, currentLead - desiredLead) * TicDup;
			if (runTics > worldTicCap)
			{
				if (HCDELiveReportIntervalElapsed(LastHCDEPredictionLeadCapReportMS, 5000u))
				{
					DebugTrace::Debugf("predict",
						"prediction lead cap engaged total=%d avail=%d run=%d capped=%d current-lead=%d desired-lead=%d gametic=%d clienttic=%d",
						totalTics, availableTics, runTics, worldTicCap,
						currentLead, desiredLead, gametic, ClientTic);
				}
				runTics = worldTicCap;
				predictionLeadHeldWorld = true;
			}
		}
	}

	const int worldTimer = primaryLevel->LocalWorldTimer;
	// If there are no tics to run, check for possible stall conditions and new
	// commands to predict.
	if (runTics <= 0)
	{
		// Suppress the stall warning when we intentionally held back the world
		// to maintain the prediction lead - the tic gate is healthy, we just
		// chose not to consume a tic this frame so prediction has something to
		// replay. The earlier cap block sets predictionLeadHeldWorld whenever
		// it reduced runTics, which is the only case where availableTics > 0
		// can reach this branch on a non-authority netgame client - so this
		// single flag covers every scenario where the runTics<=0 fallthrough
		// is *expected*.
		if (!predictionLeadHeldWorld
			&& !(netgame && !demoplayback && (LevelStartStatus != LST_READY || LevelStartDelay > 0))
			&& !(netgame && !demoplayback && !I_IsLocalHCDEServiceAuthority()
				&& availableTics <= 0 && clamp<int>(*cl_net_prediction_lead, 0, 8) > 0
				&& (ClientTic / TicDup - gametic / TicDup) <= clamp<int>(*cl_net_prediction_lead, 0, 8) + 1)
			&& netgame && totalTics > 0 && EnterTic - LastTicGateStallTrace >= TICRATE)
		{
			LastTicGateStallTrace = EnterTic;
			DebugTrace::Warningf("predict", "tic gate stalled total=%d available=%d lowest=%d gametic=%d clienttic=%d room=%u map=%s levelstart=%s delay=%d lag=%s",
				totalTics, availableTics, lowestSequence, gametic, ClientTic, unsigned(CurrentRoomID),
				primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
				Net_LevelStartStatusName(LevelStartStatus), LevelStartDelay, Net_LagStateName(LagState));
			if (*hcde_hud_debug)
			{
				Printf(PRINT_HIGH, "HCDE tic gate stalled total=%d available=%d lowest=%d gametic=%d clienttic=%d room=%u map=%s levelstart=%s delay=%d lag=%s\n",
					totalTics, availableTics, lowestSequence, gametic, ClientTic, unsigned(CurrentRoomID),
					primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
					Net_LevelStartStatusName(LevelStartStatus), LevelStartDelay, Net_LagStateName(LagState));
				for (auto client : NetworkClients)
				{
					const auto& state = ClientStates[client];
					Printf(PRINT_HIGH, "  HCDE tic gate client=%d gate=%d network=%d playeringame=%d reserved=%d authority=%d waiting=%d seq=%d ack=%d flags=0x%x\n",
						client, Net_IsTicGateClient(client) ? 1 : 0, NetworkClients.InGame(client) ? 1 : 0,
						playeringame[client] ? 1 : 0, I_IsServerReservedSlot(client) ? 1 : 0,
						I_IsHCDEServiceAuthoritySlot(client) ? 1 : 0, players[client].waiting ? 1 : 0,
						state.CurrentSequence, state.SequenceAck, state.Flags);
				}
			}
			for (auto client : NetworkClients)
			{
				const auto& state = ClientStates[client];
				DebugTrace::Warningf("predict", "tic gate client=%d gate=%d network=%d playeringame=%d reserved=%d authority=%d waiting=%d seq=%d ack=%d flags=0x%x",
					client, Net_IsTicGateClient(client) ? 1 : 0, NetworkClients.InGame(client) ? 1 : 0,
					playeringame[client] ? 1 : 0, I_IsServerReservedSlot(client) ? 1 : 0,
					I_IsHCDEServiceAuthoritySlot(client) ? 1 : 0, players[client].waiting ? 1 : 0,
					state.CurrentSequence, state.SequenceAck, state.Flags);
			}
		}

		if (netgame
			&& totalTics > 0
			&& I_UsesDedicatedServerSlot()
			&& !I_IsLocalHCDEServiceAuthority()
			&& HCDELiveReportIntervalElapsed(LastHCDELiveTicGateReportMS, 2000u))
		{
			// available=0 with total=1 is normal one-tic pipelining while waiting
			// for the next authority snapshot. Only trace sustained stalls where the
			// client has built a meaningful command backlog or the sim has gone stale.
			const int commandBacklog = ClientTic - gametic;
			const bool sustainedStall = availableTics < 0
				|| (availableTics <= 0 && commandBacklog > TicDup * 3)
				|| (availableTics <= 0 && EnterTic - LastGameUpdate > TICRATE / 2);
			if (sustainedStall)
			{
				const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
				if (authoritySlot >= 0 && authoritySlot < MAXPLAYERS)
				{
					const auto& state = ClientStates[authoritySlot];
					const auto& peer = HCDELivePeers[authoritySlot];
					DebugTrace::Warningf("net",
						"tic gate sustained stall total=%d available=%d lowest=%d gametic=%d clienttic=%d backlog=%d authority=%d seq=%d ack=%d flags=0x%x snap-rx=%u ctrl-rx=%u any-rx=%u snap-count=%u input-sent=%u dup=%u unsupported=%u lag=%s",
						totalTics, availableTics, lowestSequence, gametic, ClientTic, commandBacklog,
						authoritySlot, state.CurrentSequence, state.SequenceAck, unsigned(state.Flags),
						peer.RxServerSnapshotSequence, peer.RxControlSequence, peer.RxSequence,
						peer.SnapshotReceived, peer.ClientCommandSent, peer.DuplicateCount,
						peer.UnsupportedReceived, Net_LagStateName(LagState));
					if (*hcde_hud_debug)
					{
						Printf(PRINT_HIGH,
							"HCDE tic gate sustained stall total=%d available=%d lowest=%d gametic=%d clienttic=%d backlog=%d authority=%d seq=%d ack=%d flags=0x%x snap-rx=%u ctrl-rx=%u any-rx=%u snap-count=%u input-sent=%u dup=%u unsupported=%u lag=%s\n",
							totalTics, availableTics, lowestSequence, gametic, ClientTic, commandBacklog,
							authoritySlot, state.CurrentSequence, state.SequenceAck, unsigned(state.Flags),
							peer.RxServerSnapshotSequence, peer.RxControlSequence, peer.RxSequence,
							peer.SnapshotReceived, peer.ClientCommandSent, peer.DuplicateCount,
							peer.UnsupportedReceived, Net_LagStateName(LagState));
					}
				}
				else
				{
					DebugTrace::Warningf("net",
						"tic gate sustained stall total=%d available=%d lowest=%d gametic=%d clienttic=%d backlog=%d authority=%d lag=%s",
						totalTics, availableTics, lowestSequence, gametic, ClientTic, commandBacklog,
						authoritySlot, Net_LagStateName(LagState));
					if (*hcde_hud_debug)
					{
						Printf(PRINT_HIGH,
							"HCDE tic gate sustained stall total=%d available=%d lowest=%d gametic=%d clienttic=%d backlog=%d authority=%d lag=%s\n",
							totalTics, availableTics, lowestSequence, gametic, ClientTic, commandBacklog,
							authoritySlot, Net_LagStateName(LagState));
					}
				}
			}
			else if (*hcde_hud_debug)
			{
				Printf(PRINT_HIGH,
					"HCDE tic gate (dedicated idle) total=%d available=%d lowest=%d gametic=%d clienttic=%d backlog=%d lastWorldTics=%d lag=%s\n",
					totalTics, availableTics, lowestSequence, gametic, ClientTic, commandBacklog,
					EnterTic - LastGameUpdate, Net_LagStateName(LagState));
			}
		}

		// If we're in between a tic, try and balance things out.
		if (totalTics <= 0)
		{
			TicStabilityWait();
		}
		else
		{
			P_ClearLevelInterpolation();
			LagState = LAG_WAITING;
		}

		// If we actually advanced a command, update the player's position (even if a
		// tic passes this isn't guaranteed to happen since it's capped to 35 in advance).
		if (ClientTic > startCommand)
		{
			const int commandBacklog = ClientTic - gametic;
			const int latestCommandTic = max<int>(ClientTic - 1, 0);
			const usercmd_t& latestCmd = LocalCmds[latestCommandTic % LOCALCMDTICS];
			const bool wantsAttack = (latestCmd.buttons & BT_ATTACK) != 0;
			const bool wantsMove = latestCmd.forwardmove != 0 || latestCmd.sidemove != 0 || latestCmd.upmove != 0;
			const bool localClient = consoleplayer >= 0 && consoleplayer < MAXPLAYERS;
			const player_t* localPlayer = localClient ? &players[consoleplayer] : nullptr;
			const AActor* localActor = localPlayer != nullptr ? localPlayer->mo : nullptr;
			const bool compatPredicting = localPlayer != nullptr && (localPlayer->cheats & CF_PREDICTING) != 0;
			const bool staleAuthorityGate = availableTics <= 0
				&& (commandBacklog > TicDup * 4 || EnterTic - LastGameUpdate > TICRATE / 2);
			if (netgame
				&& totalTics > 0
				&& !Net_IsLocalInvasionAuthority()
				&& EnterTic >= HCDEPredictionPauseGraceUntil
				&& staleAuthorityGate
				&& (wantsAttack || wantsMove || compatPredicting)
				&& HCDELiveReportIntervalElapsed(LastHCDEPredictionFaultReportMS, 500u))
			{
				unsigned liveMirrors = 0u;
				unsigned blockingMirrors = 0u;
				unsigned projectileMirrors = 0u;
				for (const auto& ref : InvasionReplicatedActors)
				{
					AActor* actor = ref.Actor.Get();
					if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0 || Net_IsInvasionActorCorpseLike(actor))
						continue;
					++liveMirrors;
					if (ref.IsProjectile)
						++projectileMirrors;
					else
						++blockingMirrors;
				}

				const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
				const FHCDELivePeerState* authorityPeer = authoritySlot >= 0 && authoritySlot < MAXPLAYERS ? &HCDELivePeers[authoritySlot] : nullptr;
				const FClientNetState* authorityState = authoritySlot >= 0 && authoritySlot < MAXPLAYERS ? &ClientStates[authoritySlot] : nullptr;
				++HCDELiveProfile.PredictionFaultReports;
				if (wantsAttack)
					++HCDELiveProfile.PredictionFaultAttackReports;
				if (wantsMove)
					++HCDELiveProfile.PredictionFaultMoveReports;

				DebugTrace::Warningf("net",
					"HCDE prediction fault total=%d available=%d lowest=%d gametic=%d clienttic=%d backlog=%d room=%u map=%s lag=%s predicting=%d compat-predict=%d attack=%d move=%d buttons=0x%08x fwd=%d side=%d up=%d last-world=%d authority=%d seq=%d ack=%d snap-rx=%u ctrl-rx=%u any-rx=%u snap-count=%u input-sent=%u invasion=%s wave=%d active=%d tracked=%u live-mirrors=%u blocking-mirrors=%u projectile-mirrors=%u pending-spawns=%u pending-events=%u actor-missing=%llu pos=(%.1f,%.1f,%.1f) vel=(%.2f,%.2f,%.2f) bob=%.4f health=%d",
					totalTics, availableTics, lowestSequence, gametic, ClientTic, commandBacklog,
					unsigned(CurrentRoomID),
					primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
					Net_LagStateName(LagState),
					NetworkEntityManager::IsPredicting() ? 1 : 0,
					compatPredicting ? 1 : 0,
					wantsAttack ? 1 : 0,
					wantsMove ? 1 : 0,
					latestCmd.buttons,
					latestCmd.forwardmove,
					latestCmd.sidemove,
					latestCmd.upmove,
					EnterTic - LastGameUpdate,
					authoritySlot,
					authorityState != nullptr ? authorityState->CurrentSequence : -1,
					authorityState != nullptr ? authorityState->SequenceAck : -1,
					authorityPeer != nullptr ? authorityPeer->RxServerSnapshotSequence : 0u,
					authorityPeer != nullptr ? authorityPeer->RxControlSequence : 0u,
					authorityPeer != nullptr ? authorityPeer->RxSequence : 0u,
					authorityPeer != nullptr ? authorityPeer->SnapshotReceived : 0u,
					authorityPeer != nullptr ? authorityPeer->ClientCommandSent : 0u,
					Net_InvasionStateName(InvasionState),
					InvasionWaveDirector.Wave,
					Net_GetInvasionActiveMonsterCount(),
					unsigned(InvasionReplicatedActors.Size()),
					liveMirrors,
					blockingMirrors,
					projectileMirrors,
					unsigned(InvasionPendingMirrorSpawns.Size()),
					unsigned(InvasionPendingSpawnEvents.Size()),
					static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2RecordsMissing),
					localActor != nullptr ? localActor->X() : 0.0,
					localActor != nullptr ? localActor->Y() : 0.0,
					localActor != nullptr ? localActor->Z() : 0.0,
					localActor != nullptr ? localActor->Vel.X : 0.0,
					localActor != nullptr ? localActor->Vel.Y : 0.0,
					localActor != nullptr ? localActor->Vel.Z : 0.0,
					localPlayer != nullptr ? double(localPlayer->bob) : 0.0,
					localActor != nullptr ? localActor->health : 0);
				const uint64_t faultTimeMS = I_msTime();
				const FString faultLogPath = FStringf("%s/hcde_prediction_faults.log", M_GetAppDataPath(true).GetChars());
				if (FILE* faultLog = fopen(faultLogPath.GetChars(), "a"))
				{
					fprintf(faultLog,
						"%llu HCDE prediction fault total=%d available=%d lowest=%d gametic=%d clienttic=%d backlog=%d room=%u map=%s lag=%s predicting=%d compat-predict=%d attack=%d move=%d buttons=0x%08x fwd=%d side=%d up=%d last-world=%d authority=%d seq=%d ack=%d snap-rx=%u ctrl-rx=%u any-rx=%u snap-count=%u input-sent=%u invasion=%s wave=%d active=%d tracked=%u live-mirrors=%u blocking-mirrors=%u projectile-mirrors=%u pending-spawns=%u pending-events=%u actor-missing=%llu pos=(%.1f,%.1f,%.1f) vel=(%.2f,%.2f,%.2f) bob=%.4f health=%d\n",
						static_cast<unsigned long long>(faultTimeMS),
						totalTics, availableTics, lowestSequence, gametic, ClientTic, commandBacklog,
						unsigned(CurrentRoomID),
						primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
						Net_LagStateName(LagState),
						NetworkEntityManager::IsPredicting() ? 1 : 0,
						compatPredicting ? 1 : 0,
						wantsAttack ? 1 : 0,
						wantsMove ? 1 : 0,
						latestCmd.buttons,
						latestCmd.forwardmove,
						latestCmd.sidemove,
						latestCmd.upmove,
						EnterTic - LastGameUpdate,
						authoritySlot,
						authorityState != nullptr ? authorityState->CurrentSequence : -1,
						authorityState != nullptr ? authorityState->SequenceAck : -1,
						authorityPeer != nullptr ? authorityPeer->RxServerSnapshotSequence : 0u,
						authorityPeer != nullptr ? authorityPeer->RxControlSequence : 0u,
						authorityPeer != nullptr ? authorityPeer->RxSequence : 0u,
						authorityPeer != nullptr ? authorityPeer->SnapshotReceived : 0u,
						authorityPeer != nullptr ? authorityPeer->ClientCommandSent : 0u,
						Net_InvasionStateName(InvasionState),
						InvasionWaveDirector.Wave,
						Net_GetInvasionActiveMonsterCount(),
						unsigned(InvasionReplicatedActors.Size()),
						liveMirrors,
						blockingMirrors,
						projectileMirrors,
						unsigned(InvasionPendingMirrorSpawns.Size()),
						unsigned(InvasionPendingSpawnEvents.Size()),
						static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2RecordsMissing),
						localActor != nullptr ? localActor->X() : 0.0,
						localActor != nullptr ? localActor->Y() : 0.0,
						localActor != nullptr ? localActor->Z() : 0.0,
						localActor != nullptr ? localActor->Vel.X : 0.0,
						localActor != nullptr ? localActor->Vel.Y : 0.0,
						localActor != nullptr ? localActor->Vel.Z : 0.0,
						localPlayer != nullptr ? double(localPlayer->bob) : 0.0,
						localActor != nullptr ? localActor->health : 0);
					fclose(faultLog);
				}
				// Forensic capture for the fault is gated by a long cooldown so a
				// stretch of consecutive faults can't lock the playsim thread on
				// disk I/O. The diagnostic warning above ALWAYS fires (so the
				// trace file still records every fault), but the multi-megabyte
				// trace snapshot, blackbox flush, and full bundle write happen
				// at most once per minute. Without this, a string of faults
				// (e.g. while the user fights through a heavy room) produces a
				// fresh ~2-3MB bundle every 500ms - the same I/O storm pattern
				// that the checksum mismatch path was already gated against.
				constexpr uint64_t PredictionFaultBundleCooldownMS = 60000u;
				if (LastPredictionFaultBundleMS == 0u
					|| faultTimeMS - LastPredictionFaultBundleMS >= PredictionFaultBundleCooldownMS)
				{
					LastPredictionFaultBundleMS = faultTimeMS;
					const FString tracePath = FStringf("%s/hcde_prediction_fault_trace_%llu.txt",
						M_GetAppDataPath(true).GetChars(), static_cast<unsigned long long>(faultTimeMS));
					DebugTrace::SaveToFile(tracePath.GetChars(), "net", DebugTrace::Severity::Debug);
					Net_BlackboxAutoSave("prediction_fault");
					FString faultBundlePath;
					Net_DiagWriteBundle("prediction_fault", faultBundlePath);
				}
				if (*hcde_hud_debug)
				{
					Printf(PRINT_HIGH,
						"HCDE prediction fault available=%d backlog=%d attack=%d move=%d compat-predict=%d snap-rx=%u input-sent=%u wave=%d active=%d tracked=%u blocking=%u pending=%u/%u missing=%llu\n",
						availableTics, commandBacklog, wantsAttack ? 1 : 0, wantsMove ? 1 : 0,
						compatPredicting ? 1 : 0,
						authorityPeer != nullptr ? authorityPeer->SnapshotReceived : 0u,
						authorityPeer != nullptr ? authorityPeer->ClientCommandSent : 0u,
						InvasionWaveDirector.Wave,
						Net_GetInvasionActiveMonsterCount(),
						unsigned(InvasionReplicatedActors.Size()),
						blockingMirrors,
						unsigned(InvasionPendingMirrorSpawns.Size()),
						unsigned(InvasionPendingSpawnEvents.Size()),
						static_cast<unsigned long long>(HCDELiveProfile.ActorDeltaV2RecordsMissing));
				}
			}
			if (availableTics <= 0 && commandBacklog > TicDup * 4)
			{
				// Do not keep ghost-walking on predicted input when the authority has
				// stopped advancing our confirmed sequence. Stay on the last authoritative
				// pose until snapshots catch up instead of drifting further away.
				P_UnPredictClient();
				LagState = LAG_WAITING;
			}
			else
			{
				LagState = LAG_PREDICTING;
				if (netgame
					&& totalTics > 0
					&& !Net_IsLocalInvasionAuthority())
				{
					InvasionMirrorVisualTickBudget = 1;
					Net_TickInvasionMirrorVisualFrame();
					InvasionMirrorVisualTickBudget = 0;
				}
				P_PredictClient();
			}
		}

		// If we actually did have some tics available, make sure the UI
		// still has a chance to run.
		for (int i = 0; i < totalTics; ++i)
			P_RunClientSideLogic();

		if (totalTics > 0)
		{
			S_UpdateSounds(players[consoleplayer].camera, primaryLevel->LocalWorldTimer - min<int>(primaryLevel->LocalWorldTimer, worldTimer));
			NetworkEntityManager::VerifyPredictedEntities();
		}

		const bool ticGateStalled = runTics <= 0 && totalTics > 0
			&& (availableTics < 0
				|| (availableTics <= 0 && ClientTic - gametic > TicDup * 3)
				|| (availableTics <= 0 && EnterTic - LastGameUpdate > TICRATE / 2));
		if (ticGateStalled)
			G_TickStalledCutscene();
		Net_RefreshLagHUDMetrics(availableTics, 0, totalTics, 0, ticGateStalled);
		return;
	}

	LagState = ClientTic > startCommand ? LAG_NONE : LAG_SKIPPING;
	for (auto client : NetworkClients)
		players[client].waiting = false;

	// Update the last time the game tic'd.
	LastGameUpdate = EnterTic;

	// Run the available tics.
	P_UnPredictClient();
	HCDEApplyPendingLocalHealthRepair();
	int worldStepsUsedThisFrame = 0;
	while (runTics--)
	{
		++worldStepsUsedThisFrame;
		const bool stabilize = ShouldStabilizeTick();
		if (stabilize)
			TicStabilityBegin();

		const uint64_t worldTicStartUS = HCDEProfileNowUS();
		if (advancedemo)
			D_DoAdvanceDemo();

		if (Net_IsLocalInvasionAuthority())
			Net_PrepareInvasionSimulationLOD();
		G_Ticker();
		++gametic;
		Net_ChecksumTick();
		Net_BlackboxRecordTicIndex();
		if (Net_IsLocalInvasionAuthority())
			Net_TickInvasionState();
		else
			Net_TickInvasionMirrorState();
		Net_TickHCDEModeActorMigration();
		Net_TickInvasionAnnouncements();
		MakeConsistencies();
		HCDERewind_OnServerTick();
		HCDEProfileRecordWorldTic(HCDEProfileNowUS() - worldTicStartUS);

		if (stabilize)
			TicStabilityEnd();

		if (bCommandsReset)
		{
			bCommandsReset = false;
			break;
		}
	}
	if (!Net_IsLocalInvasionAuthority())
	{
		InvasionMirrorVisualWorldSteps = (netgame && worldStepsUsedThisFrame > 0)
			? clamp<int>(worldStepsUsedThisFrame, 1, 32)
			: 1;
		InvasionMirrorVisualTickBudget = 1;
		Net_TickInvasionMirrorVisualFrame();
	}
	InvasionMirrorVisualTickBudget = 0;
	InvasionMirrorVisualWorldSteps = 1;
	P_PredictClient();

	// These should use the actual tics since they're not actually tied to the gameplay logic.
	// Make sure it always comes after so the HUD has the correct game state when updating.
	for (int i = 0; i < totalTics; ++i)
		P_RunClientSideLogic();

	// Since the level could get reset mid-tick, make sure the smaller of the two values is used
	// since it should only go up otherwise.
	S_UpdateSounds(players[consoleplayer].camera, primaryLevel->LocalWorldTimer - min<int>(primaryLevel->LocalWorldTimer, worldTimer));
	NetworkEntityManager::VerifyPredictedEntities();
	Net_RefreshLagHUDMetrics(availableTics, runTics, totalTics, worldStepsUsedThisFrame, false);
}

void Net_NewClientTic()
{
	NetEvents.NewClientTic();
}

void Net_Initialize()
{
	NetEvents.InitializeEventData();
}

uint8_t Net_GetCurrentRoomID()
{
	return CurrentRoomID;
}

void Net_WriteInt8(uint8_t it)
{
	NetEvents << it;
}

void Net_WriteInt16(int16_t it)
{
	NetEvents << it;
}

void Net_WriteInt32(int32_t it)
{
	NetEvents << it;
}

void Net_WriteInt64(int64_t it)
{
	NetEvents << it;
}

void Net_WriteFloat(float it)
{
	NetEvents << it;
}

void Net_WriteDouble(double it)
{
	NetEvents << it;
}

void Net_WriteString(const char *it)
{
	NetEvents << it;
}

void Net_WriteBytes(const uint8_t *block, int len)
{
	while (len--)
		NetEvents << *block++;
}

//==========================================================================
//
// Dynamic buffer interface
//
//==========================================================================

FDynamicBuffer::FDynamicBuffer()
{
	m_Data = nullptr;
	m_Len = m_BufferLen = 0;
}

FDynamicBuffer::~FDynamicBuffer()
{
	if (m_Data != nullptr)
	{
		M_Free(m_Data);
		m_Data = nullptr;
	}
	m_Len = m_BufferLen = 0;
}

void FDynamicBuffer::SetData(const uint8_t *data, int len)
{
	if (len > m_BufferLen)
	{
		m_BufferLen = (len + 255) & ~255;
		m_Data = (uint8_t *)M_Realloc(m_Data, m_BufferLen);
	}

	if (data != nullptr)
	{
		m_Len = len;
		memcpy(m_Data, data, len);
	}
	else
	{
		m_Len = 0;
	}
}

uint8_t *FDynamicBuffer::GetData(int *len)
{
	if (len != nullptr)
		*len = m_Len;
	return m_Len ? m_Data : nullptr;
}

TArrayView<uint8_t> FDynamicBuffer::GetTArrayView()
{
	return TArrayView(m_Data, m_Len);
}

static int RemoveClass(FLevelLocals *Level, const PClass *cls)
{
	AActor *actor;
	int removecount = 0;
	bool player = false;
	auto iterator = Level->GetThinkerIterator<AActor>(cls->TypeName);
	while ((actor = iterator.Next()))
	{
		if (actor->IsA(cls))
		{
			// [MC]Do not remove LIVE players.
			if (actor->player != nullptr)
			{
				player = true;
				continue;
			}
			// [SP] Don't remove owned inventory objects.
			if (!actor->IsMapActor())
				continue;

			removecount++;
			actor->ClearCounters();
			actor->Destroy();
		}
	}

	if (player)
		Printf("Cannot remove live players!\n");

	return removecount;

}

EXTERN_CVAR(Int, displaynametags)
EXTERN_CVAR(Int, nametagcolor)

static void SelectWeapon(int player, int slot)
{
	auto mo = players[player].mo;
	if (mo == nullptr || gamestate != GS_LEVEL || paused
		|| players[player].playerstate != PST_LIVE)
	{
		return;
	}

	AActor* weap = nullptr;
	if (slot >= 0 && slot < NUM_WEAPON_SLOTS)
	{
		IFVIRTUALPTRNAME(mo, NAME_PlayerPawn, PickWeapon)
			weap = CallVM<AActor*>(func, mo, slot, (int)!(dmflags2 & DF2_DONTCHECKAMMO));
	}
	else if (slot == WST_NEXT)
	{
		IFVIRTUALPTRNAME(mo, NAME_PlayerPawn, PickNextWeapon)
			weap = CallVM<AActor*>(func, mo);
	}
	else if (slot == WST_PREV)
	{
		IFVIRTUALPTRNAME(mo, NAME_PlayerPawn, PickPrevWeapon)
			weap = CallVM<AActor*>(func, mo);
	}

	if (weap == nullptr)
		return;

	// Make sure the returned weapon actually exists in that player's inventory.
	const unsigned id = weap->InventoryID;
	AActor* invItem = mo->Inventory;
	for (; invItem != nullptr; invItem = invItem->Inventory)
	{
		if (invItem->InventoryID == id)
			break;
	}

	if (invItem != weap)
		return;

	if (player == consoleplayer)
	{
		if (weap != players[player].ReadyWeapon)
			S_Sound(mo, CHAN_AUTO, 0, "misc/weaponchange", 1.0, ATTN_NONE);

		// [Nash] Option to display the name of the weapon being switched to.
		if ((displaynametags & 2) && StatusBar != nullptr && SmallFont != nullptr)
		{
			StatusBar->AttachMessage(Create<DHUDMessageFadeOut>(nullptr, weap->GetTag(),
				1.5f, 0.90f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID('W', 'E', 'P', 'N'));
		}
	}

	mo->UseInventory(weap);
}

static void UseFlechette(int player)
{
	auto mo = players[player].mo;
	if (mo == nullptr || gamestate != GS_LEVEL || paused
		|| players[player].playerstate != PST_LIVE)
	{
		return;
	}

	AActor* item = nullptr;
	IFVIRTUALPTRNAME(mo, NAME_PlayerPawn, GetFlechetteItem)
		item = CallVM<AActor*>(func, mo);

	if (item == nullptr)
		return;

	// Make sure the returned item actually exists in that player's inventory.
	const unsigned id = item->InventoryID;
	AActor* invItem = mo->Inventory;
	for (; invItem != nullptr; invItem = invItem->Inventory)
	{
		if (invItem->InventoryID == id)
			break;
	}

	if (invItem == item)
		mo->UseInventory(item);
}

// [RH] Execute a special "ticcmd". The type byte should
//		have already been read, and the stream is positioned
//		at the beginning of the command's actual data.
void Net_DoCommand(int cmd, TArrayView<uint8_t>& stream, int player)
{
	uint8_t pos = 0;
	const char* s = nullptr;
	int i = 0;

	switch (cmd)
	{
	case DEM_SAY:
		{
			const char *name = players[player].userinfo.GetName();
			uint8_t who = ReadInt8(stream);

			s = ReadStringConst(stream);
			// If chat is disabled, there's nothing else to do here since the stream has been advanced.
			if (cl_showchat == CHAT_DISABLED || (MutedClients & ((uint64_t)1u << player)))
				break;

			constexpr int MSG_TEAM = 1;
			constexpr int MSG_BOLD = 2;
			if (!(who & MSG_TEAM))
			{
				if (cl_showchat < CHAT_GLOBAL)
					break;

				// Said to everyone
				if (deathmatch && teamplay)
					Printf(PRINT_CHAT, "(All) ");
				if ((who & MSG_BOLD) && !cl_noboldchat)
					Printf(PRINT_CHAT, TEXTCOLOR_BOLD "* %s [%d]" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, player, s);
				else
					Printf(PRINT_CHAT, "%s [%d]" TEXTCOLOR_CHAT ": %s" TEXTCOLOR_CHAT "\n", name, player, s);

				if (!cl_nochatsound)
					S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
			}
			else if (!deathmatch || players[player].userinfo.GetTeam() == players[consoleplayer].userinfo.GetTeam())
			{
				if (cl_showchat < CHAT_TEAM_ONLY)
					break;

				// Said only to members of the player's team
				if (deathmatch && teamplay)
					Printf(PRINT_TEAMCHAT, "(Team) ");
				if ((who & MSG_BOLD) && !cl_noboldchat)
					Printf(PRINT_TEAMCHAT, TEXTCOLOR_BOLD "* %s [%d]" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, player, s);
				else
					Printf(PRINT_TEAMCHAT, "%s [%d]" TEXTCOLOR_TEAMCHAT ": %s" TEXTCOLOR_TEAMCHAT "\n", name, player, s);

				if (!cl_nochatsound)
					S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
			}
		}
		break;

	case DEM_MUSICCHANGE:
		S_ChangeMusic(ReadStringConst(stream));
		break;

	case DEM_PRINT:
		Printf("%s", ReadStringConst(stream));
		break;

	case DEM_CENTERPRINT:
		C_MidPrint(nullptr, ReadStringConst(stream));
		break;

	case DEM_UINFCHANGED:
		D_ReadUserInfoStrings(player, stream, true);
		break;

	case DEM_SINFCHANGED:
		D_DoServerInfoChange(stream, false);
		break;

	case DEM_SINFCHANGEDXOR:
		D_DoServerInfoChange(stream, true);
		break;

	case DEM_GIVECHEAT:
		s = ReadStringConst(stream);
		cht_Give(&players[player], s, ReadInt32(stream));
		if (player != consoleplayer)
		{
			FString message = GStrings.GetString("TXT_X_CHEATS");
			message.Substitute("%s", players[player].userinfo.GetName());
			Printf("%s: give %s\n", message.GetChars(), s);
		}
		break;

	case DEM_TAKECHEAT:
		s = ReadStringConst(stream);
		cht_Take(&players[player], s, ReadInt32(stream));
		break;

	case DEM_SETINV:
		s = ReadStringConst(stream);
		i = ReadInt32(stream);
		cht_SetInv(&players[player], s, i, !!ReadInt8(stream));
		break;

	case DEM_WARPCHEAT:
		{
			int x = ReadInt16(stream);
			int y = ReadInt16(stream);
			int z = ReadInt16(stream);
			P_TeleportMove(players[player].mo, DVector3(x, y, z), true);
		}
		break;

	case DEM_GENERICCHEAT:
		cht_DoCheat(&players[player], ReadInt8(stream));
		break;

	case DEM_CHANGEMAP2:
		pos = ReadInt8(stream);
		[[fallthrough]];
	case DEM_CHANGEMAP:
		// Change to another map without disconnecting other players
		s = ReadStringConst(stream);
		// Using LEVEL_NOINTERMISSION tends to throw the game out of sync.
		// That was a long time ago. Maybe it works now?
		primaryLevel->flags |= LEVEL_CHANGEMAPCHEAT;
		primaryLevel->ChangeLevel(s, pos, 0);
		break;

	case DEM_SUICIDE:
		cht_Suicide(&players[player]);
		break;

	case DEM_ADDBOT:
		primaryLevel->BotInfo.TryAddBot(primaryLevel, stream, player);
		break;

	case DEM_KILLBOTS:
		primaryLevel->BotInfo.RemoveAllBots(primaryLevel, true);
		Printf ("Removed all bots\n");
		break;

	case DEM_CENTERVIEW:
		players[player].centering = true;
		break;

	case DEM_INVUSEALL:
		if (gamestate == GS_LEVEL && !paused
			&& players[player].playerstate != PST_DEAD)
		{
			AActor *item = players[player].mo->Inventory;
			auto pitype = PClass::FindActor(NAME_PuzzleItem);
			while (item != nullptr)
			{
				AActor *next = item->Inventory;
				IFVIRTUALPTRNAME(item, NAME_Inventory, UseAll)
				{
					VMValue param[] = { item, players[player].mo };
					VMCall(func, param, 2, nullptr, 0);
				}
				item = next;
			}
		}
		break;

	case DEM_INVUSE:
	case DEM_INVDROP:
		{
			uint32_t which = ReadInt32(stream);
			int amt = -1;
			if (cmd == DEM_INVDROP)
				amt = ReadInt32(stream);

			if (gamestate == GS_LEVEL && !paused
				&& players[player].playerstate != PST_DEAD)
			{
				auto item = players[player].mo->Inventory;
				while (item != nullptr && item->InventoryID != which)
					item = item->Inventory;

				if (item != nullptr)
				{
					if (cmd == DEM_INVUSE)
						players[player].mo->UseInventory(item);
					else
						players[player].mo->DropInventory(item, amt);
				}
			}
		}
		break;

	case DEM_SUMMON:
	case DEM_SUMMONFRIEND:
	case DEM_SUMMONFOE:
	case DEM_SUMMONMBF:
	case DEM_SUMMON2:
	case DEM_SUMMONFRIEND2:
	case DEM_SUMMONFOE2:
		{
			int angle = 0;
			int16_t tid = 0;
			uint8_t special = 0;
			int args[5];

			s = ReadStringConst(stream);
			if (cmd >= DEM_SUMMON2 && cmd <= DEM_SUMMONFOE2)
			{
				angle = ReadInt16(stream);
				tid = ReadInt16(stream);
				special = ReadInt8(stream);
				for (i = 0; i < 5; i++) args[i] = ReadInt32(stream);
			}

			AActor* source = players[player].mo;
			if (source != NULL)
			{
				PClassActor* typeinfo = PClass::FindActor(s);
				if (typeinfo != NULL)
				{
					if (GetDefaultByType(typeinfo)->flags & MF_MISSILE)
					{
						P_SpawnPlayerMissile(source, 0, 0, 0, typeinfo, source->Angles.Yaw);
					}
					else
					{
						const AActor* def = GetDefaultByType(typeinfo);
						DVector3 spawnpos = source->Vec3Angle(def->radius * 2 + source->radius, source->Angles.Yaw, 8.);

						AActor* spawned = Spawn(primaryLevel, typeinfo, spawnpos, ALLOW_REPLACE);
						if (spawned != NULL)
						{
							spawned->SpawnFlags |= MTF_CONSOLETHING;
							if (cmd == DEM_SUMMONFRIEND || cmd == DEM_SUMMONFRIEND2 || cmd == DEM_SUMMONMBF)
							{
								if (spawned->CountsAsKill())
								{
									primaryLevel->total_monsters--;
								}
								spawned->FriendPlayer = player + 1;
								spawned->flags |= MF_FRIENDLY;
								spawned->LastHeard = players[player].mo;
								spawned->health = spawned->SpawnHealth();
								if (cmd == DEM_SUMMONMBF)
									spawned->flags3 |= MF3_NOBLOCKMONST;
							}
							else if (cmd == DEM_SUMMONFOE || cmd == DEM_SUMMONFOE2)
							{
								spawned->FriendPlayer = 0;
								spawned->flags &= ~MF_FRIENDLY;
								spawned->health = spawned->SpawnHealth();
							}

							if (cmd >= DEM_SUMMON2 && cmd <= DEM_SUMMONFOE2)
							{
								spawned->Angles.Yaw = source->Angles.Yaw - DAngle::fromDeg(angle);
								spawned->special = special;
								for (i = 0; i < 5; i++) {
									spawned->args[i] = args[i];
								}
								if (tid) spawned->SetTID(tid);
							}
						}
					}
				}
				else
				{ // not an actor, must be a visualthinker
					PClass* typeinfo = PClass::FindClass(s);
					if (typeinfo && typeinfo->IsDescendantOf("VisualThinker"))
					{
						DVector3 spawnpos = source->Vec3Angle(source->radius * 4, source->Angles.Yaw, 8.);
						auto vt = DVisualThinker::NewVisualThinker(source->Level, typeinfo, false);
						if (vt)
						{
							vt->PT.Pos = spawnpos;
							vt->UpdateSector();
						}
					}
				}
			}
		}
		break;

	case DEM_SPRAY:
		s = ReadStringConst(stream);
		SprayDecal(players[player].mo, s);
		break;

	case DEM_MDK:
		s = ReadStringConst(stream);
		cht_DoMDK(&players[player], s);
		break;

	case DEM_PAUSE:
		if (gamestate == GS_LEVEL)
		{
			if (paused)
			{
				paused = 0;
				S_ResumeSound(false);
			}
			else
			{
				paused = player + 1;
				S_PauseSound(false, false);
			}
		}
		break;

	case DEM_SAVEGAME:
		if (gamestate == GS_LEVEL)
		{
			savegamefile = ReadStringConst(stream);
			savedescription = ReadStringConst(stream);
			if (player != consoleplayer)
			{
				// Paths sent over the network will be valid for the system that sent
				// the save command. For other systems, the path needs to be changed.
				FString basename = ExtractFileBase(savegamefile.GetChars(), true);
				savegamefile = G_BuildSaveName(basename.GetChars());
			}
		}
		G_TraceSetGameAction(ga_savegame, "net-autosave-request");
		break;

	case DEM_CHECKAUTOSAVE:
		// Do not autosave in multiplayer games or when dead.
		// For demo playback, DEM_DOAUTOSAVE already exists in the demo if the
		// autosave happened. And if it doesn't, we must not generate it.
		if (!netgame && !demoplayback && disableautosave < 2 && autosavecount
			&& players[player].playerstate == PST_LIVE && !deathmatch)
		{
			Net_WriteInt8(DEM_DOAUTOSAVE);
		}
		break;

	case DEM_DOAUTOSAVE:
		G_TraceSetGameAction(ga_autosave, "net-autosave");
		break;

	case DEM_FOV:
		{
			float newfov = ReadFloat(stream);
			if (newfov != players[player].DesiredFOV)
			{
				Printf("FOV%s set to %g\n",
					I_IsHCDEServiceAuthoritySlot(player) ? " for everyone" : "",
					newfov);
			}

			for (auto client : NetworkClients)
				players[client].DesiredFOV = newfov;
		}
		break;

	case DEM_MYFOV:
		players[player].DesiredFOV = ReadFloat(stream);
		break;

	case DEM_RUNSCRIPT:
	case DEM_RUNSCRIPT2:
		{
			int snum = ReadInt16(stream);
			int argn = ReadInt8(stream);
			RunScript(stream, players[player].mo, snum, argn, (cmd == DEM_RUNSCRIPT2) ? ACS_ALWAYS : 0);
		}
		break;

	case DEM_RUNNAMEDSCRIPT:
		{
			s = ReadStringConst(stream);
			int argn = ReadInt8(stream);
			RunScript(stream, players[player].mo, -FName(s).GetIndex(), argn & 127, (argn & 128) ? ACS_ALWAYS : 0);
		}
		break;

	case DEM_RUNSPECIAL:
		{
			int snum = ReadInt16(stream);
			int argn = ReadInt8(stream);
			int arg[5] = {};

			for (i = 0; i < argn; ++i)
			{
				int argval = ReadInt32(stream);
				if ((unsigned)i < countof(arg))
					arg[i] = argval;
			}

			if (!CheckCheatmode(player == consoleplayer))
				P_ExecuteSpecial(primaryLevel, snum, nullptr, players[player].mo, false, arg[0], arg[1], arg[2], arg[3], arg[4]);
		}
		break;

	case DEM_CROUCH:
		if (gamestate == GS_LEVEL && players[player].mo != nullptr
			&& players[player].playerstate == PST_LIVE && !(players[player].oldbuttons & BT_JUMP)
			&& !P_IsPlayerTotallyFrozen(&players[player]))
		{
			players[player].crouching = players[player].crouchdir < 0 ? 1 : -1;
		}
		break;

	case DEM_MORPHEX:
		{
			s = ReadStringConst(stream);
			FString msg = cht_Morph(players + player, PClass::FindActor(s), false);
			if (player == consoleplayer)
				Printf("%s\n", msg[0] != '\0' ? msg.GetChars() : "Morph failed.");
		}
		break;

	case DEM_ADDCONTROLLER:
		{
			uint8_t playernum = ReadInt8(stream);
			players[playernum].settings_controller = true;
			if (consoleplayer == playernum)
				Printf("You can now control game settings\n");
			else if (I_IsLocalHCDEServiceAuthority())
				Printf("%s [%d] is now a settings controller\n", players[playernum].userinfo.GetName(), playernum);
		}
		break;

	case DEM_DELCONTROLLER:
		{
			uint8_t playernum = ReadInt8(stream);
			players[playernum].settings_controller = false;
			if (consoleplayer == playernum)
				Printf("You can no longer control game settings\n");
			else if (I_IsLocalHCDEServiceAuthority())
				Printf("%s [%d] is no longer a settings controller\n", players[playernum].userinfo.GetName(), playernum);
		}
		break;

	case DEM_KILLCLASSCHEAT:
		{
			s = ReadStringConst(stream);
			int killcount = 0;
			PClassActor *cls = PClass::FindActor(s);

			if (cls != nullptr)
			{
				killcount = primaryLevel->Massacre(false, cls->TypeName);
				PClassActor *cls_rep = cls->GetReplacement(primaryLevel);
				if (cls != cls_rep)
					killcount += primaryLevel->Massacre(false, cls_rep->TypeName);

				Printf("Killed %d monsters of type %s.\n", killcount, s);
			}
			else
			{
				Printf("%s is not an actor class.\n", s);
			}
		}
		break;

	case DEM_REMOVE:
		{
			s = ReadStringConst(stream);
			int removecount = 0;
			PClassActor *cls = PClass::FindActor(s);
			if (cls != nullptr && cls->IsDescendantOf(RUNTIME_CLASS(AActor)))
			{
				removecount = RemoveClass(primaryLevel, cls);
				const PClass *cls_rep = cls->GetReplacement(primaryLevel);
				if (cls != cls_rep)
					removecount += RemoveClass(primaryLevel, cls_rep);

				Printf("Removed %d actors of type %s.\n", removecount, s);
			}
			else
			{
				Printf("%s is not an actor class.\n", s);
			}
		}
		break;

	case DEM_CONVREPLY:
	case DEM_CONVCLOSE:
	case DEM_CONVNULL:
		P_ConversationCommand(cmd, player, stream);
		break;

	case DEM_SETSLOT:
	case DEM_SETSLOTPNUM:
		{
			int pnum = player;
			if (cmd == DEM_SETSLOTPNUM)
				pnum = ReadInt8(stream);

			unsigned int slot = ReadInt8(stream);
			int count = ReadInt8(stream);
			if (slot < NUM_WEAPON_SLOTS)
				players[pnum].weapons.ClearSlot(slot);

			for (i = 0; i < count; ++i)
			{
				PClassActor *wpn = Net_ReadWeapon(stream);
				players[pnum].weapons.AddSlot(slot, wpn, pnum == consoleplayer);
			}
		}
		break;

	case DEM_ADDSLOT:
		{
			int slot = ReadInt8(stream);
			PClassActor *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlot(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_ADDSLOTDEFAULT:
		{
			int slot = ReadInt8(stream);
			PClassActor *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlotDefault(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_SETPITCHLIMIT:
		players[player].MinPitch = DAngle::fromDeg(-ReadInt8(stream));		// up
		players[player].MaxPitch = DAngle::fromDeg(ReadInt8(stream));		// down
		break;

	case DEM_REVERTCAMERA:
		players[player].camera = players[player].mo;
		break;

	case DEM_FINISHGAME:
		// Simulate an end-of-game action
		primaryLevel->ChangeLevel(nullptr, 0, 0);
		break;

	case DEM_NETEVENT:
		{
			s = ReadStringConst(stream);
			int argn = ReadInt8(stream);
			int arg[3] = { 0, 0, 0 };
			for (int i = 0; i < 3; i++)
				arg[i] = ReadInt32(stream);
			bool manual = !!ReadInt8(stream);
			primaryLevel->localEventManager->Console(player, s, arg[0], arg[1], arg[2], manual, false);
		}
		break;

	case DEM_ENDSCREENJOB:
		EndScreenJob();
		break;

	case DEM_READIED:
		Net_PlayerReadiedUp(player);
		break;

	case DEM_ZSC_CMD:
		{
			FName cmd = ReadStringConst(stream);
			unsigned int size = ReadInt16(stream);

			TArray<uint8_t> buffer;
			if (size)
			{
				buffer.Grow(size);
				for (unsigned int i = 0u; i < size; ++i)
					buffer.Push(ReadInt8(stream));
			}

			FNetworkCommand netCmd = { player, cmd, buffer };
			primaryLevel->localEventManager->NetCommand(netCmd);
		}
		break;

	case DEM_CHANGESKILL:
		NextSkill = ReadInt32(stream);
		break;

	case DEM_KICK:
		{
			const int pNum = ReadInt8(stream);
			if (pNum == consoleplayer)
			{
				I_Error("You have been kicked from the game");
			}
			else if (NetworkClients.InGame(pNum))
			{
				Printf("%s [%d] has been kicked from the game\n", players[pNum].userinfo.GetName(), pNum);
				DebugTrace::Warningf("net", "kick command disconnecting player=%d by=%d gametic=%d clienttic=%d room=%u",
					pNum, player, gametic, ClientTic, unsigned(CurrentRoomID));
				DisconnectClient(pNum);
			}
		}
		break;

	case DEM_WEAPSELECT:
		SelectWeapon(player, ReadInt8(stream));
		break;

	case DEM_USEFLECHETTE:
		UseFlechette(player);
		break;

	default:
		I_Error("Unknown net command: %d", cmd);
		break;
	}
}

// Used by DEM_RUNSCRIPT, DEM_RUNSCRIPT2, and DEM_RUNNAMEDSCRIPT
static void RunScript(TArrayView<uint8_t>& stream, AActor *pawn, int snum, int argn, int always)
{
	// Scripts can be invoked without a level loaded, e.g. via puke(name) CCMD in fullscreen console
	if (pawn == nullptr)
		return;

	int arg[4] = {};
	for (int i = 0; i < argn; ++i)
	{
		int argval = ReadInt32(stream);
		if ((unsigned)i < countof(arg))
			arg[i] = argval;
	}

	P_StartScript(pawn->Level, pawn, nullptr, snum, primaryLevel->MapName.GetChars(), arg, min<int>(countof(arg), argn), ACS_NET | always);
}

// TODO: This really needs to be replaced with some kind of packet system that can simply read through packets and opt
// not to execute them. Right now this is making setting up net commands a nightmare.
// Reads through the network stream but doesn't actually execute any command. Used for getting the size of a stream.
// The skip amount is the number of bytes the command possesses. This should mirror the bytes in Net_DoCommand().
static bool Net_TrySkipCommand(int cmd, TArrayView<uint8_t>& stream)
{
	size_t skip = 0;
	auto tryReadCStringBytes = [&stream](size_t offset, size_t& bytesWithNull) -> bool
	{
		if (offset > stream.Size())
			return false;

		const uint8_t* start = stream.Data() + offset;
		const size_t available = stream.Size() - offset;
		const void* terminator = memchr(start, 0, available);
		if (terminator == nullptr)
			return false;

		bytesWithNull = size_t(static_cast<const uint8_t*>(terminator) - start) + 1u;
		return true;
	};

	size_t stringBytes = 0u;
	switch (cmd)
	{
		case DEM_SAY:
			if (!tryReadCStringBytes(1u, stringBytes))
				return false;
			skip = 1u + stringBytes;
			break;

		case DEM_ADDBOT:
			if (!tryReadCStringBytes(1u, stringBytes))
				return false;
			skip = stringBytes + 5u;
			break;

		case DEM_GIVECHEAT:
		case DEM_TAKECHEAT:
			if (!tryReadCStringBytes(0u, stringBytes))
				return false;
			skip = stringBytes + 4u;
			break;

		case DEM_SETINV:
			if (!tryReadCStringBytes(0u, stringBytes))
				return false;
			skip = stringBytes + 5u;
			break;

		case DEM_NETEVENT:
			if (!tryReadCStringBytes(0u, stringBytes))
				return false;
			skip = stringBytes + 14u;
			break;

		case DEM_ZSC_CMD:
			if (!tryReadCStringBytes(0u, stringBytes))
				return false;
			skip = stringBytes;
			if (skip > stream.Size() || stream.Size() - skip < 2u)
				return false;
			skip += 2u + ((size_t(stream[skip]) << 8) | size_t(stream[skip + 1]));
			break;

		case DEM_SUMMON2:
		case DEM_SUMMONFRIEND2:
		case DEM_SUMMONFOE2:
			if (!tryReadCStringBytes(0u, stringBytes))
				return false;
			skip = stringBytes + 25u;
			break;
		case DEM_CHANGEMAP2:
			if (!tryReadCStringBytes(1u, stringBytes))
				return false;
			skip = 1u + stringBytes;
			break;
		case DEM_MUSICCHANGE:
		case DEM_PRINT:
		case DEM_CENTERPRINT:
		case DEM_UINFCHANGED:
		case DEM_CHANGEMAP:
		case DEM_SUMMON:
		case DEM_SUMMONFRIEND:
		case DEM_SUMMONFOE:
		case DEM_SUMMONMBF:
		case DEM_REMOVE:
		case DEM_SPRAY:
		case DEM_MORPHEX:
		case DEM_KILLCLASSCHEAT:
		case DEM_MDK:
			if (!tryReadCStringBytes(0u, stringBytes))
				return false;
			skip = stringBytes;
			break;

		case DEM_WARPCHEAT:
			skip = 6;
			break;

		case DEM_INVUSE:
		case DEM_FOV:
		case DEM_MYFOV:
		case DEM_CHANGESKILL:
			skip = 4;
			break;

		case DEM_INVDROP:
			skip = 8;
			break;

		case DEM_GENERICCHEAT:
		case DEM_DROPPLAYER:
		case DEM_ADDCONTROLLER:
		case DEM_DELCONTROLLER:
		case DEM_KICK:
		case DEM_WEAPSELECT:
			skip = 1;
			break;

		case DEM_SAVEGAME:
			if (!tryReadCStringBytes(0u, stringBytes))
				return false;
			skip = stringBytes;
			if (!tryReadCStringBytes(skip, stringBytes))
				return false;
			skip += stringBytes;
			break;

		case DEM_SINFCHANGEDXOR:
		case DEM_SINFCHANGED:
			{
				if (stream.Size() < 1u)
					return false;
				uint8_t t = stream[0];
				skip = 1u + (t & 63u);
				if (cmd == DEM_SINFCHANGED)
				{
					switch (t >> 6)
					{
					case CVAR_Bool:
						skip += 1;
						break;
					case CVAR_Int:
					case CVAR_Float:
						skip += 4;
						break;
					case CVAR_String:
						if (!tryReadCStringBytes(skip, stringBytes))
							return false;
						skip += stringBytes;
						break;
					}
				}
				else
				{
					skip += 1u;
				}
			}
			break;

		case DEM_RUNSCRIPT:
		case DEM_RUNSCRIPT2:
			if (stream.Size() < 3u)
				return false;
			skip = 3u + size_t(stream[2]) * 4u;
			break;

		case DEM_RUNNAMEDSCRIPT:
			if (!tryReadCStringBytes(0u, stringBytes))
				return false;
			skip = stringBytes + 1u;
			if (skip > stream.Size())
				return false;
			skip += size_t(stream[skip - 1u] & 127u) * 4u;
			break;

		case DEM_RUNSPECIAL:
			if (stream.Size() < 3u)
				return false;
			skip = 3u + size_t(stream[2]) * 4u;
			break;

		case DEM_CONVREPLY:
			skip = 3;
			break;

		case DEM_SETSLOT:
		case DEM_SETSLOTPNUM:
			{
				skip = 2u + (cmd == DEM_SETSLOTPNUM ? 1u : 0u);
				if (skip == 0u || skip > stream.Size())
					return false;
				for (int numweapons = stream[skip - 1u]; numweapons > 0; --numweapons)
				{
					if (skip >= stream.Size())
						return false;
					skip += 1u + (stream[skip] >> 7);
				}
			}
			break;

		case DEM_ADDSLOT:
		case DEM_ADDSLOTDEFAULT:
			if (stream.Size() < 2u)
				return false;
			skip = 2u + (stream[1] >> 7);
			break;

		case DEM_SETPITCHLIMIT:
			skip = 2u;
			break;

		// Commands with no payload bytes.
		case DEM_SUICIDE:
		case DEM_KILLBOTS:
		case DEM_INVUSEALL:
		case DEM_PAUSE:
		case DEM_CENTERVIEW:
		case DEM_CROUCH:
		case DEM_CHECKAUTOSAVE:
		case DEM_DOAUTOSAVE:
		case DEM_CONVCLOSE:
		case DEM_CONVNULL:
		case DEM_REVERTCAMERA:
		case DEM_FINISHGAME:
		case DEM_ENDSCREENJOB:
		case DEM_READIED:
		case DEM_USEFLECHETTE:
			skip = 0u;
			break;

		default:
			return false;
	}

	if (skip > stream.Size())
		return false;
	AdvanceStream(stream, skip);
	return true;
}

static bool Net_TrySkipUserCmdMessage(TArrayView<uint8_t>& stream, bool allowImplicitEmptyAtEnd)
{
	bool sawEventCommand = false;
	while (stream.Size() > 0u)
	{
		const uint8_t type = stream[0];
		AdvanceStream(stream, 1u);
		if (type == DEM_USERCMD)
		{
			if (stream.Size() < 1u)
				return false;

			const uint8_t flags = stream[0];
			AdvanceStream(stream, 1u);

			if (flags & UCMDF_BUTTONS)
			{
				if (stream.Size() < 1u)
					return false;
				uint8_t in = stream[0];
				AdvanceStream(stream, 1u);
				if (in & MoreButtons)
				{
					if (stream.Size() < 1u)
						return false;
					in = stream[0];
					AdvanceStream(stream, 1u);
					if (in & MoreButtons)
					{
						if (stream.Size() < 1u)
							return false;
						in = stream[0];
						AdvanceStream(stream, 1u);
						if (in & MoreButtons)
						{
							if (stream.Size() < 1u)
								return false;
							AdvanceStream(stream, 1u);
						}
					}
				}
			}

			auto skipAxis = [&stream]() -> bool
			{
				if (stream.Size() < 2u)
					return false;
				AdvanceStream(stream, 2u);
				return true;
			};

			if ((flags & UCMDF_PITCH) && !skipAxis()) return false;
			if ((flags & UCMDF_YAW) && !skipAxis()) return false;
			if ((flags & UCMDF_FORWARDMOVE) && !skipAxis()) return false;
			if ((flags & UCMDF_SIDEMOVE) && !skipAxis()) return false;
			if ((flags & UCMDF_UPMOVE) && !skipAxis()) return false;
			if ((flags & UCMDF_ROLL) && !skipAxis()) return false;
			return true;
		}
		if (type == DEM_EMPTYUSERCMD)
			return true;

		if (!Net_TrySkipCommand(type, stream))
			return false;
		sawEventCommand = true;
	}

	// Some transitions can legally produce a command-only tic record
	// (events with no movement payload). Only allow this when the caller
	// knows this record is the final one in the packet, otherwise record
	// boundaries become ambiguous and can consume the next tic by mistake.
	return allowImplicitEmptyAtEnd && sawEventCommand;
}

static bool Net_TrySkipRemainingUserCmdRecords(const uint8_t* data, size_t dataSize,
	uint8_t commandTics, uint64_t commandOffsetsSeen, uint8_t remainingRecords)
{
	size_t cursor = 0u;
	uint64_t offsetsSeen = commandOffsetsSeen;
	for (uint8_t record = 0u; record < remainingRecords; ++record)
	{
		if (cursor >= dataSize)
			return false;

		const uint8_t commandOffset = data[cursor++];
		if (commandOffset >= commandTics || commandOffset >= 64u)
			return false;

		const uint64_t commandMask = uint64_t(1u) << commandOffset;
		if ((offsetsSeen & commandMask) != 0u)
			return false;
		offsetsSeen |= commandMask;

		size_t recordBytes = 0u;
		const bool finalRecord = record + 1u == remainingRecords;
		if (!Net_TrySkipUserCmdMessageWithBoundary(&data[cursor], dataSize - cursor,
			finalRecord, commandTics, offsetsSeen, remainingRecords - record - 1u, recordBytes))
		{
			return false;
		}
		if (recordBytes > dataSize - cursor)
			return false;
		cursor += recordBytes;
	}
	return cursor == dataSize;
}

static bool Net_TrySkipUserCmdMessageWithBoundary(const uint8_t* data, size_t dataSize,
	bool allowImplicitEmptyAtEnd, uint8_t commandTics, uint64_t commandOffsetsSeen,
	uint8_t remainingRecords, size_t& recordBytes)
{
	recordBytes = 0u;
	size_t cursor = 0u;
	bool sawEventCommand = false;
	while (cursor < dataSize)
	{
		const size_t commandStart = cursor;
		const uint8_t type = data[cursor++];
		if (type == DEM_USERCMD || type == DEM_EMPTYUSERCMD)
		{
			TArrayView<uint8_t> skipper = TArrayView(const_cast<uint8_t*>(&data[commandStart]), dataSize - commandStart);
			if (!Net_TrySkipUserCmdMessage(skipper, false))
				return false;
			recordBytes = size_t(skipper.Data() - data);
			return true;
		}

		TArrayView<uint8_t> eventStream = TArrayView(const_cast<uint8_t*>(&data[cursor]), dataSize - cursor);
		if (!Net_TrySkipCommand(type, eventStream))
			return false;
		cursor = size_t(eventStream.Data() - data);
		sawEventCommand = true;

		if (remainingRecords > 0u && cursor < dataSize)
		{
			const uint8_t nextOffset = data[cursor];
			const bool nextByteIsUserCommand = nextOffset == DEM_USERCMD || nextOffset == DEM_EMPTYUSERCMD;
			const bool plausibleNextOffset = nextOffset < commandTics
				&& nextOffset < 64u
				&& (commandOffsetsSeen & (uint64_t(1u) << nextOffset)) == 0u
				&& !nextByteIsUserCommand;
			if (plausibleNextOffset
				&& Net_TrySkipRemainingUserCmdRecords(&data[cursor], dataSize - cursor,
					commandTics, commandOffsetsSeen, remainingRecords))
			{
				recordBytes = cursor;
				return true;
			}
		}
	}

	if (allowImplicitEmptyAtEnd && sawEventCommand)
	{
		recordBytes = dataSize;
		return true;
	}
	return false;
}

void Net_SkipCommand(int cmd, TArrayView<uint8_t>& stream)
{
	if (!Net_TrySkipCommand(cmd, stream))
	{
		// Keep progress monotonic in skip mode so malformed payloads cannot stall
		// packet scanning loops.
		if (stream.Size() > 0u)
			AdvanceStream(stream, 1u);
	}
}

// This was taken out of shared_hud, because UI code shouldn't do low level calculations that may change if the backing implementation changes.
int Net_GetLatency(int* localDelay, int* arbitratorDelay)
{
	const int gameDelayMs = (ClientTic - gametic) * 1000 / TICRATE;
	int severity = 0;
	if (gameDelayMs >= 160)
		severity = 3;
	else if (gameDelayMs >= 120)
		severity = 2;
	else if (gameDelayMs >= 80)
		severity = 1;

	*localDelay = gameDelayMs;
	*arbitratorDelay = consoleplayer >= 0 && consoleplayer < MAXPLAYERS ? ClientStates[consoleplayer].AverageLatency : 0;
	return severity;
}

//==========================================================================
//
//
//
//==========================================================================

EInvasionState Net_GetInvasionState()
{
	return InvasionState;
}

const char* Net_GetInvasionStateName()
{
	return Net_InvasionStateName(InvasionState);
}

int Net_GetInvasionStateTics()
{
	if (InvasionState == INVS_COUNTDOWN)
		return max(CutsceneCountdown, max(InvasionStateTics, 0));
	return max(InvasionStateTics, 0);
}

int Net_GetClassicInvasionState()
{
	// Skulltag/Zandronum ACS compiled IS_COUNTDOWN as 5. Keep this mapping
	// separate from HCDE's compact internal enum so legacy invasion maps can
	// keep using GetInvasionState() bytecode directly.
	switch (InvasionState)
	{
	case INVS_COUNTDOWN:
		return 5;
	case INVS_SPAWNING:
	case INVS_CLEANUP:
		return 6;
	case INVS_INTERMISSION:
		return 7;
	case INVS_VICTORY:
		return 8;
	case INVS_FAILURE:
		return 9;
	case INVS_WAITING:
	case INVS_DISABLED:
	default:
		return 0;
	}
}

int Net_GetInvasionWave()
{
	return max(InvasionWaveDirector.Wave, 0);
}

int Net_GetInvasionMaxWaves()
{
	return max(InvasionWaveDirector.MaxWaves, 0);
}

int Net_GetInvasionWaveBudget()
{
	return max(InvasionWaveDirector.WaveBudget, 0);
}

int Net_GetInvasionWaveSpawned()
{
	return max(InvasionWaveDirector.WaveSpawned, 0);
}

int Net_GetInvasionWaveCleared()
{
	return max(InvasionWaveDirector.WaveCleared, 0);
}

int Net_GetInvasionActiveMonsterCount()
{
	if (!Net_IsLocalInvasionAuthority())
		return max(InvasionReplicatedActiveMonsterCount, 0);
	return max(int(InvasionWaveDirector.ActiveMonsters.Size()), 0);
}

bool Net_IsInvasionBossWave()
{
	return (InvasionWaveDirector.WaveFlags & INV_WAVEF_BOSS) != 0u;
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, Net_GetInvasionWave, Net_GetInvasionWave)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(Net_GetInvasionWave());
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, Net_GetInvasionMaxWaves, Net_GetInvasionMaxWaves)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(Net_GetInvasionMaxWaves());
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, Net_GetInvasionActiveMonsterCount, Net_GetInvasionActiveMonsterCount)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(Net_GetInvasionActiveMonsterCount());
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, Net_GetInvasionWaveBudget, Net_GetInvasionWaveBudget)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(Net_GetInvasionWaveBudget());
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, Net_GetInvasionWaveSpawned, Net_GetInvasionWaveSpawned)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(Net_GetInvasionWaveSpawned());
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, Net_GetInvasionWaveCleared, Net_GetInvasionWaveCleared)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(Net_GetInvasionWaveCleared());
}

int Net_GetInvasionSpawnSpotCount()
{
	return max(InvasionSpawnDirectory.TotalSpotCount, 0);
}

int Net_GetInvasionActiveSpawnSpotCount()
{
	return max(InvasionSpawnDirectory.ActiveSpotCount, 0);
}

int Net_GetInvasionSpawnPlanBudget()
{
	return max(InvasionSpawnDirectory.TotalSpawnBudget, 0);
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, Net_GetInvasionSpawnSpotCount, Net_GetInvasionSpawnSpotCount)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(Net_GetInvasionSpawnSpotCount());
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, Net_GetInvasionActiveSpawnSpotCount, Net_GetInvasionActiveSpawnSpotCount)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(Net_GetInvasionActiveSpawnSpotCount());
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, Net_GetInvasionSpawnPlanBudget, Net_GetInvasionSpawnPlanBudget)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(Net_GetInvasionSpawnPlanBudget());
}

int Net_GetInvasionSpawnActiveTag()
{
	return InvasionSpawnDirectory.ActiveTag;
}

bool Net_IsInvasionSpawnUsingFallback()
{
	return InvasionSpawnDirectory.UsingFallback;
}

int Net_GetInvasionSpawnFallbackSource()
{
	return int(InvasionSpawnDirectory.FallbackSource);
}

int Net_ControlInvasion(int action, const char* reason)
{
	if (!Net_IsLocalInvasionAuthority())
		return 0;
	if (!Net_IsInvasionModeEnabled())
		return 0;

	switch (action)
	{
	case INVCTL_NEXTWAVE:
		Net_StartInvasionWave(reason != nullptr ? reason : "api-nextwave");
		return 1;

	case INVCTL_VICTORY:
		Net_MarkInvasionVictory(reason != nullptr ? reason : "api-victory");
		return 1;

	case INVCTL_FAILURE:
		Net_MarkInvasionFailure(reason != nullptr ? reason : "api-failure");
		return 1;

	default:
		return 0;
	}
}

// Invasion scripting API exports.
static int InvasionGetState()
{
	return int(Net_GetInvasionState());
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetState, InvasionGetState)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetState());
}

static int InvasionGetStateTics()
{
	return Net_GetInvasionStateTics();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetStateTics, InvasionGetStateTics)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetStateTics());
}

static int InvasionGetWave()
{
	return Net_GetInvasionWave();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetWave, InvasionGetWave)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetWave());
}

static int InvasionGetMaxWaves()
{
	return Net_GetInvasionMaxWaves();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetMaxWaves, InvasionGetMaxWaves)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetMaxWaves());
}

static int InvasionGetWaveBudget()
{
	return Net_GetInvasionWaveBudget();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetWaveBudget, InvasionGetWaveBudget)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetWaveBudget());
}

static int InvasionGetWaveSpawned()
{
	return Net_GetInvasionWaveSpawned();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetWaveSpawned, InvasionGetWaveSpawned)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetWaveSpawned());
}

static int InvasionGetWaveCleared()
{
	return Net_GetInvasionWaveCleared();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetWaveCleared, InvasionGetWaveCleared)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetWaveCleared());
}

static int InvasionGetActiveMonsterCount()
{
	return Net_GetInvasionActiveMonsterCount();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetActiveMonsterCount, InvasionGetActiveMonsterCount)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetActiveMonsterCount());
}

static int InvasionIsBossWave()
{
	return Net_IsInvasionBossWave() ? 1 : 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionIsBossWave, InvasionIsBossWave)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_BOOL(InvasionIsBossWave());
}

static int InvasionGetSpawnSpotCount()
{
	return Net_GetInvasionSpawnSpotCount();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetSpawnSpotCount, InvasionGetSpawnSpotCount)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetSpawnSpotCount());
}

static int InvasionGetActiveSpawnSpotCount()
{
	return Net_GetInvasionActiveSpawnSpotCount();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetActiveSpawnSpotCount, InvasionGetActiveSpawnSpotCount)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetActiveSpawnSpotCount());
}

static int InvasionGetSpawnPlanBudget()
{
	return Net_GetInvasionSpawnPlanBudget();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetSpawnPlanBudget, InvasionGetSpawnPlanBudget)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetSpawnPlanBudget());
}

static int InvasionGetSpawnActiveTag()
{
	return Net_GetInvasionSpawnActiveTag();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetSpawnActiveTag, InvasionGetSpawnActiveTag)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetSpawnActiveTag());
}

static int InvasionIsSpawnUsingFallback()
{
	return Net_IsInvasionSpawnUsingFallback() ? 1 : 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionIsSpawnUsingFallback, InvasionIsSpawnUsingFallback)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_BOOL(InvasionIsSpawnUsingFallback());
}

static int InvasionGetSpawnFallbackSource()
{
	return Net_GetInvasionSpawnFallbackSource();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionGetSpawnFallbackSource, InvasionGetSpawnFallbackSource)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(InvasionGetSpawnFallbackSource());
}

static int InvasionControl(int action)
{
	return Net_ControlInvasion(action, "zscript-api");
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, InvasionControl, InvasionControl)
{
	PARAM_PROLOGUE;
	PARAM_INT(action);
	ACTION_RETURN_BOOL(InvasionControl(action) != 0);
}

// Intermission lobby info
static int IsPlayerReady(int player)
{
	return Net_IsPlayerReady(player);
}

DEFINE_ACTION_FUNCTION_NATIVE(_ScreenJobRunner, IsPlayerReady, IsPlayerReady)
{
	PARAM_PROLOGUE;
	PARAM_INT(player);
	ACTION_RETURN_BOOL(IsPlayerReady(player));
}

static int IsPlayerReadyParticipant(int player)
{
	return Net_IsCutsceneReadyParticipant(player) && players[player].Bot == nullptr;
}

DEFINE_ACTION_FUNCTION_NATIVE(_ScreenJobRunner, IsPlayerReadyParticipant, IsPlayerReadyParticipant)
{
	PARAM_PROLOGUE;
	PARAM_INT(player);
	ACTION_RETURN_BOOL(IsPlayerReadyParticipant(player));
}

static void ReadyPlayer()
{
	if (netgame && !demoplayback)
	{
		// Apply ready locally so skip works even when the tic gate is stalled and
		// DEM_READIED cannot round-trip through the authority snapshot yet.
		if (consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
			Net_PlayerReadiedUp(consoleplayer);
		Net_WriteInt8(DEM_READIED);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_ScreenJobRunner, ReadyPlayer, ReadyPlayer)
{
	PARAM_PROLOGUE;
	ReadyPlayer();
	return 0;
}

static void ResetReadyTimer()
{
	Net_StartCutscene();
}

DEFINE_ACTION_FUNCTION_NATIVE(_ScreenJobRunner, ResetReadyTimer, ResetReadyTimer)
{
	PARAM_PROLOGUE;
	ResetReadyTimer();
	return 0;
}

static int GetReadyTimer()
{
	return CutsceneCountdown;
}

DEFINE_ACTION_FUNCTION_NATIVE(_ScreenJobRunner, GetReadyTimer, GetReadyTimer)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(GetReadyTimer());
}

CCMD(invasion_victory)
{
	if (!Net_ControlInvasion(INVCTL_VICTORY, "console-command"))
		Printf("Invasion control rejected (authority + sv_gametype 4 required)\n");
}

CCMD(invasion_failure)
{
	if (!Net_ControlInvasion(INVCTL_FAILURE, "console-command"))
		Printf("Invasion control rejected (authority + sv_gametype 4 required)\n");
}

CCMD(invasion_nextwave)
{
	if (!Net_ControlInvasion(INVCTL_NEXTWAVE, "console-command"))
		Printf("Invasion control rejected (authority + sv_gametype 4 required)\n");
}

// Apply the explicit invasion baseline without opening the GUI.
// This mirrors the dedicated preset in i_mainwindow.cpp for headless servers.
static void Net_ApplyInvasionPreset(const char* traceReason)
{
	if (!I_IsLocalHCDEServiceAuthority())
	{
		Printf("Invasion preset rejected (authority required)\n");
		return;
	}

	Net_TraceSetSvGametype(4, traceReason != nullptr ? traceReason : "invasion-preset");
	sv_invasioncountdowntime = 30.0f;
	sv_invasionspawntime = 8.0f;
	sv_invasioncleanuptime = 4.0f;
	sv_invasionintermissiontime = 6.0f;
	sv_invasionresulttime = 8.0f;
	sv_invasionexitonvictory = true;
	sv_invasiondebug = 0;
	sv_invasionwaves = 8;
	wavelimit_compat = 0;
	duellimit_compat = 0;
	sv_usemapsettingswavelimit = true;
	sv_invasionbasebudget = 24;
	sv_invasionbudgetstep = 8;
	sv_invasionperplayer = 6;
	sv_invasionspawninterval = 0.35f;
	sv_invasionspawnburst = 3;
	sv_invasionmaxactive = 0;
	sv_invasionbosswaveevery = 5;
	sv_invasionbossbonus = 20;
	sv_invasionspotusemaptags = false;
	sv_invasionspotfallback = true;
	dmflags = int(dmflags) & ~DF_FAST_MONSTERS;

	Printf("Applied Invasion preset (sv_gametype=4, fastmonsters=0, waves=%d explicit-wavelimit=%d use-map-settings=%d exit-victory=%d budget=%d+%d/wave, per-player=%d)\n",
		int(sv_invasionwaves), int(wavelimit_compat), sv_usemapsettingswavelimit ? 1 : 0, sv_invasionexitonvictory ? 1 : 0,
		int(sv_invasionbasebudget), int(sv_invasionbudgetstep), int(sv_invasionperplayer));
}

CCMD(invasion_preset)
{
	Net_ApplyInvasionPreset("invasion-preset");
}

// Compatibility alias for older scripts/admin notes. Invasion is an explicit
// mode preset, not HCDE's default or "standard" server mode.
CCMD(invasion_standard)
{
	Net_ApplyInvasionPreset("invasion-standard-alias");
}

// Print a concise runtime + configuration summary for remote admin/debug use.
CCMD(invasion_status)
{
	Printf("Invasion status: state=%s (%d tics) wave=%d/%d budget=%d spawned=%d cleared=%d active=%d cap=%d speedscale=%.2f boss=%d fastmonsters=%d\n",
		Net_GetInvasionStateName(),
		Net_GetInvasionStateTics(),
		Net_GetInvasionWave(),
		Net_GetInvasionMaxWaves(),
		Net_GetInvasionWaveBudget(),
		Net_GetInvasionWaveSpawned(),
		Net_GetInvasionWaveCleared(),
		Net_GetInvasionActiveMonsterCount(),
		Net_GetInvasionActiveMonsterCap(),
		Net_GetInvasionSkillMonsterSpeedScale(),
		Net_IsInvasionBossWave() ? 1 : 0,
		(dmflags & DF_FAST_MONSTERS) != 0 ? 1 : 0);
	const int mapWaveLimit = (primaryLevel != nullptr && primaryLevel->info != nullptr)
		? clamp<int>(primaryLevel->info->InvasionWaveLimit, 0, HCDEInvasionMaxWavesLimit)
		: 0;
	const int mapDuelLimit = (primaryLevel != nullptr && primaryLevel->info != nullptr)
		? clamp<int>(primaryLevel->info->DuelLimit, 0, 255)
		: 0;
	const bool canOverrideCmpgninfLimits = HCDE_CanOverrideCmpgninfLimits();
	Printf("Invasion settings: count=%.2f spawn=%.2f cleanup=%.2f inter=%.2f result=%.2f exit-victory=%d debug=%d interval=%.2f burst=%d maxactive=%d waves=%d explicit-wavelimit=%d use-map-wavelimit=%d map-wavelimit=%d resolved-wavelimit=%d map-duellimit=%d legacy-duellimit=%d allow-offline-overrides=%d base=%d step=%d perplayer=%d boss-every=%d boss-bonus=%d map-tags=%d fallback=%d\n",
		double(sv_invasioncountdowntime),
		double(sv_invasionspawntime),
		double(sv_invasioncleanuptime),
		double(sv_invasionintermissiontime),
		double(sv_invasionresulttime),
		sv_invasionexitonvictory ? 1 : 0,
		int(sv_invasiondebug),
		double(sv_invasionspawninterval),
		int(sv_invasionspawnburst),
		int(sv_invasionmaxactive),
		int(sv_invasionwaves),
		int(wavelimit_compat),
		sv_usemapsettingswavelimit ? 1 : 0,
		mapWaveLimit,
		Net_ResolveInvasionMaxWaves(),
		mapDuelLimit,
		clamp<int>(duellimit_compat, 0, 255),
		canOverrideCmpgninfLimits ? 1 : 0,
		int(sv_invasionbasebudget),
		int(sv_invasionbudgetstep),
		int(sv_invasionperplayer),
		int(sv_invasionbosswaveevery),
		int(sv_invasionbossbonus),
		sv_invasionspotusemaptags ? 1 : 0,
		sv_invasionspotfallback ? 1 : 0);
}

CCMD(invasion_spots)
{
	Printf("Invasion spots: total=%d active=%d budget=%d tag=%d fallback=%d source=%s wave=%d spawned=%d\n",
		InvasionSpawnDirectory.TotalSpotCount,
		InvasionSpawnDirectory.ActiveSpotCount,
		InvasionSpawnDirectory.TotalSpawnBudget,
		InvasionSpawnDirectory.ActiveTag,
		InvasionSpawnDirectory.UsingFallback ? 1 : 0,
		Net_InvasionSpawnSourceName(InvasionSpawnDirectory.FallbackSource),
		InvasionWaveDirector.Wave,
		InvasionSpawnDirectory.SpawnedThisWave);

	for (int i = 0; i < int(InvasionSpawnDirectory.Spots.Size()); ++i)
	{
		const auto& spot = InvasionSpawnDirectory.Spots[i];
		Printf("#%d src=%s tid=%d pos=(%.1f, %.1f, %.1f) first=%d start=%d max=%d delay=%d round=%d planned=%d spawned=%d ready=%d\n",
			i,
			Net_InvasionSpawnSourceName(spot.Source),
			spot.Tag,
			spot.Pos.X,
			spot.Pos.Y,
			spot.Pos.Z,
			spot.FirstWave,
			spot.StartSpawnNumber,
			spot.MaxSpawnNumber,
			spot.SpawnDelayTics,
			spot.RoundSpawnDelayTics,
			spot.PlannedSpawnCount,
			spot.SpawnedCount,
			gametic >= spot.NextSpawnGameTic ? 1 : 0);
	}
}

// [RH] List "ping" times
CCMD(pings)
{
	if (!netgame)
	{
		Printf("This command can only be used when playing in a net game\n");
		return;
	}

	if (NetworkClients.Size() <= 1)
		return;

	for (auto client : NetworkClients)
	{
		if (!I_IsHCDEServiceAuthoritySlot(client))
			Printf("%ums %s [%d]\n", ClientStates[client].AverageLatency, players[client].userinfo.GetName(), client);
	}
}

CCMD(kick)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: kick <client numbers>\nRemove these clients from the game\n");
		return;
	}

	if (!netgame)
	{
		Printf("This command can only be used when playing in a net game\n");
		return;
	}

	// Do not give settings controllers access to this. That remains reserved
	// for the current service authority.
	if (!I_IsLocalHCDEServiceAuthority())
	{
		Printf("This command is only accessible to the authority\n");
		return;
	}

	TArray<int> cNums = {};
	for (size_t i = 1u; i < argv.argc(); ++i)
	{
		int cNum = -1;
		if (!C_IsValidInt(argv[i], cNum) || cNum < 0 || cNum >= MAXPLAYERS)
			Printf("Bad client number %s\n", argv[i]);
		else if (cNum != consoleplayer && cNums.Find(cNum) >= cNums.Size())
			cNums.Push(cNum);
	}

	for (auto cNum : cNums)
	{
		if (!NetworkClients.InGame(cNum))
		{
			Printf("Client %d is not in game\n", cNum);
		}
		else
		{
			Net_WriteInt8(DEM_KICK);
			Net_WriteInt8(cNum);
		}
	}
}

CCMD(mute)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: mute <player numbers>\nDisable messages from these players\n");
		return;
	}

	if (!multiplayer)
	{
		Printf("This command can only be used when playing in multiplayer\n");
		return;
	}

	TArray<int> pNums = {};
	for (size_t i = 1u; i < argv.argc(); ++i)
	{
		int pNum = -1;
		if (!C_IsValidInt(argv[i], pNum) || pNum < 0 || pNum >= MAXPLAYERS)
			Printf("Bad player number %s\n", argv[i]);
		else if (pNum != consoleplayer && pNums.Find(pNum) >= pNums.Size())
			pNums.Push(pNum);
	}

	for (auto pNum : pNums)
	{
		if (!playeringame[pNum])
		{
			Printf("Player %d is not in game\n", pNum);
		}
		else
		{
			MutedClients |= (uint64_t)1u << pNum;
			Printf("Muted player %s [%d]\n", players[pNum].userinfo.GetName(), pNum);
		}
	}
}

CCMD(muteall)
{
	if (!multiplayer)
	{
		Printf("This command can only be used when playing in multiplayer\n");
		return;
	}

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && i != consoleplayer)
			MutedClients |= (uint64_t)1u << i;
	}
}

CCMD(listmuted)
{
	if (!multiplayer)
	{
		Printf("This command can only be used when playing in multiplayer\n");
		return;
	}

	bool found = false;
	for (unsigned int i = 0; i < MAXPLAYERS; ++i)
	{
		if (MutedClients & ((uint64_t)1u << i))
		{
			found = true;
			Printf("%d. %s\n", i, players[i].userinfo.GetName());
		}
	}

	if (!found)
		Printf("No one currently muted\n");
}

CCMD(unmute)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: unmute <player numbers>\nAllow messages from these players again\n");
		return;
	}

	if (!multiplayer)
	{
		Printf("This command can only be used when playing in multiplayer\n");
		return;
	}

	TArray<int> pNums = {};
	for (size_t i = 1u; i < argv.argc(); ++i)
	{
		int pNum = -1;
		if (!C_IsValidInt(argv[i], pNum) || pNum < 0 || pNum >= MAXPLAYERS)
			Printf("Bad player number %s\n", argv[i]);
		else if (pNum != consoleplayer && pNums.Find(pNum) >= pNums.Size())
			pNums.Push(pNum);
	}

	for (auto pNum : pNums)
	{
		if (!playeringame[pNum])
		{
			Printf("Player %d is not in game\n", pNum);
		}
		else
		{
			MutedClients &= ~((uint64_t)1u << pNum);
			Printf("Unmuted player %s [%d]\n", players[pNum].userinfo.GetName(), pNum);
		}
	}
}

CCMD(unmuteall)
{
	if (!multiplayer)
	{
		Printf("This command can only be used when playing in multiplayer\n");
		return;
	}

	MutedClients = 0u;
}

//==========================================================================
//
// Net_ChangeSettingsControllers
//
// Implement players who have the ability to change settings in a network
// game.
//
//==========================================================================

static void Net_ChangeSettingsControllers(const TArray<int>& cNums, bool add)
{
	if (!netgame)
	{
		Printf("This command can only be used when playing in a net game\n");
		return;
	}

	if (!I_IsLocalHCDEServiceAuthority())
	{
		Printf("This command is only accessible to the authority\n");
		return;
	}

	for (auto cNum : cNums)
	{
		if (I_IsHCDEServiceAuthoritySlot(cNum))
		{
			Printf("The authority cannot change its own settings controller status\n");
		}
		else if (!NetworkClients.InGame(cNum))
		{
			Printf("Client %d is not in game\n", cNum);
		}
		else if (players[cNum].settings_controller && add)
		{
			Printf("Client %d is already a settings controller\n", cNum);
		}
		else if (!players[cNum].settings_controller && !add)
		{
			Printf("Client %d is already not a settings controller\n", cNum);
		}
		else
		{
			Net_WriteInt8(add ? DEM_ADDCONTROLLER : DEM_DELCONTROLLER);
			Net_WriteInt8(cNum);
		}
	}
}

//==========================================================================
//
// CCMD addsettingscontrollers
//
//==========================================================================

CCMD(addsettingscontrollers)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: addsettingscontrollers <client numbers>\nAllow these clients to control game settings\n");
		return;
	}

	TArray<int> cNums = {};
	for (size_t i = 1u; i < argv.argc(); ++i)
	{
		int cNum = -1;
		if (!C_IsValidInt(argv[i], cNum) || cNum < 0 || cNum >= MAXPLAYERS)
			Printf("Bad client number %s\n", argv[i]);
		else if (!I_IsHCDEServiceAuthoritySlot(cNum) && cNums.Find(cNum) >= cNums.Size())
			cNums.Push(cNum);
	}

	Net_ChangeSettingsControllers(cNums, true);
}

//==========================================================================
//
// CCMD removesettingscontrollers
//
//==========================================================================

CCMD(removesettingscontrollers)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: removesettingscontrollers <client numbers>\nRemove the ability for these clients to control game settings\n");
		return;
	}

	TArray<int> cNums = {};
	for (size_t i = 1u; i < argv.argc(); ++i)
	{
		int cNum = -1;
		if (!C_IsValidInt(argv[i], cNum) || cNum < 0 || cNum >= MAXPLAYERS)
			Printf("Bad player number %s\n", argv[i]);
		else if (!I_IsHCDEServiceAuthoritySlot(cNum) && cNums.Find(cNum) >= cNums.Size())
			cNums.Push(cNum);
	}

	Net_ChangeSettingsControllers(cNums, false);
}

//==========================================================================
//
// CCMD removeallsettingscontrollers
//
//==========================================================================

CCMD(removeallsettingscontrollers)
{
	TArray<int> cNums = {};
	for (auto client : NetworkClients)
	{
		if (!I_IsHCDEServiceAuthoritySlot(client) && players[client].settings_controller)
			cNums.Push(client);
	}

	Net_ChangeSettingsControllers(cNums, false);
}

//==========================================================================
//
// CCMD listsettingscontrollers
//
//==========================================================================

CCMD(listsettingscontrollers)
{
	if (!netgame)
	{
		Printf("This command can only be used when playing in a net game\n");
		return;
	}

	TArray<int> cNums = {};
	for (auto client : NetworkClients)
	{
		if (!I_IsHCDEServiceAuthoritySlot(client) && players[client].settings_controller)
			cNums.Push(client);
	}

	if (!cNums.Size())
	{
		Printf("No other settings controllers\n");
		return;
	}

	Printf("The following players can change the game settings:\n");
	for (auto cNum : cNums)
	{
		Printf("%d. %s", cNum, players[cNum].userinfo.GetName());
		if (cNum == consoleplayer)
			Printf(" [*]");
		Printf("\n");
	}
}
