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
#include "p_spec.h"
#include "p_trace.h"
#include "r_utility.h"
#include "s_music.h"
#include "savegamemanager.h"
#include "sbar.h"
#include "screenjob.h"
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
constexpr size_t HCDEInvasionSpawnEventsMagicOffset = 0u;
constexpr size_t HCDEInvasionSpawnEventsVersionOffset = 4u;
constexpr size_t HCDEInvasionSpawnEventsFlagsOffset = 5u;
constexpr size_t HCDEInvasionSpawnEventsCountOffset = 6u;
constexpr size_t HCDEInvasionSpawnEventsReservedOffset = 7u;
constexpr size_t HCDEInvasionSpawnEventsHeaderSize = 8u;
constexpr uint8_t HCDEInvasionSpawnEventsProtocolVersion = 1u;
constexpr uint8_t HCDEInvasionSpawnEventsMagic[4] = { 'H', 'C', 'I', 'S' };
constexpr size_t HCDEInvasionSpawnEventReplayLimit = 64u;
constexpr size_t HCDEInvasionSpawnEventHistoryLimit = 128u;
constexpr size_t HCDEInvasionSpawnEventPacketLimit = 4u;
constexpr size_t HCDEInvasionSpawnEventActorDeltaReserveBytes = 900u;
constexpr size_t HCDEInvasionActorDeltasMagicOffset = 0u;
constexpr size_t HCDEInvasionActorDeltasVersionOffset = 4u;
constexpr size_t HCDEInvasionActorDeltasFlagsOffset = 5u;
constexpr size_t HCDEInvasionActorDeltasCountOffset = 6u;
constexpr size_t HCDEInvasionActorDeltasReservedOffset = 7u;
constexpr size_t HCDEInvasionActorDeltasHeaderSize = 8u;
constexpr size_t HCDEInvasionActorDeltaV1RecordSize = 64u;
constexpr uint8_t HCDEInvasionActorDeltasProtocolVersion = 5u;
constexpr uint8_t HCDEInvasionActorDeltasMagic[4] = { 'H', 'C', 'I', 'A' };
constexpr uint8_t HCDEInvasionActorDeltasFlagComplete = 1u << 0;
constexpr uint8_t HCDEInvasionActorDeltaFlagLive = 1u << 0;
constexpr uint8_t HCDEInvasionActorActionNone = 0u;
constexpr uint8_t HCDEInvasionActorActionSpawn = 1u;
constexpr uint8_t HCDEInvasionActorActionSee = 2u;
constexpr uint8_t HCDEInvasionActorActionMelee = 3u;
constexpr uint8_t HCDEInvasionActorActionMissile = 4u;
constexpr uint8_t HCDEInvasionActorActionPain = 5u;
constexpr uint8_t HCDEInvasionActorActionMax = HCDEInvasionActorActionPain;
constexpr int HCDEInvasionActorActionHoldTics = TICRATE / 2;
constexpr uint16_t HCDEInvasionActorDeltaFieldClass = 1u << 0;
constexpr uint16_t HCDEInvasionActorDeltaFieldFlags = 1u << 1;
constexpr uint16_t HCDEInvasionActorDeltaFieldAction = 1u << 2;
constexpr uint16_t HCDEInvasionActorDeltaFieldHealth = 1u << 3;
constexpr uint16_t HCDEInvasionActorDeltaFieldPos = 1u << 4;
constexpr uint16_t HCDEInvasionActorDeltaFieldVel = 1u << 5;
constexpr uint16_t HCDEInvasionActorDeltaFieldAngles = 1u << 6;
constexpr uint16_t HCDEInvasionActorDeltaFieldAll =
	HCDEInvasionActorDeltaFieldClass
	| HCDEInvasionActorDeltaFieldFlags
	| HCDEInvasionActorDeltaFieldAction
	| HCDEInvasionActorDeltaFieldHealth
	| HCDEInvasionActorDeltaFieldPos
	| HCDEInvasionActorDeltaFieldVel
	| HCDEInvasionActorDeltaFieldAngles;
constexpr size_t HCDEInvasionSnapshotPayloadBudgetBytes = 1200u;
constexpr double HCDEInvasionMirrorVisualFallbackStepPerTic = 8.0;
constexpr double HCDEInvasionMirrorVisualSpeedMultiplier = 1.10;
constexpr double HCDEInvasionMirrorVisualMaxStepPerTic = 12.0;
constexpr double HCDEInvasionMirrorVisualSnapDistance = 384.0;
constexpr int HCDEInvasionProjectileMirrorMaxAgeTics = TICRATE * 3;
constexpr double HCDEServerBaselineRepairDistance = 128.0;
constexpr double HCDEServerReconcileDistance = 20.0;
constexpr double HCDEServerReconcileHardDistance = 384.0;

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
	uint32_t UnsupportedReceived = 0u;
	uint32_t AuthorityRejected = 0u;
	uint32_t WorldDeltaReceived = 0u;
	uint32_t BaselineRepairs = 0u;
	uint32_t BaselineLocalDrift = 0u;
	uint32_t Reconciliations = 0u;
	uint32_t HardReconciliations = 0u;

	void Clear()
	{
		*this = {};
	}
};

static uint8_t	CurrentRoomID = 0u;	// Ignore commands not from this room (useful when transitioning levels).
static int		LastGameUpdate = 0;		// Track the last time the game actually ran the world.
static uint64_t	MutedClients = 0u;		// Ignore messages from these clients.
static_assert(MAXPLAYERS <= 64, "MAXPLAYERS must remain <=64 while using fixed-sized late-join bitmask state.");
static uint64_t LateJoinSyncPending = 0u; // Clients admitted during an active match but not yet live-synced.
static int LateJoinSyncTargetSequence[MAXPLAYERS] = {};
static int LateJoinSyncTargetConsistency[MAXPLAYERS] = {};
static int LateJoinSyncStartTic[MAXPLAYERS] = {};
static int ConsistencyGraceUntilTic[MAXPLAYERS] = {};
static uint64_t	LastHCDELiveControlMS = 0u;
static uint64_t LastHCDELiveSequenceRejectReportMS = 0u;
static uint64_t LastHCDELiveSnapshotRejectReportMS = 0u;
static uint64_t LastHCDELiveTicGateReportMS = 0u;
static FHCDELivePeerState HCDELivePeers[MAXPLAYERS] = {};

struct FHCDEPendingLocalHealthRepair
{
	bool Valid = false;
	uint32_t ServerTic = 0u;
	int Health = 0;
	bool OnGround = false;
};

static FHCDEPendingLocalHealthRepair PendingLocalHealthRepair = {};

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

struct FInvasionReplicatedSpawnEvent
{
	uint32_t Id = 0u;
	int Tic = 0;
	int Wave = 0;
	FString ClassName;
	DVector3 Pos = {};
	DAngle Yaw = nullAngle;
	int Health = 0;
};

struct FInvasionReplicatedActorRef
{
	struct FSentClientState
	{
		bool Valid = false;
		uint8_t ActorFlags = 0u;
		uint8_t ActionState = HCDEInvasionActorActionNone;
		int Health = 0;
		FString ClassName;
		DVector3 Pos = {};
		DVector3 Vel = {};
		uint32_t Yaw = 0u;
		uint32_t Pitch = 0u;
	};

	uint32_t Id = 0u;
	TObjPtr<AActor*> Actor = MakeObjPtr<AActor*>(nullptr);
	bool DeathDeltaSent = false;
	bool IsProjectile = false;
	bool ForceDeathDelta = false;
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
	uint8_t VisualActionState = HCDEInvasionActorActionNone;
	int VisualActionTic = 0;
	int LastFullDeltaTic[MAXPLAYERS] = {};
	FSentClientState LastSent[MAXPLAYERS] = {};
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
static TArray<FInvasionReplicatedSpawnEvent> InvasionRecentSpawnEvents = {};
static TArray<FInvasionReplicatedSpawnEvent> InvasionPendingSpawnEvents = {};
static TArray<FInvasionPendingMirrorSpawn> InvasionPendingMirrorSpawns = {};
static TArray<FInvasionReplicatedActorRef> InvasionReplicatedActors = {};
static uint32_t InvasionNextSpawnEventId = 1u;
static uint32_t InvasionLastAppliedSpawnEventId = 0u;
static size_t InvasionActorDeltaSendCursor = 0u;
static int InvasionMirrorNextVisualDiagnosticTic = 0;
static int InvasionMirrorVisualTickBudget = 0;

static void Net_RebuildInvasionSpawnSpots(int wave);
static void Net_PrepareInvasionMirrorFromSnapshot(EInvasionState previousState, int previousWave);
static FInvasionReplicatedActorRef* Net_FindInvasionReplicatedActor(uint32_t id);
static FInvasionReplicatedActorRef* Net_FindInvasionReplicatedActorByActor(const AActor* actor);
static void Net_RegisterInvasionReplicatedActor(uint32_t id, AActor* actor);
static void Net_DrainPendingInvasionSpawnEvents();
static void Net_DrainPendingInvasionMirrorSpawns();
static AActor* Net_SelectInvasionCombatTarget(AActor* actor);
static int Net_GetInvasionSkillWakeDelayTics();
static void Net_AwakenInvasionMonster(AActor* actor);
static bool Net_IsInvasionReplicatedProjectile(const AActor* actor);
static bool Net_IsInvasionActorCorpseLike(const AActor* actor);
static void Net_SetInvasionMirrorVisualOnly(uint32_t id, AActor* actor);
static void Net_TickInvasionMirrorVisualActors();
static void Net_LogInvasionMirrorVisualDiagnostic();

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
			msg.Format("Prepare for invasion: %d", seconds);
			Net_ShowInvasionStatusMessage(msg.GetChars());
			InvasionAnnouncementLastCountdownSecond = seconds;
		}
	}
}

static int  LevelStartDebug = 0;
static int	LevelStartDelay = 0; // While this is > 0, don't start generating packets yet.
static ELevelStartStatus LevelStartStatus = LST_READY; // Listen for when to actually start making tics.
static uint64_t	LevelStartAck = 0u; // Used by the host to determine if everyone has loaded in.

static int FullLatencyCycle = MAXSENDTICS * 3;	// Give ~3 seconds to gather latency info about clients on boot up.
static int LastLatencyUpdate = 0;				// Update average latency every ~1 second.

static ELagType	LagState = LAG_NONE;	// What kind of lag the game is currently getting.
static int 	EnterTic = 0;
static int	LastEnterTic = 0;
static bool bCommandsReset = false;		// If true, commands were recently cleared. Don't generate any more tics.
static int	AuthorityWaitGraceUntil = 0;
static int	LastTicGateStallTrace = 0;

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
	Printf(PRINT_HIGH, "HCDE invasion state: %s -> %s tics=%d reason=%s wave=%d/%d spawned=%d/%d cleared=%d active=%d participants=%d alive=%d gametic=%d map=%s\n",
		Net_InvasionStateName(prevState), Net_InvasionStateName(InvasionState), InvasionStateTics,
		reason != nullptr ? reason : "none",
		InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves,
		InvasionWaveDirector.WaveSpawned, InvasionWaveDirector.WaveBudget,
		InvasionWaveDirector.WaveCleared, int(InvasionWaveDirector.ActiveMonsters.Size()),
		Net_CountInvasionParticipants(), Net_CountInvasionAliveParticipants(), gametic,
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
}

void D_ProcessEvents(void);
void G_BuildTiccmd(usercmd_t *cmd);
void D_DoAdvanceDemo(void);
void P_ClearPredictionData();

static void RunScript(TArrayView<uint8_t>& stream, AActor *pawn, int snum, int argn, int always);

extern	bool	 advancedemo;

static size_t GetNetBufferSize();
static bool Net_TrySkipCommand(int cmd, TArrayView<uint8_t>& stream);
static bool Net_TrySkipUserCmdMessage(TArrayView<uint8_t>& stream, bool allowImplicitEmptyAtEnd = false);
static bool Net_TrySkipUserCmdMessageWithBoundary(const uint8_t* data, size_t dataSize,
	bool allowImplicitEmptyAtEnd, uint8_t commandTics, uint64_t commandOffsetsSeen,
	uint8_t remainingRecords, size_t& recordBytes);
static void HSendPacket(int client, size_t size);

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

static void RequestHCDELiveReplay(int clientNum, const char* reason)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;

	auto& state = ClientStates[clientNum];
	auto& peer = HCDELivePeers[clientNum];
	state.Flags |= CF_MISSING_SEQ;
	DebugTrace::Warningf("net", "requested HCDE live replay client=%d reason=%s seq=%d ack=%d rx-snapshot=%u rx-command=%u room=%u",
		clientNum,
		reason != nullptr ? reason : "unknown",
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

	return playerCount <= 1u && playerNum == senderClient;
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
		return false;
	}

	payload = &envelope[HCDEGameplayHeaderSize];
	gameplayPayloadSize = payloadSize - HCDEGameplayHeaderSize;
	return true;
}

static bool RejectHCDEServerSnapshotBuild(int client, const char* reason, size_t legacySize, size_t cursor)
{
	DebugTrace::Markf("net", "refused legacy server snapshot for client=%d reason=%s size=%zu cursor=%zu",
		client, reason != nullptr ? reason : "unknown", legacySize, cursor);
	return false;
}

static bool HCDEAppendByte(uint8_t* output, size_t outputCapacity, size_t& cursor, uint8_t value)
{
	if (cursor >= outputCapacity)
	{
		DebugTrace::Markf("net", "HCDE append byte failed: cursor=%zu outputCapacity=%zu reason=cursor_ge_capacity", cursor, outputCapacity);
		return false;
	}
	output[cursor++] = value;
	return true;
}

static bool HCDEAppendBE16(uint8_t* output, size_t outputCapacity, size_t& cursor, uint16_t value)
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

static bool HCDEAppendBE32(uint8_t* output, size_t outputCapacity, size_t& cursor, uint32_t value)
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

static bool HCDEAppendBytes(uint8_t* output, size_t outputCapacity, size_t& cursor, const uint8_t* data, size_t size)
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

static bool HCDEReadByteField(const uint8_t* data, size_t dataSize, size_t& cursor, uint8_t& value)
{
	if (cursor >= dataSize)
	{
		DebugTrace::Markf("net", "HCDE read byte failed: cursor=%zu dataSize=%zu", cursor, dataSize);
		return false;
	}
	value = data[cursor++];
	return true;
}

static bool HCDEReadBE16Field(const uint8_t* data, size_t dataSize, size_t& cursor, uint16_t& value)
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

static bool HCDEReadBE32Field(const uint8_t* data, size_t dataSize, size_t& cursor, uint32_t& value)
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
	cursor += 2u;
	command.forwardmove = int16_t(HCDELiveReadBE16(&data[cursor]));
	cursor += 2u;
	command.sidemove = int16_t(HCDELiveReadBE16(&data[cursor]));
	cursor += 2u;
	command.upmove = int16_t(HCDELiveReadBE16(&data[cursor]));
	cursor += 2u;
	return true;
}

static bool HCDEAppendServerWorldDeltas(uint8_t* output, size_t outputCapacity, size_t& cursor, const uint8_t* playerNums, size_t playerCount)
{
	if (playerCount > MAXPLAYERS || playerCount > UINT8_MAX)
		return false;

	if (!HCDEAppendBytes(output, outputCapacity, cursor, HCDEServerWorldDeltaMagic, sizeof(HCDEServerWorldDeltaMagic))
		|| !HCDEAppendByte(output, outputCapacity, cursor, HCDEServerWorldDeltaProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max<int>(gametic, 0)))
		|| !HCDEAppendByte(output, outputCapacity, cursor, uint8_t(playerCount)))
	{
		return false;
	}

	for (size_t i = 0u; i < playerCount; ++i)
	{
		const uint8_t playerNum = playerNums[i];
		if (playerNum >= MAXPLAYERS)
			return false;

		const player_t& player = players[playerNum];
		const AActor* mo = player.mo;
		uint8_t flags = 0u;
		int health = player.health;
		DVector3 pos = {};
		DVector3 vel = {};
		uint32_t yaw = 0u;
		uint32_t pitch = 0u;
		if (mo != nullptr)
		{
			flags |= HCDEServerWorldDeltaPoseHasActor;
			if (player.playerstate == PST_LIVE)
				flags |= HCDEServerWorldDeltaPoseLive;
			if (player.onground)
				flags |= HCDEServerWorldDeltaPoseOnGround;
			health = mo->health;
			pos = mo->Pos();
			vel = mo->Vel;
			yaw = mo->Angles.Yaw.BAMs();
			pitch = mo->Angles.Pitch.BAMs();
		}

		if (!HCDEAppendByte(output, outputCapacity, cursor, playerNum)
			|| !HCDEAppendByte(output, outputCapacity, cursor, flags)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(health, INT16_MIN, INT16_MAX)))
			|| !HCDEAppendFloat(output, outputCapacity, cursor, pos.X)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, pos.Y)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, pos.Z)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, vel.X)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, vel.Y)
			|| !HCDEAppendFloat(output, outputCapacity, cursor, vel.Z)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, yaw)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, pitch))
		{
			return false;
		}
	}
	return true;
}

static void HCDEApplyLocalHealthFields(player_t& player, int serverHealth, bool onGround)
{
	AActor* mo = player.mo;
	if (mo == nullptr)
		return;

	const int previousHealth = max<int>(player.health, mo->health);
	mo->health = serverHealth;
	player.health = serverHealth;
	player.onground = onGround;
	if (serverHealth < previousHealth)
		player.damagecount = clamp<int>(player.damagecount + previousHealth - serverHealth, 0, 100);
}

static void HCDEQueuePredictedLocalHealthRepair(uint32_t serverTic, int serverHealth, bool onGround)
{
	if (PendingLocalHealthRepair.Valid && serverTic < PendingLocalHealthRepair.ServerTic)
		return;

	PendingLocalHealthRepair.Valid = true;
	PendingLocalHealthRepair.ServerTic = serverTic;
	PendingLocalHealthRepair.Health = serverHealth;
	PendingLocalHealthRepair.OnGround = onGround;

	if (consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
		HCDEApplyLocalHealthFields(players[consoleplayer], serverHealth, onGround);
}

static void HCDEApplyPendingLocalHealthRepair()
{
	if (!PendingLocalHealthRepair.Valid)
		return;

	if (consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
	{
		HCDEApplyLocalHealthFields(players[consoleplayer],
			PendingLocalHealthRepair.Health,
			PendingLocalHealthRepair.OnGround);
		DebugTrace::Markf("net", "HCDE pending local health repair applied tic=%u health=%d",
			PendingLocalHealthRepair.ServerTic,
			PendingLocalHealthRepair.Health);
	}

	PendingLocalHealthRepair.Valid = false;
}

static bool HCDEValidateServerWorldDeltas(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& bodyCursor, uint8_t playerCount, uint64_t snapshotPlayers)
{
	if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < HCDEServerWorldDeltaHeaderSize)
		return false;
	if (memcmp(&body[bodyCursor + HCDEServerWorldDeltaMagicOffset], HCDEServerWorldDeltaMagic, sizeof(HCDEServerWorldDeltaMagic)) != 0)
		return false;

	size_t cursor = bodyCursor + HCDEServerWorldDeltaHeaderSize;
	const uint8_t version = body[bodyCursor + HCDEServerWorldDeltaVersionOffset];
	const uint8_t flags = body[bodyCursor + HCDEServerWorldDeltaFlagsOffset];
	const uint8_t deltaCount = body[bodyCursor + HCDEServerWorldDeltaCountOffset];
	if ((version != 1u && version != HCDEServerWorldDeltaProtocolVersion) || flags != 0u || playerCount > MAXPLAYERS || deltaCount > MAXPLAYERS)
		return false;
	const size_t deltaRecordSize = version >= 2u ? HCDEServerWorldDeltaRecordV2Size : HCDEServerWorldDeltaRecordV1Size;

	uint64_t deltaPlayers = 0u;
	uint32_t serverTic = 0u;
	size_t ticCursor = bodyCursor + HCDEServerWorldDeltaTicOffset;
	if (!HCDEReadBE32Field(body, bodyBytes, ticCursor, serverTic))
		return false;

	const bool canMutatePlaysim = HCDEWorldDeltasCanMutatePlaysim();
	for (uint8_t i = 0u; i < deltaCount; ++i)
	{
		if (cursor > bodyBytes || bodyBytes - cursor < deltaRecordSize)
			return false;

		uint8_t playerNum = 0u;
		uint8_t poseFlags = 0u;
		uint16_t healthBits = 0u;
		uint32_t yaw = 0u;
		uint32_t pitch = 0u;
		double values[6] = {};
		if (!HCDEReadByteField(body, bodyBytes, cursor, playerNum)
			|| !HCDEReadByteField(body, bodyBytes, cursor, poseFlags)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, healthBits))
		{
			return false;
		}
		for (double& value : values)
		{
			if (version >= 2u
				? !HCDEReadFloatField(body, bodyBytes, cursor, value)
				: !HCDEReadDoubleField(body, bodyBytes, cursor, value))
			{
				return false;
			}
		}
		if (!HCDEReadBE32Field(body, bodyBytes, cursor, yaw)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, pitch))
		{
			return false;
		}

		if (playerNum >= MAXPLAYERS || playerNum >= 64u || (poseFlags & ~(HCDEServerWorldDeltaPoseHasActor | HCDEServerWorldDeltaPoseLive | HCDEServerWorldDeltaPoseOnGround)) != 0u)
			return false;
		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if ((deltaPlayers & playerMask) != 0u)
			return false;
		deltaPlayers |= playerMask;

		if ((poseFlags & HCDEServerWorldDeltaPoseHasActor) == 0u)
			continue;

		player_t& player = players[playerNum];
		AActor* mo = player.mo;
		if (mo == nullptr)
			continue;

		auto& peer = HCDELivePeers[clientNum];
		++peer.WorldDeltaReceived;
		const DVector3 serverPos = { values[0], values[1], values[2] };
		const DVector3 serverVel = { values[3], values[4], values[5] };
		DVector3 delta = mo->Pos() - serverPos;
		if (mo->Level != nullptr && mo->Sector != nullptr)
		{
			sector_t* serverSector = mo->Level->PointInSector(serverPos);
			if (serverSector != nullptr)
				delta += mo->Level->Displacements.getOffset(mo->Sector->PortalGroup, serverSector->PortalGroup);
		}
		const double drift = delta.LengthSquared();
		const int serverHealth = int(int16_t(healthBits));
		if (playerNum == consoleplayer)
		{
			const bool serverReportsOnGround = (poseFlags & HCDEServerWorldDeltaPoseOnGround) != 0u;
			const bool serverReportsLive = (poseFlags & HCDEServerWorldDeltaPoseLive) != 0u && serverHealth > 0;
			const bool serverReportsDead = serverHealth <= 0;
			const bool localNeedsRespawnRepair = serverReportsLive
				&& (player.playerstate != PST_LIVE
					|| mo->health <= 0
					|| player.health <= 0
					|| (mo->flags & MF_CORPSE) != 0);
			const bool localNeedsDeathRepair = serverReportsDead
				&& player.playerstate == PST_LIVE
				&& (mo->health > 0 || player.health > 0)
				&& (mo->flags & MF_CORPSE) == 0;
			const bool needsLocalStateRepair = localNeedsRespawnRepair
				|| localNeedsDeathRepair
				|| mo->health != serverHealth
				|| player.health != serverHealth
				|| player.onground != serverReportsOnGround;
			if (!needsLocalStateRepair)
				continue;

			++peer.BaselineLocalDrift;
			if (!canMutatePlaysim)
			{
				DebugTrace::Markf("net", "HCDE client local state repair deferred from=%d player=%u drift=%.2f health=%d local-health=%d",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, player.health);
				continue;
			}

			if (NetworkEntityManager::IsPredicting()
				&& !localNeedsRespawnRepair
				&& !localNeedsDeathRepair)
			{
				HCDEQueuePredictedLocalHealthRepair(serverTic, serverHealth, serverReportsOnGround);
				++peer.Reconciliations;
				DebugTrace::Markf("net", "HCDE client local health repair queued from=%d player=%u drift=%.2f health=%d reconciliations=%u",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, peer.Reconciliations);
				continue;
			}

			if (NetworkEntityManager::IsPredicting())
			{
				P_UnPredictClient();
				mo = player.mo;
				if (mo == nullptr)
					continue;
				PendingLocalHealthRepair.Valid = false;
			}

			if (localNeedsRespawnRepair)
			{
				// A death/respawn handoff is the one time the local client must
				// accept a full server-authored pawn state. Otherwise the client
				// can keep predicting from a stale corpse while the server has
				// already put the player back in the live round.
				const DVector3 oldPos = mo->Pos();
				const int oldPortalGroup = mo->Sector != nullptr ? mo->Sector->PortalGroup : mo->PrevPortalGroup;
				const double defaultViewHeight = player.DefaultViewHeight();
				const double viewZOffset = defaultViewHeight > 0.0 ? defaultViewHeight : player.viewz - mo->Z();
				const AActor* defaults = mo->GetDefault();
				mo->flags &= ~MF_CORPSE;
				if (defaults != nullptr)
					mo->flags |= defaults->flags & (MF_SOLID | MF_SHOOTABLE);
				mo->SetOrigin(serverPos, false);
				mo->Vel = serverVel;
				mo->SetAngle(DAngle::fromBam(yaw), 0);
				mo->SetPitch(DAngle::fromBam(pitch), 0);
				mo->health = serverHealth;
				player.health = serverHealth;
				player.playerstate = PST_LIVE;
				player.camera = mo;
				player.damagecount = 0;
				player.bonuscount = 0;
				player.poisoncount = 0;
				player.fixedcolormap = NOFIXEDCOLORMAP;
				player.fixedlightlevel = -1;
				player.extralight = 0;
				player.BlendR = player.BlendG = player.BlendB = player.BlendA = 0.f;
				player.attacker = nullptr;
				player.viewheight = viewZOffset;
				player.onground = serverReportsOnGround;
				player.viewz = serverPos.Z + viewZOffset;
				mo->Prev = oldPos;
				mo->PrevPortalGroup = oldPortalGroup;
				mo->renderflags |= RF_NOINTERPOLATEVIEW;
				mo->ClearInterpolation();
				P_ClearPredictionData();
				PendingLocalHealthRepair.Valid = false;
				++peer.HardReconciliations;
				++peer.Reconciliations;
				DebugTrace::Markf("net", "HCDE client respawn repair from=%d player=%u drift=%.2f health=%d reconciliations=%u hard=%u",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, peer.Reconciliations, peer.HardReconciliations);
				continue;
			}

			if (localNeedsDeathRepair)
			{
				const int previousHealth = max<int>(player.health, mo->health);
				mo->health = min<int>(serverHealth, 0);
				player.health = mo->health;
				player.onground = serverReportsOnGround;
				if (previousHealth > serverHealth)
					player.damagecount = clamp<int>(player.damagecount + previousHealth - serverHealth, 0, 100);
				mo->CallDie(nullptr, nullptr, DMG_FORCED, NAME_None);
				P_ClearPredictionData();
				PendingLocalHealthRepair.Valid = false;
				++peer.HardReconciliations;
				++peer.Reconciliations;
				DebugTrace::Markf("net", "HCDE client death repair from=%d player=%u drift=%.2f health=%d reconciliations=%u hard=%u",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, peer.Reconciliations, peer.HardReconciliations);
				continue;
			}

			// Local clients own prediction for their own pawn. Do not apply
			// server-authored position, velocity, yaw, or pitch here; doing so
			// can pin movement if the dedicated input route lags or is stale.
			// Health and grounded state are safe compact corrections.
			HCDEApplyLocalHealthFields(player, serverHealth, serverReportsOnGround);
			PendingLocalHealthRepair.Valid = false;
			++peer.Reconciliations;
			DebugTrace::Markf("net", "HCDE client local state repair from=%d player=%u drift=%.2f health=%d reconciliations=%u",
				clientNum, unsigned(playerNum), sqrt(drift), serverHealth, peer.Reconciliations);
			continue;
		}

		const bool needsPoseRepair = drift > HCDEServerBaselineRepairDistance * HCDEServerBaselineRepairDistance;
		const bool needsStateRepair = mo->health != serverHealth || player.health != serverHealth || player.onground != ((poseFlags & HCDEServerWorldDeltaPoseOnGround) != 0u);
		if (needsPoseRepair || needsStateRepair)
		{
			if (!canMutatePlaysim)
			{
				DebugTrace::Markf("net", "HCDE baseline repair deferred client=%d player=%u drift=%.2f health=%d local-health=%d",
					clientNum, unsigned(playerNum), sqrt(drift), serverHealth, player.health);
				continue;
			}

			mo->SetOrigin(serverPos, false);
			mo->Vel = serverVel;
			mo->SetAngle(DAngle::fromBam(yaw), 0);
			mo->SetPitch(DAngle::fromBam(pitch), 0);
			mo->health = serverHealth;
			player.health = serverHealth;
			player.onground = (poseFlags & HCDEServerWorldDeltaPoseOnGround) != 0u;
			mo->ClearInterpolation();
			++peer.BaselineRepairs;
			DebugTrace::Markf("net", "HCDE baseline repair client=%d player=%u drift=%.2f health=%d repairs=%u",
				clientNum, unsigned(playerNum), sqrt(drift), serverHealth, peer.BaselineRepairs);
		}
	}

	if ((deltaPlayers & snapshotPlayers) != snapshotPlayers)
		return false;

	bodyCursor = cursor;
	DebugTrace::Markf("net", "HCDE server world delta recv tic=%u players=%u bytes=%zu",
		serverTic, unsigned(deltaCount), size_t(deltaCount) * deltaRecordSize + HCDEServerWorldDeltaHeaderSize);
	return true;
}

static void Net_RecordInvasionSpawnEvent(AActor* spawned)
{
	if (!I_IsLocalHCDEServiceAuthority() || spawned == nullptr)
		return;

	const char* className = spawned->GetClass()->TypeName.GetChars();
	if (className == nullptr || className[0] == '\0')
		return;

	FInvasionReplicatedSpawnEvent event;
	event.Id = InvasionNextSpawnEventId++;
	if (InvasionNextSpawnEventId == 0u)
		InvasionNextSpawnEventId = 1u;
	event.Tic = gametic;
	event.Wave = InvasionWaveDirector.Wave;
	event.ClassName = className;
	event.Pos = spawned->Pos();
	event.Yaw = spawned->Angles.Yaw;
	event.Health = spawned->health;
	InvasionRecentSpawnEvents.Push(event);
	while (InvasionRecentSpawnEvents.Size() > HCDEInvasionSpawnEventHistoryLimit)
	{
		InvasionRecentSpawnEvents.Delete(0);
	}
	Net_RegisterInvasionReplicatedActor(event.Id, spawned);

	DebugTrace::Markf("invasion", "replicate spawn id=%u wave=%d class=%s pos=(%.1f,%.1f,%.1f) health=%d",
		unsigned(event.Id),
		event.Wave,
		event.ClassName.GetChars(),
		event.Pos.X,
		event.Pos.Y,
		event.Pos.Z,
		event.Health);
	Printf(PRINT_HIGH, "Invasion spawned id=%u wave=%d class=%s pos=(%.1f, %.1f, %.1f) health=%d\n",
		unsigned(event.Id),
		event.Wave,
		event.ClassName.GetChars(),
		event.Pos.X,
		event.Pos.Y,
		event.Pos.Z,
		event.Health);
}

static bool HCDEAppendInvasionSpawnEvents(uint8_t* output, size_t outputCapacity, size_t& cursor)
{
	if (!HCDEAppendBytes(output, outputCapacity, cursor, HCDEInvasionSpawnEventsMagic, sizeof(HCDEInvasionSpawnEventsMagic))
		|| !HCDEAppendByte(output, outputCapacity, cursor, HCDEInvasionSpawnEventsProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u))
	{
		return false;
	}

	const size_t countOffset = cursor;
	if (!HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u))
	{
		return false;
	}

	uint8_t count = 0u;
	const size_t eventCount = InvasionRecentSpawnEvents.Size();
	const size_t replayLimit = min<size_t>(HCDEInvasionSpawnEventReplayLimit, HCDEInvasionSpawnEventPacketLimit);
	const size_t start = eventCount > replayLimit ? eventCount - replayLimit : 0u;
	for (size_t i = start; i < eventCount && count < UINT8_MAX; ++i)
	{
		const auto& event = InvasionRecentSpawnEvents[i];
		const char* className = event.ClassName.GetChars();
		const size_t classNameLen = className != nullptr ? strlen(className) : 0u;
		if (event.Id == 0u || classNameLen == 0u || classNameLen > UINT8_MAX)
			continue;

		constexpr size_t fixedRecordBytes = 4u + 4u + 2u + 1u + 3u * sizeof(double) + 4u + 2u;
		const size_t recordBytes = fixedRecordBytes + classNameLen;
		const size_t actorDeltaReserve = InvasionReplicatedActors.Size() > 0 ? HCDEInvasionSpawnEventActorDeltaReserveBytes : 0u;
		if (cursor > outputCapacity
			|| outputCapacity - cursor < recordBytes
			|| outputCapacity - cursor - recordBytes < actorDeltaReserve)
		{
			break;
		}

		const size_t recordStart = cursor;
		if (!HCDEAppendBE32(output, outputCapacity, cursor, event.Id)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, uint32_t(max<int>(event.Tic, 0)))
			|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(event.Wave, 0, UINT16_MAX)))
			|| !HCDEAppendByte(output, outputCapacity, cursor, uint8_t(classNameLen))
			|| !HCDEAppendBytes(output, outputCapacity, cursor, reinterpret_cast<const uint8_t*>(className), classNameLen)
			|| !HCDEAppendDouble(output, outputCapacity, cursor, event.Pos.X)
			|| !HCDEAppendDouble(output, outputCapacity, cursor, event.Pos.Y)
			|| !HCDEAppendDouble(output, outputCapacity, cursor, event.Pos.Z)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, event.Yaw.BAMs())
			|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(event.Health, INT16_MIN, INT16_MAX))))
		{
			cursor = recordStart;
			break;
		}

		++count;
	}

	output[countOffset] = count;
	DebugTrace::Markf("invasion", "spawn events send count=%u history=%zu bytes-left=%zu",
		unsigned(count), eventCount, cursor <= outputCapacity ? outputCapacity - cursor : 0u);
	return true;
}

static bool Net_HasPendingInvasionSpawnEvent(uint32_t id)
{
	for (const auto& pending : InvasionPendingSpawnEvents)
	{
		if (pending.Id == id)
			return true;
	}
	return false;
}

static void Net_QueueInvasionSpawnEvent(const FInvasionReplicatedSpawnEvent& event)
{
	if (event.Id == 0u
		|| event.Id <= InvasionLastAppliedSpawnEventId
		|| Net_HasPendingInvasionSpawnEvent(event.Id))
	{
		return;
	}

	InvasionPendingSpawnEvents.Push(event);
	while (InvasionPendingSpawnEvents.Size() > HCDEInvasionSpawnEventHistoryLimit)
	{
		InvasionPendingSpawnEvents.Delete(0);
	}

	DebugTrace::Markf("invasion", "mirror spawn queued id=%u wave=%d class=%s reason=prediction-active",
		unsigned(event.Id), event.Wave, event.ClassName.GetChars());
}

static bool Net_HasPendingInvasionMirrorSpawn(uint32_t id)
{
	for (const auto& pending : InvasionPendingMirrorSpawns)
	{
		if (pending.Id == id)
			return true;
	}
	return false;
}

static void Net_QueueInvasionMirrorSpawn(uint32_t id, int wave, const FString& className,
	const DVector3& pos, const DVector3& vel, DAngle yaw, DAngle pitch, int health,
	bool markApplied)
{
	if (id == 0u
		|| className.IsEmpty()
		|| Net_HasPendingInvasionMirrorSpawn(id))
	{
		return;
	}
	if (auto existing = Net_FindInvasionReplicatedActor(id); existing != nullptr && existing->Actor != nullptr)
		return;

	FInvasionPendingMirrorSpawn pending;
	pending.Id = id;
	pending.Wave = wave;
	pending.ClassName = className;
	pending.Pos = pos;
	pending.Vel = vel;
	pending.Yaw = yaw;
	pending.Pitch = pitch;
	pending.Health = health;
	pending.MarkApplied = markApplied;
	pending.QueuedTic = gametic;
	InvasionPendingMirrorSpawns.Push(pending);
	while (InvasionPendingMirrorSpawns.Size() > HCDEInvasionSpawnEventHistoryLimit)
	{
		InvasionPendingMirrorSpawns.Delete(0);
	}

	DebugTrace::Markf("invasion", "mirror delta spawn queued id=%u wave=%d class=%s mark=%d reason=prediction-active",
		unsigned(id), wave, className.GetChars(), markApplied ? 1 : 0);
}

static void Net_SetInvasionMirrorVisualOnly(uint32_t id, AActor* actor)
{
	if (I_IsLocalHCDEServiceAuthority()
		|| actor == nullptr
		|| (actor->ObjectFlags & OF_EuthanizeMe) != 0)
	{
		return;
	}

	const bool wasThinking = actor->GetStatNum() >= STAT_FIRST_THINKING;
	const bool projectileMirror = Net_IsInvasionReplicatedProjectile(actor);
	const bool blockmapMirror = !projectileMirror;
	const bool needsWorldRelink = projectileMirror
		? ((actor->flags & MF_NOBLOCKMAP) == 0
			|| (actor->flags & (MF_SOLID | MF_SHOOTABLE)) != 0)
		: blockmapMirror
		? ((actor->flags & MF_NOBLOCKMAP) != 0
			|| (actor->flags & MF_SOLID) != 0
			|| (actor->flags & MF_NOCLIP) == 0
			|| (actor->flags & MF_SHOOTABLE) == 0)
		: ((actor->flags & MF_NOBLOCKMAP) == 0);
	if (needsWorldRelink)
	{
		FLinkContext ctx;
		actor->UnlinkFromWorld(&ctx);
		if (projectileMirror)
		{
			actor->flags |= MF_NOBLOCKMAP;
			actor->flags &= ~(MF_SOLID | MF_SHOOTABLE);
			actor->renderflags |= RF_NOSPRITESHADOW;
		}
		else if (blockmapMirror)
		{
			actor->flags &= ~(MF_NOBLOCKMAP | MF_SOLID);
			actor->flags |= MF_NOCLIP | MF_SHOOTABLE;
		}
		else
		{
			actor->flags |= MF_NOBLOCKMAP;
			actor->flags &= ~(MF_SOLID | MF_SHOOTABLE);
		}
		actor->LinkToWorld(&ctx);
	}
	else
	{
		if (projectileMirror)
		{
			actor->flags &= ~(MF_SOLID | MF_SHOOTABLE);
			actor->renderflags |= RF_NOSPRITESHADOW;
		}
		else if (blockmapMirror)
		{
			actor->flags &= ~MF_SOLID;
			actor->flags |= MF_NOCLIP | MF_SHOOTABLE;
		}
		else
		{
			actor->flags &= ~(MF_SOLID | MF_SHOOTABLE);
		}
	}

	actor->flags |= MF_NOCLIP;
	actor->flags4 |= MF4_STANDSTILL;
	actor->flags5 |= MF5_NOINTERACTION | MF5_NOINFIGHTING;
	actor->flags7 &= ~MF7_INCHASE;
	actor->target = nullptr;
	actor->lastenemy = nullptr;
	actor->goal = nullptr;
	actor->Vel = DVector3(0, 0, 0);

	if (!projectileMirror && actor->state == actor->SpawnState && actor->SeeState != nullptr)
		actor->SetState(actor->SeeState, true);

	if (wasThinking)
		actor->ChangeStatNum(STAT_INFO);

	if (wasThinking || needsWorldRelink)
	{
		DebugTrace::Markf("invasion", "mirror client replica armed id=%u class=%s stat=%d blockmap=%d projectile=%d flags=0x%x flags5=0x%x pos=(%.1f,%.1f,%.1f)",
			unsigned(id),
			actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
			actor->GetStatNum(),
			blockmapMirror ? 1 : 0,
			projectileMirror ? 1 : 0,
			actor->flags.GetValue(),
			actor->flags5.GetValue(),
			actor->X(),
			actor->Y(),
			actor->Z());
	}
}

static void Net_SeedInvasionMirrorVisualTarget(FInvasionReplicatedActorRef& ref, AActor* actor)
{
	if (actor == nullptr)
		return;

	if (Net_IsInvasionReplicatedProjectile(actor))
		ref.IsProjectile = true;
	ref.HasVisualTarget = true;
	ref.VisualTargetPos = actor->Pos();
	ref.VisualTargetVel = actor->Vel;
	ref.VisualTargetYaw = actor->Angles.Yaw;
	ref.VisualTargetPitch = actor->Angles.Pitch;
	ref.VisualTargetHealth = actor->health;
	ref.VisualTargetTic = gametic;
}

static void Net_SetInvasionMirrorVisualTarget(FInvasionReplicatedActorRef& ref, const DVector3& pos,
	const DVector3& vel, DAngle yaw, DAngle pitch, int health)
{
	ref.HasVisualTarget = true;
	ref.VisualTargetPos = pos;
	ref.VisualTargetVel = vel;
	ref.VisualTargetYaw = yaw;
	ref.VisualTargetPitch = pitch;
	ref.VisualTargetHealth = health;
	ref.VisualTargetTic = gametic;
}

static double Net_GetInvasionMirrorVisualStepCap(const AActor* actor)
{
	double baseSpeed = HCDEInvasionMirrorVisualFallbackStepPerTic;
	if (actor != nullptr && actor->Speed > 0.0 && actor->Speed < 128.0)
		baseSpeed = actor->Speed;
	return clamp<double>(baseSpeed * HCDEInvasionMirrorVisualSpeedMultiplier, 2.0, HCDEInvasionMirrorVisualMaxStepPerTic);
}

static void Net_TickInvasionMirrorVisualActors()
{
	if (Net_IsLocalInvasionAuthority() || InvasionReplicatedActors.Size() == 0)
		return;

	for (auto& ref : InvasionReplicatedActors)
	{
		AActor* actor = ref.Actor;
		if (actor == nullptr
			|| (actor->ObjectFlags & OF_EuthanizeMe) != 0
			|| Net_IsInvasionActorCorpseLike(actor))
		{
			continue;
		}

		Net_SetInvasionMirrorVisualOnly(ref.Id, actor);
		if (ref.HasVisualTarget)
		{
			actor->health = ref.VisualTargetHealth;
			actor->Angles.Yaw = ref.VisualTargetYaw;
			actor->Angles.Pitch = ref.VisualTargetPitch;

			const DVector3 oldPos = actor->Pos();
			const DVector3 delta = ref.VisualTargetPos - oldPos;
			const double distSq = delta.LengthSquared();
			const double snapDistanceSq = HCDEInvasionMirrorVisualSnapDistance * HCDEInvasionMirrorVisualSnapDistance;
			if (distSq > snapDistanceSq)
			{
				actor->SetOrigin(ref.VisualTargetPos, false);
				actor->Prev = ref.VisualTargetPos;
				actor->PrevPortalGroup = actor->Sector != nullptr ? actor->Sector->PortalGroup : actor->PrevPortalGroup;
				actor->ClearInterpolation();
				DebugTrace::Markf("invasion", "mirror visual snap id=%u class=%s dist=%.1f pos=(%.1f,%.1f,%.1f)",
					unsigned(ref.Id),
					actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
					sqrt(distSq),
					ref.VisualTargetPos.X,
					ref.VisualTargetPos.Y,
					ref.VisualTargetPos.Z);
			}
			else if (distSq > 0.01 || (ref.IsProjectile && ref.VisualTargetVel.LengthSquared() > 0.01))
			{
				const DVector3 oldRenderPos = actor->Pos();
				const int oldPortalGroup = actor->Sector != nullptr ? actor->Sector->PortalGroup : actor->PrevPortalGroup;
				DVector3 nextPos;
				if (ref.IsProjectile)
				{
					const DVector3 drift = ref.VisualTargetVel + delta * 0.25;
					nextPos = oldRenderPos + drift;
				}
				else
				{
					const double dist = sqrt(distSq);
					const double step = min(dist, Net_GetInvasionMirrorVisualStepCap(actor));
					nextPos = oldRenderPos + delta * (step / dist);
				}
				actor->SetOrigin(nextPos, false);
				actor->Prev = oldRenderPos;
				actor->PrevPortalGroup = oldPortalGroup;
			}
			actor->Vel = ref.IsProjectile ? ref.VisualTargetVel : DVector3(0, 0, 0);
		}

		if (actor->state == nullptr
			|| actor->tics == -1
			|| (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			continue;
		}

		if (actor->tics > 0)
			--actor->tics;
		if (actor->tics <= 0)
			actor->SetState(actor->state->GetNextState(), true);
	}
}

static bool Net_SpawnInvasionMirrorActor(uint32_t id, int wave, const FString& className,
	const DVector3& pos, DAngle yaw, DAngle pitch, int health, const char* source, bool markApplied)
{
	if (Net_IsLocalInvasionAuthority())
		return true;
	if (id == 0u || className.IsEmpty())
		return false;
	if (primaryLevel == nullptr || gamestate != GS_LEVEL || NetworkEntityManager::IsPredicting())
		return false;

	if (auto existing = Net_FindInvasionReplicatedActor(id); existing != nullptr && existing->Actor != nullptr)
	{
		Net_SetInvasionMirrorVisualTarget(*existing, pos, DVector3(0, 0, 0), yaw, pitch, health);
		if (markApplied && id > InvasionLastAppliedSpawnEventId)
			InvasionLastAppliedSpawnEventId = id;
		return true;
	}

	PClassActor* cls = PClass::FindActor(className.GetChars());
	if (cls == nullptr)
	{
		if (markApplied && id > InvasionLastAppliedSpawnEventId)
			InvasionLastAppliedSpawnEventId = id;
		Printf(PRINT_HIGH, "Invasion mirror spawn skipped: missing actor class %s\n", className.GetChars());
		DebugTrace::Markf("invasion", "mirror spawn skipped id=%u wave=%d class=%s source=%s reason=missing-class",
			unsigned(id), wave, className.GetChars(), source != nullptr ? source : "unknown");
		return true;
	}

	AActor* actor = Spawn(primaryLevel, cls, pos, ALLOW_REPLACE);
	if (actor == nullptr)
	{
		if (markApplied && id > InvasionLastAppliedSpawnEventId)
			InvasionLastAppliedSpawnEventId = id;
		DebugTrace::Markf("invasion", "mirror spawn skipped id=%u wave=%d class=%s source=%s reason=spawn-null",
			unsigned(id), wave, className.GetChars(), source != nullptr ? source : "unknown");
		return true;
	}

	actor->Angles.Yaw = yaw;
	actor->Angles.Pitch = pitch;
	if (health > 0)
		actor->health = health;
	Net_ApplyInvasionMonsterSkillTuning(actor);
	actor->ClearInterpolation();
	Net_SetInvasionMirrorVisualOnly(id, actor);
	Net_RegisterInvasionReplicatedActor(id, actor);
	if (auto ref = Net_FindInvasionReplicatedActor(id); ref != nullptr)
	{
		if (!ref->IsProjectile)
			InvasionWaveDirector.ActiveMonsters.Push(MakeObjPtr<AActor*>(actor));
		Net_SetInvasionMirrorVisualTarget(*ref, pos, actor->Vel, yaw, pitch, actor->health);
	}
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
			ConsistencyGraceUntilTic[i] = max<int>(ConsistencyGraceUntilTic[i], gametic + TICRATE);
	}
	if (markApplied && id > InvasionLastAppliedSpawnEventId)
		InvasionLastAppliedSpawnEventId = id;

	DebugTrace::Markf("invasion", "mirror spawned id=%u wave=%d class=%s source=%s pos=(%.1f,%.1f,%.1f)",
		unsigned(id),
		wave,
		className.GetChars(),
		source != nullptr ? source : "unknown",
		pos.X,
		pos.Y,
		pos.Z);
	if (auto ref = Net_FindInvasionReplicatedActor(id); ref == nullptr || !ref->IsProjectile)
	{
		Printf(PRINT_HIGH, "Invasion mirror spawned id=%u wave=%d class=%s source=%s pos=(%.1f, %.1f, %.1f) health=%d\n",
			unsigned(id),
			wave,
			className.GetChars(),
			source != nullptr ? source : "unknown",
			pos.X,
			pos.Y,
			pos.Z,
			actor->health);
	}
	return true;
}

static void Net_DrainPendingInvasionMirrorSpawns()
{
	if (Net_IsLocalInvasionAuthority()
		|| InvasionPendingMirrorSpawns.Size() == 0
		|| NetworkEntityManager::IsPredicting())
	{
		return;
	}

	TArray<FInvasionPendingMirrorSpawn> retained;
	for (const auto& pending : InvasionPendingMirrorSpawns)
	{
		if (auto existing = Net_FindInvasionReplicatedActor(pending.Id); existing != nullptr && existing->Actor != nullptr)
			continue;

		if (pending.Wave < InvasionWaveDirector.Wave || InvasionState == INVS_DISABLED)
			continue;

		if (gametic - pending.QueuedTic > TICRATE * 2)
			continue;

		if (pending.Wave != InvasionWaveDirector.Wave
			|| !Net_IsInvasionRoundActiveState(InvasionState)
			|| primaryLevel == nullptr
			|| gamestate != GS_LEVEL)
		{
			retained.Push(pending);
			continue;
		}

		if (!Net_SpawnInvasionMirrorActor(pending.Id, pending.Wave, pending.ClassName,
			pending.Pos, pending.Yaw, pending.Pitch, pending.Health,
			"delta-repair-queued", pending.MarkApplied))
		{
			retained.Push(pending);
			continue;
		}

		if (auto ref = Net_FindInvasionReplicatedActor(pending.Id); ref != nullptr && ref->Actor != nullptr)
		{
			ref->Actor->Vel = pending.Vel;
			Net_SetInvasionMirrorVisualTarget(*ref, pending.Pos, pending.Vel, pending.Yaw, pending.Pitch, pending.Health);
		}
	}

	InvasionPendingMirrorSpawns.Swap(retained);
}

static bool Net_ApplyInvasionSpawnEvent(const FInvasionReplicatedSpawnEvent& event)
{
	if (Net_IsLocalInvasionAuthority())
		return true;

	if (event.Id <= InvasionLastAppliedSpawnEventId)
		return true;

	if (event.Wave != InvasionWaveDirector.Wave
		|| !Net_IsInvasionRoundActiveState(InvasionState)
		|| primaryLevel == nullptr
		|| gamestate != GS_LEVEL)
	{
		// Keep unapplied events replayable across the join/load handoff. Spawn
		// history is included in later snapshots, so the client can mirror them
		// once the level and invasion state are ready.
		return true;
	}

	if (NetworkEntityManager::IsPredicting())
	{
		// Spawn events are authoritative server state. If they are applied inside
		// the client prediction window, the rollback cleaner can classify them as
		// predicted scratch actors and delete them before they render.
		Net_QueueInvasionSpawnEvent(event);
		return true;
	}

	return Net_SpawnInvasionMirrorActor(event.Id, event.Wave, event.ClassName, event.Pos,
		event.Yaw, nullAngle, event.Health, "spawn-event", true);
}

static void Net_DrainPendingInvasionSpawnEvents()
{
	if (Net_IsLocalInvasionAuthority()
		|| InvasionPendingSpawnEvents.Size() == 0
		|| NetworkEntityManager::IsPredicting())
	{
		return;
	}

	TArray<FInvasionReplicatedSpawnEvent> retained;
	const uint32_t previousAppliedSpawnEventId = InvasionLastAppliedSpawnEventId;
	for (const auto& event : InvasionPendingSpawnEvents)
	{
		if (event.Id <= InvasionLastAppliedSpawnEventId)
			continue;

		if (event.Wave < InvasionWaveDirector.Wave || InvasionState == INVS_DISABLED)
			continue;

		if (event.Wave != InvasionWaveDirector.Wave
			|| !Net_IsInvasionRoundActiveState(InvasionState)
			|| primaryLevel == nullptr
			|| gamestate != GS_LEVEL)
		{
			retained.Push(event);
			continue;
		}

		Net_ApplyInvasionSpawnEvent(event);
		if (event.Id > InvasionLastAppliedSpawnEventId)
			retained.Push(event);
	}

	InvasionPendingSpawnEvents.Swap(retained);
	if (InvasionLastAppliedSpawnEventId != previousAppliedSpawnEventId)
	{
		Printf(PRINT_HIGH, "HCDE invasion mirror drained spawn events through %u active=%d wave=%d pending=%u\n",
			unsigned(InvasionLastAppliedSpawnEventId),
			Net_GetInvasionActiveMonsterCount(),
			Net_GetInvasionWave(),
			unsigned(InvasionPendingSpawnEvents.Size()));
	}
}

static void Net_LogInvasionMirrorVisualDiagnostic()
{
	if (Net_IsLocalInvasionAuthority()
		|| InvasionState == INVS_DISABLED
		|| gamestate != GS_LEVEL
		|| gametic < InvasionMirrorNextVisualDiagnosticTic)
	{
		return;
	}

	InvasionMirrorNextVisualDiagnosticTic = gametic + TICRATE * 2;

	AActor* camera = nullptr;
	if (consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
	{
		camera = players[consoleplayer].camera;
		if (camera == nullptr)
			camera = players[consoleplayer].mo;
	}

	const DVector3 cameraPos = camera != nullptr ? camera->Pos() : DVector3(0, 0, 0);
	int live = 0;
	int drawable = 0;
	int hidden = 0;
	int dormant = 0;
	int visualOnly = 0;
	int euthanized = 0;
	double nearestDistSq = -1.0;
	uint32_t nearestId = 0u;
	AActor* nearest = nullptr;

	for (auto& ref : InvasionReplicatedActors)
	{
		AActor* actor = ref.Actor;
		if (actor == nullptr)
			continue;

		if ((actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			++euthanized;
			continue;
		}

		if (actor->health > 0)
			++live;
		if ((actor->flags2 & MF2_DORMANT) != 0)
			++dormant;
		if (actor->GetStatNum() < STAT_FIRST_THINKING)
			++visualOnly;

		const bool actorDrawable = (actor->renderflags & RF_INVISIBLE) == 0
			&& actor->RenderStyle.IsVisible(actor->Alpha);
		if (actorDrawable)
			++drawable;
		else
			++hidden;

		if (camera == nullptr)
			continue;

		const double distSq = (actor->Pos() - cameraPos).LengthSquared();
		if (nearest == nullptr || nearestDistSq < 0.0 || distSq < nearestDistSq)
		{
			nearestDistSq = distSq;
			nearestId = ref.Id;
			nearest = actor;
		}
	}

	if (nearest != nullptr)
	{
		const double dist = sqrt(nearestDistSq);
		Printf(PRINT_HIGH,
			"HCDE invasion mirror visual: state=%s wave=%d tracked=%u live=%d drawable=%d hidden=%d dormant=%d visualonly=%d euth=%d camera=(%.1f, %.1f, %.1f) nearest=%u:%s pos=(%.1f, %.1f, %.1f) dist=%.1f health=%d speed=%.2f stepcap=%.2f stat=%d flags=0x%x flags2=0x%x rflags=0x%x style=0x%x alpha=%.2f\n",
			Net_InvasionStateName(InvasionState),
			InvasionWaveDirector.Wave,
			unsigned(InvasionReplicatedActors.Size()),
			live,
			drawable,
			hidden,
			dormant,
			visualOnly,
			euthanized,
			cameraPos.X,
			cameraPos.Y,
			cameraPos.Z,
			unsigned(nearestId),
			nearest->GetClass()->TypeName.GetChars(),
			nearest->X(),
			nearest->Y(),
			nearest->Z(),
			dist,
			nearest->health,
			nearest->Speed,
			Net_GetInvasionMirrorVisualStepCap(nearest),
			nearest->GetStatNum(),
			nearest->flags.GetValue(),
			nearest->flags2.GetValue(),
			nearest->renderflags.GetValue(),
			unsigned(nearest->RenderStyle.AsDWORD),
			nearest->Alpha);
	}
	else
	{
		Printf(PRINT_HIGH,
			"HCDE invasion mirror visual: state=%s wave=%d tracked=%u live=%d drawable=%d hidden=%d dormant=%d visualonly=%d euth=%d camera=%s\n",
			Net_InvasionStateName(InvasionState),
			InvasionWaveDirector.Wave,
			unsigned(InvasionReplicatedActors.Size()),
			live,
			drawable,
			hidden,
			dormant,
			visualOnly,
			euthanized,
			camera != nullptr ? "ready" : "missing");
	}
}

static bool HCDEApplyInvasionSpawnEvents(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& bodyCursor)
{
	if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < HCDEInvasionSpawnEventsHeaderSize)
		return false;
	if (memcmp(&body[bodyCursor + HCDEInvasionSpawnEventsMagicOffset], HCDEInvasionSpawnEventsMagic, sizeof(HCDEInvasionSpawnEventsMagic)) != 0)
		return false;

	const uint8_t version = body[bodyCursor + HCDEInvasionSpawnEventsVersionOffset];
	const uint8_t flags = body[bodyCursor + HCDEInvasionSpawnEventsFlagsOffset];
	const uint8_t count = body[bodyCursor + HCDEInvasionSpawnEventsCountOffset];
	const uint8_t reserved = body[bodyCursor + HCDEInvasionSpawnEventsReservedOffset];
	if (version != HCDEInvasionSpawnEventsProtocolVersion || flags != 0u || reserved != 0u)
		return false;

	size_t cursor = bodyCursor + HCDEInvasionSpawnEventsHeaderSize;
	const uint32_t previousAppliedSpawnEventId = InvasionLastAppliedSpawnEventId;
	for (uint8_t i = 0u; i < count; ++i)
	{
		uint32_t id = 0u;
		uint32_t tic = 0u;
		uint16_t wave = 0u;
		uint8_t classNameLen = 0u;
		uint32_t yaw = 0u;
		uint16_t healthBits = 0u;
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		if (!HCDEReadBE32Field(body, bodyBytes, cursor, id)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, tic)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, wave)
			|| !HCDEReadByteField(body, bodyBytes, cursor, classNameLen)
			|| classNameLen == 0u
			|| cursor > bodyBytes
			|| classNameLen > bodyBytes - cursor)
		{
			return false;
		}

		FString className(reinterpret_cast<const char*>(&body[cursor]), classNameLen);
		cursor += classNameLen;
		if (!HCDEReadDoubleField(body, bodyBytes, cursor, x)
			|| !HCDEReadDoubleField(body, bodyBytes, cursor, y)
			|| !HCDEReadDoubleField(body, bodyBytes, cursor, z)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, yaw)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, healthBits))
		{
			return false;
		}

		FInvasionReplicatedSpawnEvent event;
		event.Id = id;
		event.Tic = int(tic);
		event.Wave = int(wave);
		event.ClassName = className;
		event.Pos = DVector3(x, y, z);
		event.Yaw = DAngle::fromBam(yaw);
		event.Health = int(int16_t(healthBits));
		if (!Net_ApplyInvasionSpawnEvent(event))
			return false;
	}

	bodyCursor = cursor;
	if (!I_IsLocalHCDEServiceAuthority() && InvasionLastAppliedSpawnEventId != previousAppliedSpawnEventId)
	{
		Printf(PRINT_HIGH, "HCDE invasion mirror applied spawn events through %u active=%d wave=%d\n",
			unsigned(InvasionLastAppliedSpawnEventId), Net_GetInvasionActiveMonsterCount(), Net_GetInvasionWave());
	}
	DebugTrace::Markf("net", "HCDE invasion spawn events recv client=%d count=%u last=%u",
		clientNum, unsigned(count), unsigned(InvasionLastAppliedSpawnEventId));
	return true;
}

static bool Net_IsInvasionActorCorpseLike(const AActor* actor)
{
	return actor != nullptr
		&& (actor->health <= 0 || (actor->flags & MF_CORPSE) != 0);
}

static bool Net_IsInvasionReplicatedProjectile(const AActor* actor)
{
	return actor != nullptr
		&& ((actor->flags & MF_MISSILE) != 0 || (actor->BounceFlags & BOUNCE_MBF) != 0);
}

static void Net_PrepareInvasionMirrorCorpsePhysics(AActor* actor, bool snapToFloor)
{
	if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		return;

	if (actor->GetStatNum() < STAT_FIRST_THINKING)
		actor->ChangeStatNum(STAT_DEFAULT);
	actor->flags &= ~MF_NOCLIP;
	actor->flags4 &= ~MF4_STANDSTILL;
	if (actor->Sector != nullptr)
	{
		P_FindFloorCeiling(actor);
		if (snapToFloor && (actor->flags & MF_NOGRAVITY) == 0 && fabs(actor->Z() - actor->floorz) > 0.5)
		{
			actor->SetZ(actor->floorz, false);
			actor->Prev.Z = actor->Z();
		}
	}
	actor->Vel = DVector3(0, 0, 0);
	actor->ClearInterpolation();
}

static bool Net_InvasionStateSequenceContains(const AActor* actor, FState* start, FState* state)
{
	if (actor == nullptr || start == nullptr || state == nullptr)
		return false;

	FState* current = start;
	for (int steps = 0; current != nullptr && steps < 32; ++steps)
	{
		if (current == state)
			return true;

		FState* next = current->GetNextState();
		if (next == nullptr
			|| next == start
			|| next == actor->SpawnState
			|| next == actor->SeeState)
		{
			return false;
		}
		current = next;
	}
	return false;
}

static uint8_t Net_GetInvasionActorActionState(const AActor* actor)
{
	if (actor == nullptr || actor->state == nullptr)
		return HCDEInvasionActorActionNone;
	if (Net_IsInvasionActorCorpseLike(actor))
		return HCDEInvasionActorActionNone;
	if (auto ref = Net_FindInvasionReplicatedActorByActor(actor); ref != nullptr)
	{
		if ((ref->ServerForcedActionState == HCDEInvasionActorActionMelee
				|| ref->ServerForcedActionState == HCDEInvasionActorActionMissile
				|| ref->ServerForcedActionState == HCDEInvasionActorActionPain)
			&& gametic - ref->ServerForcedActionTic <= HCDEInvasionActorActionHoldTics)
		{
			return ref->ServerForcedActionState;
		}
	}
	if (Net_InvasionStateSequenceContains(actor, actor->MeleeState, actor->state))
		return HCDEInvasionActorActionMelee;
	if (Net_InvasionStateSequenceContains(actor, actor->MissileState, actor->state))
		return HCDEInvasionActorActionMissile;
	if (Net_InvasionStateSequenceContains(actor, actor->FindState(NAME_Pain), actor->state))
		return HCDEInvasionActorActionPain;
	if (actor->SeeState != nullptr)
		return HCDEInvasionActorActionSee;
	if (actor->SpawnState != nullptr)
		return HCDEInvasionActorActionSpawn;
	return HCDEInvasionActorActionNone;
}

static FState* Net_GetInvasionMirrorActionState(AActor* actor, uint8_t actionState)
{
	if (actor == nullptr)
		return nullptr;

	switch (actionState)
	{
	case HCDEInvasionActorActionSpawn:
		return actor->SpawnState;
	case HCDEInvasionActorActionSee:
		return actor->SeeState != nullptr ? actor->SeeState : actor->SpawnState;
	case HCDEInvasionActorActionMelee:
		return actor->MeleeState != nullptr ? actor->MeleeState : actor->SeeState;
	case HCDEInvasionActorActionMissile:
		return actor->MissileState != nullptr ? actor->MissileState : actor->SeeState;
	case HCDEInvasionActorActionPain:
		if (FState* pain = actor->FindState(NAME_Pain); pain != nullptr)
			return pain;
		return actor->SeeState;
	default:
		return nullptr;
	}
}

static bool Net_IsInvasionActorActionPriority(uint8_t actionState)
{
	return actionState == HCDEInvasionActorActionMelee
		|| actionState == HCDEInvasionActorActionMissile
		|| actionState == HCDEInvasionActorActionPain;
}

static void Net_ApplyInvasionMirrorActionState(FInvasionReplicatedActorRef& ref, AActor* actor, uint8_t actionState)
{
	if (actor == nullptr
		|| actionState == HCDEInvasionActorActionNone
		|| actionState > HCDEInvasionActorActionMax
		|| Net_IsInvasionActorCorpseLike(actor))
	{
		return;
	}

	FState* targetState = Net_GetInvasionMirrorActionState(actor, actionState);
	if (targetState == nullptr)
		return;

	const bool forceAttackRefresh =
		(actionState == HCDEInvasionActorActionMelee || actionState == HCDEInvasionActorActionMissile)
		&& ref.VisualActionState == actionState
		&& gametic - ref.VisualActionTic > TICRATE / 4;
	if (ref.VisualActionState != actionState || actor->state == nullptr || forceAttackRefresh)
	{
		actor->SetState(targetState, true);
		ref.VisualActionState = actionState;
		ref.VisualActionTic = gametic;
	}
}

static void Net_DetachInvasionMirrorCorpse(FInvasionReplicatedActorRef& ref)
{
	AActor* actor = ref.Actor;
	if (actor != nullptr && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
	{
		// A retired mirror corpse is no longer server-driven, so stop any last
		// replicated velocity from making the death frame slide around.
		Net_PrepareInvasionMirrorCorpsePhysics(actor, true);
		DebugTrace::Markf("invasion", "mirror corpse detached id=%u class=%s health=%d pos=(%.1f,%.1f,%.1f)",
			unsigned(ref.Id),
			actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
			actor->health,
			actor->X(),
			actor->Y(),
			actor->Z());
	}
	ref.Actor = MakeObjPtr<AActor*>(nullptr);
	ref.DeathDeltaSent = true;
}

static void Net_RetireInvasionMirrorProjectile(FInvasionReplicatedActorRef& ref)
{
	AActor* actor = ref.Actor;
	if (actor != nullptr && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
	{
		if (actor->GetStatNum() < STAT_FIRST_THINKING)
			actor->ChangeStatNum(STAT_DEFAULT);
		actor->flags |= MF_NOBLOCKMAP | MF_NOCLIP;
		actor->flags &= ~(MF_SOLID | MF_SHOOTABLE);
		actor->flags5 |= MF5_NOINTERACTION;

		DebugTrace::Markf("invasion", "mirror projectile retired id=%u class=%s pos=(%.1f,%.1f,%.1f) flags=0x%x bounce=0x%x",
			unsigned(ref.Id),
			actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
			actor->X(),
			actor->Y(),
			actor->Z(),
			actor->flags.GetValue(),
			actor->BounceFlags.GetValue());

		if (Net_IsInvasionReplicatedProjectile(actor))
		{
			P_ExplodeMissile(actor, nullptr, nullptr);
		}
		else
		{
			actor->ClearCounters();
			actor->Destroy();
		}
	}
	ref.Actor = MakeObjPtr<AActor*>(nullptr);
	ref.DeathDeltaSent = true;
}

static void Net_RetireInvasionMirrorActor(FInvasionReplicatedActorRef& ref, int serverHealth)
{
	AActor* actor = ref.Actor;
	if (actor == nullptr)
	{
		ref.DeathDeltaSent = true;
		return;
	}

	if (ref.IsProjectile)
	{
		Net_RetireInvasionMirrorProjectile(ref);
		return;
	}

	const bool alreadyCorpse = Net_IsInvasionActorCorpseLike(actor);
	if (!alreadyCorpse && serverHealth <= 0 && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
	{
		actor->health = min<int>(actor->health, serverHealth);
		Net_PrepareInvasionMirrorCorpsePhysics(actor, false);
		if ((actor->flags & MF_CORPSE) == 0)
			actor->CallDie(nullptr, nullptr);
	}

	if (Net_IsInvasionActorCorpseLike(actor) && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
	{
		Net_DetachInvasionMirrorCorpse(ref);
		return;
	}

	DebugTrace::Markf("invasion", "mirror stale actor destroyed id=%u class=%s health=%d server-health=%d pos=(%.1f,%.1f,%.1f)",
		unsigned(ref.Id),
		actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
		actor->health,
		serverHealth,
		actor->X(),
		actor->Y(),
		actor->Z());
	actor->ClearCounters();
	actor->Destroy();
	ref.Actor = MakeObjPtr<AActor*>(nullptr);
	ref.DeathDeltaSent = true;
}

static int Net_CompactInvasionReplicatedActors()
{
	size_t writeIdx = 0u;
	for (size_t i = 0u; i < InvasionReplicatedActors.Size(); ++i)
	{
		AActor* actor = InvasionReplicatedActors[i].Actor;
		if (InvasionReplicatedActors[i].Id == 0u
			|| actor == nullptr
			|| (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			continue;
		}

		if (InvasionReplicatedActors[i].IsProjectile)
		{
			const bool projectileExpired = InvasionReplicatedActors[i].SpawnTic > 0
				&& gametic - InvasionReplicatedActors[i].SpawnTic > HCDEInvasionProjectileMirrorMaxAgeTics;
			if (!Net_IsInvasionReplicatedProjectile(actor) || projectileExpired || InvasionReplicatedActors[i].ForceDeathDelta)
			{
				if (InvasionReplicatedActors[i].DeathDeltaSent)
					continue;

				// Send one final non-live packet so clients can play a local
				// projectile impact instead of leaving a stale missile sprite.
				InvasionReplicatedActors[i].DeathDeltaSent = true;
				if (projectileExpired)
					InvasionReplicatedActors[i].ForceDeathDelta = true;
			}
			else
			{
				InvasionReplicatedActors[i].DeathDeltaSent = false;
			}
		}
		else if (Net_IsInvasionActorCorpseLike(actor))
		{
			if (InvasionReplicatedActors[i].DeathDeltaSent)
				continue;

			// Keep a newly dead monster for one more packet so clients can
			// retire the mirror actor into a local corpse instead of deleting it.
			InvasionReplicatedActors[i].DeathDeltaSent = true;
		}
		else
		{
			InvasionReplicatedActors[i].DeathDeltaSent = false;
		}

		if (writeIdx != i)
			InvasionReplicatedActors[writeIdx] = InvasionReplicatedActors[i];
		++writeIdx;
	}

	if (writeIdx < InvasionReplicatedActors.Size())
		InvasionReplicatedActors.Resize(unsigned(writeIdx));
	return int(writeIdx);
}

static FInvasionReplicatedActorRef* Net_FindInvasionReplicatedActor(uint32_t id)
{
	if (id == 0u)
		return nullptr;

	for (size_t i = 0u; i < InvasionReplicatedActors.Size(); ++i)
	{
		if (InvasionReplicatedActors[i].Id == id)
			return &InvasionReplicatedActors[i];
	}
	return nullptr;
}

static FInvasionReplicatedActorRef* Net_FindInvasionReplicatedActorByActor(const AActor* actor)
{
	if (actor == nullptr)
		return nullptr;

	for (size_t i = 0u; i < InvasionReplicatedActors.Size(); ++i)
	{
		if (InvasionReplicatedActors[i].Actor == actor)
			return &InvasionReplicatedActors[i];
	}
	return nullptr;
}

static void Net_ForceInvasionActorAction(const AActor* actor, uint8_t actionState)
{
	if (actor == nullptr
		|| (actionState != HCDEInvasionActorActionMelee
			&& actionState != HCDEInvasionActorActionMissile
			&& actionState != HCDEInvasionActorActionPain))
	{
		return;
	}

	if (auto ref = Net_FindInvasionReplicatedActorByActor(actor); ref != nullptr)
	{
		ref->ServerForcedActionState = actionState;
		ref->ServerForcedActionTic = gametic;
	}
}

bool Net_IsInvasionClientMirrorActor(const AActor* actor)
{
	if (actor == nullptr || !I_UsesDedicatedServerSlot() || I_IsLocalHCDEServiceAuthority())
		return false;

	return Net_FindInvasionReplicatedActorByActor(actor) != nullptr;
}

bool Net_IsInvasionClientMirrorBlockingActor(const AActor* actor)
{
	if (actor == nullptr
		|| !I_UsesDedicatedServerSlot()
		|| I_IsLocalHCDEServiceAuthority()
		|| (actor->ObjectFlags & OF_EuthanizeMe) != 0
		|| Net_IsInvasionActorCorpseLike(actor))
	{
		return false;
	}

	auto ref = Net_FindInvasionReplicatedActorByActor(actor);
	return ref != nullptr && !ref->IsProjectile;
}

void Net_RecordInvasionActorAttack(AActor* attacker, AActor* target)
{
	if (!I_IsLocalHCDEServiceAuthority()
		|| !Net_IsInvasionModeEnabled()
		|| gamestate != GS_LEVEL
		|| attacker == nullptr
		|| target == nullptr
		|| attacker->health <= 0
		|| (attacker->flags3 & MF3_ISMONSTER) == 0
		|| Net_FindInvasionReplicatedActorByActor(attacker) == nullptr)
	{
		return;
	}

	uint8_t actionState = Net_GetInvasionActorActionState(attacker);
	if (actionState != HCDEInvasionActorActionMelee && actionState != HCDEInvasionActorActionMissile)
	{
		const double meleeRange = max<double>(attacker->meleerange, 0.0)
			+ attacker->radius
			+ target->radius
			+ 32.0;
		const bool likelyMelee = attacker->MeleeState != nullptr
			&& attacker->Distance3D(target) <= meleeRange;
		if (likelyMelee)
			actionState = HCDEInvasionActorActionMelee;
		else if (attacker->MissileState != nullptr)
			actionState = HCDEInvasionActorActionMissile;
		else if (attacker->MeleeState != nullptr)
			actionState = HCDEInvasionActorActionMelee;
	}

	Net_ForceInvasionActorAction(attacker, actionState);
}

static void Net_RegisterInvasionReplicatedActor(uint32_t id, AActor* actor)
{
	if (id == 0u || actor == nullptr)
		return;

	if (auto existing = Net_FindInvasionReplicatedActor(id); existing != nullptr)
	{
		existing->Actor = MakeObjPtr<AActor*>(actor);
		existing->DeathDeltaSent = false;
		existing->ForceDeathDelta = false;
		if (Net_IsInvasionReplicatedProjectile(actor))
			existing->IsProjectile = true;
		Net_SeedInvasionMirrorVisualTarget(*existing, actor);
		return;
	}

	FInvasionReplicatedActorRef ref;
	ref.Id = id;
	ref.Actor = MakeObjPtr<AActor*>(actor);
	ref.IsProjectile = Net_IsInvasionReplicatedProjectile(actor);
	ref.SpawnTic = gametic;
	Net_SeedInvasionMirrorVisualTarget(ref, actor);
	InvasionReplicatedActors.Push(ref);
}

static bool Net_InvasionDeltaVectorChanged(const DVector3& a, const DVector3& b, double epsilon)
{
	return fabs(a.X - b.X) > epsilon
		|| fabs(a.Y - b.Y) > epsilon
		|| fabs(a.Z - b.Z) > epsilon;
}

static void Net_ResetInvasionActorDeltaBaseline(int clientNum)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;

	for (auto& ref : InvasionReplicatedActors)
	{
		ref.LastSent[clientNum] = {};
		ref.LastFullDeltaTic[clientNum] = 0;
	}
}

void Net_RegisterInvasionReplicatedMissile(AActor* missile, const AActor* source)
{
	if (!I_IsLocalHCDEServiceAuthority()
		|| !Net_IsInvasionModeEnabled()
		|| gamestate != GS_LEVEL
		|| primaryLevel == nullptr
		|| missile == nullptr
		|| source == nullptr
		|| (missile->ObjectFlags & OF_EuthanizeMe) != 0
		|| !Net_IsInvasionReplicatedProjectile(missile)
		|| Net_FindInvasionReplicatedActorByActor(missile) != nullptr)
	{
		return;
	}

	auto sourceRef = Net_FindInvasionReplicatedActorByActor(source);
	if (sourceRef == nullptr)
		return;
	sourceRef->ServerForcedActionState = HCDEInvasionActorActionMissile;
	sourceRef->ServerForcedActionTic = gametic;

	const uint32_t projectileId = InvasionNextSpawnEventId++;
	if (InvasionNextSpawnEventId == 0u)
		InvasionNextSpawnEventId = 1u;
	Net_RegisterInvasionReplicatedActor(projectileId, missile);

	DebugTrace::Markf("invasion", "missile replicated id=%u class=%s source=%s pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f)",
		unsigned(projectileId),
		missile->GetClass() != nullptr ? missile->GetClass()->TypeName.GetChars() : "<unknown>",
		source->GetClass() != nullptr ? source->GetClass()->TypeName.GetChars() : "<unknown>",
		missile->X(),
		missile->Y(),
		missile->Z(),
		missile->Vel.X,
		missile->Vel.Y,
		missile->Vel.Z);
}

static bool HCDEAppendInvasionActorDeltas(int clientNum, uint8_t* output, size_t outputCapacity, size_t& cursor)
{
	if (!I_IsLocalHCDEServiceAuthority())
	{
		return HCDEAppendBytes(output, outputCapacity, cursor, HCDEInvasionActorDeltasMagic, sizeof(HCDEInvasionActorDeltasMagic))
			&& HCDEAppendByte(output, outputCapacity, cursor, HCDEInvasionActorDeltasProtocolVersion)
			&& HCDEAppendByte(output, outputCapacity, cursor, HCDEInvasionActorDeltasFlagComplete)
			&& HCDEAppendByte(output, outputCapacity, cursor, 0u)
			&& HCDEAppendByte(output, outputCapacity, cursor, 0u);
	}
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return false;

	const int activeRefs = Net_CompactInvasionReplicatedActors();
	if (activeRefs <= 0)
		InvasionActorDeltaSendCursor = 0u;
	else if (InvasionActorDeltaSendCursor >= size_t(activeRefs))
		InvasionActorDeltaSendCursor %= size_t(activeRefs);

	const size_t headerCursor = cursor;
	if (!HCDEAppendBytes(output, outputCapacity, cursor, HCDEInvasionActorDeltasMagic, sizeof(HCDEInvasionActorDeltasMagic))
		|| !HCDEAppendByte(output, outputCapacity, cursor, HCDEInvasionActorDeltasProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u)
		|| !HCDEAppendByte(output, outputCapacity, cursor, 0u))
	{
		return false;
	}

	uint8_t count = 0u;
	uint8_t priorityCount = 0u;
	size_t nextSendCursor = InvasionActorDeltaSendCursor;
	TArray<size_t> sentIndexes;
	auto wasIndexSent = [&sentIndexes](size_t actorIndex) -> bool
	{
		for (const size_t sentIndex : sentIndexes)
		{
			if (sentIndex == actorIndex)
				return true;
		}
		return false;
	};
	auto appendActorDelta = [&](size_t actorIndex, bool priority) -> int
	{
		AActor* actor = InvasionReplicatedActors[actorIndex].Actor;
		if (actor == nullptr)
			return -1;
		auto& ref = InvasionReplicatedActors[actorIndex];
		auto& sent = ref.LastSent[clientNum];

		const char* className = actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : nullptr;
		const size_t classNameLen = className != nullptr ? strlen(className) : 0u;
		if (classNameLen == 0u || classNameLen > UINT8_MAX)
			return -1;

		uint8_t actorFlags = 0u;
		const bool projectileLive = ref.IsProjectile && Net_IsInvasionReplicatedProjectile(actor) && !ref.ForceDeathDelta;
		if ((actor->health > 0 || projectileLive) && (actor->ObjectFlags & OF_EuthanizeMe) == 0)
			actorFlags |= HCDEInvasionActorDeltaFlagLive;
		const uint8_t actorActionState = Net_GetInvasionActorActionState(actor);
		const int actorHealth = projectileLive && actor->health <= 0 ? 1 : actor->health;
		const DVector3 actorPos = actor->Pos();
		const DVector3 actorVel = actor->Vel;
		const uint32_t actorYaw = actor->Angles.Yaw.BAMs();
		const uint32_t actorPitch = actor->Angles.Pitch.BAMs();
		const bool forceFull = !sent.Valid
			|| gametic - ref.LastFullDeltaTic[clientNum] >= TICRATE
			|| (actorFlags & HCDEInvasionActorDeltaFlagLive) == 0u;

		uint16_t fieldMask = 0u;
		if (forceFull || sent.ClassName.Compare(className) != 0)
			fieldMask |= HCDEInvasionActorDeltaFieldClass;
		if (forceFull || sent.ActorFlags != actorFlags)
			fieldMask |= HCDEInvasionActorDeltaFieldFlags;
		if (forceFull || sent.ActionState != actorActionState || (priority && Net_IsInvasionActorActionPriority(actorActionState)))
			fieldMask |= HCDEInvasionActorDeltaFieldAction;
		if (forceFull || sent.Health != actorHealth)
			fieldMask |= HCDEInvasionActorDeltaFieldHealth;
		if (forceFull || Net_InvasionDeltaVectorChanged(sent.Pos, actorPos, 0.03125))
			fieldMask |= HCDEInvasionActorDeltaFieldPos;
		if (forceFull || Net_InvasionDeltaVectorChanged(sent.Vel, actorVel, 0.03125))
			fieldMask |= HCDEInvasionActorDeltaFieldVel;
		if (forceFull || sent.Yaw != actorYaw || sent.Pitch != actorPitch)
			fieldMask |= HCDEInvasionActorDeltaFieldAngles;
		if (fieldMask == 0u)
			return -2;

		size_t recordBytes = 4u + 2u;
		if (fieldMask & HCDEInvasionActorDeltaFieldClass)
			recordBytes += 1u + classNameLen;
		if (fieldMask & HCDEInvasionActorDeltaFieldFlags)
			recordBytes += 1u;
		if (fieldMask & HCDEInvasionActorDeltaFieldAction)
			recordBytes += 1u;
		if (fieldMask & HCDEInvasionActorDeltaFieldHealth)
			recordBytes += 2u;
		if (fieldMask & HCDEInvasionActorDeltaFieldPos)
			recordBytes += 3u * sizeof(float);
		if (fieldMask & HCDEInvasionActorDeltaFieldVel)
			recordBytes += 3u * sizeof(float);
		if (fieldMask & HCDEInvasionActorDeltaFieldAngles)
			recordBytes += 8u;
		if (cursor > outputCapacity || outputCapacity - cursor < recordBytes)
		{
			return 0;
		}

		if (!HCDEAppendBE32(output, outputCapacity, cursor, ref.Id)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, fieldMask))
			return -1;
		if ((fieldMask & HCDEInvasionActorDeltaFieldClass)
			&& (!HCDEAppendByte(output, outputCapacity, cursor, uint8_t(classNameLen))
				|| !HCDEAppendBytes(output, outputCapacity, cursor, reinterpret_cast<const uint8_t*>(className), classNameLen)))
			return -1;
		if ((fieldMask & HCDEInvasionActorDeltaFieldFlags)
			&& !HCDEAppendByte(output, outputCapacity, cursor, actorFlags))
			return -1;
		if ((fieldMask & HCDEInvasionActorDeltaFieldAction)
			&& !HCDEAppendByte(output, outputCapacity, cursor, actorActionState))
			return -1;
		if ((fieldMask & HCDEInvasionActorDeltaFieldHealth)
			&& !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(clamp<int>(actorHealth, INT16_MIN, INT16_MAX))))
			return -1;
		if ((fieldMask & HCDEInvasionActorDeltaFieldPos)
			&& (!HCDEAppendFloat(output, outputCapacity, cursor, actorPos.X)
				|| !HCDEAppendFloat(output, outputCapacity, cursor, actorPos.Y)
				|| !HCDEAppendFloat(output, outputCapacity, cursor, actorPos.Z)))
			return -1;
		if ((fieldMask & HCDEInvasionActorDeltaFieldVel)
			&& (!HCDEAppendFloat(output, outputCapacity, cursor, actorVel.X)
				|| !HCDEAppendFloat(output, outputCapacity, cursor, actorVel.Y)
				|| !HCDEAppendFloat(output, outputCapacity, cursor, actorVel.Z)))
			return -1;
		if ((fieldMask & HCDEInvasionActorDeltaFieldAngles)
			&& (!HCDEAppendBE32(output, outputCapacity, cursor, actorYaw)
				|| !HCDEAppendBE32(output, outputCapacity, cursor, actorPitch)))
			return -1;

		sent.Valid = true;
		sent.ActorFlags = actorFlags;
		sent.ActionState = actorActionState;
		sent.Health = actorHealth;
		sent.ClassName = className;
		sent.Pos = actorPos;
		sent.Vel = actorVel;
		sent.Yaw = actorYaw;
		sent.Pitch = actorPitch;
		if (forceFull)
			ref.LastFullDeltaTic[clientNum] = gametic;
		++count;
		if (priority)
			++priorityCount;
		sentIndexes.Push(actorIndex);
		return 1;
	};

	for (size_t actorIndex = 0u; actorIndex < size_t(activeRefs) && count < UINT8_MAX; ++actorIndex)
	{
		AActor* actor = InvasionReplicatedActors[actorIndex].Actor;
		if (actor == nullptr)
			return false;
		if (!InvasionReplicatedActors[actorIndex].IsProjectile
			&& !Net_IsInvasionActorActionPriority(Net_GetInvasionActorActionState(actor)))
			continue;

		const int appendResult = appendActorDelta(actorIndex, true);
		if (appendResult == -2)
			continue;
		if (appendResult < 0)
			return false;
		if (appendResult == 0)
			break;
	}

	for (int sentCandidates = 0; sentCandidates < activeRefs && count < UINT8_MAX; ++sentCandidates)
	{
		const size_t actorIndex = (InvasionActorDeltaSendCursor + size_t(sentCandidates)) % size_t(activeRefs);
		if (wasIndexSent(actorIndex))
			continue;

		const int appendResult = appendActorDelta(actorIndex, false);
		if (appendResult == -2)
			continue;
		if (appendResult < 0)
			return false;
		if (appendResult == 0)
		{
			nextSendCursor = actorIndex;
			break;
		}
		nextSendCursor = (actorIndex + 1u) % size_t(activeRefs);
	}

	InvasionActorDeltaSendCursor = activeRefs > 0 ? nextSendCursor : 0u;
	const uint8_t flags = count == activeRefs ? HCDEInvasionActorDeltasFlagComplete : 0u;
	output[headerCursor + HCDEInvasionActorDeltasFlagsOffset] = flags;
	output[headerCursor + HCDEInvasionActorDeltasCountOffset] = count;

	DebugTrace::Markf("invasion", "actor deltas send count=%u priority=%u complete=%d wave=%d active=%d cursor=%zu bytes-left=%zu",
		unsigned(count), unsigned(priorityCount),
		(flags & HCDEInvasionActorDeltasFlagComplete) != 0u ? 1 : 0,
		InvasionWaveDirector.Wave, activeRefs, InvasionActorDeltaSendCursor,
		cursor <= outputCapacity ? outputCapacity - cursor : 0u);
	return true;
}

static bool HCDEApplyInvasionActorDeltas(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& bodyCursor)
{
	if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < HCDEInvasionActorDeltasHeaderSize)
		return false;
	if (memcmp(&body[bodyCursor + HCDEInvasionActorDeltasMagicOffset], HCDEInvasionActorDeltasMagic, sizeof(HCDEInvasionActorDeltasMagic)) != 0)
		return false;

	const uint8_t version = body[bodyCursor + HCDEInvasionActorDeltasVersionOffset];
	const uint8_t flags = body[bodyCursor + HCDEInvasionActorDeltasFlagsOffset];
	const uint8_t count = body[bodyCursor + HCDEInvasionActorDeltasCountOffset];
	const uint8_t reserved = body[bodyCursor + HCDEInvasionActorDeltasReservedOffset];
	if ((version < 1u || version > HCDEInvasionActorDeltasProtocolVersion)
		|| (flags & ~HCDEInvasionActorDeltasFlagComplete) != 0u
		|| reserved != 0u)
	{
		return false;
	}
	const bool hasClassNames = version >= 2u;
	const bool hasActionState = version >= 3u;
	const bool hasCompactTransform = version >= 4u;
	const bool hasFieldMask = version >= 5u;

	size_t cursor = bodyCursor + HCDEInvasionActorDeltasHeaderSize;
	TArray<uint32_t> seenIds;
	seenIds.Reserve(count);
	int applied = 0;
	int missing = 0;
	for (uint8_t i = 0u; i < count; ++i)
	{
		uint32_t id = 0u;
		uint8_t actorFlags = 0u;
		uint8_t actorReserved = 0u;
		uint8_t classNameLen = 0u;
		uint16_t fieldMask = HCDEInvasionActorDeltaFieldAll;
		uint16_t healthBits = 0u;
		uint32_t yaw = 0u;
		uint32_t pitch = 0u;
		double values[6] = {};
		FString className;
		if (hasFieldMask)
		{
			if (!HCDEReadBE32Field(body, bodyBytes, cursor, id)
				|| !HCDEReadBE16Field(body, bodyBytes, cursor, fieldMask))
			{
				return false;
			}
			if (fieldMask == 0u || (fieldMask & ~HCDEInvasionActorDeltaFieldAll) != 0u)
				return false;
		}
		else
		{
			if (!hasClassNames && (cursor > bodyBytes || bodyBytes - cursor < HCDEInvasionActorDeltaV1RecordSize))
				return false;
			if (!HCDEReadBE32Field(body, bodyBytes, cursor, id)
				|| !HCDEReadByteField(body, bodyBytes, cursor, actorFlags)
				|| !HCDEReadByteField(body, bodyBytes, cursor, actorReserved))
			{
				return false;
			}
		}

		if (fieldMask & HCDEInvasionActorDeltaFieldClass)
		{
			if (!HCDEReadByteField(body, bodyBytes, cursor, classNameLen)
				|| classNameLen == 0u
				|| cursor > bodyBytes
				|| classNameLen > bodyBytes - cursor)
			{
				return false;
			}
			className = FString(reinterpret_cast<const char*>(&body[cursor]), classNameLen);
			cursor += classNameLen;
		}
		if ((fieldMask & HCDEInvasionActorDeltaFieldFlags)
			&& !HCDEReadByteField(body, bodyBytes, cursor, actorFlags))
		{
			return false;
		}
		if ((fieldMask & HCDEInvasionActorDeltaFieldAction)
			&& !HCDEReadByteField(body, bodyBytes, cursor, actorReserved))
		{
			return false;
		}
		if ((fieldMask & HCDEInvasionActorDeltaFieldHealth)
			&& !HCDEReadBE16Field(body, bodyBytes, cursor, healthBits))
		{
			return false;
		}
		if (fieldMask & HCDEInvasionActorDeltaFieldPos)
		{
			for (int valueIndex = 0; valueIndex < 3; ++valueIndex)
			{
				if (hasCompactTransform
					? !HCDEReadFloatField(body, bodyBytes, cursor, values[valueIndex])
					: !HCDEReadDoubleField(body, bodyBytes, cursor, values[valueIndex]))
				{
					return false;
				}
			}
		}
		if (fieldMask & HCDEInvasionActorDeltaFieldVel)
		{
			for (int valueIndex = 3; valueIndex < 6; ++valueIndex)
			{
				if (hasCompactTransform
					? !HCDEReadFloatField(body, bodyBytes, cursor, values[valueIndex])
					: !HCDEReadDoubleField(body, bodyBytes, cursor, values[valueIndex]))
				{
					return false;
				}
			}
		}
		if ((fieldMask & HCDEInvasionActorDeltaFieldAngles)
			&& (!HCDEReadBE32Field(body, bodyBytes, cursor, yaw)
				|| !HCDEReadBE32Field(body, bodyBytes, cursor, pitch)))
		{
			return false;
		}
		if (id == 0u
			|| (!hasActionState && actorReserved != 0u)
			|| (hasActionState && actorReserved > HCDEInvasionActorActionMax)
			|| (actorFlags & ~HCDEInvasionActorDeltaFlagLive) != 0u)
		{
			return false;
		}

		seenIds.Push(id);
		auto ref = Net_FindInvasionReplicatedActor(id);
		AActor* existingActor = ref != nullptr ? ref->Actor : nullptr;
		if (hasFieldMask && existingActor != nullptr)
		{
			if ((fieldMask & HCDEInvasionActorDeltaFieldFlags) == 0u
				&& (existingActor->health > 0 || (ref != nullptr && ref->IsProjectile))
				&& (existingActor->ObjectFlags & OF_EuthanizeMe) == 0)
			{
				actorFlags = HCDEInvasionActorDeltaFlagLive;
			}
			if ((fieldMask & HCDEInvasionActorDeltaFieldAction) == 0u)
				actorReserved = ref != nullptr ? ref->VisualActionState : HCDEInvasionActorActionNone;
			if ((fieldMask & HCDEInvasionActorDeltaFieldHealth) == 0u)
				healthBits = uint16_t(clamp<int>(existingActor->health, INT16_MIN, INT16_MAX));
			if ((fieldMask & HCDEInvasionActorDeltaFieldPos) == 0u)
			{
				const DVector3 pos = ref != nullptr && ref->HasVisualTarget ? ref->VisualTargetPos : existingActor->Pos();
				values[0] = pos.X;
				values[1] = pos.Y;
				values[2] = pos.Z;
			}
			if ((fieldMask & HCDEInvasionActorDeltaFieldVel) == 0u)
			{
				values[3] = existingActor->Vel.X;
				values[4] = existingActor->Vel.Y;
				values[5] = existingActor->Vel.Z;
			}
			if ((fieldMask & HCDEInvasionActorDeltaFieldAngles) == 0u)
			{
				const DAngle fallbackYaw = ref != nullptr && ref->HasVisualTarget ? ref->VisualTargetYaw : existingActor->Angles.Yaw;
				const DAngle fallbackPitch = ref != nullptr && ref->HasVisualTarget ? ref->VisualTargetPitch : existingActor->Angles.Pitch;
				yaw = fallbackYaw.BAMs();
				pitch = fallbackPitch.BAMs();
			}
		}

		const int health = int(int16_t(healthBits));
		const DVector3 pos(values[0], values[1], values[2]);
		const DAngle targetYaw = DAngle::fromBam(yaw);
		const DAngle targetPitch = DAngle::fromBam(pitch);
		if (ref == nullptr || ref->Actor == nullptr)
		{
			const bool hasSpawnState = (fieldMask & (HCDEInvasionActorDeltaFieldClass | HCDEInvasionActorDeltaFieldFlags | HCDEInvasionActorDeltaFieldHealth | HCDEInvasionActorDeltaFieldPos | HCDEInvasionActorDeltaFieldAngles))
				== (HCDEInvasionActorDeltaFieldClass | HCDEInvasionActorDeltaFieldFlags | HCDEInvasionActorDeltaFieldHealth | HCDEInvasionActorDeltaFieldPos | HCDEInvasionActorDeltaFieldAngles);
			if (hasClassNames
				&& (!hasFieldMask || hasSpawnState)
				&& (actorFlags & HCDEInvasionActorDeltaFlagLive) != 0u
				&& health > 0
				&& Net_SpawnInvasionMirrorActor(id, InvasionWaveDirector.Wave, className, pos,
					targetYaw, targetPitch, health, "delta-repair", false))
			{
				ref = Net_FindInvasionReplicatedActor(id);
			}
			else if (hasClassNames
				&& (!hasFieldMask || hasSpawnState)
				&& (actorFlags & HCDEInvasionActorDeltaFlagLive) != 0u
				&& health > 0
				&& NetworkEntityManager::IsPredicting())
			{
				Net_QueueInvasionMirrorSpawn(id, InvasionWaveDirector.Wave, className, pos,
					DVector3(values[3], values[4], values[5]), targetYaw, targetPitch, health, false);
			}
			if (ref == nullptr || ref->Actor == nullptr)
			{
				++missing;
				continue;
			}
		}

		AActor* actor = ref->Actor;
		if ((actorFlags & HCDEInvasionActorDeltaFlagLive) == 0u || health <= 0)
		{
			Net_RetireInvasionMirrorActor(*ref, health);
			continue;
		}

		const DVector3 oldPos = actor->Pos();
		const bool firstVisualTarget = !ref->HasVisualTarget;
		Net_SetInvasionMirrorVisualTarget(*ref, pos, DVector3(values[3], values[4], values[5]),
			targetYaw, targetPitch, health);
		actor->health = health;
		actor->Angles.Yaw = targetYaw;
		actor->Angles.Pitch = targetPitch;
		Net_SetInvasionMirrorVisualOnly(id, actor);
		if (hasActionState && !ref->IsProjectile)
			Net_ApplyInvasionMirrorActionState(*ref, actor, actorReserved);
		const double distSq = (pos - oldPos).LengthSquared();
		const double snapDistanceSq = HCDEInvasionMirrorVisualSnapDistance * HCDEInvasionMirrorVisualSnapDistance;
		if (firstVisualTarget || ref->IsProjectile || distSq > snapDistanceSq)
		{
			actor->SetOrigin(pos, false);
			actor->Prev = pos;
			actor->PrevPortalGroup = actor->Sector != nullptr ? actor->Sector->PortalGroup : actor->PrevPortalGroup;
			actor->ClearInterpolation();
			if (!firstVisualTarget && distSq > snapDistanceSq)
			{
				DebugTrace::Markf("invasion", "mirror visual snap id=%u class=%s dist=%.1f pos=(%.1f,%.1f,%.1f)",
					unsigned(id),
					actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
					sqrt(distSq),
					pos.X,
					pos.Y,
					pos.Z);
			}
		}
		++applied;
	}

	bodyCursor = cursor;
	if ((flags & HCDEInvasionActorDeltasFlagComplete) != 0u)
	{
		for (size_t i = 0u; i < InvasionReplicatedActors.Size(); ++i)
		{
			const uint32_t id = InvasionReplicatedActors[i].Id;
			bool seen = false;
			for (const uint32_t seenId : seenIds)
			{
				if (seenId == id)
				{
					seen = true;
					break;
				}
			}
			if (seen)
				continue;

			if (AActor* actor = InvasionReplicatedActors[i].Actor; actor != nullptr)
			{
				if (Net_IsInvasionActorCorpseLike(actor))
				{
					Net_DetachInvasionMirrorCorpse(InvasionReplicatedActors[i]);
					continue;
				}

				// If a complete live set omits an actor while the wave is active,
				// treat it as a missed death delta. That favors a stable local
				// corpse over a visible monster popping out of existence.
				const int impliedHealth = Net_IsInvasionRoundActiveState(InvasionState) ? 0 : actor->health;
				Net_RetireInvasionMirrorActor(InvasionReplicatedActors[i], impliedHealth);
			}
			InvasionReplicatedActors[i].Actor = MakeObjPtr<AActor*>(nullptr);
		}
		Net_CompactInvasionReplicatedActors();
	}

	DebugTrace::Markf("invasion", "actor deltas recv client=%d count=%u applied=%d missing=%d tracked=%d",
		clientNum, unsigned(count), applied, missing, int(InvasionReplicatedActors.Size()));
	return true;
}

static bool HCDEAppendInvasionSnapshot(int clientNum, uint8_t* output, size_t outputCapacity, size_t& cursor)
{
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
	const size_t invasionPayloadEnd = min(outputCapacity, cursor + HCDEInvasionSnapshotPayloadBudgetBytes);
	return HCDEAppendInvasionSpawnEvents(output, invasionPayloadEnd, cursor)
		&& HCDEAppendInvasionActorDeltas(clientNum, output, invasionPayloadEnd, cursor);
}

static bool HCDEApplyInvasionSnapshot(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& bodyCursor)
{
	if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < HCDEInvasionSnapshotHeaderV1Size)
		return false;
	if (memcmp(&body[bodyCursor + HCDEInvasionSnapshotMagicOffset], HCDEInvasionSnapshotMagic, sizeof(HCDEInvasionSnapshotMagic)) != 0)
		return false;

	const uint8_t version = body[bodyCursor + HCDEInvasionSnapshotVersionOffset];
	const uint8_t flags = body[bodyCursor + HCDEInvasionSnapshotFlagsOffset];
	const uint8_t stateRaw = body[bodyCursor + HCDEInvasionSnapshotStateOffset];
	const uint8_t reserved = body[bodyCursor + HCDEInvasionSnapshotReservedOffset];
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

	InvasionState = EInvasionState(stateRaw);
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
	if (bodyCursor < bodyBytes
		&& bodyBytes - bodyCursor >= HCDEInvasionSpawnEventsHeaderSize
		&& memcmp(&body[bodyCursor + HCDEInvasionSpawnEventsMagicOffset], HCDEInvasionSpawnEventsMagic, sizeof(HCDEInvasionSpawnEventsMagic)) == 0)
	{
		if (!HCDEApplyInvasionSpawnEvents(clientNum, body, bodyBytes, bodyCursor))
			return false;
	}
	if (bodyCursor < bodyBytes
		&& bodyBytes - bodyCursor >= HCDEInvasionActorDeltasHeaderSize
		&& memcmp(&body[bodyCursor + HCDEInvasionActorDeltasMagicOffset], HCDEInvasionActorDeltasMagic, sizeof(HCDEInvasionActorDeltasMagic)) == 0)
	{
		if (!HCDEApplyInvasionActorDeltas(clientNum, body, bodyBytes, bodyCursor))
			return false;
	}

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

static bool HCDEDecodeLegacyUserCmdRecord(int playerNum, uint32_t sequence, const uint8_t* data, size_t dataSize, size_t& eventBytes, usercmd_t& command)
{
	eventBytes = 0u;
	size_t inputCursor = 0u;
	bool sawEventCommand = false;
	while (inputCursor < dataSize)
	{
		const size_t eventStart = inputCursor;
		uint8_t type = 0u;
		if (!HCDEReadByteField(data, dataSize, inputCursor, type))
			return false;

		if (type == DEM_USERCMD)
		{
			TArrayView<uint8_t> stream = TArrayView(const_cast<uint8_t*>(&data[inputCursor]), dataSize - inputCursor);
			UnpackUserCmd(command, HCDEBuildUserCmdBasis(playerNum, sequence), stream);
			eventBytes = eventStart;
			return stream.Size() == 0u;
		}
		if (type == DEM_EMPTYUSERCMD)
		{
			if (const usercmd_t* basis = HCDEBuildUserCmdBasis(playerNum, sequence))
				memcpy(&command, basis, sizeof(command));
			else
				memset(&command, 0, sizeof(command));
			eventBytes = eventStart;
			return inputCursor == dataSize;
		}

		uint8_t canonicalScratch[MAX_MSGLEN];
		size_t canonicalCursor = 0u;
		if (!HCDEIsAllowedTicEventType(type)
			|| !HCDEAppendCanonicalEventPayload(type, canonicalScratch, sizeof(canonicalScratch), canonicalCursor, data, dataSize, inputCursor))
		{
			return false;
		}
		sawEventCommand = true;
	}

	// Some transition tics may contain only event commands and no explicit
	// DEM_USERCMD/DEM_EMPTYUSERCMD marker. Preserve the events and synthesize
	// an empty command from the rolling basis so map/teleport transitions do not
	// get rejected as malformed.
	if (sawEventCommand)
	{
		if (const usercmd_t* basis = HCDEBuildUserCmdBasis(playerNum, sequence))
			memcpy(&command, basis, sizeof(command));
		else
			memset(&command, 0, sizeof(command));
		eventBytes = dataSize;
		return true;
	}

	return false;
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

static bool HCDEAppendEventRecords(uint8_t* output, size_t outputCapacity, size_t& cursor, const uint8_t* data, size_t dataSize)
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
	case DEM_ADDSLOTDEFAULT:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 1u)
			&& HCDEAppendLegacyWeaponIndex(output, outputCapacity, outputCursor, data, dataSize, inputCursor);
		break;

	case DEM_SETPITCHLIMIT:
		ok = HCDEAppendFieldBytes(output, outputCapacity, outputCursor, data, dataSize, inputCursor, 2u);
		break;

	default:
		ok = false;
		break;
	}

	return ok && inputCursor == dataSize;
}

static bool BuildHCDEServerSnapshotPayload(int client, const uint8_t* legacyPacket, size_t legacySize, uint8_t* output, size_t outputCapacity, size_t& outputSize)
{
	outputSize = 0u;
	const uint8_t disallowedControlFlags = NCMD_EXIT | NCMD_SETUP | NCMD_LEVELREADY | NCMD_LATENCY | NCMD_LATENCYACK | NCMD_COMPRESSED;
	if (legacySize < 13u || legacySize > MAX_MSGLEN)
		return RejectHCDEServerSnapshotBuild(client, "malformed-size", legacySize, 0u);

	const uint8_t controlFlags = legacyPacket[0];
	if (controlFlags & disallowedControlFlags)
	{
		DebugTrace::Markf("net", "refused legacy server snapshot for client=%d control flags=%u",
			client, unsigned(controlFlags & disallowedControlFlags));
		return false;
	}

	size_t cursor = 10u;
	size_t quitterBytes = 0u;
	if (controlFlags & NCMD_QUITTERS)
	{
		if (cursor >= legacySize)
			return RejectHCDEServerSnapshotBuild(client, "missing-quitter-count", legacySize, cursor);
		quitterBytes = size_t(legacyPacket[cursor]) + 1u;
		if (quitterBytes > legacySize - cursor)
			return RejectHCDEServerSnapshotBuild(client, "truncated-quitters", legacySize, cursor);
		cursor += quitterBytes;
	}

	if (cursor > legacySize || legacySize - cursor < 3u)
		return RejectHCDEServerSnapshotBuild(client, "missing-snapshot-counts", legacySize, cursor);

	const uint8_t playerCount = legacyPacket[cursor++];
	const uint8_t commandTics = legacyPacket[cursor++];
	uint32_t baseSequence = 0u;
	if (commandTics > 0u)
	{
		if (commandTics > MAXSENDTICS || cursor > legacySize || legacySize - cursor < 4u)
			return RejectHCDEServerSnapshotBuild(client, "invalid-command-tics", legacySize, cursor);
		baseSequence = HCDELiveReadBE32(&legacyPacket[cursor]);
		cursor += 4u;
	}

	if (cursor >= legacySize)
		return RejectHCDEServerSnapshotBuild(client, "missing-consistency-count", legacySize, cursor);

	const uint8_t consistencyTics = legacyPacket[cursor++];
	uint32_t baseConsistency = 0u;
	if (consistencyTics > 0u)
	{
		if (consistencyTics > MAXSENDTICS || cursor > legacySize || legacySize - cursor < 4u)
			return RejectHCDEServerSnapshotBuild(client, "invalid-consistency-tics", legacySize, cursor);
		baseConsistency = HCDELiveReadBE32(&legacyPacket[cursor]);
		cursor += 4u;
	}

	if (cursor >= legacySize)
		return RejectHCDEServerSnapshotBuild(client, "missing-stability-byte", legacySize, cursor);

	const uint8_t stabilityBuffer = legacyPacket[cursor++];
	const size_t rawSnapshotBytes = legacySize - cursor;
	if (playerCount > MAXPLAYERS)
		return RejectHCDEServerSnapshotBuild(client, "too-many-players", legacySize, cursor);
	if (quitterBytes > 0xffffu)
		return RejectHCDEServerSnapshotBuild(client, "too-many-quitter-bytes", legacySize, cursor);
	if ((controlFlags & NCMD_QUITTERS) == 0u && quitterBytes != 0u)
		return RejectHCDEServerSnapshotBuild(client, "unexpected-quitter-bytes", legacySize, cursor);
	if ((controlFlags & NCMD_QUITTERS) != 0u && quitterBytes == 0u)
		return RejectHCDEServerSnapshotBuild(client, "missing-quitter-bytes", legacySize, cursor);
	if (playerCount == 0u && (commandTics != 0u || consistencyTics != 0u || rawSnapshotBytes != 0u))
		return RejectHCDEServerSnapshotBuild(client, "invalid-empty-snapshot", legacySize, cursor);
	if (HCDEServerSnapshotHeaderSize + quitterBytes > outputCapacity)
		return RejectHCDEServerSnapshotBuild(client, "output-overflow", legacySize, cursor);

	size_t recordCursor = cursor;
	size_t bodyCursor = HCDEServerSnapshotHeaderSize + quitterBytes;
	uint64_t playersSeen = 0u;
	uint64_t worldDeltaPlayersSeen = 0u;
	uint8_t worldDeltaPlayers[MAXPLAYERS] = {};
	size_t worldDeltaPlayerCount = 0u;
	auto addWorldDeltaPlayer = [&](uint8_t playerNum) -> bool
	{
		if (playerNum >= MAXPLAYERS || playerNum >= 64u)
			return false;
		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if ((worldDeltaPlayersSeen & playerMask) != 0u)
			return true;
		if (worldDeltaPlayerCount >= MAXPLAYERS)
			return false;
		worldDeltaPlayersSeen |= playerMask;
		worldDeltaPlayers[worldDeltaPlayerCount++] = playerNum;
		return true;
	};
	if (!HCDEAppendBytes(output, outputCapacity, bodyCursor, HCDEServerSnapshotRecordsMagic, sizeof(HCDEServerSnapshotRecordsMagic))
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, HCDEServerSnapshotRecordsProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, playerCount))
	{
		return RejectHCDEServerSnapshotBuild(client, "output-overflow", legacySize, recordCursor);
	}

	for (uint8_t p = 0u; p < playerCount; ++p)
	{
		if (recordCursor + 3u > legacySize)
			return RejectHCDEServerSnapshotBuild(client, "missing-player-record", legacySize, recordCursor);

		const uint8_t playerNum = legacyPacket[recordCursor++];
		if (playerNum >= MAXPLAYERS || playerNum >= 64u)
			return RejectHCDEServerSnapshotBuild(client, "invalid-player-record", legacySize, recordCursor);

		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if (playersSeen & playerMask)
			return RejectHCDEServerSnapshotBuild(client, "duplicate-player-record", legacySize, recordCursor);
		playersSeen |= playerMask;
		if (!addWorldDeltaPlayer(playerNum))
			return RejectHCDEServerSnapshotBuild(client, "too-many-world-delta-players", legacySize, recordCursor);

		const uint16_t latency = HCDELiveReadBE16(&legacyPacket[recordCursor]);
		recordCursor += 2u;
		if (!HCDEAppendByte(output, outputCapacity, bodyCursor, playerNum)
			|| !HCDEAppendBE16(output, outputCapacity, bodyCursor, latency)
			|| !HCDEAppendByte(output, outputCapacity, bodyCursor, consistencyTics)
			|| !HCDEAppendByte(output, outputCapacity, bodyCursor, commandTics))
		{
			return RejectHCDEServerSnapshotBuild(client, "output-overflow", legacySize, recordCursor);
		}

		uint64_t consistencyOffsetsSeen = 0u;
		for (uint8_t r = 0u; r < consistencyTics; ++r)
		{
			if (recordCursor + 3u > legacySize)
				return RejectHCDEServerSnapshotBuild(client, "truncated-consistency-record", legacySize, recordCursor);

			const uint8_t consistencyOffset = legacyPacket[recordCursor];
			if (consistencyOffset >= consistencyTics)
				return RejectHCDEServerSnapshotBuild(client, "invalid-consistency-offset", legacySize, recordCursor);
			const uint64_t consistencyMask = uint64_t(1u) << consistencyOffset;
			if (consistencyOffsetsSeen & consistencyMask)
				return RejectHCDEServerSnapshotBuild(client, "duplicate-consistency-offset", legacySize, recordCursor);
			consistencyOffsetsSeen |= consistencyMask;

			if (!HCDEAppendBytes(output, outputCapacity, bodyCursor, &legacyPacket[recordCursor], 3u))
				return RejectHCDEServerSnapshotBuild(client, "output-overflow", legacySize, recordCursor);
			recordCursor += 3u;
		}

		uint64_t commandOffsetsSeen = 0u;
		for (uint8_t t = 0u; t < commandTics; ++t)
		{
			if (recordCursor >= legacySize)
				return RejectHCDEServerSnapshotBuild(client, "truncated-command-record", legacySize, recordCursor);

			const uint8_t commandOffset = legacyPacket[recordCursor++];
			if (commandOffset >= commandTics)
				return RejectHCDEServerSnapshotBuild(client, "invalid-command-offset", legacySize, recordCursor);
			const uint64_t commandMask = uint64_t(1u) << commandOffset;
			if (commandOffsetsSeen & commandMask)
				return RejectHCDEServerSnapshotBuild(client, "duplicate-command-offset", legacySize, recordCursor);
			commandOffsetsSeen |= commandMask;

			size_t commandPayloadBytes = 0u;
			if (!Net_TrySkipUserCmdMessageWithBoundary(&legacyPacket[recordCursor], legacySize - recordCursor,
				t + 1u == commandTics, commandTics, commandOffsetsSeen, commandTics - t - 1u, commandPayloadBytes))
			{
				return RejectHCDEServerSnapshotBuild(client, "invalid-command-record", legacySize, recordCursor);
			}
			if (commandPayloadBytes == 0u)
				return RejectHCDEServerSnapshotBuild(client, "empty-command-record", legacySize, recordCursor);

			size_t eventBytes = 0u;
			usercmd_t command = {};
			const uint32_t commandSequence = baseSequence + commandOffset;
			if (!HCDEDecodeLegacyUserCmdRecord(playerNum, commandSequence, &legacyPacket[recordCursor], commandPayloadBytes, eventBytes, command))
				return RejectHCDEServerSnapshotBuild(client, "invalid-command-record", legacySize, recordCursor);
			if (eventBytes > 0xffffu)
				return RejectHCDEServerSnapshotBuild(client, "too-many-event-bytes", legacySize, recordCursor);

			if (!HCDEAppendByte(output, outputCapacity, bodyCursor, commandOffset))
				return RejectHCDEServerSnapshotBuild(client, "output-overflow", legacySize, recordCursor);
			if (!HCDEAppendEventRecords(output, outputCapacity, bodyCursor, &legacyPacket[recordCursor], eventBytes))
				return RejectHCDEServerSnapshotBuild(client, "invalid-event-records", legacySize, recordCursor);
			if (!HCDEAppendUserCmdFields(output, outputCapacity, bodyCursor, command))
				return RejectHCDEServerSnapshotBuild(client, "output-overflow", legacySize, recordCursor);
			recordCursor += commandPayloadBytes;
		}
	}

	for (uint8_t playerNum = 0u; playerNum < MAXPLAYERS; ++playerNum)
	{
		if (!playeringame[playerNum] || I_IsServerReservedSlot(playerNum) || players[playerNum].mo == nullptr)
			continue;
		if (!addWorldDeltaPlayer(playerNum))
			return RejectHCDEServerSnapshotBuild(client, "too-many-world-delta-players", legacySize, recordCursor);
	}

	if (recordCursor != legacySize)
		return RejectHCDEServerSnapshotBuild(client, "trailing-snapshot-bytes", legacySize, recordCursor);

	if (!HCDEAppendServerWorldDeltas(output, outputCapacity, bodyCursor, worldDeltaPlayers, worldDeltaPlayerCount))
		return RejectHCDEServerSnapshotBuild(client, "world-delta-overflow", legacySize, recordCursor);
	if (!HCDEAppendInvasionSnapshot(client, output, outputCapacity, bodyCursor))
		return RejectHCDEServerSnapshotBuild(client, "invasion-overflow", legacySize, recordCursor);

	const size_t bodyBytes = bodyCursor - HCDEServerSnapshotHeaderSize - quitterBytes;
	if (bodyBytes > 0xffffu)
		return RejectHCDEServerSnapshotBuild(client, "too-many-record-bytes", legacySize, recordCursor);

	// Explicit bounds validation before accessing offsets
	if (legacySize < 2u)
		return RejectHCDEServerSnapshotBuild(client, "insufficient-offset-bounds", legacySize, cursor);
	if (legacySize < 6u)
		return RejectHCDEServerSnapshotBuild(client, "insufficient-offset-bounds", legacySize, cursor);
	if (legacySize < 10u + quitterBytes)
		return RejectHCDEServerSnapshotBuild(client, "insufficient-quitter-bytes", legacySize, cursor);

	memcpy(&output[HCDEServerSnapshotMagicOffset], HCDEServerSnapshotMagic, sizeof(HCDEServerSnapshotMagic));
	output[HCDEServerSnapshotVersionOffset] = HCDEServerSnapshotProtocolVersion;
	output[HCDEServerSnapshotFlagsOffset] = controlFlags;
	output[HCDEServerSnapshotRoutingOffset] = legacyPacket[1];
	output[HCDEServerSnapshotPlayerCountOffset] = playerCount;
	memcpy(&output[HCDEServerSnapshotSequenceAckOffset], &legacyPacket[2], 4u);
	memcpy(&output[HCDEServerSnapshotConsistencyAckOffset], &legacyPacket[6], 4u);
	HCDELiveWriteBE16(&output[HCDEServerSnapshotQuitterBytesOffset], uint16_t(quitterBytes));
	HCDELiveWriteBE32(&output[HCDEServerSnapshotBaseSequenceOffset], baseSequence);
	HCDELiveWriteBE32(&output[HCDEServerSnapshotBaseConsistencyOffset], baseConsistency);
	output[HCDEServerSnapshotCommandTicsOffset] = commandTics;
	output[HCDEServerSnapshotConsistencyTicsOffset] = consistencyTics;
	output[HCDEServerSnapshotStabilityOffset] = stabilityBuffer;
	HCDELiveWriteBE16(&output[HCDEServerSnapshotBodyBytesOffset], uint16_t(bodyBytes));
	if (quitterBytes > 0u)
		memcpy(&output[HCDEServerSnapshotHeaderSize], &legacyPacket[10], quitterBytes);
	outputSize = bodyCursor;

	DebugTrace::Markf("net", "HCDE server snapshot payload build client=%d players=%u tics=%u consistencies=%u quitters=%zu records=%zu deltas=%zu raw=%zu",
		client, unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), quitterBytes, bodyBytes, worldDeltaPlayerCount, rawSnapshotBytes);
	return true;
}

static bool RejectHCDEClientInputBuild(int client, const char* reason, size_t legacySize, size_t cursor)
{
	DebugTrace::Markf("net", "refused legacy client input for client=%d reason=%s size=%zu cursor=%zu",
		client, reason != nullptr ? reason : "unknown", legacySize, cursor);
	return false;
}

static bool BuildHCDEClientInputPayload(int client, const uint8_t* legacyPacket, size_t legacySize, uint8_t* output, size_t outputCapacity, size_t& outputSize)
{
	outputSize = 0u;
	const uint8_t disallowedControlFlags = NCMD_EXIT | NCMD_SETUP | NCMD_LEVELREADY | NCMD_QUITTERS | NCMD_LATENCY | NCMD_LATENCYACK | NCMD_COMPRESSED;
	if (legacySize < 13u || legacySize > MAX_MSGLEN)
		return RejectHCDEClientInputBuild(client, "malformed-size", legacySize, 0u);

	const uint8_t controlFlags = legacyPacket[0];
	if (controlFlags & disallowedControlFlags)
	{
		DebugTrace::Markf("net", "refused legacy client input for client=%d control flags=%u",
			client, unsigned(controlFlags & disallowedControlFlags));
		return false;
	}

	size_t cursor = 10u;
	const uint8_t playerCount = legacyPacket[cursor++];
	const uint8_t commandTics = legacyPacket[cursor++];
	uint32_t baseSequence = 0u;
	if (commandTics > 0u)
	{
		if (commandTics > MAXSENDTICS || cursor > legacySize || legacySize - cursor < 4u)
			return RejectHCDEClientInputBuild(client, "invalid-command-tics", legacySize, cursor);
		baseSequence = HCDELiveReadBE32(&legacyPacket[cursor]);
		cursor += 4u;
	}

	if (cursor >= legacySize)
		return RejectHCDEClientInputBuild(client, "missing-consistency-count", legacySize, cursor);

	const uint8_t consistencyTics = legacyPacket[cursor++];
	uint32_t baseConsistency = 0u;
	if (consistencyTics > 0u)
	{
		if (consistencyTics > MAXSENDTICS || cursor > legacySize || legacySize - cursor < 4u)
			return RejectHCDEClientInputBuild(client, "invalid-consistency-tics", legacySize, cursor);
		baseConsistency = HCDELiveReadBE32(&legacyPacket[cursor]);
		cursor += 4u;
	}

	if (cursor >= legacySize)
		return RejectHCDEClientInputBuild(client, "missing-stability-byte", legacySize, cursor);

	const uint8_t stabilityBuffer = legacyPacket[cursor++];
	const size_t rawCommandBytes = legacySize - cursor;
	if (playerCount > MAXPLAYERS)
		return RejectHCDEClientInputBuild(client, "too-many-players", legacySize, cursor);
	if (playerCount == 0u && (commandTics != 0u || consistencyTics != 0u || rawCommandBytes != 0u))
		return RejectHCDEClientInputBuild(client, "invalid-empty-heartbeat", legacySize, cursor);
	if (HCDEClientInputHeaderSize > outputCapacity)
		return RejectHCDEClientInputBuild(client, "output-overflow", legacySize, cursor);

	size_t recordCursor = cursor;
	size_t bodyCursor = HCDEClientInputHeaderSize;
	uint64_t playersSeen = 0u;
	if (!HCDEAppendBytes(output, outputCapacity, bodyCursor, HCDEClientInputRecordsMagic, sizeof(HCDEClientInputRecordsMagic))
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, HCDEClientInputRecordsProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, bodyCursor, playerCount))
	{
		return RejectHCDEClientInputBuild(client, "output-overflow", legacySize, recordCursor);
	}

	for (uint8_t p = 0u; p < playerCount; ++p)
	{
		if (recordCursor >= legacySize)
			return RejectHCDEClientInputBuild(client, "missing-player-record", legacySize, recordCursor);

		const uint8_t playerNum = legacyPacket[recordCursor++];
		if (playerNum >= MAXPLAYERS || playerNum >= 64u)
			return RejectHCDEClientInputBuild(client, "invalid-player-record", legacySize, recordCursor);

		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if (playersSeen & playerMask)
			return RejectHCDEClientInputBuild(client, "duplicate-player-record", legacySize, recordCursor);
		playersSeen |= playerMask;

		if (!HCDEAppendByte(output, outputCapacity, bodyCursor, playerNum)
			|| !HCDEAppendByte(output, outputCapacity, bodyCursor, consistencyTics)
			|| !HCDEAppendByte(output, outputCapacity, bodyCursor, commandTics))
		{
			return RejectHCDEClientInputBuild(client, "output-overflow", legacySize, recordCursor);
		}

		uint64_t consistencyOffsetsSeen = 0u;
		for (uint8_t r = 0u; r < consistencyTics; ++r)
		{
			if (recordCursor + 3u > legacySize)
				return RejectHCDEClientInputBuild(client, "truncated-consistency-record", legacySize, recordCursor);

			const uint8_t consistencyOffset = legacyPacket[recordCursor];
			if (consistencyOffset >= consistencyTics)
				return RejectHCDEClientInputBuild(client, "invalid-consistency-offset", legacySize, recordCursor);
			const uint64_t consistencyMask = uint64_t(1u) << consistencyOffset;
			if (consistencyOffsetsSeen & consistencyMask)
				return RejectHCDEClientInputBuild(client, "duplicate-consistency-offset", legacySize, recordCursor);
			consistencyOffsetsSeen |= consistencyMask;

			if (!HCDEAppendBytes(output, outputCapacity, bodyCursor, &legacyPacket[recordCursor], 3u))
				return RejectHCDEClientInputBuild(client, "output-overflow", legacySize, recordCursor);
			recordCursor += 3u;
		}

		uint64_t commandOffsetsSeen = 0u;
		for (uint8_t t = 0u; t < commandTics; ++t)
		{
			if (recordCursor >= legacySize)
				return RejectHCDEClientInputBuild(client, "truncated-command-record", legacySize, recordCursor);

			const uint8_t commandOffset = legacyPacket[recordCursor++];
			if (commandOffset >= commandTics)
				return RejectHCDEClientInputBuild(client, "invalid-command-offset", legacySize, recordCursor);
			const uint64_t commandMask = uint64_t(1u) << commandOffset;
			if (commandOffsetsSeen & commandMask)
				return RejectHCDEClientInputBuild(client, "duplicate-command-offset", legacySize, recordCursor);
			commandOffsetsSeen |= commandMask;

			const uint8_t* commandStart = &legacyPacket[recordCursor];
			size_t commandPayloadBytes = 0u;
			if (!Net_TrySkipUserCmdMessageWithBoundary(commandStart, legacySize - recordCursor,
				t + 1u == commandTics, commandTics, commandOffsetsSeen, commandTics - t - 1u, commandPayloadBytes))
			{
				return RejectHCDEClientInputBuild(client, "invalid-command-record", legacySize, recordCursor);
			}
			if (commandPayloadBytes == 0u)
				return RejectHCDEClientInputBuild(client, "empty-command-record", legacySize, recordCursor);

			size_t eventBytes = 0u;
			usercmd_t command = {};
			const uint32_t commandSequence = baseSequence + commandOffset;
			if (!HCDEDecodeLegacyUserCmdRecord(playerNum, commandSequence, commandStart, commandPayloadBytes, eventBytes, command))
				return RejectHCDEClientInputBuild(client, "invalid-command-record", legacySize, recordCursor);
			if (eventBytes > 0xffffu)
				return RejectHCDEClientInputBuild(client, "too-many-event-bytes", legacySize, recordCursor);

			if (!HCDEAppendByte(output, outputCapacity, bodyCursor, commandOffset))
				return RejectHCDEClientInputBuild(client, "output-overflow", legacySize, recordCursor);
			if (!HCDEAppendEventRecords(output, outputCapacity, bodyCursor, commandStart, eventBytes))
				return RejectHCDEClientInputBuild(client, "invalid-event-records", legacySize, recordCursor);
			if (!HCDEAppendUserCmdFields(output, outputCapacity, bodyCursor, command))
				return RejectHCDEClientInputBuild(client, "output-overflow", legacySize, recordCursor);
			recordCursor += commandPayloadBytes;
		}
	}

	if (recordCursor != legacySize)
		return RejectHCDEClientInputBuild(client, "trailing-input-bytes", legacySize, recordCursor);

	const size_t bodyBytes = bodyCursor - HCDEClientInputHeaderSize;
	if (bodyBytes > 0xffffu)
		return RejectHCDEClientInputBuild(client, "too-many-record-bytes", legacySize, recordCursor);

	// Explicit bounds validation before accessing offsets
	if (legacySize < 2u)
		return RejectHCDEClientInputBuild(client, "insufficient-offset-bounds", legacySize, recordCursor);
	if (legacySize < 6u)
		return RejectHCDEClientInputBuild(client, "insufficient-offset-bounds", legacySize, recordCursor);

	memcpy(&output[HCDEClientInputMagicOffset], HCDEClientInputMagic, sizeof(HCDEClientInputMagic));
	output[HCDEClientInputVersionOffset] = HCDEClientInputProtocolVersion;
	output[HCDEClientInputFlagsOffset] = controlFlags;
	output[HCDEClientInputRoutingOffset] = legacyPacket[1];
	output[HCDEClientInputPlayerCountOffset] = playerCount;
	memcpy(&output[HCDEClientInputSequenceAckOffset], &legacyPacket[2], 4u);
	memcpy(&output[HCDEClientInputConsistencyAckOffset], &legacyPacket[6], 4u);
	HCDELiveWriteBE32(&output[HCDEClientInputBaseSequenceOffset], baseSequence);
	HCDELiveWriteBE32(&output[HCDEClientInputBaseConsistencyOffset], baseConsistency);
	output[HCDEClientInputCommandTicsOffset] = commandTics;
	output[HCDEClientInputConsistencyTicsOffset] = consistencyTics;
	output[HCDEClientInputStabilityOffset] = stabilityBuffer;
	HCDELiveWriteBE16(&output[HCDEClientInputBodyBytesOffset], uint16_t(bodyBytes));
	outputSize = HCDEClientInputHeaderSize + bodyBytes;

	DebugTrace::Markf("net", "HCDE client input payload build client=%d players=%u tics=%u consistencies=%u records=%zu raw=%zu",
		client, unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), bodyBytes, rawCommandBytes);
	return true;
}

static bool UnwrapHCDELiveClientInputPayload(int clientNum, size_t payloadSize)
{
	auto& peer = HCDELivePeers[clientNum];
	const uint8_t* payload = nullptr;
	size_t inputPayloadSize = 0u;
	uint32_t remoteGameTic = 0u;
	if (!UnwrapHCDEGameplayEnvelope(clientNum, payloadSize, "client input", HGP_CLIENT_INPUTS, payload, inputPayloadSize, remoteGameTic))
		return false;

	if (inputPayloadSize < HCDEClientInputHeaderSize
		|| memcmp(&payload[HCDEClientInputMagicOffset], HCDEClientInputMagic, sizeof(HCDEClientInputMagic)) != 0)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "malformed HCDE client input from client=%d size=%zu", clientNum, inputPayloadSize);
		return false;
	}

	const uint8_t inputVersion = payload[HCDEClientInputVersionOffset];
	const uint8_t controlFlags = payload[HCDEClientInputFlagsOffset];
	const uint8_t routingByte = payload[HCDEClientInputRoutingOffset];
	const uint8_t playerCount = payload[HCDEClientInputPlayerCountOffset];
	const uint32_t baseSequence = HCDELiveReadBE32(&payload[HCDEClientInputBaseSequenceOffset]);
	const uint32_t baseConsistency = HCDELiveReadBE32(&payload[HCDEClientInputBaseConsistencyOffset]);
	const uint8_t commandTics = payload[HCDEClientInputCommandTicsOffset];
	const uint8_t consistencyTics = payload[HCDEClientInputConsistencyTicsOffset];
	const uint8_t stabilityBuffer = payload[HCDEClientInputStabilityOffset];
	const size_t bodyBytes = HCDELiveReadBE16(&payload[HCDEClientInputBodyBytesOffset]);
	const uint8_t disallowedControlFlags = NCMD_EXIT | NCMD_SETUP | NCMD_LEVELREADY | NCMD_QUITTERS | NCMD_LATENCY | NCMD_LATENCYACK | NCMD_COMPRESSED;
	auto rejectClientInput = [&](const char* reason)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE client input from client=%d reason=%s version=%u flags=%u players=%u tics=%u consistencies=%u body=%zu size=%zu",
			clientNum, reason != nullptr ? reason : "unknown", unsigned(inputVersion), unsigned(controlFlags & disallowedControlFlags),
			unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), bodyBytes, inputPayloadSize);
		return false;
	};
	auto rejectClientAuthority = [&](int playerNum)
	{
		++peer.AuthorityRejected;
		DebugTrace::Markf("net", "ignored HCDE client input from client=%d reason=unauthorized-player-record player=%d players=%u authority-rejected=%u",
			clientNum, playerNum, unsigned(playerCount), peer.AuthorityRejected);
		return rejectClientInput("unauthorized-player-record");
	};

	if (inputVersion != HCDEClientInputProtocolVersion
		|| (controlFlags & disallowedControlFlags)
		|| playerCount > MAXPLAYERS
		|| commandTics > MAXSENDTICS
		|| consistencyTics > MAXSENDTICS
		|| (playerCount == 0u && (commandTics != 0u || consistencyTics != 0u))
		|| HCDEClientInputHeaderSize + bodyBytes != inputPayloadSize)
		return rejectClientInput("invalid-header");

	const uint8_t* body = &payload[HCDEClientInputHeaderSize];
	if (bodyBytes < HCDEClientInputRecordsHeaderSize
		|| memcmp(&body[HCDEClientInputRecordsMagicOffset], HCDEClientInputRecordsMagic, sizeof(HCDEClientInputRecordsMagic)) != 0)
		return rejectClientInput("missing-records");

	const uint8_t recordsVersion = body[HCDEClientInputRecordsVersionOffset];
	const uint8_t recordPlayerCount = body[HCDEClientInputRecordsPlayerCountOffset];
	if (recordsVersion != HCDEClientInputRecordsProtocolVersion || recordPlayerCount != playerCount)
		return rejectClientInput("invalid-record-header");

	NetBuffer[0] = controlFlags;
	NetBuffer[1] = routingByte;
	memcpy(&NetBuffer[2], &payload[HCDEClientInputSequenceAckOffset], 4u);
	memcpy(&NetBuffer[6], &payload[HCDEClientInputConsistencyAckOffset], 4u);

	size_t cursor = 10u;
	NetBuffer[cursor++] = playerCount;
	NetBuffer[cursor++] = commandTics;
	if (commandTics > 0u)
	{
		HCDELiveWriteBE32(&NetBuffer[cursor], baseSequence);
		cursor += 4u;
	}
	NetBuffer[cursor++] = consistencyTics;
	if (consistencyTics > 0u)
	{
		HCDELiveWriteBE32(&NetBuffer[cursor], baseConsistency);
		cursor += 4u;
	}
	NetBuffer[cursor++] = stabilityBuffer;

	size_t bodyCursor = HCDEClientInputRecordsHeaderSize;
	uint64_t playersSeen = 0u;
	for (uint8_t p = 0u; p < playerCount; ++p)
	{
		if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u)
			return rejectClientInput("truncated-player-record");

		const uint8_t playerNum = body[bodyCursor++];
		const uint8_t consistencyCount = body[bodyCursor++];
		const uint8_t commandCount = body[bodyCursor++];
		if (playerNum >= MAXPLAYERS || playerNum >= 64u || consistencyCount != consistencyTics || commandCount != commandTics)
			return rejectClientInput("invalid-player-record");

		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if (playersSeen & playerMask)
			return rejectClientInput("duplicate-player-record");
		playersSeen |= playerMask;
		if (!HCDEInputRecordAuthorized(clientNum, playerNum, playerCount))
			return rejectClientAuthority(playerNum);

		if (!HCDEAppendByte(NetBuffer, MAX_MSGLEN, cursor, playerNum))
			return rejectClientInput("packet-overflow");

		usercmd_t rollingCommandBasis = {};
		bool hasRollingCommandBasis = false;
		uint8_t expectedCommandOffset = 0u;
		if (const usercmd_t* basis = HCDEReceiveUserCmdBasis(playerNum, baseSequence))
		{
			rollingCommandBasis = *basis;
			hasRollingCommandBasis = true;
		}

		uint64_t consistencyOffsetsSeen = 0u;
		for (uint8_t r = 0u; r < consistencyCount; ++r)
		{
			if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u)
				return rejectClientInput("truncated-consistency-record");
			if (body[bodyCursor] >= consistencyTics)
				return rejectClientInput("invalid-consistency-offset");
			const uint64_t consistencyMask = uint64_t(1u) << body[bodyCursor];
			if (consistencyOffsetsSeen & consistencyMask)
				return rejectClientInput("duplicate-consistency-offset");
			consistencyOffsetsSeen |= consistencyMask;
			if (!HCDEAppendBytes(NetBuffer, MAX_MSGLEN, cursor, &body[bodyCursor], 3u))
				return rejectClientInput("packet-overflow");
			bodyCursor += 3u;
		}

		uint64_t commandOffsetsSeen = 0u;
		for (uint8_t t = 0u; t < commandCount; ++t)
		{
			if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u + HCDEExplicitUserCmdBytes)
				return rejectClientInput("truncated-command-record");

			const uint8_t commandOffset = body[bodyCursor++];
			const size_t eventCount = HCDELiveReadBE16(&body[bodyCursor]);
			bodyCursor += 2u;
			if (commandOffset >= commandTics)
				return rejectClientInput("invalid-command-record");
			const uint64_t commandMask = uint64_t(1u) << commandOffset;
			if (commandOffsetsSeen & commandMask)
				return rejectClientInput("duplicate-command-offset");
			commandOffsetsSeen |= commandMask;

			if (!HCDEAppendByte(NetBuffer, MAX_MSGLEN, cursor, commandOffset))
				return rejectClientInput("packet-overflow");

			for (size_t e = 0u; e < eventCount; ++e)
			{
				if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u + HCDEExplicitUserCmdBytes)
					return rejectClientInput("truncated-event-record");

				const uint8_t eventType = body[bodyCursor++];
				const size_t eventPayloadBytes = HCDELiveReadBE16(&body[bodyCursor]);
				bodyCursor += 2u;
				if (!HCDEIsAllowedClientInputEventType(eventType)
					|| bodyCursor > bodyBytes
					|| eventPayloadBytes > bodyBytes - bodyCursor
					|| bodyBytes - bodyCursor - eventPayloadBytes < HCDEExplicitUserCmdBytes)
				{
					return rejectClientInput("invalid-event-record");
				}

				if (!HCDEAppendByte(NetBuffer, MAX_MSGLEN, cursor, eventType))
					return rejectClientInput("packet-overflow");
				if (!HCDEAppendLegacyEventPayload(eventType, &body[bodyCursor], eventPayloadBytes, NetBuffer, MAX_MSGLEN, cursor))
					return rejectClientInput("invalid-event-payload");
				bodyCursor += eventPayloadBytes;
			}

			usercmd_t command = {};
			if (!HCDEReadUserCmdFields(body, bodyBytes, bodyCursor, command))
				return rejectClientInput("truncated-command-record");

			if (cursor > MAX_MSGLEN || MAX_MSGLEN - cursor < 18u)
				return rejectClientInput("packet-overflow");
			uint8_t* commandOutputStart = &NetBuffer[cursor];
			TArrayView<uint8_t> commandOutput = TArrayView(commandOutputStart, MAX_MSGLEN - cursor);
			const usercmd_t* basis = (hasRollingCommandBasis && commandOffset == expectedCommandOffset)
				? &rollingCommandBasis
				: HCDEReceiveUserCmdBasis(playerNum, baseSequence + commandOffset);
			WriteUserCmdMessage(command, basis, commandOutput);
			// The legacy reader applies commands sequentially and uses the command it
			// just read as the next basis. Mirror that here so zero yaw/pitch deltas
			// after a nonzero turn are encoded as explicit clears, not stale repeats.
			rollingCommandBasis = command;
			hasRollingCommandBasis = true;
			expectedCommandOffset = commandOffset + 1u;
			cursor += size_t(commandOutput.Data() - commandOutputStart);
		}
	}

	if (bodyCursor != bodyBytes)
		return rejectClientInput("trailing-record-bytes");

	NetBufferLength = cursor;

	const size_t expectedSize = GetNetBufferSize();
	if (NetBufferLength != expectedSize)
	{
		++peer.UnsupportedReceived;
		DPrintf(DMSG_WARNING, "Malformed HCDE client input from client %d: size %zu (expected %zu)\n",
			clientNum, NetBufferLength, expectedSize);
		return false;
	}

	DebugTrace::Markf("net", "HCDE client input recv client=%d room=%u tic=%u players=%u tics=%u consistencies=%u records=%zu",
		clientNum, unsigned(CurrentRoomID), remoteGameTic, unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), bodyBytes);
	return true;
}

static bool UnwrapHCDELiveServerSnapshotPayload(int clientNum, size_t payloadSize)
{
	auto& peer = HCDELivePeers[clientNum];
	const uint8_t* payload = nullptr;
	size_t snapshotPayloadSize = 0u;
	uint32_t remoteGameTic = 0u;
	if (!UnwrapHCDEGameplayEnvelope(clientNum, payloadSize, "server snapshot", HGP_SERVER_SNAPSHOT, payload, snapshotPayloadSize, remoteGameTic))
		return false;

	if (snapshotPayloadSize < HCDEServerSnapshotHeaderSize
		|| memcmp(&payload[HCDEServerSnapshotMagicOffset], HCDEServerSnapshotMagic, sizeof(HCDEServerSnapshotMagic)) != 0)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "malformed HCDE server snapshot from client=%d size=%zu", clientNum, snapshotPayloadSize);
		return false;
	}

	const uint8_t snapshotVersion = payload[HCDEServerSnapshotVersionOffset];
	const uint8_t controlFlags = payload[HCDEServerSnapshotFlagsOffset];
	const uint8_t routingByte = payload[HCDEServerSnapshotRoutingOffset];
	const uint8_t playerCount = payload[HCDEServerSnapshotPlayerCountOffset];
	const size_t quitterBytes = HCDELiveReadBE16(&payload[HCDEServerSnapshotQuitterBytesOffset]);
	const uint32_t baseSequence = HCDELiveReadBE32(&payload[HCDEServerSnapshotBaseSequenceOffset]);
	const uint32_t baseConsistency = HCDELiveReadBE32(&payload[HCDEServerSnapshotBaseConsistencyOffset]);
	const uint8_t commandTics = payload[HCDEServerSnapshotCommandTicsOffset];
	const uint8_t consistencyTics = payload[HCDEServerSnapshotConsistencyTicsOffset];
	const uint8_t stabilityBuffer = payload[HCDEServerSnapshotStabilityOffset];
	const size_t bodyBytes = HCDELiveReadBE16(&payload[HCDEServerSnapshotBodyBytesOffset]);
	const uint8_t disallowedControlFlags = NCMD_EXIT | NCMD_SETUP | NCMD_LEVELREADY | NCMD_LATENCY | NCMD_LATENCYACK | NCMD_COMPRESSED;
	auto rejectServerSnapshot = [&](const char* reason)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE server snapshot from client=%d reason=%s version=%u flags=%u players=%u tics=%u consistencies=%u quitters=%zu body=%zu size=%zu",
			clientNum, reason != nullptr ? reason : "unknown", unsigned(snapshotVersion), unsigned(controlFlags & disallowedControlFlags),
			unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), quitterBytes, bodyBytes, snapshotPayloadSize);
		if (!I_IsLocalHCDEServiceAuthority()
			&& HCDELiveReportIntervalElapsed(LastHCDELiveSnapshotRejectReportMS, 2000u))
		{
			Printf(PRINT_HIGH,
				"HCDE net snapshot rejected: reason=%s from=%d snapshot-seq=%u any-seq=%u ack=%u players=%u tics=%u body=%zu size=%zu gametic=%d clienttic=%d unsupported=%u\n",
				reason != nullptr ? reason : "unknown", clientNum,
				peer.RxServerSnapshotSequence, peer.RxSequence, peer.PeerAck,
				unsigned(playerCount), unsigned(commandTics), bodyBytes, snapshotPayloadSize,
				gametic, ClientTic, peer.UnsupportedReceived);
		}
		return false;
	};

	const bool payloadLengthsFit = snapshotPayloadSize >= HCDEServerSnapshotHeaderSize
		&& quitterBytes <= snapshotPayloadSize - HCDEServerSnapshotHeaderSize
		&& bodyBytes == snapshotPayloadSize - HCDEServerSnapshotHeaderSize - quitterBytes;
	const bool quitterLengthMatches = quitterBytes == 0u
		|| (HCDEServerSnapshotHeaderSize < snapshotPayloadSize && size_t(payload[HCDEServerSnapshotHeaderSize]) + 1u == quitterBytes);

	if (snapshotVersion != HCDEServerSnapshotProtocolVersion
		|| (controlFlags & disallowedControlFlags)
		|| playerCount > MAXPLAYERS
		|| commandTics > MAXSENDTICS
		|| consistencyTics > MAXSENDTICS
		|| ((controlFlags & NCMD_QUITTERS) == 0u && quitterBytes != 0u)
		|| ((controlFlags & NCMD_QUITTERS) != 0u && quitterBytes == 0u)
		|| (playerCount == 0u && (commandTics != 0u || consistencyTics != 0u))
		|| !payloadLengthsFit
		|| !quitterLengthMatches)
		return rejectServerSnapshot("invalid-header");

	const uint8_t* body = &payload[HCDEServerSnapshotHeaderSize + quitterBytes];
	if (bodyBytes < HCDEServerSnapshotRecordsHeaderSize
		|| memcmp(&body[HCDEServerSnapshotRecordsMagicOffset], HCDEServerSnapshotRecordsMagic, sizeof(HCDEServerSnapshotRecordsMagic)) != 0)
		return rejectServerSnapshot("missing-records");

	const uint8_t recordsVersion = body[HCDEServerSnapshotRecordsVersionOffset];
	const uint8_t recordPlayerCount = body[HCDEServerSnapshotRecordsPlayerCountOffset];
	if (recordsVersion != HCDEServerSnapshotRecordsProtocolVersion || recordPlayerCount != playerCount)
		return rejectServerSnapshot("invalid-record-header");

	NetBuffer[0] = controlFlags;
	NetBuffer[1] = routingByte;
	memcpy(&NetBuffer[2], &payload[HCDEServerSnapshotSequenceAckOffset], 4u);
	memcpy(&NetBuffer[6], &payload[HCDEServerSnapshotConsistencyAckOffset], 4u);

	size_t cursor = 10u;
	if (quitterBytes > 0u)
	{
		memmove(&NetBuffer[cursor], &payload[HCDEServerSnapshotHeaderSize], quitterBytes);
		cursor += quitterBytes;
	}
	NetBuffer[cursor++] = playerCount;
	NetBuffer[cursor++] = commandTics;
	if (commandTics > 0u)
	{
		HCDELiveWriteBE32(&NetBuffer[cursor], baseSequence);
		cursor += 4u;
	}
	NetBuffer[cursor++] = consistencyTics;
	if (consistencyTics > 0u)
	{
		HCDELiveWriteBE32(&NetBuffer[cursor], baseConsistency);
		cursor += 4u;
	}
	NetBuffer[cursor++] = stabilityBuffer;

	size_t bodyCursor = HCDEServerSnapshotRecordsHeaderSize;
	uint64_t playersSeen = 0u;
	for (uint8_t p = 0u; p < playerCount; ++p)
	{
		if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 5u)
			return rejectServerSnapshot("truncated-player-record");

		const uint8_t playerNum = body[bodyCursor++];
		const uint16_t latency = HCDELiveReadBE16(&body[bodyCursor]);
		bodyCursor += 2u;
		const uint8_t consistencyCount = body[bodyCursor++];
		const uint8_t commandCount = body[bodyCursor++];
		if (playerNum >= MAXPLAYERS || playerNum >= 64u || consistencyCount != consistencyTics || commandCount != commandTics)
			return rejectServerSnapshot("invalid-player-record");

		const uint64_t playerMask = uint64_t(1u) << playerNum;
		if (playersSeen & playerMask)
			return rejectServerSnapshot("duplicate-player-record");
		playersSeen |= playerMask;

		if (!HCDEAppendByte(NetBuffer, MAX_MSGLEN, cursor, playerNum)
			|| !HCDEAppendBE16(NetBuffer, MAX_MSGLEN, cursor, latency))
		{
			return rejectServerSnapshot("packet-overflow");
		}

		usercmd_t rollingCommandBasis = {};
		bool hasRollingCommandBasis = false;
		uint8_t expectedCommandOffset = 0u;
		if (const usercmd_t* basis = HCDEReceiveUserCmdBasis(playerNum, baseSequence))
		{
			rollingCommandBasis = *basis;
			hasRollingCommandBasis = true;
		}

		uint64_t consistencyOffsetsSeen = 0u;
		for (uint8_t r = 0u; r < consistencyCount; ++r)
		{
			if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u)
				return rejectServerSnapshot("truncated-consistency-record");
			if (body[bodyCursor] >= consistencyTics)
				return rejectServerSnapshot("invalid-consistency-offset");
			const uint64_t consistencyMask = uint64_t(1u) << body[bodyCursor];
			if (consistencyOffsetsSeen & consistencyMask)
				return rejectServerSnapshot("duplicate-consistency-offset");
			consistencyOffsetsSeen |= consistencyMask;
			if (!HCDEAppendBytes(NetBuffer, MAX_MSGLEN, cursor, &body[bodyCursor], 3u))
				return rejectServerSnapshot("packet-overflow");
			bodyCursor += 3u;
		}

		uint64_t commandOffsetsSeen = 0u;
		for (uint8_t t = 0u; t < commandCount; ++t)
		{
			if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u + HCDEExplicitUserCmdBytes)
				return rejectServerSnapshot("truncated-command-record");

			const uint8_t commandOffset = body[bodyCursor++];
			const size_t eventCount = HCDELiveReadBE16(&body[bodyCursor]);
			bodyCursor += 2u;
			if (commandOffset >= commandTics)
				return rejectServerSnapshot("invalid-command-record");
			const uint64_t commandMask = uint64_t(1u) << commandOffset;
			if (commandOffsetsSeen & commandMask)
				return rejectServerSnapshot("duplicate-command-offset");
			commandOffsetsSeen |= commandMask;

			if (!HCDEAppendByte(NetBuffer, MAX_MSGLEN, cursor, commandOffset))
				return rejectServerSnapshot("packet-overflow");

			for (size_t e = 0u; e < eventCount; ++e)
			{
				if (bodyCursor > bodyBytes || bodyBytes - bodyCursor < 3u + HCDEExplicitUserCmdBytes)
					return rejectServerSnapshot("truncated-event-record");

				const uint8_t eventType = body[bodyCursor++];
				const size_t eventPayloadBytes = HCDELiveReadBE16(&body[bodyCursor]);
				bodyCursor += 2u;
				if (!HCDEIsAllowedTicEventType(eventType)
					|| bodyCursor > bodyBytes
					|| eventPayloadBytes > bodyBytes - bodyCursor
					|| bodyBytes - bodyCursor - eventPayloadBytes < HCDEExplicitUserCmdBytes)
				{
					return rejectServerSnapshot("invalid-event-record");
				}

				if (!HCDEAppendByte(NetBuffer, MAX_MSGLEN, cursor, eventType))
					return rejectServerSnapshot("packet-overflow");
				if (!HCDEAppendLegacyEventPayload(eventType, &body[bodyCursor], eventPayloadBytes, NetBuffer, MAX_MSGLEN, cursor))
					return rejectServerSnapshot("invalid-event-payload");
				bodyCursor += eventPayloadBytes;
			}

			usercmd_t command = {};
			if (!HCDEReadUserCmdFields(body, bodyBytes, bodyCursor, command))
				return rejectServerSnapshot("truncated-command-record");

			if (cursor > MAX_MSGLEN || MAX_MSGLEN - cursor < 18u)
				return rejectServerSnapshot("packet-overflow");
			uint8_t* commandOutputStart = &NetBuffer[cursor];
			TArrayView<uint8_t> commandOutput = TArrayView(commandOutputStart, MAX_MSGLEN - cursor);
			const usercmd_t* basis = (hasRollingCommandBasis && commandOffset == expectedCommandOffset)
				? &rollingCommandBasis
				: HCDEReceiveUserCmdBasis(playerNum, baseSequence + commandOffset);
			WriteUserCmdMessage(command, basis, commandOutput);
			// Keep the re-packed legacy stream's basis in step with the parser that
			// consumes it immediately after unwrapping.
			rollingCommandBasis = command;
			hasRollingCommandBasis = true;
			expectedCommandOffset = commandOffset + 1u;
			cursor += size_t(commandOutput.Data() - commandOutputStart);
		}
	}

	if (!HCDEValidateServerWorldDeltas(clientNum, body, bodyBytes, bodyCursor, playerCount, playersSeen))
		return rejectServerSnapshot("invalid-world-deltas");
	if (bodyCursor < bodyBytes && !HCDEApplyInvasionSnapshot(clientNum, body, bodyBytes, bodyCursor))
		return rejectServerSnapshot("invalid-invasion-snapshot");

	if (bodyCursor != bodyBytes)
		return rejectServerSnapshot("trailing-record-bytes");

	NetBufferLength = cursor;

	const size_t expectedSize = GetNetBufferSize();
	if (NetBufferLength != expectedSize)
	{
		++peer.UnsupportedReceived;
		DPrintf(DMSG_WARNING, "Malformed HCDE server snapshot from client %d: size %zu (expected %zu)\n",
			clientNum, NetBufferLength, expectedSize);
		return false;
	}

	DebugTrace::Markf("net", "HCDE server snapshot recv client=%d room=%u tic=%u players=%u tics=%u consistencies=%u quitters=%zu records=%zu",
		clientNum, unsigned(CurrentRoomID), remoteGameTic, unsigned(playerCount), unsigned(commandTics), unsigned(consistencyTics), quitterBytes, bodyBytes);
	return true;
}

static bool HandleHCDELivePacket(int clientNum)
{
	auto& peer = HCDELivePeers[clientNum];
	const uint8_t version = NetBuffer[HCDELiveVersionOffset];
	const uint8_t type = NetBuffer[HCDELiveTypeOffset];
	const uint32_t sequence = HCDELiveReadBE32(&NetBuffer[HCDELiveTxSequenceOffset]);
	const uint32_t ack = HCDELiveReadBE32(&NetBuffer[HCDELiveAckOffset]);
	const size_t payloadSize = NetBufferLength - HCDELiveHeaderSize;

	if (!I_ClientUsesHCDEService(clientNum))
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE live %s from unnegotiated client=%d",
			HCDELiveMessageName(type), clientNum);
		return true;
	}

	if (version != HCDELiveProtocolVersion)
	{
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored HCDE live %s from client=%d with unsupported version=%u",
			HCDELiveMessageName(type), clientNum, unsigned(version));
		return true;
	}

	if (!HCDELiveSequenceIsFresh(clientNum, type, sequence))
		return true;

	switch (EHCDELiveMessage(type))
	{
	case HLIVE_CONTROL:
	{
		if (payloadSize < 6u)
		{
			DebugTrace::Markf("net", "malformed HCDE live control from client=%d payload=%zu", clientNum, payloadSize);
			return true;
		}

		AcceptHCDELiveSequence(clientNum, type, sequence);
		peer.PeerAck = ack;
		++peer.ControlReceived;
		if (peer.ControlReceived == 1u)
			Printf("%s:: HCDE live channel active with client %d\n", I_IsLocalHCDELiveAuthority() ? "NetServer" : "NetSession", clientNum);

		const uint32_t remoteGameTic = HCDELiveReadBE32(&NetBuffer[HCDELiveHeaderSize]);
		const uint8_t remoteConsole = NetBuffer[HCDELiveHeaderSize + 4u];
		const uint8_t remoteMaxClients = NetBuffer[HCDELiveHeaderSize + 5u];
		DebugTrace::Markf("net", "HCDE live control recv client=%d seq=%u ack=%u gametic=%u console=%u max=%u",
			clientNum, sequence, ack, remoteGameTic, remoteConsole, remoteMaxClients);
		break;
	}
	case HLIVE_CLIENT_COMMANDS:
	{
		if (!I_ShouldAcceptHCDELiveClientInputFrom(clientNum))
		{
			++peer.UnsupportedReceived;
			DebugTrace::Markf("net", "ignored HCDE live client commands from client=%d route-authority=%d local-authority=%d",
				clientNum, I_IsHCDELiveAuthoritySlot(clientNum), I_IsLocalHCDELiveAuthority());
			return true;
		}

		if (!UnwrapHCDELiveClientInputPayload(clientNum, payloadSize))
		{
			RequestHCDELiveReplay(clientNum, "client-input-decode");
			return true;
		}

		AcceptHCDELiveSequence(clientNum, type, sequence);
		peer.PeerAck = ack;
		++peer.ClientCommandReceived;
		if (peer.ClientCommandReceived == 1u)
			Printf("NetServer:: HCDE live client inputs active with client %d\n", clientNum);
		DebugTrace::Markf("net", "HCDE live client-input boundary recv client=%d payload=%zu local-authority=%d",
			clientNum, payloadSize, I_IsLocalHCDELiveAuthority());
		return false;
	}
	case HLIVE_SERVER_SNAPSHOT:
	{
		if (!I_ShouldAcceptHCDELiveServerSnapshotFrom(clientNum))
		{
			++peer.UnsupportedReceived;
			DebugTrace::Markf("net", "ignored HCDE live server snapshot from client=%d route-authority=%d local-authority=%d",
				clientNum, I_IsHCDELiveAuthoritySlot(clientNum), I_IsLocalHCDELiveAuthority());
			return true;
		}

		if (!UnwrapHCDELiveServerSnapshotPayload(clientNum, payloadSize))
		{
			RequestHCDELiveReplay(clientNum, "server-snapshot-decode");
			return true;
		}

		AcceptHCDELiveSequence(clientNum, type, sequence);
		peer.PeerAck = ack;
		++peer.SnapshotReceived;
		if (peer.SnapshotReceived == 1u)
			Printf("NetSession:: HCDE live server snapshots active with client %d\n", clientNum);
		DebugTrace::Markf("net", "HCDE live snapshot boundary recv client=%d payload=%zu from-authority=%d",
			clientNum, payloadSize, I_IsHCDELiveAuthoritySlot(clientNum));
		return false;
	}
	default:
		++peer.UnsupportedReceived;
		DebugTrace::Markf("net", "ignored unsupported HCDE live type=%u from client=%d payload=%zu",
			unsigned(type), clientNum, payloadSize);
		break;
	}
	return true;
}

static void SendHCDELiveControl()
{
	const uint64_t now = I_msTime();
	if (LastHCDELiveControlMS != 0u && now - LastHCDELiveControlMS < HCDELiveControlIntervalMS)
		return;

	LastHCDELiveControlMS = now;
	for (auto client : NetworkClients)
	{
		if (!I_ShouldSendHCDELiveControlTo(client))
			continue;

		auto& peer = HCDELivePeers[client];
		const size_t payloadOffset = BeginHCDELivePacket(client, HLIVE_CONTROL);
		HCDELiveWriteBE32(&NetBuffer[payloadOffset], uint32_t(max<int>(gametic, 0)));
		NetBuffer[payloadOffset + 4u] = uint8_t(clamp<int>(consoleplayer, 0, UINT8_MAX));
		NetBuffer[payloadOffset + 5u] = uint8_t(clamp<int>(MaxClients, 0, UINT8_MAX));
		++peer.ControlSent;
		DebugTrace::Markf("net", "HCDE live control send client=%d seq=%u ack=%u gametic=%d sent=%u",
			client, peer.TxSequence, peer.RxSequence, gametic, peer.ControlSent);
		HSendPacket(client, HCDELiveHeaderSize + 6u);
	}
}

static void HSendLiveGameplayPacket(int client, size_t size)
{
	const bool clientCommand = ShouldWrapHCDEClientCommandPacket(client);
	const bool serverSnapshot = ShouldWrapHCDEServerSnapshotPacket(client);
	if (!clientCommand && !serverSnapshot)
	{
		HSendPacket(client, size);
		return;
	}
	if (size > MAX_MSGLEN)
	{
		I_FatalError("HCDE live gameplay packet for client %d exceeded NetBuffer: %zu bytes", client, size);
	}

	uint8_t legacyPacket[MAX_MSGLEN];
	memcpy(legacyPacket, NetBuffer, size);

	uint8_t gameplayPayload[MAX_MSGLEN];
	size_t gameplayPayloadSize = 0u;
	auto& peer = HCDELivePeers[client];
	const EHCDELiveMessage type = clientCommand ? HLIVE_CLIENT_COMMANDS : HLIVE_SERVER_SNAPSHOT;
	const EHCDEGameplayPayload gameplayKind = clientCommand ? HGP_CLIENT_INPUTS : HGP_SERVER_SNAPSHOT;
	if (clientCommand)
	{
		if (!BuildHCDEClientInputPayload(client, legacyPacket, size, gameplayPayload, sizeof(gameplayPayload), gameplayPayloadSize))
		{
			DPrintf(DMSG_WARNING, "HCDE live client input for client %d could not be encoded: %zu bytes\n", client, size);
			HSendPacket(client, size);
			return;
		}
	}
	else
	{
		if (!BuildHCDEServerSnapshotPayload(client, legacyPacket, size, gameplayPayload, sizeof(gameplayPayload), gameplayPayloadSize))
		{
			ClientStates[client].Flags |= CF_RETRANSMIT_SEQ;
			Printf(PRINT_HIGH, "HCDE live server snapshot for client %d could not be encoded; requesting resend instead of legacy fallback.\n", client);
			DebugTrace::Warningf("net", "HCDE live server snapshot encode failed client=%d size=%zu seq=%d ack=%d room=%u",
				client, size, ClientStates[client].CurrentSequence, ClientStates[client].SequenceAck, unsigned(CurrentRoomID));
			return;
		}
	}

	if (gameplayPayloadSize > MAX_MSGLEN - HCDELiveHeaderSize - HCDEGameplayHeaderSize)
	{
		if (serverSnapshot)
		{
			ClientStates[client].Flags |= CF_RETRANSMIT_SEQ;
			Printf(PRINT_HIGH, "HCDE live server snapshot for client %d is too large to wrap; requesting resend instead of legacy fallback.\n", client);
			DebugTrace::Warningf("net", "HCDE live server snapshot wrap too large client=%d payload=%zu legacy=%zu seq=%d ack=%d room=%u",
				client, gameplayPayloadSize, size, ClientStates[client].CurrentSequence, ClientStates[client].SequenceAck, unsigned(CurrentRoomID));
			return;
		}

		DPrintf(DMSG_WARNING, "HCDE live gameplay packet for client %d is too large to wrap: %zu bytes\n", client, gameplayPayloadSize);
		HSendPacket(client, size);
		return;
	}

	const size_t payloadOffset = BeginHCDELivePacket(client, type);
	WriteHCDEGameplayEnvelope(&NetBuffer[payloadOffset], gameplayKind);
	memcpy(&NetBuffer[payloadOffset + HCDEGameplayHeaderSize], gameplayPayload, gameplayPayloadSize);
	if (clientCommand)
	{
		++peer.ClientCommandSent;
		if (peer.ClientCommandSent == 1u)
			Printf("NetSession:: HCDE live client inputs active with client %d\n", client);
	}
	else
	{
		++peer.SnapshotSent;
		if (peer.SnapshotSent == 1u)
			Printf("NetServer:: HCDE live server snapshots active with client %d\n", client);
	}
	DebugTrace::Markf("net", "HCDE live %s send client=%d seq=%u ack=%u payload=%zu sent=%u",
		HCDELiveMessageName(uint8_t(type)), client, peer.TxSequence, peer.RxSequence, gameplayPayloadSize,
		clientCommand ? peer.ClientCommandSent : peer.SnapshotSent);
	HSendPacket(client, payloadOffset + HCDEGameplayHeaderSize + gameplayPayloadSize);

	memcpy(NetBuffer, legacyPacket, size);
	NetBufferLength = size;
}

CVAR(Bool, vid_dontdowait, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, vid_lowerinbackground, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR(Bool, net_ticbalance, true, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_extratic, false, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_limitsaves, true, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_repeatableactioncooldown, true, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_limitconversations, false, CVAR_SERVERINFO | CVAR_NOSAVE)
CUSTOM_CVAR(Int, net_disablepause, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
	else if (self > 2)
		self = 2;
}
CUSTOM_CVAR(Int, net_cutscenereadytype, RT_VOTE, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < RT_VOTE)
		self = RT_VOTE;
	else if (self > RT_HOST_ONLY)
		self = RT_HOST_ONLY;
}
CUSTOM_CVAR(Float, net_cutscenereadypercent, 0.5f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
	else if (self > 1.0f)
		self = 1.0f;
}
CVAR(Float, net_cutscenecountdown, 30.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
CUSTOM_CVAR(Float, sv_invasioncountdowntime, 30.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	// Compatibility/server-ops cvar:
	// keep this accepted now so launchers/admin scripts can tune invasion/horde
	// countdown timing even while that mode is still being expanded.
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Float, sv_invasionspawntime, 8.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Float, sv_invasioncleanuptime, 4.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Float, sv_invasionintermissiontime, 6.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Float, sv_invasionresulttime, 8.0f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.0f)
		self = 0.0f;
}
CUSTOM_CVAR(Int, sv_invasionwaves, 8, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 1)
		self = 1;
	else if (self > 255)
		self = 255;
}
CUSTOM_CVAR_NAMED(Int, wavelimit_compat, wavelimit, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
	else if (self > 255)
		self = 255;
}
CUSTOM_CVAR_NAMED(Int, duellimit_compat, duellimit, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
	else if (self > 255)
		self = 255;
}
CUSTOM_CVAR(Bool, sv_usemapsettingswavelimit, true, CVAR_SERVERINFO | CVAR_NOSAVE)
{
}
CUSTOM_CVAR(Int, sv_invasionbasebudget, 24, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 1)
		self = 1;
}
CUSTOM_CVAR(Int, sv_invasionbudgetstep, 8, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Int, sv_invasionperplayer, 6, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Float, sv_invasionspawninterval, 0.35f, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0.05f)
		self = 0.05f;
}
CUSTOM_CVAR(Int, sv_invasionspawnburst, 3, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 1)
		self = 1;
}
CUSTOM_CVAR(Int, sv_invasionmaxactive, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Int, sv_invasionbosswaveevery, 5, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Int, sv_invasionbossbonus, 20, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
		self = 0;
}
CUSTOM_CVAR(Bool, sv_invasionspotusemaptags, false, CVAR_SERVERINFO | CVAR_NOSAVE)
{
}
CUSTOM_CVAR(Bool, sv_invasionspotfallback, true, CVAR_SERVERINFO | CVAR_NOSAVE)
{
}

CVAR(Bool, cl_noboldchat, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, cl_nochatsound, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Int, cl_showchat, CHAT_GLOBAL, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < CHAT_DISABLED)
		self = CHAT_DISABLED;
	else if (self > CHAT_GLOBAL)
		self = CHAT_GLOBAL;
}

CUSTOM_CVAR(Int, cl_debugprediction, 0, CVAR_CHEAT)
{
	if (self < 0)
		self = 0;
	else if (self > BACKUPTICS - 1)
		self = BACKUPTICS - 1;
}
CVAR(Bool, net_desyncdebug, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Used to write out all network events that occured leading up to the next tick.
static struct NetEventData
{
	struct FStream {
		uint8_t* Stream;
		size_t Used = 0;

		FStream()
		{
			Grow(256);
		}

		~FStream()
		{
			if (Stream != nullptr)
				M_Free(Stream);
		}

		void Grow(size_t size)
		{
			Stream = (uint8_t*)M_Realloc(Stream, size);
		}
	} Streams[BACKUPTICS];

private:
	size_t CurrentSize = 0;
	size_t MaxSize = 256;
	int CurrentClientTic = 0;

	// Make more room for special Command.
	void GetMoreBytes(size_t newSize)
	{
		MaxSize = max<size_t>(MaxSize * 2, newSize + 30);

		DPrintf(DMSG_NOTIFY, "Expanding special size to %zu\n", MaxSize);

		for (auto& stream : Streams)
			stream.Grow(MaxSize);

		CurrentStream = Streams[CurrentClientTic % BACKUPTICS].Stream + CurrentSize;
	}

	void AddBytes(size_t bytes)
	{
		if (CurrentSize + bytes >= MaxSize)
			GetMoreBytes(CurrentSize + bytes);

		CurrentSize += bytes;
	}

public:
	uint8_t* CurrentStream = nullptr;

	// Boot up does some faux network events so we need to wait until after
	// everything is initialized to actually set up the network stream.
	void InitializeEventData()
	{
		CurrentStream = Streams[0].Stream;
		CurrentSize = 0;
	}

	void ResetStream()
	{
		CurrentClientTic = ClientTic / TicDup;
		CurrentStream = Streams[CurrentClientTic % BACKUPTICS].Stream;
		CurrentSize = 0;
	}

	void NewClientTic()
	{
		const int tic = ClientTic / TicDup;
		if (CurrentClientTic == tic)
			return;

		Streams[CurrentClientTic % BACKUPTICS].Used = CurrentSize;

		CurrentClientTic = tic;
		CurrentStream = Streams[tic % BACKUPTICS].Stream;
		CurrentSize = 0;
	}

	NetEventData& operator<<(uint8_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(1);
			UncheckedWriteInt8(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int16_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(2);
			UncheckedWriteInt16(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int32_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(4);
			UncheckedWriteInt32(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int64_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(8);
			UncheckedWriteInt64(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(float it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(4);
			UncheckedWriteFloat(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(double it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(8);
			UncheckedWriteDouble(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(const char *it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(strlen(it) + 1);
			UncheckedWriteString(it, &CurrentStream);
		}
		return *this;
	}
} NetEvents;

static const char* Net_InvasionSpawnSourceName(EInvasionSpawnSource source)
{
	switch (source)
	{
	case INVSPAWN_CLASSIC: return "classic";
	case INVSPAWN_MAPSPOT: return "mapspot";
	case INVSPAWN_DEATHMATCH: return "deathmatch";
	case INVSPAWN_PLAYERSTART: return "playerstart";
	default: return "none";
	}
}

static PClassActor* Net_GetInvasionSpotBaseClass()
{
	static bool resolved = false;
	static PClassActor* cls = nullptr;
	if (!resolved)
	{
		cls = PClass::FindActor("CustomMonsterInvasionSpot");
		if (cls == nullptr)
			cls = PClass::FindActor("BaseMonsterInvasionSpot");
		resolved = true;
	}
	return cls;
}

void Net_BeginInvasionSpawnRegistration(FLevelLocals* level)
{
	InvasionRegisteredSpawnSpotLevel = level;
	InvasionRegisteredSpawnSpots.Clear();
}

bool Net_RegisterInvasionSpawnSpotFromMapThing(FLevelLocals* level, const FMapThing* mapThing, PClassActor* spotClass)
{
	if (level == nullptr || mapThing == nullptr || spotClass == nullptr)
		return false;

	const PClassActor* invasionSpotBase = Net_GetInvasionSpotBaseClass();
	if (invasionSpotBase == nullptr || !spotClass->IsDescendantOf(invasionSpotBase))
		return false;

	if (InvasionRegisteredSpawnSpotLevel != level)
	{
		Net_BeginInvasionSpawnRegistration(level);
	}

	FInvasionSpawnSpotRecord record;
	record.Pos = mapThing->pos;
	// Map thing Z is a floor-relative offset, matching FLevelLocals::SpawnMapThing.
	if (sector_t* sector = level->PointInSector(record.Pos); sector != nullptr)
	{
		record.Pos.Z = sector->floorplane.ZatPoint(record.Pos) + mapThing->pos.Z;
	}
	record.Yaw = DAngle::fromDeg(mapThing->angle);
	record.Tag = mapThing->thingid;
	record.StartSpawnNumber = max<int>(mapThing->args[0], 1);
	record.SpawnDelayTics = max(Net_InvasionSecondsToTics(float(mapThing->args[1])), 0);
	record.RoundSpawnDelayTics = max(Net_InvasionSecondsToTics(float(mapThing->args[2])), 0);
	record.FirstWave = max<int>(mapThing->args[3], 1);
	record.MaxSpawnNumber = max<int>(mapThing->args[4], 0);
	record.Source = INVSPAWN_CLASSIC;
	record.SpotClass = spotClass;
	record.PlannedSpawnCount = 0;
	record.SpawnedCount = 0;
	record.NextSpawnGameTic = 0;
	InvasionRegisteredSpawnSpots.Push(record);
	return true;
}

static PClassActor* Net_GetMapSpotClass()
{
	static bool resolved = false;
	static PClassActor* cls = nullptr;
	if (!resolved)
	{
		cls = PClass::FindActor("MapSpot");
		resolved = true;
	}
	return cls;
}

static bool Net_IsNoMonstersActive()
{
	if ((dmflags & DF_NO_MONSTERS) != 0)
		return true;
	return primaryLevel != nullptr && (primaryLevel->flags2 & LEVEL2_NOMONSTERS) != 0;
}

static bool Net_IsValidInvasionMonsterClass(const PClassActor* cls)
{
	if (cls == nullptr)
		return false;

	const AActor* defaults = GetDefaultByType(cls);
	if (defaults == nullptr)
		return false;

	return (defaults->flags3 & MF3_ISMONSTER) != 0;
}

static PClassActor* Net_ResolveInvasionMonsterClass(const char* className)
{
	if (className == nullptr || *className == '\0')
		return nullptr;

	PClassActor* cls = PClass::FindActor(className);
	if (cls == nullptr)
		return nullptr;

	cls = cls->GetReplacement(primaryLevel);
	return Net_IsValidInvasionMonsterClass(cls) ? cls : nullptr;
}

static PClassActor* Net_SelectInvasionMonsterFromPool(const char* const* pool, size_t count, int wave)
{
	if (pool == nullptr || count == 0)
		return nullptr;

	TArray<PClassActor*> resolved;
	for (size_t i = 0; i < count; ++i)
	{
		PClassActor* cls = Net_ResolveInvasionMonsterClass(pool[i]);
		if (cls == nullptr)
			continue;

		bool waveMet = true;
		for (const auto& profile : InvasionMonsterProfiles)
		{
			if (stricmp(profile.ClassName, pool[i]) == 0)
			{
				if (wave < profile.MinWave)
					waveMet = false;
				break;
			}
		}

		if (waveMet)
			resolved.Push(cls);
	}

	if (resolved.Size() == 0)
		return nullptr;

	return resolved[pr_invasion() % resolved.Size()];
}

static PClassActor* Net_SelectInvasionMonsterFromDropList(const PClassActor* spotClass)
{
	if (spotClass == nullptr)
		return nullptr;

	const AActor* defaults = GetDefaultByType(spotClass);
	if (defaults == nullptr)
		return nullptr;

	const FDropItem* drop = defaults->GetDropItems();
	if (drop == nullptr)
		return nullptr;

	int weightTotal = 0;
	for (auto di = drop; di != nullptr; di = di->Next)
	{
		if (di->Name == NAME_None)
			continue;

		PClassActor* candidate = PClass::FindActor(di->Name);
		if (!Net_IsValidInvasionMonsterClass(candidate))
			continue;

		const int amount = max(di->Amount, 1);
		weightTotal += amount;
	}

	if (weightTotal <= 0)
		return nullptr;

	int pick = pr_invasion() % weightTotal;
	for (auto di = drop; di != nullptr; di = di->Next)
	{
		if (di->Name == NAME_None)
			continue;

		PClassActor* candidate = PClass::FindActor(di->Name);
		if (!Net_IsValidInvasionMonsterClass(candidate))
			continue;

		const int amount = max(di->Amount, 1);
		pick -= amount;
		if (pick >= 0)
			continue;

		const int probability = clamp<int>(di->Probability, 0, 255);
		if (pr_invasion() > probability)
			return nullptr;

		candidate = candidate->GetReplacement(primaryLevel);
		return Net_IsValidInvasionMonsterClass(candidate) ? candidate : nullptr;
	}

	return nullptr;
}

static PClassActor* Net_SelectInvasionMonsterForSpot(const FInvasionSpawnSpotRecord& spot)
{
	static const char* const weakPool[] = {
		"ZombieMan", "ShotgunGuy", "ChaingunGuy", "DoomImp", "Demon", "Spectre", "LostSoul"
	};
	static const char* const strongPool[] = {
		"Cacodemon", "HellKnight", "BaronOfHell", "Revenant", "Fatso", "Arachnotron", "PainElemental"
	};
	static const char* const bossPool[] = {
		"Archvile", "Cyberdemon", "SpiderMastermind", "BaronOfHell", "Revenant"
	};
	static const char* const anyPool[] = {
		"ZombieMan", "ShotgunGuy", "ChaingunGuy", "DoomImp", "Demon", "Spectre", "LostSoul",
		"Cacodemon", "HellKnight", "BaronOfHell", "Revenant", "Fatso", "Arachnotron",
		"PainElemental", "Archvile", "WolfensteinSS", "Cyberdemon", "SpiderMastermind"
	};

	if (Net_IsNoMonstersActive())
		return nullptr;

	if (spot.SpotClass != nullptr)
	{
		// First preference: let modded/map-authored invasion spots provide a drop list.
		if (PClassActor* fromDropList = Net_SelectInvasionMonsterFromDropList(spot.SpotClass); fromDropList != nullptr)
			return fromDropList;

		auto checkSpot = [&](const char* className) {
			auto cls = PClass::FindActor(className);
			return cls != nullptr && spot.SpotClass->IsDescendantOf(cls);
		};

		if (checkSpot("ImpSpot")) return Net_ResolveInvasionMonsterClass("DoomImp");
		if (checkSpot("DemonSpot")) return Net_ResolveInvasionMonsterClass("Demon");
		if (checkSpot("SpectreSpot")) return Net_ResolveInvasionMonsterClass("Spectre");
		if (checkSpot("ZombieManSpot")) return Net_ResolveInvasionMonsterClass("ZombieMan");
		if (checkSpot("ShotgunGuySpot")) return Net_ResolveInvasionMonsterClass("ShotgunGuy");
		if (checkSpot("ChaingunGuySpot")) return Net_ResolveInvasionMonsterClass("ChaingunGuy");
		if (checkSpot("CacodemonSpot")) return Net_ResolveInvasionMonsterClass("Cacodemon");
		if (checkSpot("RevenantSpot")) return Net_ResolveInvasionMonsterClass("Revenant");
		if (checkSpot("FatsoSpot")) return Net_ResolveInvasionMonsterClass("Fatso");
		if (checkSpot("ArachnotronSpot")) return Net_ResolveInvasionMonsterClass("Arachnotron");
		if (checkSpot("HellKnightSpot")) return Net_ResolveInvasionMonsterClass("HellKnight");
		if (checkSpot("BaronOfHellSpot")) return Net_ResolveInvasionMonsterClass("BaronOfHell");
		if (checkSpot("LostSoulSpot")) return Net_ResolveInvasionMonsterClass("LostSoul");
		if (checkSpot("PainElementalSpot")) return Net_ResolveInvasionMonsterClass("PainElemental");
		if (checkSpot("CyberdemonSpot")) return Net_ResolveInvasionMonsterClass("Cyberdemon");
		if (checkSpot("SpiderMastermindSpot")) return Net_ResolveInvasionMonsterClass("SpiderMastermind");
		if (checkSpot("ArchvileSpot")) return Net_ResolveInvasionMonsterClass("Archvile");
		if (checkSpot("WolfensteinSSSpot")) return Net_ResolveInvasionMonsterClass("WolfensteinSS");

		if (checkSpot("WeakDoomMonsterSpot"))
			return Net_SelectInvasionMonsterFromPool(weakPool, countof(weakPool), InvasionWaveDirector.Wave);
		if (checkSpot("PowerfulDoomMonsterSpot"))
			return Net_SelectInvasionMonsterFromPool(strongPool, countof(strongPool), InvasionWaveDirector.Wave);
		if (checkSpot("VeryPowerfulDoomMonsterSpot"))
			return Net_SelectInvasionMonsterFromPool(bossPool, countof(bossPool), InvasionWaveDirector.Wave);
		if (checkSpot("AnyDoomMonsterSpot"))
			return Net_SelectInvasionMonsterFromPool(anyPool, countof(anyPool), InvasionWaveDirector.Wave);
	}

	if (InvasionWaveDirector.Wave >= 8)
		return Net_SelectInvasionMonsterFromPool(bossPool, countof(bossPool), InvasionWaveDirector.Wave);
	if (InvasionWaveDirector.Wave >= 4)
		return Net_SelectInvasionMonsterFromPool(strongPool, countof(strongPool), InvasionWaveDirector.Wave);
	return Net_SelectInvasionMonsterFromPool(weakPool, countof(weakPool), InvasionWaveDirector.Wave);
}

static DVector3 Net_GetInvasionSpawnCandidate(const DVector3& basePos, double xOffset, double yOffset)
{
	DVector3 candidate = basePos + DVector3(xOffset, yOffset, 0.0);
	if (primaryLevel == nullptr)
		return candidate;

	double zOffset = 0.0;
	if (sector_t* baseSector = primaryLevel->PointInSector(basePos); baseSector != nullptr)
	{
		zOffset = basePos.Z - baseSector->floorplane.ZatPoint(basePos);
	}
	if (sector_t* candidateSector = primaryLevel->PointInSector(candidate); candidateSector != nullptr)
	{
		candidate.Z = candidateSector->floorplane.ZatPoint(candidate) + zOffset;
	}
	return candidate;
}

static AActor* Net_SelectInvasionCombatTarget(AActor* actor)
{
	AActor* best = nullptr;
	double bestDistSq = 0.0;
	for (int player = 0; player < MAXPLAYERS; ++player)
	{
		if (!playeringame[player] || I_IsServerReservedSlot(player))
			continue;

		player_t& playerState = players[player];
		AActor* playerMo = playerState.mo;
		if (playerMo == nullptr || playerState.playerstate != PST_LIVE)
			continue;

		if (playerMo->health <= 0 && playerState.health <= 0)
			continue;
		if ((playerMo->flags & MF_SHOOTABLE) == 0)
			continue;
		if ((playerState.cheats & CF_NOTARGET) != 0)
			continue;
		if (actor != nullptr && actor->IsFriend(playerMo))
			continue;

		const double distSq = actor != nullptr ? (playerMo->Pos() - actor->Pos()).LengthSquared() : 0.0;
		if (best == nullptr || distSq < bestDistSq)
		{
			best = playerMo;
			bestDistSq = distSq;
		}
	}

	return best;
}

static void Net_AwakenInvasionMonster(AActor* actor)
{
	if (actor == nullptr || actor->health <= 0 || (actor->flags3 & MF3_ISMONSTER) == 0)
		return;

	AActor* target = Net_SelectInvasionCombatTarget(actor);
	if (target == nullptr)
		return;

	actor->target = target;
	actor->LastHeard = target;
	actor->lastenemy = nullptr;
	actor->reactiontime = max<int>(actor->reactiontime, Net_GetInvasionSkillWakeDelayTics());
	actor->threshold = max<int>(actor->DefThreshold, 10);
	actor->flags4 |= MF4_INCOMBAT;
	actor->Angles.Yaw = actor->AngleTo(target);
	if (actor->SeeState != nullptr)
		actor->SetState(actor->SeeState, true);

	DebugTrace::Markf("invasion", "monster awakened class=%s target=%d dist=%.1f threshold=%d reaction=%d skill=%d",
		actor->GetClass() != nullptr ? actor->GetClass()->TypeName.GetChars() : "<unknown>",
		target->player != nullptr ? int(target->player - players) : -1,
		(target->Pos() - actor->Pos()).Length(),
		actor->threshold,
		actor->reactiontime,
		clamp<int>(gameskill, 0, 4));
}

// Spawns a monster at the specified spot. Handles telefrag protection and
// collision validation, and returns a compact reason for diagnostics when it
// cannot spawn yet.
static bool Net_SpawnInvasionMonster(FInvasionSpawnSpotRecord& spot, const char** failureReason = nullptr, PClassActor* forcedMonsterClass = nullptr, PClassActor** resolvedMonsterClass = nullptr)
{
	auto fail = [failureReason](const char* reason) {
		if (failureReason != nullptr)
			*failureReason = reason;
		return false;
	};
	if (failureReason != nullptr)
		*failureReason = nullptr;

	if (primaryLevel == nullptr || gamestate != GS_LEVEL)
		return fail("no-level");

	PClassActor* monsterClass = forcedMonsterClass != nullptr ? forcedMonsterClass : Net_SelectInvasionMonsterForSpot(spot);
	if (resolvedMonsterClass != nullptr)
		*resolvedMonsterClass = monsterClass;
	if (monsterClass == nullptr)
		return fail("no-monster-class");

	const AActor* monsterDefaults = GetDefaultByType(monsterClass);
	auto trySpawnAt = [&](const DVector3& pos, const char** reason) -> AActor*
	{
		// Telefrag protection: prevent spawning if a live player is currently inside the spot's footprint.
		// This uses a conservative radius-sum check to avoid 'telefrag-jiggle' on high-latency clients.
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (!playeringame[i] || players[i].mo == nullptr || players[i].mo->health <= 0)
				continue;

			const AActor* playerMo = players[i].mo;
			const double dist = (playerMo->Pos() - pos).Length();
			if (dist < playerMo->radius + monsterDefaults->radius + 8.0)
			{
				if (reason != nullptr)
					*reason = "player-blocking-spot";
				return nullptr;
			}
		}

		AActor* spawned = Spawn(primaryLevel, monsterClass, pos, ALLOW_REPLACE);
		if (spawned == nullptr)
		{
			if (reason != nullptr)
				*reason = "spawn-returned-null";
			return nullptr;
		}

		// Perform a final location validation. Briefly allow actor overlap to mimic
		// standard Doom spawner behavior.
		const ActorFlags2 oldFlags2 = spawned->flags2;
		spawned->flags2 |= MF2_PASSMOBJ;
		if (!P_TestMobjLocation(spawned))
		{
			spawned->flags2 = oldFlags2;
			spawned->ClearCounters();
			spawned->Destroy();
			if (reason != nullptr)
				*reason = "blocked-location";
			return nullptr;
		}

		spawned->flags2 = oldFlags2;
		spawned->Angles.Yaw = spot.Yaw;
		return spawned;
	};

	const char* lastReason = nullptr;
	AActor* spawned = trySpawnAt(spot.Pos, &lastReason);
	if (spawned == nullptr && lastReason != nullptr && stricmp(lastReason, "blocked-location") == 0)
	{
		static const double radii[] = { 16.0, 32.0, 48.0, 64.0, 96.0 };
		for (double radius : radii)
		{
			for (int step = 0; step < 8 && spawned == nullptr; ++step)
			{
				const DAngle angle = DAngle::fromDeg(step * 45.0);
				const DVector3 candidate = Net_GetInvasionSpawnCandidate(spot.Pos, angle.Cos() * radius, angle.Sin() * radius);
				spawned = trySpawnAt(candidate, &lastReason);
			}
			if (spawned != nullptr)
				break;
		}
	}

	if (spawned == nullptr)
		return fail(lastReason != nullptr ? lastReason : "blocked-location");

	// Track the monster for wave-cleared calculations.
	Net_ApplyInvasionMonsterSkillTuning(spawned);
	Net_AwakenInvasionMonster(spawned);
	InvasionWaveDirector.ActiveMonsters.Push(MakeObjPtr<AActor*>(spawned));
	Net_RecordInvasionSpawnEvent(spawned);
	return true;
}

static bool Net_IsInvasionSpotActiveForCurrentTag(const FInvasionSpawnSpotRecord& spot)
{
	if (InvasionSpawnDirectory.ActiveTag > 0 && spot.Tag > 0 && spot.Tag != InvasionSpawnDirectory.ActiveTag)
		return false;
	return true;
}

static bool Net_TryRelocateBlockedClassicInvasionSpawn(FInvasionSpawnSpotRecord& blockedSpot, PClassActor* monsterClass, const char** failureReason)
{
	if (blockedSpot.Source != INVSPAWN_CLASSIC || monsterClass == nullptr)
		return false;

	const char* lastReason = nullptr;
	for (int pass = 0; pass < 2; ++pass)
	{
		for (auto& candidate : InvasionSpawnDirectory.Spots)
		{
			if (&candidate == &blockedSpot
				|| candidate.Source != INVSPAWN_CLASSIC
				|| !Net_IsInvasionSpotActiveForCurrentTag(candidate))
			{
				continue;
			}

			const bool sameSpotClass = candidate.SpotClass == blockedSpot.SpotClass;
			if ((pass == 0 && !sameSpotClass) || (pass == 1 && sameSpotClass))
				continue;

			if (Net_SpawnInvasionMonster(candidate, &lastReason, monsterClass))
			{
				Printf(PRINT_HIGH,
					"Invasion relocated blocked classic spawn: class=%s from=%s tid=%d to=%s tid=%d\n",
					monsterClass->TypeName.GetChars(),
					blockedSpot.SpotClass != nullptr ? blockedSpot.SpotClass->TypeName.GetChars() : "<fallback>",
					blockedSpot.Tag,
					candidate.SpotClass != nullptr ? candidate.SpotClass->TypeName.GetChars() : "<fallback>",
					candidate.Tag);
				return true;
			}
		}
	}

	if (failureReason != nullptr && lastReason != nullptr)
		*failureReason = lastReason;
	return false;
}

// Scans the active monster list and removes dead or invalid actors.
// Updates the global WaveCleared progress counter.
static void Net_UpdateInvasionWaveClearProgress()
{
	int aliveMonsters = 0;
	size_t writeIdx = 0;
	for (size_t i = 0; i < InvasionWaveDirector.ActiveMonsters.Size(); ++i)
	{
		AActor* actor = InvasionWaveDirector.ActiveMonsters[i];
		if (actor == nullptr || actor->health <= 0 || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
		{
			continue;
		}

		// Perform O(n) list compaction.
		if (writeIdx != i)
		{
			InvasionWaveDirector.ActiveMonsters[writeIdx] = InvasionWaveDirector.ActiveMonsters[i];
		}
		++writeIdx;
		++aliveMonsters;
	}

	if (writeIdx < InvasionWaveDirector.ActiveMonsters.Size())
	{
		InvasionWaveDirector.ActiveMonsters.Resize(unsigned(writeIdx));
	}

	InvasionWaveDirector.WaveCleared = max(InvasionWaveDirector.WaveSpawned - aliveMonsters, 0);
}

static int Net_DestroyTrackedInvasionMonsters(const char* reason)
{
	int removed = 0;
	if (gamestate == GS_LEVEL)
	{
		for (size_t i = 0; i < InvasionWaveDirector.ActiveMonsters.Size(); ++i)
		{
			AActor* actor = InvasionWaveDirector.ActiveMonsters[i];
			if (actor == nullptr || (actor->ObjectFlags & OF_EuthanizeMe) != 0)
				continue;

			actor->ClearCounters();
			actor->Destroy();
			++removed;
		}
	}

	InvasionWaveDirector.ActiveMonsters.Clear();
	InvasionReplicatedActors.Clear();
	InvasionPendingMirrorSpawns.Clear();
	InvasionActorDeltaSendCursor = 0u;
	if (removed > 0)
	{
		DebugTrace::Markf("invasion", "cleanup removed=%d reason=%s wave=%d map=%s",
			removed,
			reason != nullptr ? reason : "(none)",
			InvasionWaveDirector.Wave,
			primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	}
	return removed;
}

static double Net_GetInvasionMonsterDifficultyModifier()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 1)
		return 1.25;
	if (skillIndex == 2)
		return 1.5;
	return 1.6225;
}

static double Net_GetInvasionSkillBudgetScale()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return 0.50;
	if (skillIndex == 1)
		return 0.70;
	if (skillIndex == 2)
		return 0.85;
	return 1.0;
}

static int Net_ApplyInvasionSkillBudgetScale(int budget)
{
	if (budget <= 1)
		return max<int>(budget, 1);

	const double scaled = double(budget) * Net_GetInvasionSkillBudgetScale();
	return clamp<int>(int(floor(scaled + 0.5)), 1, 65535);
}

static double Net_GetInvasionSkillSpawnIntervalScale()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return 1.75;
	if (skillIndex == 1)
		return 1.35;
	if (skillIndex == 2)
		return 1.15;
	return 1.0;
}

static int Net_GetInvasionSkillWakeDelayTics()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return TICRATE;
	if (skillIndex == 1)
		return TICRATE / 2;
	return 0;
}

static double Net_GetInvasionSkillMonsterSpeedScale()
{
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return 0.65;
	if (skillIndex == 1)
		return 0.75;
	if (skillIndex == 2)
		return 0.85;
	return 1.0;
}

static void Net_ApplyInvasionMonsterSkillTuning(AActor* actor)
{
	if (actor == nullptr || (actor->flags3 & MF3_ISMONSTER) == 0)
		return;

	const double speedScale = Net_GetInvasionSkillMonsterSpeedScale();
	if (speedScale < 0.999)
	{
		actor->Speed *= speedScale;
	}
}

static int Net_GetInvasionActiveMonsterCap()
{
	if (sv_invasionmaxactive > 0)
		return clamp<int>(sv_invasionmaxactive, 1, 1024);

	const int players = max(Net_CountInvasionParticipants(), 1);
	const int skillIndex = clamp<int>(gameskill, 0, 4);
	if (skillIndex <= 0)
		return 8 + max(players - 1, 0) * 4;
	if (skillIndex == 1)
		return 11 + max(players - 1, 0) * 5;
	if (skillIndex == 2)
		return 16 + max(players - 1, 0) * 6;
	return 24 + max(players - 1, 0) * 8;
}

static int Net_ComputeInvasionSpotWaveCount(const FInvasionSpawnSpotRecord& spot, int wave)
{
	const int firstWave = max(spot.FirstWave, 1);
	if (wave < firstWave)
		return 0;

	const int startAmount = max(spot.StartSpawnNumber, 1);
	const int waveAge = max(wave - firstWave, 0);
	const double modifier = Net_GetInvasionMonsterDifficultyModifier();
	const double scaled = pow(modifier, double(waveAge)) * double(startAmount);
	int count = max<int>(int(floor(scaled + 0.5)), 0);
	if (spot.MaxSpawnNumber > 0)
		count = min(count, spot.MaxSpawnNumber);
	return count;
}

static int Net_SelectInvasionSpotTag(const TArray<FInvasionSpawnSpotRecord>& spots, int wave)
{
	if (!sv_invasionspotusemaptags)
		return 0;

	TArray<int> tags;
	for (const auto& spot : spots)
	{
		if (spot.Tag <= 0 || spot.Source != INVSPAWN_CLASSIC)
			continue;

		bool exists = false;
		for (const int tag : tags)
		{
			if (tag == spot.Tag)
			{
				exists = true;
				break;
			}
		}
		if (!exists)
			tags.Push(spot.Tag);
	}

	if (tags.Size() == 0)
		return 0;

	for (const int tag : tags)
	{
		if (tag == wave)
			return tag;
	}

	for (unsigned i = 0; i + 1 < tags.Size(); ++i)
	{
		for (unsigned j = i + 1; j < tags.Size(); ++j)
		{
			if (tags[j] < tags[i])
			{
				const int swapValue = tags[i];
				tags[i] = tags[j];
				tags[j] = swapValue;
			}
		}
	}
	const int index = clamp<int>(wave - 1, 0, int(tags.Size()) - 1);
	return tags[index];
}

static void Net_BuildFallbackInvasionSpots(FLevelLocals* level)
{
	InvasionSpawnDirectory.UsingFallback = true;
	InvasionSpawnDirectory.FallbackSource = INVSPAWN_NONE;

	if (sv_invasionspotfallback && level != nullptr)
	{
		if (auto mapSpotClass = Net_GetMapSpotClass(); mapSpotClass != nullptr)
		{
			auto it = level->GetThinkerIterator<AActor>();
			AActor* actor = nullptr;
			while ((actor = it.Next()) != nullptr)
			{
				if (!actor->IsA(mapSpotClass))
					continue;

				FInvasionSpawnSpotRecord record;
				record.Pos = actor->Pos();
				record.Yaw = actor->Angles.Yaw;
				record.Tag = actor->tid;
				record.Source = INVSPAWN_MAPSPOT;
				record.PlannedSpawnCount = 0;
				record.NextSpawnGameTic = gametic;
				InvasionSpawnDirectory.Spots.Push(record);
			}
		}
	}

	if (InvasionSpawnDirectory.Spots.Size() > 0)
	{
		InvasionSpawnDirectory.FallbackSource = INVSPAWN_MAPSPOT;
		return;
	}

	if (level != nullptr)
	{
		for (const auto& start : level->deathmatchstarts)
		{
			FInvasionSpawnSpotRecord record;
			record.Pos = start.pos;
			record.Yaw = DAngle::fromDeg(start.angle);
			record.Source = INVSPAWN_DEATHMATCH;
			record.PlannedSpawnCount = 0;
			record.NextSpawnGameTic = gametic;
			InvasionSpawnDirectory.Spots.Push(record);
		}
	}

	if (InvasionSpawnDirectory.Spots.Size() > 0)
	{
		InvasionSpawnDirectory.FallbackSource = INVSPAWN_DEATHMATCH;
		return;
	}

	if (level != nullptr)
	{
		for (const auto& start : level->AllPlayerStarts)
		{
			FInvasionSpawnSpotRecord record;
			record.Pos = start.pos;
			record.Yaw = DAngle::fromDeg(start.angle);
			record.Source = INVSPAWN_PLAYERSTART;
			record.PlannedSpawnCount = 0;
			record.NextSpawnGameTic = gametic;
			InvasionSpawnDirectory.Spots.Push(record);
		}
	}

	if (InvasionSpawnDirectory.Spots.Size() > 0)
		InvasionSpawnDirectory.FallbackSource = INVSPAWN_PLAYERSTART;
}

static void Net_RebuildInvasionSpawnSpots(int wave)
{
	if (primaryLevel == nullptr || gamestate != GS_LEVEL)
	{
		InvasionSpawnDirectory.Reset();
		return;
	}

	if (InvasionSpawnDirectory.Spots.Size() == 0)
	{
		if (InvasionRegisteredSpawnSpotLevel == primaryLevel && InvasionRegisteredSpawnSpots.Size() > 0)
		{
			for (const auto& spot : InvasionRegisteredSpawnSpots)
			{
				InvasionSpawnDirectory.Spots.Push(spot);
			}
			DebugTrace::Markf("invasion", "registered-spots copied=%u map=%s",
				unsigned(InvasionRegisteredSpawnSpots.Size()),
				primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
		}

		const PClassActor* invasionSpotBase = Net_GetInvasionSpotBaseClass();
		if (InvasionSpawnDirectory.Spots.Size() == 0 && invasionSpotBase != nullptr)
		{
			auto it = primaryLevel->GetThinkerIterator<AActor>();
			AActor* actor = nullptr;
			while ((actor = it.Next()) != nullptr)
			{
				if (!actor->IsA(invasionSpotBase))
					continue;

				FInvasionSpawnSpotRecord record;
				record.Pos = actor->Pos();
				record.Yaw = actor->Angles.Yaw;
				record.Tag = actor->tid;
				record.StartSpawnNumber = max<int>(actor->args[0], 1);
				record.SpawnDelayTics = max(Net_InvasionSecondsToTics(float(actor->args[1])), 0);
				record.RoundSpawnDelayTics = max(Net_InvasionSecondsToTics(float(actor->args[2])), 0);
				record.FirstWave = max<int>(actor->args[3], 1);
				record.MaxSpawnNumber = max<int>(actor->args[4], 0);
				record.Source = INVSPAWN_CLASSIC;
				record.SpotClass = actor->GetClass();
				record.PlannedSpawnCount = 0;
				record.SpawnedCount = 0;
				record.NextSpawnGameTic = 0;
				InvasionSpawnDirectory.Spots.Push(record);
			}
		}

		if (InvasionSpawnDirectory.Spots.Size() == 0)
		{
			Net_BuildFallbackInvasionSpots(primaryLevel);
		}
		InvasionSpawnDirectory.TotalSpotCount = int(InvasionSpawnDirectory.Spots.Size());
	}

	InvasionSpawnDirectory.ActiveTag = Net_SelectInvasionSpotTag(InvasionSpawnDirectory.Spots, wave);
	InvasionSpawnDirectory.TotalSpawnBudget = 0;
	InvasionSpawnDirectory.ActiveSpotCount = 0;
	InvasionSpawnDirectory.SpawnedThisWave = 0;

	for (auto& spot : InvasionSpawnDirectory.Spots)
	{
		spot.SpawnedCount = 0;
		spot.NextSpawnGameTic = gametic + spot.RoundSpawnDelayTics;

		if (InvasionSpawnDirectory.ActiveTag > 0 && spot.Tag > 0 && spot.Tag != InvasionSpawnDirectory.ActiveTag)
		{
			spot.PlannedSpawnCount = 0;
			continue;
		}

		spot.PlannedSpawnCount = Net_ComputeInvasionSpotWaveCount(spot, wave);
		if (spot.PlannedSpawnCount > 0)
		{
			InvasionSpawnDirectory.TotalSpawnBudget += spot.PlannedSpawnCount;
			++InvasionSpawnDirectory.ActiveSpotCount;
		}
	}
}

static void Net_PrepareInvasionMirrorFromSnapshot(EInvasionState previousState, int previousWave)
{
	if (I_IsLocalHCDEServiceAuthority())
		return;

	// Clients mirror only compact invasion UI/status fields from the server.
	// Monster spawning remains server-authored through explicit spawn events;
	// rebuilding local spot plans here made join clients run a second invasion.
	const bool waveChanged = previousWave != InvasionWaveDirector.Wave;
	const bool leftActiveRound = Net_IsInvasionRoundActiveState(previousState)
		&& !Net_IsInvasionRoundActiveState(InvasionState);
	if (waveChanged || leftActiveRound || InvasionState == INVS_DISABLED)
	{
		Net_DestroyTrackedInvasionMonsters("mirror-snapshot-reset");
		InvasionPendingSpawnEvents.Clear();
		InvasionPendingMirrorSpawns.Clear();
		InvasionLastAppliedSpawnEventId = 0u;
		InvasionMirrorNextVisualDiagnosticTic = 0;
	}

	InvasionSpawnDirectory.Spots.Clear();
	InvasionWaveDirector.SpawnIntervalTics = 0;
	InvasionWaveDirector.SpawnIntervalCountdown = 0;
}

static int Net_GetInvasionSpotRetryDelayTics(const FInvasionSpawnSpotRecord& spot)
{
	// Keep a short floor so blocked spots (for example near live players) do not
	// spin every tic and starve the rest of the spawn set.
	return max(spot.SpawnDelayTics, TICRATE / 2);
}

static int Net_ComputeInvasionSpotSpawnWindowFloorTics()
{
	int window = 0;
	for (const auto& spot : InvasionSpawnDirectory.Spots)
	{
		if (spot.PlannedSpawnCount <= 0)
			continue;

		// Native Skulltag/Zandronum invasion spots own their spawn pacing. The
		// generic spawn-time cvar must not fail a wave before those spot delays
		// have had a chance to release every planned spawn.
		const int planned = max(spot.PlannedSpawnCount, 1);
		const int roundDelay = max(spot.RoundSpawnDelayTics, 0);
		const int retryDelay = Net_GetInvasionSpotRetryDelayTics(spot);
		window = max(window, roundDelay + retryDelay * planned + TICRATE);
	}
	return window;
}

static bool Net_IsInvasionSpotSpawnReady(const FInvasionSpawnSpotRecord& spot)
{
	if (gametic < spot.NextSpawnGameTic)
		return false;

	if (spot.Source == INVSPAWN_CLASSIC)
	{
		if (spot.PlannedSpawnCount <= 0 || spot.SpawnedCount >= spot.PlannedSpawnCount)
			return false;
	}

	return true;
}

static bool Net_HasPendingClassicInvasionSpotSpawns()
{
	for (const auto& spot : InvasionSpawnDirectory.Spots)
	{
		if (spot.Source == INVSPAWN_CLASSIC
			&& spot.PlannedSpawnCount > 0
			&& spot.SpawnedCount < spot.PlannedSpawnCount)
		{
			return true;
		}
	}
	return false;
}

static void Net_LogInvasionSpawnDiagnostic(const char* event, const FInvasionSpawnSpotRecord* spot, const char* reason, int readySpots, int nextReadyDelayTics)
{
	static int lastWave = -1;
	static int consoleCount = 0;
	static int lastConsoleTic = 0;

	if (lastWave != InvasionWaveDirector.Wave)
	{
		lastWave = InvasionWaveDirector.Wave;
		consoleCount = 0;
		lastConsoleTic = 0;
	}

	const char* spotClassName = "<none>";
	int tid = 0;
	DVector3 pos = DVector3(0, 0, 0);
	int planned = 0;
	int spawned = 0;
	int nextReady = 0;
	if (spot != nullptr)
	{
		spotClassName = spot->SpotClass != nullptr ? spot->SpotClass->TypeName.GetChars() : "<fallback>";
		tid = spot->Tag;
		pos = spot->Pos;
		planned = spot->PlannedSpawnCount;
		spawned = spot->SpawnedCount;
		nextReady = max(spot->NextSpawnGameTic - gametic, 0);
	}

	DebugTrace::Markf("invasion", "spawn %s reason=%s wave=%d/%d state=%s budget=%d spawned=%d ready=%d next-ready=%d spots=%d active=%d tag=%d spot=%s tid=%d planned=%d spot-spawned=%d spot-next=%d pos=(%.1f,%.1f,%.1f)",
		event != nullptr ? event : "diagnostic",
		reason != nullptr ? reason : "unknown",
		InvasionWaveDirector.Wave,
		InvasionWaveDirector.MaxWaves,
		Net_InvasionStateName(InvasionState),
		InvasionWaveDirector.WaveBudget,
		InvasionWaveDirector.WaveSpawned,
		readySpots,
		nextReadyDelayTics,
		InvasionSpawnDirectory.TotalSpotCount,
		InvasionSpawnDirectory.ActiveSpotCount,
		InvasionSpawnDirectory.ActiveTag,
		spotClassName,
		tid,
		planned,
		spawned,
		nextReady,
		pos.X,
		pos.Y,
		pos.Z);

	if (consoleCount >= 8 && gametic - lastConsoleTic < TICRATE * 5)
		return;

	++consoleCount;
	lastConsoleTic = gametic;
	Printf(PRINT_HIGH,
		"Invasion spawn %s: reason=%s wave=%d/%d budget=%d spawned=%d ready=%d next-ready=%d spots=%d active=%d tag=%d spot=%s tid=%d planned=%d spot-spawned=%d\n",
		event != nullptr ? event : "diagnostic",
		reason != nullptr ? reason : "unknown",
		InvasionWaveDirector.Wave,
		InvasionWaveDirector.MaxWaves,
		InvasionWaveDirector.WaveBudget,
		InvasionWaveDirector.WaveSpawned,
		readySpots,
		nextReadyDelayTics,
		InvasionSpawnDirectory.TotalSpotCount,
		InvasionSpawnDirectory.ActiveSpotCount,
		InvasionSpawnDirectory.ActiveTag,
		spotClassName,
		tid,
		planned,
		spawned);
}

static bool Net_TryConsumeInvasionSpawnSlot()
{
	if (InvasionSpawnDirectory.Spots.Size() == 0)
	{
		Net_LogInvasionSpawnDiagnostic("deferred", nullptr, "no-spots", 0, -1);
		return false;
	}

	TArray<int> available;
	int nextReadyDelayTics = -1;
	for (int i = 0; i < int(InvasionSpawnDirectory.Spots.Size()); ++i)
	{
		const auto& spot = InvasionSpawnDirectory.Spots[i];
		if (Net_IsInvasionSpotSpawnReady(spot))
		{
			available.Push(i);
			continue;
		}

		if (spot.Source == INVSPAWN_CLASSIC
			&& spot.PlannedSpawnCount > 0
			&& spot.SpawnedCount < spot.PlannedSpawnCount)
		{
			const int delay = max(spot.NextSpawnGameTic - gametic, 0);
			if (nextReadyDelayTics < 0 || delay < nextReadyDelayTics)
				nextReadyDelayTics = delay;
		}
	}

	if (available.Size() == 0)
	{
		Net_LogInvasionSpawnDiagnostic("deferred", nullptr,
			nextReadyDelayTics >= 0 ? "waiting-for-spot-delay" : "no-ready-spots",
			0, nextReadyDelayTics);
		return false;
	}

	// Try every currently-ready spot at most once per call. This prevents one
	// blocked random pick from stalling the whole burst when other spots are
	// available.
	while (available.Size() > 0)
	{
		const int pickIdx = pr_invasion() % available.Size();
		const int pick = available[pickIdx];
		available[pickIdx] = available[available.Size() - 1];
		available.Pop();

		auto& selected = InvasionSpawnDirectory.Spots[pick];
		const char* failureReason = nullptr;
		PClassActor* attemptedMonsterClass = nullptr;
		bool spawned = Net_SpawnInvasionMonster(selected, &failureReason, nullptr, &attemptedMonsterClass);
		if (!spawned
			&& selected.Source == INVSPAWN_CLASSIC
			&& failureReason != nullptr
			&& stricmp(failureReason, "blocked-location") == 0)
		{
			// Classic invasion spots can be embedded in tight map geometry. When
			// one planned slot is permanently blocked, consume that slot by moving
			// the same monster class to another active invasion spot instead of
			// leaving the wave stuck just below its budget.
			spawned = Net_TryRelocateBlockedClassicInvasionSpawn(selected, attemptedMonsterClass, &failureReason);
		}

		if (selected.Source == INVSPAWN_CLASSIC)
		{
			if (spawned)
				++selected.SpawnedCount;

			const int spawnDelay = Net_GetInvasionSpotRetryDelayTics(selected);
			if (spawnDelay > 0)
				selected.NextSpawnGameTic = gametic + spawnDelay;
		}
		else if (!spawned)
		{
			const int retryDelay = Net_GetInvasionSpotRetryDelayTics(selected);
			if (retryDelay > 0)
				selected.NextSpawnGameTic = gametic + retryDelay;
		}

		if (!spawned)
		{
			Net_LogInvasionSpawnDiagnostic("failed", &selected, failureReason, int(available.Size()) + 1, -1);
			continue;
		}

		++InvasionSpawnDirectory.SpawnedThisWave;
		++InvasionWaveDirector.WaveSpawned;
		DebugTrace::Markf("invasion", "spawned monster wave=%d/%d spawned=%d/%d spot=%s tid=%d pos=(%.1f,%.1f,%.1f)",
			InvasionWaveDirector.Wave,
			InvasionWaveDirector.MaxWaves,
			InvasionWaveDirector.WaveSpawned,
			InvasionWaveDirector.WaveBudget,
			selected.SpotClass != nullptr ? selected.SpotClass->TypeName.GetChars() : "<fallback>",
			selected.Tag,
			selected.Pos.X,
			selected.Pos.Y,
			selected.Pos.Z);
		return true;
	}

	Net_LogInvasionSpawnDiagnostic("failed", nullptr, "all-ready-spots-blocked", 0, -1);
	return false;
}

static void Net_ResetInvasionState(const char* reason)
{
	Net_DestroyTrackedInvasionMonsters(reason != nullptr ? reason : "reset");
	InvasionWaveDirector.Reset();
	InvasionSpawnDirectory.Reset();
	InvasionRecentSpawnEvents.Clear();
	InvasionPendingSpawnEvents.Clear();
	InvasionPendingMirrorSpawns.Clear();
	InvasionReplicatedActors.Clear();
	InvasionActorDeltaSendCursor = 0u;
	InvasionNextSpawnEventId = 1u;
	InvasionLastAppliedSpawnEventId = 0u;
	InvasionMirrorNextVisualDiagnosticTic = 0;
	Net_ResetInvasionAnnouncements();
	InvasionReplicatedActiveMonsterCount = 0;
	InvasionWipeGraceTics = 0;
	InvasionNoParticipantTics = 0;
	if (Net_IsInvasionModeEnabled())
		Net_SetInvasionState(INVS_WAITING, 0, reason != nullptr ? reason : "reset");
	else
		Net_SetInvasionState(INVS_DISABLED, 0, reason != nullptr ? reason : "reset");
}

static int Net_CountInvasionParticipants()
{
	int count = 0;
	for (int player = 0; player < MAXPLAYERS; ++player)
	{
		if (playeringame[player] && !I_IsServerReservedSlot(player))
			++count;
	}

	return count;
}

static int Net_CountInvasionAliveParticipants()
{
	int count = 0;
	for (int player = 0; player < MAXPLAYERS; ++player)
	{
		if (!playeringame[player] || I_IsServerReservedSlot(player))
			continue;

		// Dedicated HCDE authority can receive pawn health before the cached
		// player health field is refreshed, so a live pawn with positive health
		// is enough to keep the wave from being treated as a wipe.
		if (players[player].playerstate == PST_LIVE
			&& ((players[player].mo != nullptr && players[player].mo->health > 0)
				|| players[player].health > 0))
		{
			++count;
		}
	}

	return count;
}

static int Net_ComputeInvasionWaveBudget(int wave, bool bossWave)
{
	const int players = max(Net_CountInvasionParticipants(), 1);
	int budget = sv_invasionbasebudget;
	budget += max(wave - 1, 0) * sv_invasionbudgetstep;
	budget += max(players - 1, 0) * sv_invasionperplayer;
	if (bossWave)
		budget += sv_invasionbossbonus;
	return clamp<int>(budget, 1, 65535);
}

static bool Net_IsInvasionBossWave(int wave)
{
	return sv_invasionbosswaveevery > 0 && wave > 0 && (wave % sv_invasionbosswaveevery) == 0;
}

static bool HCDE_CanOverrideCmpgninfLimits()
{
	return !HCDE_ServerMode_IsDedicatedServer() && !netgame;
}

static int Net_ResolveInvasionMaxWaves()
{
	int resolved = clamp<int>(sv_invasionwaves, 1, 255);
	const bool canOverrideCmpgninfLimits = HCDE_CanOverrideCmpgninfLimits();
	const bool hasMapLimit = primaryLevel != nullptr && primaryLevel->info != nullptr && primaryLevel->info->InvasionWaveLimit > 0;
	const bool useMapMetadata = sv_usemapsettingswavelimit && hasMapLimit;

	// Classic Skulltag/Zandronum online compatibility:
	// if enabled, prefer per-map metadata (CMPGNINF/MAPINFO wavelimit) when present.
	// Offline sessions are allowed to override it for skirmish/debug workflows.
	if (useMapMetadata && !canOverrideCmpgninfLimits)
	{
		resolved = clamp<int>(primaryLevel->info->InvasionWaveLimit, 1, 255);
	}

	// Explicit override for mods/scripts that still drive legacy wavelimit.
	// Online maps with enforced map limits keep that limit unless override mode is active.
	const int legacyLimit = clamp<int>(wavelimit_compat, 0, 255);
	if (legacyLimit > 0 && (canOverrideCmpgninfLimits || !useMapMetadata))
	{
		resolved = legacyLimit;
	}

	return resolved;
}

int Net_GetCompatDuelLimit()
{
	const bool canOverrideCmpgninfLimits = HCDE_CanOverrideCmpgninfLimits();
	const bool hasMapLimit = primaryLevel != nullptr && primaryLevel->info != nullptr && primaryLevel->info->DuelLimit > 0;

	// Skulltag/CMPGNINF compatibility: in online sessions map metadata
	// settings (duellimit) win unless an offline override mode is active.
	if (hasMapLimit && !canOverrideCmpgninfLimits)
	{
		return clamp<int>(primaryLevel->info->DuelLimit, 1, 255);
	}

	return clamp<int>(duellimit_compat, 0, 255);
}

static void Net_StartInvasionWave(const char* reason);

static void Net_StartInvasionCountdownForNextWave(const char* reason)
{
	InvasionWaveDirector.MaxWaves = Net_ResolveInvasionMaxWaves();
	if (InvasionWaveDirector.Wave >= InvasionWaveDirector.MaxWaves)
	{
		Net_SetInvasionState(INVS_VICTORY, Net_InvasionSecondsToTics(sv_invasionresulttime),
			reason != nullptr ? reason : "waves-complete");
		return;
	}

	// Skulltag invasion ACS expects GetInvasionWave() to already report the
	// pending wave while GetInvasionState() is IS_COUNTDOWN.
	Net_DestroyTrackedInvasionMonsters("wave-countdown");
	++InvasionWaveDirector.Wave;
	InvasionWaveDirector.WaveFlags = Net_IsInvasionBossWave(InvasionWaveDirector.Wave) ? INV_WAVEF_BOSS : 0u;
	InvasionWaveDirector.WaveBudget = 0;
	InvasionWaveDirector.WaveSpawned = 0;
	InvasionWaveDirector.WaveCleared = 0;
	InvasionWaveDirector.ActiveMonsters.Clear();
	InvasionSpawnDirectory.Reset();
	CutsceneCountdown = 0;

	const int countdownTics = max(Net_InvasionSecondsToTics(sv_invasioncountdowntime), 0);
	if (countdownTics > 0)
	{
		Net_SetInvasionState(INVS_COUNTDOWN, countdownTics, reason != nullptr ? reason : "wave-countdown");
	}
	else
	{
		Net_StartInvasionWave(reason != nullptr ? reason : "wave-countdown-complete");
	}
}

static void Net_StartInvasionWave(const char* reason)
{
	InvasionWaveDirector.MaxWaves = Net_ResolveInvasionMaxWaves();
	const bool usingPendingCountdownWave = InvasionState == INVS_COUNTDOWN
		&& InvasionWaveDirector.Wave > 0
		&& InvasionWaveDirector.WaveSpawned == 0
		&& InvasionWaveDirector.ActiveMonsters.Size() == 0;
	if (!usingPendingCountdownWave && InvasionWaveDirector.Wave >= InvasionWaveDirector.MaxWaves)
	{
		Net_SetInvasionState(INVS_VICTORY, Net_InvasionSecondsToTics(sv_invasionresulttime),
			reason != nullptr ? reason : "waves-complete");
		return;
	}

	// Defensive cleanup for forced transitions (console/API) so a new wave never
	// inherits live actors from an older wave.
	Net_DestroyTrackedInvasionMonsters("wave-start");

	if (!usingPendingCountdownWave)
		++InvasionWaveDirector.Wave;

	if ((dmflags & DF_FAST_MONSTERS) != 0)
	{
		dmflags = int(dmflags) & ~DF_FAST_MONSTERS;
		Printf(PRINT_HIGH, "HCDE invasion disabled fast monsters for wave %d\n", InvasionWaveDirector.Wave);
	}

	const bool bossWave = Net_IsInvasionBossWave(InvasionWaveDirector.Wave);
	InvasionWaveDirector.WaveFlags = bossWave ? INV_WAVEF_BOSS : 0u;
	InvasionWaveDirector.WaveBudget = Net_ComputeInvasionWaveBudget(InvasionWaveDirector.Wave, bossWave);
	InvasionWaveDirector.WaveSpawned = 0;
	InvasionWaveDirector.WaveCleared = 0;
	InvasionWaveDirector.ActiveMonsters.Clear();
	int unscaledWaveBudget = InvasionWaveDirector.WaveBudget;
	const int baseSpawnIntervalTics = max(Net_InvasionSecondsToTics(sv_invasionspawninterval), 1);
	InvasionWaveDirector.SpawnIntervalTics = max<int>(
		int(ceil(double(baseSpawnIntervalTics) * Net_GetInvasionSkillSpawnIntervalScale())),
		1);
	InvasionWaveDirector.SpawnIntervalCountdown = InvasionWaveDirector.SpawnIntervalTics;

	Net_RebuildInvasionSpawnSpots(InvasionWaveDirector.Wave);
	if (InvasionSpawnDirectory.TotalSpawnBudget > 0)
	{
		// Invasion spot records own wave budgets when a map provides native invasion
		// markers. The Stage 3 budget cvars remain as fallback for non-invasion maps.
		InvasionWaveDirector.WaveBudget = InvasionSpawnDirectory.TotalSpawnBudget;
		unscaledWaveBudget = InvasionWaveDirector.WaveBudget;
	}
	InvasionWaveDirector.WaveBudget = Net_ApplyInvasionSkillBudgetScale(InvasionWaveDirector.WaveBudget);

	int spawnWindow = Net_InvasionSecondsToTics(sv_invasionspawntime);
	if (spawnWindow <= 0)
	{
		const int burst = max<int>(sv_invasionspawnburst, 1);
		const int chunks = (InvasionWaveDirector.WaveBudget + burst - 1) / burst;
		spawnWindow = max(chunks * InvasionWaveDirector.SpawnIntervalTics, 1);
	}
	const int spotWindow = Net_ComputeInvasionSpotSpawnWindowFloorTics();
	if (spotWindow > spawnWindow)
	{
		Printf(PRINT_HIGH, "HCDE invasion spawn window extended: cvar=%d spot-floor=%d wave=%d/%d budget=%d spots=%d\n",
			spawnWindow, spotWindow, InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves,
			InvasionWaveDirector.WaveBudget, InvasionSpawnDirectory.ActiveSpotCount);
		spawnWindow = spotWindow;
	}

	DebugTrace::Markf("invasion", "wave=%d/%d budget=%d rawbudget=%d skill=%d spots=%d active=%d tag=%d fallback=%d source=%s interval=%d fastmonsters=%d map=%s",
		InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves, InvasionWaveDirector.WaveBudget,
		unscaledWaveBudget, clamp<int>(gameskill, 0, 4),
		InvasionSpawnDirectory.TotalSpotCount, InvasionSpawnDirectory.ActiveSpotCount, InvasionSpawnDirectory.ActiveTag,
		InvasionSpawnDirectory.UsingFallback ? 1 : 0, Net_InvasionSpawnSourceName(InvasionSpawnDirectory.FallbackSource),
		InvasionWaveDirector.SpawnIntervalTics,
		(dmflags & DF_FAST_MONSTERS) != 0 ? 1 : 0,
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	Printf(PRINT_HIGH, "Invasion wave %d/%d starting: budget=%d rawbudget=%d skill=%d spots=%d active=%d cap=%d speedscale=%.2f tag=%d fallback=%d source=%s interval=%d fastmonsters=%d map=%s\n",
		InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves, InvasionWaveDirector.WaveBudget,
		unscaledWaveBudget, clamp<int>(gameskill, 0, 4),
		InvasionSpawnDirectory.TotalSpotCount, InvasionSpawnDirectory.ActiveSpotCount,
		Net_GetInvasionActiveMonsterCap(), Net_GetInvasionSkillMonsterSpeedScale(),
		InvasionSpawnDirectory.ActiveTag,
		InvasionSpawnDirectory.UsingFallback ? 1 : 0, Net_InvasionSpawnSourceName(InvasionSpawnDirectory.FallbackSource),
		InvasionWaveDirector.SpawnIntervalTics,
		(dmflags & DF_FAST_MONSTERS) != 0 ? 1 : 0,
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");

	Net_SetInvasionState(INVS_SPAWNING, spawnWindow, reason != nullptr ? reason : "wave-start");
}

static void Net_MarkInvasionVictory(const char* reason)
{
	Net_SetInvasionState(INVS_VICTORY, Net_InvasionSecondsToTics(sv_invasionresulttime), reason);
}

static void Net_MarkInvasionFailure(const char* reason)
{
	Net_SetInvasionState(INVS_FAILURE, Net_InvasionSecondsToTics(sv_invasionresulttime), reason);
}

// Transitions the invasion mode to the next wave or victory after a wave is cleared.
static void Net_AdvanceInvasionAfterWaveClear(const char* reason)
{
	if (InvasionWaveDirector.Wave >= InvasionWaveDirector.MaxWaves)
	{
		Net_MarkInvasionVictory(reason != nullptr ? reason : "all-waves-cleared");
	}
	else
	{
		Net_SetInvasionState(INVS_INTERMISSION, Net_InvasionSecondsToTics(sv_invasionintermissiontime),
			reason != nullptr ? reason : "wave-cleared");
	}
}

// Main authoritative tick for the invasion state machine.
// Only runs on the network authority (server/host).
static void Net_TickInvasionState()
{
	// Mode is disabled globally via sv_gametype.
	if (!Net_IsInvasionModeEnabled())
	{
		if (InvasionState != INVS_DISABLED)
		{
			// Clean up and notify clients if the mode is toggled off during a level.
			Net_DestroyTrackedInvasionMonsters("mode-disabled");
			InvasionWaveDirector.Reset();
			InvasionSpawnDirectory.Reset();
			InvasionNoParticipantTics = 0;
			Net_SetInvasionState(INVS_DISABLED, 0, "mode-disabled");
		}
		return;
	}

	// Mode is enabled but has not been initialized for this level yet.
	if (InvasionState == INVS_DISABLED)
	{
		InvasionWaveDirector.Reset();
		InvasionSpawnDirectory.Reset();
		InvasionNoParticipantTics = 0;
		Net_SetInvasionState(INVS_WAITING, 0, "mode-enabled");
	}

	// Once the map is live and at least one participant exists, move out of the
	// idle waiting state automatically. This keeps native invasion maps from
	// sitting forever on "waiting" unless some external cutscene flow is used.
	if (InvasionState == INVS_WAITING)
	{
		if (Net_CountInvasionParticipants() > 0)
		{
			const int countdownTics = max(Net_InvasionSecondsToTics(sv_invasioncountdowntime), 0);
			if (countdownTics > 0)
			{
				Net_StartInvasionCountdownForNextWave("waiting-autostart");
			}
			else
			{
				Net_StartInvasionWave("waiting-autostart");
			}
		}
		return;
	}

	// Wait for countdown to finish (e.g. from Net_StartCutscene).
	if (InvasionState == INVS_COUNTDOWN)
	{
		// Support both flows:
		// - cutscene-ready countdowns continue to mirror the shared cutscene timer
		// - native invasion auto-starts count down locally when no cutscene timer exists
	if (CutsceneCountdown > 0)
	{
		if (!netgame)
			--CutsceneCountdown;
		InvasionStateTics = max(CutsceneCountdown, 0);
		if (CutsceneCountdown <= 0)
			Net_StartInvasionWave("countdown-complete");
		return;
	}

		if (InvasionStateTics > 0)
			--InvasionStateTics;

		if (InvasionStateTics <= 0)
			Net_StartInvasionWave("countdown-complete");

		return;
	}

	// Dedicated servers can temporarily have no active participants between
	// probe joins and real players. Keep the current invasion wave metadata
	// stable for a short reconnect window before forcing a safe reset.
	if (I_IsDedicatedServerMode()
		&& Net_IsInvasionRoundActiveState(InvasionState)
		&& Net_CountInvasionParticipants() <= 0)
	{
		InvasionWipeGraceTics = 0;
		if (InvasionNoParticipantTics <= 0)
		{
			InvasionNoParticipantTics = TICRATE * 30;
		}
		else if (--InvasionNoParticipantTics <= 0)
		{
			Net_ResetInvasionState("no-participants-timeout");
			return;
		}
	}
	else
	{
		InvasionNoParticipantTics = 0;
	}

	if (InvasionStateTics > 0)
		--InvasionStateTics;

	Net_UpdateInvasionWaveClearProgress();

	// Global player-wipe and participant-check logic.
	if (Net_IsInvasionRoundActiveState(InvasionState))
	{
		const int participants = Net_CountInvasionParticipants();
		if (participants <= 0)
		{
			// No active participants remain; reset any active wave state.
			Net_ResetInvasionState("no-participants");
			return;
		}

		const int aliveParticipants = Net_CountInvasionAliveParticipants();
		if (aliveParticipants <= 0)
		{
			// If every participant is dead at the same time, treat this as a wipe.
			// Keep a short grace period so same-tic state transitions do not
			// immediately trigger failure.
			if (InvasionWipeGraceTics <= 0)
			{
				InvasionWipeGraceTics = TICRATE;
			}
			else if (--InvasionWipeGraceTics <= 0)
			{
				Net_MarkInvasionFailure("all-players-dead");
				InvasionWipeGraceTics = 0;
			}
		}
		else
		{
			InvasionWipeGraceTics = 0;
		}
	}

	switch (InvasionState)
	{
	case INVS_SPAWNING:
		// Initialize the wave director if this is the first spawn tick.
		if (InvasionWaveDirector.Wave <= 0)
			Net_StartInvasionWave("spawn-wave-bootstrap");

		if (InvasionWaveDirector.WaveSpawned < InvasionWaveDirector.WaveBudget)
		{
			// Process spawn burst based on interval.
			if (--InvasionWaveDirector.SpawnIntervalCountdown <= 0)
			{
				InvasionWaveDirector.SpawnIntervalCountdown = InvasionWaveDirector.SpawnIntervalTics;
				const int activeCap = Net_GetInvasionActiveMonsterCap();
				const int activeMonsters = Net_GetInvasionActiveMonsterCount();
				if (activeMonsters >= activeCap)
				{
					DebugTrace::Markf("invasion", "spawn throttled active=%d cap=%d wave=%d/%d spawned=%d/%d",
						activeMonsters, activeCap, InvasionWaveDirector.Wave, InvasionWaveDirector.MaxWaves,
						InvasionWaveDirector.WaveSpawned, InvasionWaveDirector.WaveBudget);
				}
				else
				{
					const int burst = min<int>(max<int>(sv_invasionspawnburst, 1), activeCap - activeMonsters);
					int consumed = 0;
					while (consumed < burst && InvasionWaveDirector.WaveSpawned < InvasionWaveDirector.WaveBudget)
					{
						if (!Net_TryConsumeInvasionSpawnSlot())
							break;
						++consumed;
					}
				}
			}
		}
		Net_UpdateInvasionWaveClearProgress();

		// If the wave budget is fully spawned and already fully cleared, advance
		// immediately without waiting for cleanup timeout.
		if (InvasionWaveDirector.WaveSpawned >= InvasionWaveDirector.WaveBudget
			&& InvasionWaveDirector.WaveCleared >= InvasionWaveDirector.WaveBudget)
		{
			Net_AdvanceInvasionAfterWaveClear("wave-cleared-during-spawn");
			break;
		}

		// Move to cleanup phase once spawning is done or the spawn window expires.
		if (InvasionWaveDirector.WaveSpawned >= InvasionWaveDirector.WaveBudget || InvasionStateTics <= 0)
		{
			if (InvasionWaveDirector.WaveSpawned < InvasionWaveDirector.WaveBudget
				&& Net_HasPendingClassicInvasionSpotSpawns())
			{
				// Native invasion spots may intentionally delay late spawns. Do
				// not end the spawn phase while a planned spot still has budget.
				InvasionStateTics = TICRATE;
			}
			else
			{
				Net_SetInvasionState(INVS_CLEANUP, Net_InvasionSecondsToTics(sv_invasioncleanuptime),
					InvasionWaveDirector.WaveSpawned >= InvasionWaveDirector.WaveBudget ? "wave-spawned" : "spawn-timeout");
			}
		}
		break;

	case INVS_CLEANUP:
		// Transition to next wave or victory once all monsters are cleared.
		if (InvasionWaveDirector.WaveCleared >= InvasionWaveDirector.WaveBudget)
		{
			Net_AdvanceInvasionAfterWaveClear("cleanup-complete");
		}
		// Skulltag/Zandronum invasion waves do not fail just because spawned
		// monsters lived longer than a short cleanup timer. Keep the wave open
		// until players clear the remaining monsters or everyone wipes.
		else if (InvasionStateTics <= 0)
		{
			InvasionStateTics = TICRATE;
		}
		break;

	case INVS_INTERMISSION:
		// Brief pause between waves.
		if (InvasionStateTics <= 0)
			Net_StartInvasionCountdownForNextWave("intermission-complete");
		break;

	case INVS_VICTORY:
	case INVS_FAILURE:
		// Return to waiting state after the result screen time.
		if (InvasionStateTics <= 0)
			Net_ResetInvasionState("result-finished");
		break;

	default:
		break;
	}
}

static void Net_TickInvasionMirrorVisualFrame()
{
	if (Net_IsLocalInvasionAuthority() || InvasionState == INVS_DISABLED)
		return;

	Net_DrainPendingInvasionSpawnEvents();
	Net_DrainPendingInvasionMirrorSpawns();
	if (InvasionMirrorVisualTickBudget > 0)
	{
		// Catch-up can run several gameplay tics in one frame. Mirror actors are
		// visual-only server replicas, so move them once per frame and let the
		// latest authoritative target correct them instead of multiplying speed.
		--InvasionMirrorVisualTickBudget;
		Net_TickInvasionMirrorVisualActors();
	}
	Net_LogInvasionMirrorVisualDiagnostic();
}

static void Net_TickInvasionMirrorState()
{
	Net_TickInvasionMirrorVisualFrame();
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
	LevelStartStatus = LST_READY;
	LastTicGateStallTrace = 0;

	FullLatencyCycle = MAXSENDTICS * 3;
	LastLatencyUpdate = 0;

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
}

void Net_StartCutscene()
{
	CutsceneReady = 0u;
	for (int i = 0; i < MAXPLAYERS; ++i)
		CutsceneReadyLastToggle[i] = gametic - TICRATE;

	float countdownSeconds = net_cutscenecountdown;
	// sv_gametype 4 is the native Invasion identity and owns its own pacing.
	if (sv_gametype == 4)
		countdownSeconds = sv_invasioncountdowntime;

	const bool shouldUseCountdown = !demoplayback
		&& countdownSeconds > 0.0f
		&& (netgame || Net_IsInvasionModeEnabled());
	CutsceneCountdown = shouldUseCountdown ? static_cast<int>(ceil(countdownSeconds * TICRATE)) : 0;
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

	if (net_cutscenereadytype == RT_ANYONE)
	{
		for (auto client : NetworkClients)
		{
			if (Net_IsCutsceneReadyParticipant(client) && (CutsceneReady & ((uint64_t)1u << client)))
				return true;
		}
		return false;
	}

	if (net_cutscenereadytype == RT_HOST_ONLY)
	{
		const int readyHost = Net_GetCutsceneReadyHost();
		return readyHost >= 0 && (CutsceneReady & ((uint64_t)1u << readyHost));
	}

	uint64_t mask = 0u;
	uint64_t readyMask = 0u;
	int totalReady = 0;
	int totalClients = 0;
	for (auto client : NetworkClients)
	{
		if (!Net_IsCutsceneReadyParticipant(client))
			continue;

		mask |= (uint64_t)1u << client;
		++totalClients;
		if (Net_IsPlayerReady(client))
		{
			readyMask |= (uint64_t)1u << client;
			++totalReady;
		}
	}

	if (totalClients <= 0)
		return false;

	if ((readyMask & mask) == mask)
		return true;

	const float readyRatio = (float)totalReady / totalClients;
	const bool thresholdReached = readyRatio >= net_cutscenereadypercent;

	// Keep the countdown progressing even when nobody reaches the ready threshold.
	// This avoids an intermission deadlock if input focus is lost or clients never
	// send DEM_READIED during map transitions.
	if (CutsceneCountdown > 0)
	{
		--CutsceneCountdown;
		if (CutsceneCountdown <= 0)
		{
			DebugTrace::Markf("net", "cutscene countdown auto-advance ratio=%.3f ready=%d/%d room=%u map=%s",
				double(readyRatio), totalReady, totalClients, unsigned(CurrentRoomID),
				primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
			return true;
		}
		return false;
	}

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
		state.CurrentSequence = min<int>(state.CurrentSequence, tic);
		state.SequenceAck = min<int>(state.SequenceAck, tic);
		if (state.ResendSequenceFrom >= tic)
			state.ResendSequenceFrom = -1;

		// Make sure not to run its current command either.
		auto& curTic = state.Tics[tic % BACKUPTICS];
		const int running = (curTic.Command.buttons & BT_RUN); // This isn't delta'd so needs to be kept.
		memset(&curTic.Command, 0, sizeof(curTic.Command));
		curTic.Command.buttons |= running;
	}

	NetEvents.ResetStream();
	Net_ResetInvasionState("reset-commands");
}

void Net_SetWaiting()
{
	if (netgame && !demoplayback && NetworkClients.Size() > 1)
	{
		LevelStartAck = 0u;
		LevelStartDelay = LevelStartDebug = 0;
		FullLatencyCycle = 0;
		Net_ResetAuthorityWaitWatchdog("level-wait");
		LevelStartStatus = LST_WAITING;
		DebugTrace::Markf("net", "level start waiting room=%u map=%s authority=%d clients=%u",
			unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
			I_IsLocalHCDEServiceAuthority() ? 1 : 0, unsigned(NetworkClients.Size()));
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
		int bytes = 2;
		if (I_IsHCDEServiceAuthoritySlot(RemoteClient))
			bytes += 2;

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
			players[client].playerstate = PST_ENTER;
	}

	Net_SetLateJoinSyncPending(client, max<int>(currentSequence - 1, 0), max<int>(currentConsistency - 1, 0), "runtime-connect");
	if (!players[client].waiting)
	{
		players[client].waiting = true;
		Net_ResetAuthorityWaitWatchdog("late-join-connect");
		DebugTrace::Markf("net", "late-join setup active client=%d room=%u gametic=%d clienttic=%d replay-seq-from=%d replay-consistency-from=%d",
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
		DebugTrace::Markf("net", "runtime client slot activated client=%d source=%d room=%u gametic=%d clienttic=%d",
			client, sourceClient, unsigned(CurrentRoomID), gametic, ClientTic);
	}

	const bool reserved = I_IsServerReservedSlot(client);
	playeringame[client] = !reserved;
	if (!reserved && (players[client].playerstate == PST_GONE || players[client].playerstate == PST_DEAD))
	{
		players[client].playerstate = PST_ENTER;
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
	ConsistencyGraceUntilTic[clientNum] = 0;

	for (auto& tic : state.Tics)
		tic.Data.SetData(nullptr, 0);

	HCDELivePeers[clientNum].Clear();
	Net_ResetInvasionActorDeltaBaseline(clientNum);
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
	players[clientNum].playerstate = PST_GONE;
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
			DebugTrace::Warningf("net", "client exit queued client=%d name=%s gametic=%d clienttic=%d room=%u",
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

		HSendPacket(client, 4);
	}

	LevelStartAck = 0u;
	LevelStartStatus = I_IsLocalHCDEServiceAuthority() ? LST_HOST : LST_READY;
	LevelStartDelay = LevelStartDebug = 0;
	Net_ResetAuthorityWaitWatchdog("authority-release");
	Printf(PRINT_HIGH, "NetGame:: Authority released level start at gametic=%d clienttic=%d room=%u map=%s clients=%u\n",
		gametic, ClientTic, unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		unsigned(NetworkClients.Size()));
	DebugTrace::Markf("net", "authority released level start gametic=%d clienttic=%d room=%u map=%s clients=%u",
		gametic, ClientTic, unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		unsigned(NetworkClients.Size()));
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

			HSendPacket(client, 4);
		}

		return;
	}

	if (I_IsHCDEServiceAuthoritySlot(client))
	{
		LevelStartAck = 0u;
		LevelStartStatus = I_IsLocalHCDEServiceAuthority() ? LST_HOST : LST_READY;
		LevelStartDelay = LevelStartDebug = delayTics;
		Net_ResetAuthorityWaitWatchdog("authority-start");
		Printf(PRINT_HIGH, "NetGame:: Authority started level for client at gametic=%d clienttic=%d room=%u map=%s delay=%d\n",
			gametic, ClientTic, unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>", delayTics);
		DebugTrace::Markf("net", "client accepted authority level start gametic=%d clienttic=%d room=%u map=%s delay=%d",
			gametic, ClientTic, unsigned(CurrentRoomID), primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>", delayTics);
		return;
	}

	LevelStartAck |= (uint64_t)1u << client;
	DebugTrace::Markf("net", "level start ack client=%d ack=%llu mask=%llu loaded=%d room=%u",
		client, (unsigned long long)LevelStartAck, (unsigned long long)LevelStartPlayableMask(),
		IsMapLoaded() ? 1 : 0, unsigned(CurrentRoomID));
	TryReleaseLevelStart();
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

		if (NetBuffer[0] & NCMD_EXIT)
		{
			Printf(PRINT_HIGH, "NetGame:: Received exit packet from client %d at gametic=%d clienttic=%d room=%u\n",
				clientNum, gametic, ClientTic, unsigned(CurrentRoomID));
			DebugTrace::Warningf("net", "received exit packet client=%d gametic=%d clienttic=%d room=%u",
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

		if (NetBuffer[0] & NCMD_RETRANSMIT)
		{
			clientState.ResendID = NetBuffer[1];
			clientState.Flags |= CF_RETRANSMIT;
		}

		// Command packets must include room id, sequence ack, and consistency ack.
		// Treat undersized packets as missing/corrupt and request retransmit.
		if (NetBufferLength < 10u)
		{
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
				clientState.Flags |= CF_MISSING_SEQ;
				continue;
			}
		}

		if (!hasBytes(1))
		{
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}
		const int playerCount = NetBuffer[curByte++];

		int baseSequence = -1;
		if (!hasBytes(1))
		{
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}
		const int totalTics = NetBuffer[curByte++];
		if (totalTics > 0)
		{
			if (!hasBytes(4))
			{
				clientState.Flags |= CF_MISSING_SEQ;
				continue;
			}
			baseSequence = (NetBuffer[curByte++] << 24) | (NetBuffer[curByte++] << 16) | (NetBuffer[curByte++] << 8) | NetBuffer[curByte++];
		}

		int baseConsistency = -1;
		if (!hasBytes(1))
		{
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}
		const int ranTics = NetBuffer[curByte++];
		if (ranTics > 0)
		{
			if (!hasBytes(4))
			{
				clientState.Flags |= CF_MISSING_SEQ;
				continue;
			}
			baseConsistency = (NetBuffer[curByte++] << 24) | (NetBuffer[curByte++] << 16) | (NetBuffer[curByte++] << 8) | NetBuffer[curByte++];
		}

		if (!hasBytes(1))
		{
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
				DebugTrace::Warningf("net", "malformed packet player index=%d max=%d from=%d room=%u",
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
			clientState.Flags |= CF_MISSING_SEQ;
			continue;
		}
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

static bool Net_ServerAuthoritativeReplicationReady()
{
	if (!I_UsesDedicatedServerSlot())
		return false;

	if (I_IsLocalHCDEServiceAuthority())
	{
		for (auto client : NetworkClients)
		{
			if (client == consoleplayer || I_IsServerReservedSlot(client) || !I_ClientUsesHCDEService(client))
				continue;
			if (HCDELivePeers[client].SnapshotSent == 0u)
				return false;
		}
		return true;
	}

	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	return authoritySlot >= 0
		&& authoritySlot < MAXPLAYERS
		&& I_ClientUsesHCDEService(authoritySlot)
		&& HCDELivePeers[authoritySlot].SnapshotReceived > 0u;
}

static bool Net_UsesServerAuthoritativeConsistency()
{
	// Dedicated-slot HCDE sessions switch to server-auth consistency only after
	// the live snapshot route is proven. Until then, keep legacy checks active
	// so a broken authority handoff still reports a real desync instead of
	// hiding behind zeroed RNG consistency markers.
	return Net_ServerAuthoritativeReplicationReady();
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

		DebugTrace::Warningf("net", "desync live-peer client=%d tx=%u rx=%u dup=%u control-tx=%u control-rx=%u cmd-tx=%u cmd-rx=%u snap-tx=%u snap-rx=%u unsupported=%u rejected=%u deltas=%u repairs=%u drift=%u reconciliations=%u hard=%u",
			pNum, peer.TxSequence, peer.RxSequence, peer.DuplicateCount, peer.ControlSent, peer.ControlReceived,
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
			DebugTrace::Warningf("net", "authority wait armed client=%d gametic=%d clienttic=%d room=%u map=%s seq=%d ack=%d levelstart=%s lag=%s",
				authoritySlot, gametic, ClientTic, unsigned(CurrentRoomID),
				primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
				ClientStates[authoritySlot].CurrentSequence, ClientStates[authoritySlot].SequenceAck,
				Net_LevelStartStatusName(LevelStartStatus), Net_LagStateName(LagState));
		}
	}

	if (LevelStartStatus == LST_HOST)
		return false;

	for (auto client : NetworkClients)
	{
		if (Net_IsLateJoinSyncPending(client))
			continue;
		if (players[client].waiting)
			return false;
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
			if (SkipCommandTimer++ > TICRATE / 2)
			{
				SkipCommandTimer = 0;
				if (SkipCommandAmount <= 0)
					SkipCommandAmount = lowestDiff * TicDup;
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
			DebugTrace::Markf("net", "host heartbeat gametic=%d", gametic);
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
			if (NetworkClients.Size() == 1)
			{
				// If we got stuck in limbo waiting, force start the map.
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
		else if (LevelStartStatus == LST_HOST)
		{
			// If we're the host, idly wait until all packets have arrived. There's no point in predicting since we
			// know for a fact the game won't be started until everyone is accounted for.
			const int curTic = gametic / TicDup;
			int lowestSeq = curTic;
			for (auto client : NetworkClients)
			{
				if (!I_IsHCDEServiceAuthoritySlot(client) && ClientStates[client].CurrentSequence < lowestSeq)
					lowestSeq = ClientStates[client].CurrentSequence;
			}

			// Let normal tic availability gate the actual playsim. Requiring the
			// first post-load tic here can deadlock clients that are waiting for the
			// authority's first snapshot before they release their next command.
			if (lowestSeq >= curTic - 1)
			{
				DebugTrace::Markf("net", "authority level start host gate released curtic=%d lowestseq=%d room=%u map=%s",
					curTic, lowestSeq, unsigned(CurrentRoomID),
					primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
				LevelStartStatus = LST_READY;
			}
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
	const int commandLeadLimit = I_UsesDedicatedServerSlot() && !I_IsLocalHCDEServiceAuthority()
		? BACKUPTICS - MAXSENDTICS
		: BACKUPTICS / 2;
	if ((startTic + tics - gametic) / TicDup > commandLeadLimit)
	{
		tics = (gametic + commandLeadLimit * TicDup) - startTic;
		if (tics <= 0)
		{
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
		int lowestSeq = endSequence - 1;
		for (auto client : NetworkClients)
		{
			if (I_IsHCDEServiceAuthoritySlot(client))
				continue;

			const bool lateJoinPending = Net_IsLateJoinSyncPending(client);
			if (ClientStates[client].Flags & CF_QUIT)
			{
				quitNums[quitters++] = client;
			}
			else
			{
				++players;
				if (!lateJoinPending && ClientStates[client].CurrentSequence < lowestSeq)
					lowestSeq = ClientStates[client].CurrentSequence;
			}
		}

		endSequence = lowestSeq + 1;

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
		if (localAuthority && !isSelf && curState.SequenceAck >= 0 && curState.SequenceAck + 1 < startSequence)
		{
			// Dedicated clients can occasionally miss the explicit retransmit edge
			// while still reporting an older sequence ack every input packet. Honor
			// that ack as a soft resend request so one lost authority tic cannot
			// leave the client stuck at the tic gate forever.
			passiveResendSequenceFrom = curState.SequenceAck + 1;
			DebugTrace::Warningf("net", "passive authority resend client=%d from=%d start=%d end=%d seq=%d ack=%d room=%u",
				client, passiveResendSequenceFrom, startSequence, endSequence,
				curState.CurrentSequence, curState.SequenceAck, unsigned(CurrentRoomID));
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
							WriteBytes(TArrayView(stream.Stream, stream.Used), cmd);

							WriteUserCmdMessage(LocalCmds[realTic],
								realLastTic >= 0 ? &LocalCmds[realLastTic] : nullptr, cmd);
						}
						else
						{
							auto& netTic = clientState.Tics[curTic % BACKUPTICS];

							auto data = netTic.Data.GetTArrayView();
							WriteBytes(data, cmd);

							WriteUserCmdMessage(netTic.Command,
								lastTic >= 0 ? &clientState.Tics[lastTic % BACKUPTICS].Command : nullptr, cmd);
						}
					}
				}

				HSendLiveGameplayPacket(client, int(cmd.Data() - NetBuffer));
				if (net_extratic && !isSelf)
					HSendLiveGameplayPacket(client, int(cmd.Data() - NetBuffer));
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

// These have to be here since they have game-specific data. Only the data
// from the frontend should be put in these, all backend handling should be
// done in the core files.

size_t Net_SetEngineInfo(uint8_t*& stream)
{
	stream[0] = VER_MAJOR % 256;
	stream[1] = VER_MINOR % 256;
	stream[2] = VER_REVISION % 256;

	// Send over any loaded files to ensure their checksum is correct.
	size_t numWads = 0u;
	size_t bufferIndex = 7u;
	for (size_t i = 0u; i < fileSystem.GetNumWads(); ++i)
	{
		if (fileSystem.IsOptionalResource(i))
			continue;

		++numWads;
		const FString crc = fileSystem.GetResourceHash(i);
		memcpy(&stream[bufferIndex], crc.GetChars(), crc.Len() + 1u);
		bufferIndex += crc.Len() + 1u;
	}

	stream[3] = (numWads >> 24);
	stream[4] = (numWads >> 16);
	stream[5] = (numWads >> 8);
	stream[6] = numWads;

	return bufferIndex;
}

static bool ReadPacketString(const uint8_t* stream, size_t packetLength, size_t& offset, FString& value)
{
	if (offset >= packetLength)
		return false;

	const size_t start = offset;
	while (offset < packetLength && stream[offset] != 0u)
		++offset;

	if (offset >= packetLength)
		return false;

	value = FString(reinterpret_cast<const char*>(&stream[start]), offset - start);
	++offset;
	return true;
}

FVerificationError Net_VerifyEngine(uint8_t*& stream, size_t& offset, size_t packetLength)
{
	FVerificationError error = {};

	TArray<FString> crcs = {};
	TArray<FString> names = {};
	for (size_t i = 0u; i < fileSystem.GetNumWads(); ++i)
	{
		if (!fileSystem.IsOptionalResource(i))
		{
			crcs.Push(fileSystem.GetResourceHash(i));
			names.Push(fileSystem.GetResourceFileName(i));
		}
	}

	if (packetLength < 7u)
	{
		error.Error = FVerificationError::VE_ENGINE;
		error.Major = VER_MAJOR % 256;
		error.Minor = VER_MINOR % 256;
		error.Revision = VER_REVISION % 256;
		error.NetMajor = 0;
		error.NetMinor = 0;
		error.NetRevision = 0;
		return error;
	}

	const size_t numWads = (stream[3] << 24) | (stream[4] << 16) | (stream[5] << 8) | stream[6];
	if (numWads < crcs.Size())
		error.Error = FVerificationError::VE_FILE_MISSING;
	else if (numWads > crcs.Size())
		error.Error = FVerificationError::VE_FILE_UNKNOWN;

	TArray<size_t> unverified = {};
	for (size_t i = 0u; i < crcs.Size(); ++i)
		unverified.Push(i);

	offset = 7u;
	for (size_t i = 0u; i < numWads; ++i)
	{
		FString netCrc = {};
		if (!ReadPacketString(stream, packetLength, offset, netCrc))
		{
			error.Error = FVerificationError::VE_ENGINE;
			error.Major = VER_MAJOR % 256;
			error.Minor = VER_MINOR % 256;
			error.Revision = VER_REVISION % 256;
			error.NetMajor = stream[0];
			error.NetMinor = stream[1];
			error.NetRevision = stream[2];
			return error;
		}

		if (error.Error == FVerificationError::VE_FILE_UNKNOWN)
		{
			if (crcs.Find(netCrc) >= crcs.Size())
				error.UnknownFiles.Push(netCrc);
		}
		else if (crcs[i] != netCrc)
		{
			const size_t c = crcs.Find(netCrc);
			if (c >= crcs.Size())
			{
				error.Error = FVerificationError::VE_FILE_UNKNOWN;
				error.UnknownFiles.Push(netCrc);
			}
			else
			{
				if (error.Error == FVerificationError::VE_NONE)
					error.Error = FVerificationError::VE_FILE_ORDER;
				unverified.Delete(unverified.Find(c));
			}
		}
		else
		{
			unverified.Delete(unverified.Find(i));
		}
	}

	if (error.Error == FVerificationError::VE_FILE_MISSING)
	{
		for (auto i : unverified)
		{
			FixPathSeperator(names[i]);
			auto ar = names[i].Split('/', FString::TOK_SKIPEMPTY);
			error.MissingFiles.Push(ar.Last());
		}
	}
	else if (error.Error == FVerificationError::VE_FILE_ORDER)
	{
		error.ExpectedOrder = crcs;
		// Remove the core and iwad files.
		error.ExpectedOrder.Delete(0);
		error.ExpectedOrder.Delete(0);
	}

	// Intentionally do this last to avoid messing with the above loop.
	if (stream[0] != (VER_MAJOR % 256) || stream[1] != (VER_MINOR % 256) || stream[2] != (VER_REVISION % 256))
	{
		error.Error = FVerificationError::VE_ENGINE;
		error.Major = VER_MAJOR % 256;
		error.Minor = VER_MINOR % 256;
		error.Revision = VER_REVISION % 256;
		error.NetMajor = stream[0];
		error.NetMinor = stream[1];
		error.NetRevision = stream[2];
	}

	return error;
}

void Net_SetupUserInfo()
{
	D_SetupUserInfo();
}

const char* Net_GetClientName(int client, unsigned int charLimit)
{
	if (client < 0 || client >= MAXPLAYERS)
	{
		return "unknown";
	}
	return players[client].userinfo.GetName(charLimit);
}

void Net_GetKickableClientList(TArray<int>& clients, TArray<FString>& labels)
{
	clients.Clear();
	labels.Clear();

	if (!netgame)
	{
		return;
	}

	for (auto client : NetworkClients)
	{
		if (client == consoleplayer || client < 0 || client >= MAXPLAYERS || I_IsHCDEServiceAuthoritySlot(client))
		{
			continue;
		}

		FString label;
		label.Format("%s [%d]", Net_GetClientName(client, 24u), client);
		clients.Push(client);
		labels.Push(label);
	}
}

void Net_SetUserInfo(int client, TArrayView<uint8_t>& stream)
{
	auto str = D_GetUserInfoStrings(client, true);
	WriteFString(str, stream);
}

void Net_ReadUserInfo(int client, TArrayView<uint8_t>& stream)
{
	D_ReadUserInfoStrings(client, stream, false);
}

void Net_SetMapLoadInfo(TArrayView<uint8_t>& stream)
{
	WriteFString(startmap, stream);
	WriteInt32(rngseed, stream);

	auto load = Args->CheckValue(FArg_loadgame);
	if (load != nullptr)
	{
		WriteInt8(1, stream);
		WriteString(load, stream);
	}
	else
	{
		WriteInt8(0, stream);
	}
}

void Net_ReadMapLoadInfo(TArrayView<uint8_t>& stream)
{
	startmap = ReadStringConst(stream);
	rngseed = ReadInt32(stream);

	if (ReadInt8(stream))
	{
		auto load = ReadString(stream);
		// Don't override the existing argument in case they need to use
		// a custom savefile name.
		if (!Args->CheckParm(FArg_loadgame))
		{
			Args->AppendArg(FArg_loadgame);
			Args->AppendRawArg(load);
		}
	}

	// Reset this immediately so any further RNG calls the engine has to make will be synced.
	FRandom::StaticClearRandom();
}

void Net_SetServerInfo(TArrayView<uint8_t>& stream)
{
	C_WriteCVars(stream, CVAR_SERVERINFO, true);
}

void Net_ReadServerInfo(TArrayView<uint8_t>& stream)
{
	C_ReadCVars(stream);
}

void Net_SetGameInfo(TArrayView<uint8_t>& stream)
{
	Net_SetMapLoadInfo(stream);
	Net_SetServerInfo(stream);
}

void Net_ReadGameInfo(TArrayView<uint8_t>& stream)
{
	Net_ReadMapLoadInfo(stream);
	Net_ReadServerInfo(stream);
}

// Connects players to each other if needed.
bool D_CheckNetGame()
{
	if (!I_InitNetwork())
		return false;

	if (Args->CheckParm(FArg_extratic))
		net_extratic = true;

	HCDEInitializeAuthoritySession();
	for (auto client : NetworkClients)
	{
		if (I_IsServerReservedSlot(client))
		{
			// The dedicated authority is a transport slot, not a pawn. Keeping it
			// in playeringame lets it consume/block coop starts on maps with a
			// single player start, which can trap real clients on respawn.
			playeringame[client] = false;
			continue;
		}
		playeringame[client] = true;
	}

	if (MaxClients > 1u)
	{
		const int visibleClients = I_GetVisibleMaxClients();
		if (I_IsDedicatedServerMode())
		{
			Printf("Dedicated server for %d player%s\n", visibleClients, visibleClients == 1 ? "" : "s");
		}
		else
		{
			const int visiblePlayer = I_ToVisibleClientSlot(consoleplayer) + 1;
			Printf("Player %d of %d\n", visiblePlayer, visibleClients);
		}
	}

	return true;
}

//
// D_QuitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void D_QuitNetGame(const char* reason)
{
	if (!netgame || !usergame || consoleplayer == -1 || demoplayback || NetworkClients.Size() == 1)
		return;

	const char* quitReason = reason != nullptr && reason[0] != '\0' ? reason : "unspecified";
	Printf(PRINT_HIGH, "NetGame:: Local player %d '%s' leaving net game reason=%s at gametic=%d clienttic=%d room=%u map=%s gamestate=%d gameaction=%d levelstart=%d clients=%u\n",
		consoleplayer, players[consoleplayer].userinfo.GetName(), quitReason, gametic, ClientTic, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		int(gamestate), int(gameaction), int(LevelStartStatus), unsigned(NetworkClients.Size()));
	DebugTrace::Warningf("net", "local quit player=%d name=%s reason=%s gametic=%d clienttic=%d room=%u map=%s gamestate=%d gameaction=%d levelstart=%d clients=%u",
		consoleplayer, players[consoleplayer].userinfo.GetName(), quitReason, gametic, ClientTic, unsigned(CurrentRoomID),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
		int(gamestate), int(gameaction), int(LevelStartStatus), unsigned(NetworkClients.Size()));

	// Send a bunch of packets for stability.
	NetBuffer[0] = NCMD_EXIT;
	const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
	if (I_IsLocalHCDEServiceAuthority())
	{
		// This currently doesn't really do anything, but it's being split off into its
		// own branch should proper host migration be added in the future (i.e. sending over stored event
		// data rather than just dropping it entirely).
		NetBuffer[1] = HCDESelectNextServiceAuthoritySlot(authoritySlot);
		for (int i = 0; i < 4; ++i)
		{
			for (auto client : NetworkClients)
			{
				if (!I_IsHCDEServiceAuthoritySlot(client))
					HSendPacket(client, 2);
			}

			I_WaitVBL(1);
		}
	}
	else
	{
		for (int i = 0; i < 4; ++i)
		{
			// Only the service authority should know about this information.
			HSendPacket(authoritySlot, 1);
			I_WaitVBL(1);
		}
	}
}

ADD_STAT(network)
{
	FString out = {};
	if (!netgame || demoplayback)
	{
		out.AppendFormat("No network stats available.");
		return out;
	}

	out.AppendFormat("Max players: %d\tTic dup: %d",
		MaxClients,
		TicDup);

	if (net_extratic)
		out.AppendFormat("\tExtra tic enabled");

	const bool localAuthority = I_IsLocalHCDEServiceAuthority();
	out.AppendFormat("\nWorld tic: %06d (sequence %06d)", gametic, gametic / TicDup);
	if (!localAuthority)
		out.AppendFormat("\tStart tics delay: %d", LevelStartDebug);

	const int delay = max<int>((ClientTic - gametic) / TicDup, 0);
	const int msDelay = min<int>(delay * TicDup * 1000.0 / TICRATE, 999);
	const int buffer = max<int>(StabilityBuffer, 0);
	const int msBuffer = min<int>(buffer * 1000.0 / TICRATE, 999);
	out.AppendFormat("\nLocal\n\tIs authority: %d\tDelay: %02d (%03dms)\tStability Buffer: %02d (%03dms)",
		localAuthority,
		delay, msDelay,
		buffer, msBuffer);

	if (!localAuthority && consoleplayer >= 0 && consoleplayer < MAXPLAYERS)
		out.AppendFormat("\tAvg latency: %03ums", min<unsigned int>(ClientStates[consoleplayer].AverageLatency, 999u));

	if (LevelStartStatus != LST_READY)
	{
		if (LevelStartStatus == LST_HOST)
			out.AppendFormat("\tWaiting for packets");
		else if (localAuthority)
			out.AppendFormat("\tWaiting for acks");
		else
			out.AppendFormat("\tWaiting for authority");
	}

	int lowestSeq = ClientTic / TicDup;
	for (auto client : NetworkClients)
	{
		if (client == consoleplayer)
			continue;

		auto& state = ClientStates[client];
		if (state.CurrentSequence < lowestSeq)
			lowestSeq = state.CurrentSequence;

		out.AppendFormat("\n%s", players[client].userinfo.GetName(12));
		if (I_IsHCDEServiceAuthoritySlot(client))
			out.AppendFormat("\t(Authority)");

		if ((state.Flags & CF_RETRANSMIT) == CF_RETRANSMIT)
			out.AppendFormat("\t(RT)");
		else if (state.Flags & CF_RETRANSMIT_SEQ)
			out.AppendFormat("\t(RT SEQ)");
		else if (state.Flags & CF_RETRANSMIT_CON)
			out.AppendFormat("\t(RT CON)");

		if ((state.Flags & CF_MISSING) == CF_MISSING)
			out.AppendFormat("\t(MISS)");
		else if (state.Flags & CF_MISSING_SEQ)
			out.AppendFormat("\t(MISS SEQ)");
		else if (state.Flags & CF_MISSING_CON)
			out.AppendFormat("\t(MISS CON)");

		out.AppendFormat("\n");

		out.AppendFormat("\tAck: %06d\tConsistency: %06d", state.SequenceAck, state.ConsistencyAck);
		if (!I_IsHCDEServiceAuthoritySlot(client))
			out.AppendFormat("\tAvg latency: %03ums", min<unsigned int>(state.AverageLatency, 999u));
	}

	if (localAuthority)
		out.AppendFormat("\nAvailable tics: %03d", max<int>(lowestSeq - (gametic / TicDup), 0));
	return out;
}

// Forces playsim processing time to be consistent across frames.
// This improves interpolation for frames in between tics.
//
// With this cvar off the mods with a high playsim processing time will appear
// less smooth as the measured time used for interpolation will vary.

CVAR(Bool, r_ticstability, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

static uint64_t stabilityticduration = 0;
static uint64_t stabilitystarttime = 0;

static void TicStabilityWait()
{
	using namespace std::chrono;
	using namespace std::this_thread;

	if (!r_ticstability)
		return;

	uint64_t start = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	while (true)
	{
		uint64_t cur = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
		if (cur - start > stabilityticduration)
			break;
	}
}

static void TicStabilityBegin()
{
	using namespace std::chrono;
	stabilitystarttime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

static void TicStabilityEnd()
{
	using namespace std::chrono;
	uint64_t stabilityendtime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	stabilityticduration = min(stabilityendtime - stabilitystarttime, (uint64_t)1'000'000);
}

// Don't stabilize tics that are going to have incredibly long pauses in them.
static bool ShouldStabilizeTick()
{
	return gameaction != ga_recordgame && gameaction != ga_newgame && gameaction != ga_newgame2
			&& gameaction != ga_loadgame && gameaction != ga_loadgamehidecon && gameaction != ga_autoloadgame && gameaction != ga_loadgameplaydemo
			&& gameaction != ga_savegame && gameaction != ga_autosave && gameaction != ga_quicksave
			&& gameaction != ga_worlddone && gameaction != ga_completed && gameaction != ga_screenshot && gameaction != ga_fullconsole;
}

// If the connection has been unstable then let the game lag behind for a little bit
// while we wait for it to stabilize, otherwise everything will appear to jitter around.
static void CalculateNetStabilityBuffer(int diff)
{
	if (!netgame || demoplayback)
	{
		StabilityBuffer = 0;
		return;
	}

	if (I_UsesDedicatedServerSlot() && !I_IsLocalHCDEServiceAuthority())
	{
		const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
		const bool lowLatencyAuthority = authoritySlot >= 0
			&& authoritySlot < MAXPLAYERS
			&& HCDELivePeers[authoritySlot].SnapshotReceived > 0u
			&& HCDELivePeers[authoritySlot].RxServerSnapshotSequence > 0u
			&& ClientStates[authoritySlot].AverageLatency <= 16u;
		if (lowLatencyAuthority)
		{
			StabilityBuffer = 0;
			PrevAvailableDiff = max<int>(diff, 0);
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

	// If the game is paused, everything we need to update has already done so.
	if (pauseext)
		return;

	// Get the amount of tics the client can actually run. This accounts for waiting for other
	// players over the network.
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

	// If the lowest confirmed tic matches the server gametic or greater, allow the client
	// to run some of them.
	const int availableTics = (lowestSequence - gametic / TicDup) + 1;

	// If the amount of tics to run is falling behind the amount of available tics,
	// speed the playsim up a bit to help catch up.
	int runTics = min<int>(totalTics, availableTics);
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

	const int worldTimer = primaryLevel->LocalWorldTimer;
	// If there are no tics to run, check for possible stall conditions and new
	// commands to predict.
	if (runTics <= 0)
	{
		if (netgame && totalTics > 0 && EnterTic - LastTicGateStallTrace >= TICRATE)
		{
			LastTicGateStallTrace = EnterTic;
			DebugTrace::Warningf("net", "tic gate stalled total=%d available=%d lowest=%d gametic=%d clienttic=%d room=%u map=%s levelstart=%s delay=%d lag=%s",
				totalTics, availableTics, lowestSequence, gametic, ClientTic, unsigned(CurrentRoomID),
				primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>",
				Net_LevelStartStatusName(LevelStartStatus), LevelStartDelay, Net_LagStateName(LagState));
			for (auto client : NetworkClients)
			{
				const auto& state = ClientStates[client];
				DebugTrace::Warningf("net", "tic gate client=%d gate=%d network=%d playeringame=%d reserved=%d authority=%d waiting=%d seq=%d ack=%d flags=0x%x",
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
			const int authoritySlot = I_GetHCDEServiceAuthoritySlot();
			if (authoritySlot >= 0 && authoritySlot < MAXPLAYERS)
			{
				const auto& state = ClientStates[authoritySlot];
				const auto& peer = HCDELivePeers[authoritySlot];
				Printf(PRINT_HIGH,
					"HCDE net stall: total=%d available=%d lowest=%d gametic=%d clienttic=%d authority=%d seq=%d ack=%d flags=0x%x snap-rx=%u ctrl-rx=%u any-rx=%u snap-count=%u input-sent=%u dup=%u unsupported=%u lag=%s\n",
					totalTics, availableTics, lowestSequence, gametic, ClientTic,
					authoritySlot, state.CurrentSequence, state.SequenceAck, unsigned(state.Flags),
					peer.RxServerSnapshotSequence, peer.RxControlSequence, peer.RxSequence,
					peer.SnapshotReceived, peer.ClientCommandSent, peer.DuplicateCount,
					peer.UnsupportedReceived, Net_LagStateName(LagState));
			}
			else
			{
				Printf(PRINT_HIGH,
					"HCDE net stall: total=%d available=%d lowest=%d gametic=%d clienttic=%d authority=%d lag=%s\n",
					totalTics, availableTics, lowestSequence, gametic, ClientTic,
					authoritySlot, Net_LagStateName(LagState));
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

		// If we actually did have some tics available, make sure the UI
		// still has a chance to run.
		for (int i = 0; i < totalTics; ++i)
			P_RunClientSideLogic();

		if (totalTics > 0)
		{
			S_UpdateSounds(players[consoleplayer].camera, primaryLevel->LocalWorldTimer - min<int>(primaryLevel->LocalWorldTimer, worldTimer));
			NetworkEntityManager::VerifyPredictedEntities();
		}

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
	InvasionMirrorVisualTickBudget = Net_IsLocalInvasionAuthority() ? 0 : 1;
	while (runTics--)
	{
		const bool stabilize = ShouldStabilizeTick();
		if (stabilize)
			TicStabilityBegin();

		if (advancedemo)
			D_DoAdvanceDemo();

		G_Ticker();
		++gametic;
		if (Net_IsLocalInvasionAuthority())
			Net_TickInvasionState();
		else
			Net_TickInvasionMirrorState();
		Net_TickInvasionAnnouncements();
		MakeConsistencies();

		if (stabilize)
			TicStabilityEnd();

		if (bCommandsReset)
		{
			bCommandsReset = false;
			break;
		}
	}
	InvasionMirrorVisualTickBudget = 0;
	P_PredictClient();

	// These should use the actual tics since they're not actually tied to the gameplay logic.
	// Make sure it always comes after so the HUD has the correct game state when updating.
	for (int i = 0; i < totalTics; ++i)
		P_RunClientSideLogic();

	// Since the level could get reset mid-tick, make sure the smaller of the two values is used
	// since it should only go up otherwise.
	S_UpdateSounds(players[consoleplayer].camera, primaryLevel->LocalWorldTimer - min<int>(primaryLevel->LocalWorldTimer, worldTimer));
	NetworkEntityManager::VerifyPredictedEntities();
}

void Net_NewClientTic()
{
	NetEvents.NewClientTic();
}

void Net_Initialize()
{
	NetEvents.InitializeEventData();
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
		gameaction = ga_savegame;
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
		gameaction = ga_autosave;
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
		Net_WriteInt8(DEM_READIED);
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

// Apply the documented "Invasion Standard" baseline without opening the GUI.
// This mirrors the dedicated preset in i_mainwindow.cpp for headless servers.
CCMD(invasion_standard)
{
	if (!I_IsLocalHCDEServiceAuthority())
	{
		Printf("Invasion preset rejected (authority required)\n");
		return;
	}

	sv_gametype = 4;
	sv_invasioncountdowntime = 30.0f;
	sv_invasionspawntime = 8.0f;
	sv_invasioncleanuptime = 4.0f;
	sv_invasionintermissiontime = 6.0f;
	sv_invasionresulttime = 8.0f;
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

	Printf("Applied Invasion Standard preset (sv_gametype=4, fastmonsters=0, waves=%d map-wavelimit=%d use-map-settings=%d budget=%d+%d/wave, per-player=%d)\n",
		int(sv_invasionwaves), int(wavelimit_compat), sv_usemapsettingswavelimit ? 1 : 0,
		int(sv_invasionbasebudget), int(sv_invasionbudgetstep), int(sv_invasionperplayer));
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
		? clamp<int>(primaryLevel->info->InvasionWaveLimit, 0, 255)
		: 0;
	const int mapDuelLimit = (primaryLevel != nullptr && primaryLevel->info != nullptr)
		? clamp<int>(primaryLevel->info->DuelLimit, 0, 255)
		: 0;
	const bool canOverrideCmpgninfLimits = HCDE_CanOverrideCmpgninfLimits();
	Printf("Invasion settings: count=%.2f spawn=%.2f cleanup=%.2f inter=%.2f result=%.2f interval=%.2f burst=%d maxactive=%d waves=%d legacy-wavelimit=%d use-map-wavelimit=%d map-wavelimit=%d resolved-wavelimit=%d map-duellimit=%d legacy-duellimit=%d allow-offline-overrides=%d base=%d step=%d perplayer=%d boss-every=%d boss-bonus=%d map-tags=%d fallback=%d\n",
		double(sv_invasioncountdowntime),
		double(sv_invasionspawntime),
		double(sv_invasioncleanuptime),
		double(sv_invasionintermissiontime),
		double(sv_invasionresulttime),
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
