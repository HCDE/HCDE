// This file is split from d_net.cpp

static uint64_t LastHCDELiveTicGateReportMS = 0u;
static uint64_t LastHCDEPredictionLeadCapReportMS = 0u;
static uint64_t LastHCDEMonsterProximityDumpMS = 0u;
static uint64_t LastHCDEPredictionFaultReportMS = 0u;
// Cooldown for the multi-megabyte prediction_fault forensic bundle (trace
// snapshot + blackbox flush + Net_DiagWriteBundle). Matches the checksum
// mismatch bundle gate so a burst of faults cannot pin the playsim on I/O.
static uint64_t LastPredictionFaultBundleMS = 0u;
static int HCDEPredictionPauseGraceUntil = 0;

// =============================================================================
// HCDE PREDICTION-LOSS DEBUGGER STATE
// =============================================================================
//
// Lightweight per-frame probe that complements the (binary) prediction fault
// logger. The fault logger only fires when the tic gate fully collapses; this
// subsystem captures sub-threshold drift so we can see the warm-up to every
// stall before it becomes one. See cvar `net_predict_debug` for level control.
struct FHCDEPredictionDebugCounters
{
	uint64_t PassiveClientResends = 0u; // sends where we honored our own stale SequenceAck
	uint64_t PassiveAuthorityResends = 0u; // server-side resends when client ack went stale
};
static FHCDEPredictionDebugCounters HCDEPredictionDebugLifetime = {};
// Snapshot of profile + lifetime counters at the start of the current window.
static uint64_t HCDEPredictionDebugWindowPassiveClient = 0u;
static uint64_t HCDEPredictionDebugWindowPassiveAuth = 0u;
static uint64_t HCDEPredictionDebugWindowLocalRepairs = 0u;
static uint64_t HCDEPredictionDebugWindowHardRepairs = 0u;
static uint64_t HCDEPredictionDebugWindowFaultReports = 0u;
// Extrema observed during the current window (reset after every sample).
static int HCDEPredictionDebugMinAvailable = INT_MAX;
static int HCDEPredictionDebugMaxAckLag = 0;
static int HCDEPredictionDebugMaxStaleVisual = 0;
static int HCDEPredictionDebugMaxBacklog = 0;
static int HCDEPredictionDebugLastSampleTic = 0;
static uint64_t HCDEPredictionDebugLastSoftWarnMS = 0u;
static uint64_t HCDEPredictionDebugLastTraceDumpMS = 0u;
static bool HCDEPredictionDebugCsvHeaderProbed = false;

static FHCDELivePeerState HCDELivePeers[MAXPLAYERS] = {};
// NetUpdate() runs on the main thread, but its native send path needs sizeable
// command/event capture arrays while translating the current packet window. Keep
// that scratch storage at module scope so each inner packet loop does not reserve
// and zero tens of kilobytes on the stack.
static FHCDELiveNativeSendScratch HCDELiveNativeSendScratch = {};

static bool HCDEActorBaselineRepairActive(int clientNum)
{
	return clientNum >= 0 && clientNum < MAXPLAYERS
		&& HCDEActorBaselineRepairUntilTic[clientNum] > 0
		&& gametic <= HCDEActorBaselineRepairUntilTic[clientNum];
}

static int HCDECountActiveActorBaselineRepairs()
{
	int active = 0;
	for (int client = 0; client < MAXPLAYERS; ++client)
	{
		if (HCDEActorBaselineRepairActive(client))
			++active;
	}
	return active;
}

struct FHCDELiveProfileCounters
{
	uint64_t ControlPacketsSent = 0u;
	uint64_t ControlBytesSent = 0u;
	uint64_t ControlPacketsReceived = 0u;
	uint64_t ControlBytesReceived = 0u;
	uint64_t CapabilityControlsSent = 0u;
	uint64_t CapabilityControlsReceived = 0u;
	uint64_t LegacyControlsReceived = 0u;
	uint64_t ClientInputPacketsBuilt = 0u;
	uint64_t ClientInputBytesBuilt = 0u;
	uint64_t ClientInputNativeBuilt = 0u;
	uint64_t ClientInputPacketsReceived = 0u;
	uint64_t ClientInputBytesReceived = 0u;
	uint64_t ClientInputNativeApplied = 0u;
	uint64_t ServerSnapshotPacketsBuilt = 0u;
	uint64_t ServerSnapshotBytesBuilt = 0u;
	uint64_t ServerSnapshotNativeBuilt = 0u;
	uint64_t ServerSnapshotPacketsReceived = 0u;
	uint64_t ServerSnapshotBytesReceived = 0u;
	uint64_t ServerSnapshotNativeApplied = 0u;
	uint64_t WorldDeltaPacketsBuilt = 0u;
	uint64_t WorldDeltaBytesBuilt = 0u;
	uint64_t WorldDeltaRecordsBuilt = 0u;
	uint64_t WorldDeltaPacketsReceived = 0u;
	uint64_t WorldDeltaBytesReceived = 0u;
	uint64_t WorldDeltaRecordsReceived = 0u;
	uint64_t AuthorityEventPacketsBuilt = 0u;
	uint64_t AuthorityEventBytesBuilt = 0u;
	uint64_t AuthorityEventRecordsBuilt = 0u;
	uint64_t AuthorityEventRecordsDeferred = 0u;
	uint64_t AuthorityEventPacketsReceived = 0u;
	uint64_t AuthorityEventBytesReceived = 0u;
	uint64_t AuthorityEventRecordsReceived = 0u;
	uint64_t AuthorityEventRecordsApplied = 0u;
	uint64_t AuthorityEventRecordsMissing = 0u;
	uint64_t AuthorityEventSpawnRecordsBuilt = 0u;
	uint64_t AuthorityEventDamageRecordsBuilt = 0u;
	uint64_t AuthorityEventDespawnRecordsBuilt = 0u;
	uint64_t AuthorityEventPickupSpawnRecordsBuilt = 0u;
	uint64_t AuthorityEventPickupRetireRecordsBuilt = 0u;
	uint64_t AuthorityEventRecordsSuperseded = 0u;
	uint64_t AuthorityEventSpawnRecordsReceived = 0u;
	uint64_t AuthorityEventDamageRecordsReceived = 0u;
	uint64_t AuthorityEventDespawnRecordsReceived = 0u;
	uint64_t AuthorityEventPickupSpawnRecordsReceived = 0u;
	uint64_t AuthorityEventPickupRetireRecordsReceived = 0u;
	uint64_t InvasionSnapshotPacketsBuilt = 0u;
	uint64_t InvasionSnapshotBytesBuilt = 0u;
	uint64_t InvasionSnapshotPacketsReceived = 0u;
	uint64_t InvasionSnapshotBytesReceived = 0u;
	uint64_t InvasionActorIdLookupHits = 0u;
	uint64_t InvasionActorIdLookupMisses = 0u;
	uint64_t InvasionActorPtrLookupHits = 0u;
	uint64_t InvasionActorPtrLookupMisses = 0u;
	uint64_t InvasionActorIndexRebuilds = 0u;
	uint64_t SharedActorRegistered = 0u;
	uint64_t SharedActorUpdated = 0u;
	uint64_t SharedActorRetired = 0u;
	uint64_t SharedActorCompacted = 0u;
	uint64_t SharedActorIdLookupHits = 0u;
	uint64_t SharedActorIdLookupMisses = 0u;
	uint64_t SharedActorPtrLookupHits = 0u;
	uint64_t SharedActorPtrLookupMisses = 0u;
	uint64_t SharedActorClassRegistered = 0u;
	uint64_t ModeMigrationScans = 0u;
	uint64_t ModeMigrationActorsConsidered = 0u;
	uint64_t ModeMigrationActorsRegistered = 0u;
	uint64_t ModeMigrationInvasionActive = 0u;
	uint64_t ModeMigrationCoopActive = 0u;
	uint64_t ModeMigrationDMActive = 0u;
	uint64_t ModeMigrationPlayerActorsSuppressed = 0u;
	uint64_t ModeMigrationScriptActorsSuppressed = 0u;
	uint64_t ActorQueueBuilds = 0u;
	uint64_t ActorQueueCandidates = 0u;
	uint64_t ActorQueuePriorityCandidates = 0u;
	uint64_t ActorQueueDeferredCandidates = 0u;
	uint64_t ActorQueueMaxDepth = 0u;
	uint64_t ActorInterestCritical = 0u;
	uint64_t ActorInterestHigh = 0u;
	uint64_t ActorInterestMedium = 0u;
	uint64_t ActorInterestLow = 0u;
	uint64_t ActorInterestDormant = 0u;
	uint64_t ActorInterestSkipped = 0u;
	uint64_t ActorInterestKeepAlive = 0u;
	uint64_t ActorInterestProtected = 0u;
	uint64_t ProjectilePolicyEvaluated = 0u;
	uint64_t ProjectilePolicyCritical = 0u;
	uint64_t ProjectilePolicyHigh = 0u;
	uint64_t ProjectilePolicyMedium = 0u;
	uint64_t ProjectilePolicyLow = 0u;
	uint64_t ProjectilePolicyDormant = 0u;
	uint64_t ProjectilePolicySkipped = 0u;
	uint64_t ProjectilePolicyKeepAlive = 0u;
	uint64_t ProjectilePolicyProtected = 0u;
	uint64_t ProjectilePolicyInbound = 0u;
	uint64_t ProjectilePolicyPlayerOwned = 0u;
	uint64_t SimLODPasses = 0u;
	uint64_t SimLODFull = 0u;
	uint64_t SimLODReduced = 0u;
	uint64_t SimLODDormant = 0u;
	uint64_t SimLODSuspended = 0u;
	uint64_t SimLODRestored = 0u;
	uint64_t SimLODThinkAllowed = 0u;
	uint64_t SimLODThinkSkipped = 0u;
	uint64_t SimLODWakeHealth = 0u;
	uint64_t SimLODWakeDistance = 0u;
	uint64_t ActorDeltaV2PacketsBuilt = 0u;
	uint64_t ActorDeltaV2BytesBuilt = 0u;
	uint64_t ActorDeltaV2RecordsBuilt = 0u;
	uint64_t ActorDeltaV2FullRecordsBuilt = 0u;
	uint64_t ActorDeltaV2PartialRecordsBuilt = 0u;
	uint64_t ActorDeltaV2SkippedUnchanged = 0u;
	uint64_t ActorDeltaV2DeferredBudget = 0u;
	uint64_t ActorDeltaV2PacketsReceived = 0u;
	uint64_t ActorDeltaV2RecordsReceived = 0u;
	uint64_t ActorDeltaV2RecordsApplied = 0u;
	uint64_t ActorDeltaV2RecordsMissing = 0u;
	uint64_t ActorBaselineRepairWindows = 0u;
	uint64_t ActorBaselineRepairResets = 0u;
	uint64_t AuthorityEventCatchupRecordsBuilt = 0u;
	uint64_t AuthorityEventCatchupWindowsCompleted = 0u;
	uint64_t PlayerSnapshotBudgetPressure = 0u;
	uint64_t PlayerSnapshotMissingRecords = 0u;
	uint64_t PlayerSnapshotMaxBytes = 0u;
	uint64_t PlayerSnapshotMaxRecords = 0u;
	uint64_t PredictionLocalHealthRepairs = 0u;
	uint64_t PredictionLocalStateRepairs = 0u;
	uint64_t PredictionHardRespawnRepairs = 0u;
	uint64_t PredictionHardDeathRepairs = 0u;
	uint64_t PredictionFaultReports = 0u;
	uint64_t PredictionFaultAttackReports = 0u;
	uint64_t PredictionFaultMoveReports = 0u;
	uint64_t RemotePlayerBaselineRepairs = 0u;
	uint64_t SharedActorPlayerRecordsSuppressed = 0u;
	uint64_t LivePacketsWrapped = 0u;
	uint64_t LiveBytesWrapped = 0u;
	uint64_t LivePayloadBytesWrapped = 0u;
	uint64_t LiveNativeEncodeFailures = 0u;
	uint64_t LiveNativeCapabilityRejects = 0u;
	uint64_t LiveReplayRequests = 0u;
	uint64_t LiveReplaySuppressions = 0u;
	uint64_t LiveReplayEscalations = 0u;
	uint64_t LiveLegacyGameplayRejected = 0u;
	FHCDELiveLaneStats Lanes[HLANE_COUNT] = {};
	uint64_t WorldTics = 0u;
	uint64_t WorldTicMicros = 0u;
	uint64_t WorldTicMaxMicros = 0u;
	uint64_t MirrorVisualPasses = 0u;
	uint64_t MirrorVisualMicros = 0u;
	uint64_t MirrorVisualMaxMicros = 0u;
	uint64_t MirrorVisualActorsUpdated = 0u;
	uint64_t MirrorVisualActorsSkipped = 0u;

	void Clear()
	{
		*this = {};
	}
};

static FHCDELiveProfileCounters HCDELiveProfile = {};

static size_t HCDELiveLaneDefaultBudgetBytes(uint8_t lane);

static uint64_t HCDEProfileNowUS()
{
	using namespace std::chrono;
	return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

static void HCDEProfileRecordWorldTic(uint64_t elapsedUS)
{
	++HCDELiveProfile.WorldTics;
	HCDELiveProfile.WorldTicMicros += elapsedUS;
	HCDELiveProfile.WorldTicMaxMicros = max<uint64_t>(HCDELiveProfile.WorldTicMaxMicros, elapsedUS);
}

static void HCDEProfileRecordMirrorVisualPass(uint64_t elapsedUS, unsigned int updated, unsigned int skipped)
{
	++HCDELiveProfile.MirrorVisualPasses;
	HCDELiveProfile.MirrorVisualMicros += elapsedUS;
	HCDELiveProfile.MirrorVisualMaxMicros = max<uint64_t>(HCDELiveProfile.MirrorVisualMaxMicros, elapsedUS);
	HCDELiveProfile.MirrorVisualActorsUpdated += updated;
	HCDELiveProfile.MirrorVisualActorsSkipped += skipped;
}

static double HCDEProfileAverage(uint64_t total, uint64_t count);

static const char* HCDELiveLaneName(uint8_t lane)
{
	switch (EHCDELiveLane(lane))
	{
	case HLANE_CONTROL:
		return "control";
	case HLANE_COMMAND:
		return "command";
	case HLANE_AUTHORITY:
		return "authority";
	case HLANE_PLAYER_SNAPSHOT:
		return "player-snapshot";
	case HLANE_ACTOR_DELTA:
		return "actor-delta";
	case HLANE_QUERY_REGISTRY:
		return "query-registry";
	case HLANE_PRESENTATION_ECHO:
		return "presentation-echo";
	default:
		return "unknown";
	}
}

static const char* HCDEActorInterestName(uint8_t interest)
{
	switch (EHCDEActorInterestTier(interest))
	{
	case HINTEREST_CRITICAL:
		return "critical";
	case HINTEREST_HIGH:
		return "high";
	case HINTEREST_MEDIUM:
		return "medium";
	case HINTEREST_LOW:
		return "low";
	case HINTEREST_DORMANT:
		return "dormant";
	default:
		return "unknown";
	}
}

static const char* HCDESimulationLODName(uint8_t lod)
{
	switch (EHCDEInvasionSimulationLOD(lod))
	{
	case HSIMLOD_FULL:
		return "full";
	case HSIMLOD_REDUCED:
		return "reduced";
	case HSIMLOD_DORMANT:
		return "dormant";
	default:
		return "unknown";
	}
}

static const char* HCDEUnlaggedHealthLabel(uint32_t score)
{
	if (score >= 8u)
		return "critical";
	if (score >= 4u)
		return "degraded";
	return "good";
}

static uint32_t HCDEAssessUnlaggedPeerHealth(int averageLatency, int commandLead, const FHCDELivePeerState& peer,
	const FHCDELiveLaneStats& playerLane, const FHCDELiveLaneStats& actorLane)
{
	uint32_t score = 0u;

	if (averageLatency >= 300)
		score += 3u;
	else if (averageLatency >= 180)
		score += 2u;
	else if (averageLatency >= 120)
		score += 1u;

	if (commandLead > 15)
		score += 2u;
	else if (commandLead > 8)
		score += 1u;

	if (HCDELiveProfile.PlayerSnapshotMissingRecords > 0u)
		score += 3u;
	if (peer.PlayerSnapshotBudgetPressure > 0u)
		score += 2u;
	if (playerLane.BudgetClamps > 0u)
		score += 2u;
	if (peer.Reconciliations > 0u)
		score += 1u;
	if (peer.HardReconciliations > 0u)
		score += 3u;
	if (HCDELiveProfile.RemotePlayerBaselineRepairs > 0u)
		score += 1u;

	if (actorLane.Deferred > 0u && peer.PlayerSnapshotBudgetPressure > 0u)
		score += 1u;
	if (peer.ActorQueueDeferredDepth > 64u)
		score += 2u;
	else if (peer.ActorQueueDeferredDepth > 20u)
		score += 1u;

	if (peer.ActorQueueTopScore > 2400)
		score += 1u;

	return min<uint32_t>(score, 20u);
}

static bool HCDELiveLaneProtected(uint8_t lane)
{
	return lane == HLANE_CONTROL
		|| lane == HLANE_COMMAND
		|| lane == HLANE_AUTHORITY
		|| lane == HLANE_PLAYER_SNAPSHOT;
}

void HCDERecordLiveLaneTx(uint8_t lane, int client, size_t bytes)
{
	if (lane >= HLANE_COUNT)
		return;

	++HCDELiveProfile.Lanes[lane].TxPackets;
	HCDELiveProfile.Lanes[lane].TxBytes += bytes;
	if (client >= 0 && client < MAXPLAYERS)
	{
		++HCDELivePeers[client].Lanes[lane].TxPackets;
		HCDELivePeers[client].Lanes[lane].TxBytes += bytes;
	}
}

void HCDERecordLiveLaneRx(uint8_t lane, int client, size_t bytes)
{
	if (lane >= HLANE_COUNT)
		return;

	++HCDELiveProfile.Lanes[lane].RxPackets;
	HCDELiveProfile.Lanes[lane].RxBytes += bytes;
	if (client >= 0 && client < MAXPLAYERS)
	{
		++HCDELivePeers[client].Lanes[lane].RxPackets;
		HCDELivePeers[client].Lanes[lane].RxBytes += bytes;
	}
}

void HCDERecordLiveLaneDeferred(uint8_t lane, int client)
{
	if (lane >= HLANE_COUNT)
		return;

	++HCDELiveProfile.Lanes[lane].Deferred;
	if (client >= 0 && client < MAXPLAYERS)
		++HCDELivePeers[client].Lanes[lane].Deferred;
}

void HCDERecordLiveLaneBudgetClamp(uint8_t lane, int client)
{
	if (lane >= HLANE_COUNT)
		return;

	++HCDELiveProfile.Lanes[lane].BudgetClamps;
	if (client >= 0 && client < MAXPLAYERS)
		++HCDELivePeers[client].Lanes[lane].BudgetClamps;
}

static void HCDERecordPlayerSnapshotPressure(int client, size_t bytes, size_t records)
{
	HCDELiveProfile.PlayerSnapshotMaxBytes = max<uint64_t>(HCDELiveProfile.PlayerSnapshotMaxBytes, uint64_t(bytes));
	HCDELiveProfile.PlayerSnapshotMaxRecords = max<uint64_t>(HCDELiveProfile.PlayerSnapshotMaxRecords, uint64_t(records));
	if (client >= 0 && client < MAXPLAYERS)
	{
		auto& peer = HCDELivePeers[client];
		peer.PlayerSnapshotMaxBytes = max<uint32_t>(peer.PlayerSnapshotMaxBytes, uint32_t(min<size_t>(bytes, UINT32_MAX)));
		peer.PlayerSnapshotMaxRecords = max<uint32_t>(peer.PlayerSnapshotMaxRecords, uint32_t(min<size_t>(records, UINT32_MAX)));
	}

	const size_t playerBudget = HCDELiveLaneDefaultBudgetBytes(HLANE_PLAYER_SNAPSHOT);
	if (bytes <= playerBudget)
		return;

	++HCDELiveProfile.PlayerSnapshotBudgetPressure;
	if (client >= 0 && client < MAXPLAYERS)
		++HCDELivePeers[client].PlayerSnapshotBudgetPressure;
	DebugTrace::Warningf("net", "HCDE player snapshot exceeded nominal budget client=%d bytes=%zu budget=%zu records=%zu",
		client, bytes, playerBudget, records);
}

struct FHCDEPendingLocalHealthRepair
{
	bool Valid = false;
	uint32_t ServerTic = 0u;
	int Health = 0;
	bool OnGround = false;
	bool ApplyPose = false;
	DVector3 Pos = {};
	DVector3 Vel = {};
	uint32_t Yaw = 0u;
	uint32_t Pitch = 0u;
};

static FHCDEPendingLocalHealthRepair PendingLocalHealthRepair = {};

#include "d_net_diagnostics.h"

void HCDEGetLiveProfileSummary(FHCDENetDiagProfileSummary& out)
{
	out = {};
	out.PredictionFaultReports = HCDELiveProfile.PredictionFaultReports;
	out.PredictionLocalHealthRepairs = HCDELiveProfile.PredictionLocalHealthRepairs;
	out.PredictionLocalStateRepairs = HCDELiveProfile.PredictionLocalStateRepairs;
	out.PredictionHardRespawnRepairs = HCDELiveProfile.PredictionHardRespawnRepairs;
	out.PredictionHardDeathRepairs = HCDELiveProfile.PredictionHardDeathRepairs;
	out.RemotePlayerBaselineRepairs = HCDELiveProfile.RemotePlayerBaselineRepairs;
	out.ActorDeltaV2RecordsMissing = HCDELiveProfile.ActorDeltaV2RecordsMissing;
	out.WorldDeltaRecordsReceived = HCDELiveProfile.WorldDeltaRecordsReceived;
	out.ClientInputNativeApplied = HCDELiveProfile.ClientInputNativeApplied;
	out.ServerSnapshotNativeApplied = HCDELiveProfile.ServerSnapshotNativeApplied;
}
