// HCDE net diagnostics expansion implementation.
//
// This translation unit owns:
//   * The structured session lifecycle traces (BEGIN / NET_READY / MAP_*)
//   * The net_mark moment-capture pipeline (queueing locally, propagating to
//     remotes via a small TLV chunk on outgoing client-input packets, and
//     applying received marks on the authoritative side)
//   * The diagnostic bundle writer that aggregates trace snapshots, prediction
//     logs, and the latest black-box dump into a single directory
//   * The lightweight tracing functions wired into playsim hooks
//
// Most tracing entry points are gated by the `net_event_debug` CVAR so the
// release build can short-circuit them to an integer compare. The bundle and
// mark paths intentionally take the long path even when tracing is muted so
// the user can capture an incident on demand.

#include "d_net_diagnostics.h"
#include "d_net.h"
#include "d_net_diag.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_level.h"
#include "g_levellocals.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "common/engine/debugtrace.h"
#include "i_specialpaths.h"
#include "i_time.h"
#include "i_system.h"
#include "version.h"
#include "d_main.h"
#include "cmdlib.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <climits>
#include <cstdint>

EXTERN_CVAR(Bool, debugtrace_enable)
EXTERN_CVAR(Bool, debugtrace_stream)
EXTERN_CVAR(Int, net_predict_debug)
EXTERN_CVAR(Int, net_reconcile_debug)
EXTERN_CVAR(Int, net_echo_debug)
EXTERN_CVAR(Int, cl_net_prediction_lead)
EXTERN_CVAR(Int, net_event_debug)
EXTERN_CVAR(Bool, hcde_hud_debug)
EXTERN_CVAR(Int, sv_gametype)

// 0 = silent, 1 = standard event traces, 2 = verbose (per-tic server truth, etc).
CUSTOM_CVAR(Int, net_event_debug, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 2)
		self = 2;
}

// Pending mark state. Only mutated from the main thread (CCMD invocations and
// the network packet build/apply paths run there), so a plain bool is safe.
static char PendingMarkLabel[128] = {};
static bool PendingMarkNetworkSend = false;
// Last observed remote session ID per peer. Stored for future correlation
// tooling that wants to splice client/server traces by peer; at present only
// the trace line in Net_DiagPeerCorrelate consumes the value, but keeping the
// table around makes it cheap to add a `net_session_info` listing later.
static uint32_t PeerRemoteSessionIds[MAXPLAYERS] = {};
static bool SessionBeginLogged = false;

namespace
{
constexpr uint8_t DiagMarkMagic[4] = { 'H', 'C', 'M', 'K' };
}

// Strip the directory component from a path, handling both forward and
// backslashes on Windows. Returns a pointer into the input string (no allocation).
static const char* Net_DiagBaseName(const char* path)
{
	if (path == nullptr || *path == '\0')
		return "";
	const char* slash = strrchr(path, '/');
	const char* bslash = strrchr(path, '\\');
	const char* base = slash;
	if (bslash != nullptr && (base == nullptr || bslash > base))
		base = bslash;
	return base != nullptr ? base + 1 : path;
}

// Coarse role classification used in trace prefixes; "authority" for the
// dedicated/listen server, "client" for a remote-driven session, and "local"
// for solo play.
static const char* Net_DiagRoleName()
{
	if (I_IsLocalHCDEServiceAuthority())
		return "authority";
	if (netgame)
		return "client";
	return "local";
}

// Rough gametype label used by SESSION_NET_READY traces.
static const char* Net_DiagGameModeName()
{
	if (sv_gametype == 4)
		return "invasion";
	if (deathmatch)
		return teamplay ? "teamdm" : "dm";
	if (multiplayer)
		return "coop";
	return "single";
}

void Net_DiagGetProfileSummary(FHCDENetDiagProfileSummary& out)
{
	HCDEGetLiveProfileSummary(out);
}

void Net_DiagSessionBegin()
{
	if (SessionBeginLogged)
		return;
	SessionBeginLogged = true;
	Net_BlackboxInit();
	Net_ChecksumInit();
	DebugTrace::InitSession();
	DebugTrace::Markf("net.session",
		"SESSION_BEGIN sess=%08X process=%s role=%s version=%s git=%s os=%s "
		"net_predict_debug=%d net_reconcile_debug=%d net_echo_debug=%d cl_net_prediction_lead=%d "
		"net_event_debug=%d hcde_hud_debug=%d debugtrace_enable=%d debugtrace_stream=%d",
		unsigned(DebugTrace::GetSessionId()),
		DebugTrace::GetProcessTag(),
		Net_DiagRoleName(),
		GetVersionString(),
		GetGitHash(),
		I_DetectOS().GetChars(),
		int(*net_predict_debug),
		int(*net_reconcile_debug),
		int(*net_echo_debug),
		int(*cl_net_prediction_lead),
		int(*net_event_debug),
		*hcde_hud_debug ? 1 : 0,
		*debugtrace_enable ? 1 : 0,
		*debugtrace_stream ? 1 : 0);
}

void Net_DiagSessionNetReady()
{
	DebugTrace::Markf("net.session",
		"SESSION_NET_READY sess=%08X role=%s gamemode=%s netgame=%d multiplayer=%d dedicated=%d "
		"players=%u consoleplayer=%d TicDup=%u map=%s",
		unsigned(DebugTrace::GetSessionId()),
		Net_DiagRoleName(),
		Net_DiagGameModeName(),
		netgame ? 1 : 0,
		multiplayer ? 1 : 0,
		I_UsesDedicatedServerSlot() ? 1 : 0,
		unsigned(NetworkClients.Size()),
		consoleplayer,
		unsigned(TicDup),
		(primaryLevel != nullptr && primaryLevel->MapName.IsNotEmpty()) ? primaryLevel->MapName.GetChars() : "<none>");
}

void Net_DiagMapBegin(const char* mapName)
{
	// A map transition invalidates every cached world-state hash; clear them
	// here so the next checksum tick / snapshot apply does not see stale tic
	// anchors and silently skip recomputes.
	Net_ChecksumResetForNewLevel();
	DebugTrace::Markf("net.session",
		"MAP_BEGIN sess=%08X map=%s gametic=%d clienttic=%d role=%s room=%u",
		unsigned(DebugTrace::GetSessionId()),
		mapName != nullptr ? mapName : "<null>",
		gametic,
		ClientTic,
		Net_DiagRoleName(),
		unsigned(Net_GetCurrentRoomID()));
}

void Net_DiagMapEnd(const char* mapName, const char* reason)
{
	FHCDENetDiagProfileSummary summary = {};
	Net_DiagGetProfileSummary(summary);
	DebugTrace::Markf("net.session",
		"MAP_END sess=%08X map=%s reason=%s gametic=%d faults=%llu local-repairs=%llu hard-repairs=%llu",
		unsigned(DebugTrace::GetSessionId()),
		mapName != nullptr ? mapName : "<null>",
		reason != nullptr ? reason : "unknown",
		gametic,
		static_cast<unsigned long long>(summary.PredictionFaultReports),
		static_cast<unsigned long long>(summary.PredictionLocalHealthRepairs + summary.PredictionLocalStateRepairs),
		static_cast<unsigned long long>(summary.PredictionHardRespawnRepairs + summary.PredictionHardDeathRepairs));
}

void Net_DiagSessionEnd(const char* reason)
{
	FHCDENetDiagProfileSummary summary = {};
	Net_DiagGetProfileSummary(summary);
	DebugTrace::Markf("net.session",
		"SESSION_END sess=%08X reason=%s role=%s faults=%llu actor-missing=%llu snap-applied=%llu input-applied=%llu",
		unsigned(DebugTrace::GetSessionId()),
		reason != nullptr ? reason : "shutdown",
		Net_DiagRoleName(),
		static_cast<unsigned long long>(summary.PredictionFaultReports),
		static_cast<unsigned long long>(summary.ActorDeltaV2RecordsMissing),
		static_cast<unsigned long long>(summary.ServerSnapshotNativeApplied),
		static_cast<unsigned long long>(summary.ClientInputNativeApplied));
	DebugTrace::FlushStream();
	Net_BlackboxShutdown();
}

void Net_DiagPeerCorrelate(int clientNum, uint32_t remoteSessionId, uint64_t negotiatedCaps)
{
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;
	PeerRemoteSessionIds[clientNum] = remoteSessionId;
	DebugTrace::Markf("net.session",
		"PEER_CORRELATE local_sess=%08X client=%d remote_sess=%08X negotiated=0x%llx role=%s",
		unsigned(DebugTrace::GetSessionId()),
		clientNum,
		unsigned(remoteSessionId),
		static_cast<unsigned long long>(negotiatedCaps),
		Net_DiagRoleName());
}

// Stash a label string for the next outgoing client-input packet to embed.
// Net_DiagTryAppendMarkChunk reads + clears this state when the chunk fits.
void Net_DiagQueueMark(const char* label)
{
	const char* useLabel = (label != nullptr && *label != '\0') ? label : "mark";
	std::snprintf(PendingMarkLabel, sizeof(PendingMarkLabel), "%.127s", useLabel);
	PendingMarkNetworkSend = true;
}

bool Net_DiagHasPendingMark()
{
	return PendingMarkNetworkSend;
}

// Print a structured MARK trace anchored to the local player and the most
// recent local user command. Called either from the originating peer
// (Net_DiagRunMarkLocally) or from the receiving authority when a remote mark
// chunk arrives (Net_DiagTryConsumeMarkChunk).
void Net_DiagApplyPendingMark(int clientNum, const char* label)
{
	const char* useLabel = (label != nullptr && *label != '\0') ? label : PendingMarkLabel;
	if (useLabel[0] == '\0')
		useLabel = "mark";

	const player_t* localPlayer = (consoleplayer >= 0 && consoleplayer < MAXPLAYERS) ? &players[consoleplayer] : nullptr;
	const AActor* mo = localPlayer != nullptr ? localPlayer->mo : nullptr;
	const int latestTic = max<int>(ClientTic - 1, 0);
	const usercmd_t& cmd = LocalCmds[latestTic % LOCALCMDTICS];

	DebugTrace::Markf("net.session",
		"MARK label=%s sess=%08X role=%s client=%d gametic=%d clienttic=%d room=%u "
		"pos=(%.1f,%.1f,%.1f) health=%d buttons=0x%08x fwd=%d side=%d up=%d backlog=%d",
		useLabel,
		unsigned(DebugTrace::GetSessionId()),
		Net_DiagRoleName(),
		clientNum,
		gametic,
		ClientTic,
		unsigned(Net_GetCurrentRoomID()),
		mo != nullptr ? mo->X() : 0.0,
		mo != nullptr ? mo->Y() : 0.0,
		mo != nullptr ? mo->Z() : 0.0,
		mo != nullptr ? mo->health : 0,
		cmd.buttons,
		cmd.forwardmove,
		cmd.sidemove,
		cmd.upmove,
		ClientTic - gametic);
}

// Drive the full local "moment capture" pipeline: trace anchor, prediction
// dump, and a forced black-box flush. Used by both the CCMD and the receiving
// side when a remote mark arrives.
void Net_DiagRunMarkLocally(const char* label)
{
	const char* useLabel = (label != nullptr && *label != '\0') ? label : PendingMarkLabel;
	Net_DiagApplyPendingMark(consoleplayer >= 0 ? consoleplayer : 0, useLabel);
	Net_DiagRunPredictDump();
	Net_BlackboxAutoSave("net_mark");
}

// Append the queued HCMK mark TLV (4-byte magic, 1-byte label length, label)
// to the outgoing client-input body. Returns false only when the chunk would
// overflow the buffer; callers treat that as a build failure. On a successful
// append the queued state is cleared so subsequent packets do not duplicate
// the mark.
bool Net_DiagTryAppendMarkChunk(uint8_t* output, size_t outputCapacity, size_t& cursor)
{
	if (!PendingMarkNetworkSend)
		return true;

	const size_t labelLen = strnlen(PendingMarkLabel, sizeof(PendingMarkLabel));
	const size_t chunkSize = sizeof(DiagMarkMagic) + 1u + labelLen;
	if (cursor + chunkSize > outputCapacity)
		return false;

	memcpy(output + cursor, DiagMarkMagic, sizeof(DiagMarkMagic));
	cursor += sizeof(DiagMarkMagic);
	output[cursor++] = uint8_t(labelLen);
	if (labelLen > 0u)
	{
		memcpy(output + cursor, PendingMarkLabel, labelLen);
		cursor += labelLen;
	}
	PendingMarkNetworkSend = false;
	PendingMarkLabel[0] = '\0';
	return true;
}

bool Net_DiagPeekMarkChunk(const uint8_t* body, size_t bodyBytes, size_t bodyCursor, size_t& outChunkBytes)
{
	outChunkBytes = 0u;
	if (body == nullptr || bodyCursor + sizeof(DiagMarkMagic) + 1u > bodyBytes)
		return false;
	if (memcmp(body + bodyCursor, DiagMarkMagic, sizeof(DiagMarkMagic)) != 0)
		return false;
	const uint8_t labelLen = body[bodyCursor + sizeof(DiagMarkMagic)];
	const size_t chunkBytes = sizeof(DiagMarkMagic) + 1u + labelLen;
	if (bodyCursor + chunkBytes > bodyBytes)
		return false;
	outChunkBytes = chunkBytes;
	return true;
}

bool Net_DiagTryConsumeMarkChunk(const uint8_t* body, size_t bodyBytes, size_t& bodyCursor, int clientNum)
{
	size_t chunkBytes = 0u;
	if (!Net_DiagPeekMarkChunk(body, bodyBytes, bodyCursor, chunkBytes))
		return false;

	bodyCursor += sizeof(DiagMarkMagic);
	const uint8_t labelLen = body[bodyCursor++];

	char label[128] = {};
	if (labelLen > 0u)
	{
		// Truncate to local buffer; remote senders cap labels at 127 bytes already
		// but a malicious or stale peer could still claim a longer length.
		const size_t copyLen = min<size_t>(labelLen, sizeof(label) - 1u);
		memcpy(label, body + bodyCursor, copyLen);
		bodyCursor += labelLen;
	}

	Net_DiagApplyPendingMark(clientNum, label);
	Net_DiagRunPredictDump();
	Net_BlackboxAutoSave("net_mark_remote");
	return true;
}

// Streaming copy used by the bundle writer. Returns false if the source is
// missing or unreadable; the destination is created with truncation. Closing
// happens unconditionally so a partial copy cannot leak handles.
static bool Net_DiagCopyFileIfExists(const char* srcPath, const char* dstPath)
{
	if (srcPath == nullptr || dstPath == nullptr)
		return false;
	FILE* in = fopen(srcPath, "rb");
	if (in == nullptr)
		return false;
	FILE* out = fopen(dstPath, "wb");
	if (out == nullptr)
	{
		fclose(in);
		return false;
	}
	char buffer[8192];
	size_t readBytes = 0;
	bool ok = true;
	// Track short writes and stream errors so a truncated/corrupt copy is not
	// reported as success and then listed in the bundle manifest as complete.
	while ((readBytes = fread(buffer, 1, sizeof(buffer), in)) > 0)
	{
		if (fwrite(buffer, 1, readBytes, out) != readBytes)
		{
			ok = false;
			break;
		}
	}
	if (ferror(in))
		ok = false;
	fclose(in);
	fclose(out);
	return ok;
}

static void Net_DiagAppendManifestLine(FILE* manifest, const char* line)
{
	if (manifest != nullptr && line != nullptr)
		fprintf(manifest, "%s\n", line);
}

// Snapshot the current diagnostic surface area into a single directory:
//   * manifest.txt header summarizing role/map/profile counters.
//   * Filtered net trace snapshot.
//   * Streaming trace + latest-* trace files (when present).
//   * Prediction fault/softwarn logs and the predict metrics CSV.
//   * A dedicated black box dump (ring buffer flush).
// The output directory path is returned via outPath on success.
bool Net_DiagWriteBundle(const char* label, FString& outPath)
{
	const uint64_t nowMS = I_msTime();
	const char* useLabel = (label != nullptr && *label != '\0') ? label : "bundle";
	const FString appData = M_GetAppDataPath(true);
	const FString bundleDir = FStringf("%s/hcde_diag_bundle_%08X_%llu",
		appData.GetChars(),
		unsigned(DebugTrace::GetSessionId()),
		static_cast<unsigned long long>(nowMS));
	CreatePath(bundleDir.GetChars());

	const FString manifestPath = FStringf("%s/manifest.txt", bundleDir.GetChars());
	FILE* manifest = fopen(manifestPath.GetChars(), "w");
	if (manifest == nullptr)
		return false;

	FHCDENetDiagProfileSummary summary = {};
	Net_DiagGetProfileSummary(summary);

	fprintf(manifest,
		"HCDE diagnostic bundle\n"
		"label=%s\n"
		"session=%08X\n"
		"process=%s\n"
		"role=%s\n"
		"map=%s\n"
		"gametic=%d\n"
		"clienttic=%d\n"
		"faults=%llu\n",
		useLabel,
		unsigned(DebugTrace::GetSessionId()),
		DebugTrace::GetProcessTag(),
		Net_DiagRoleName(),
		(primaryLevel != nullptr) ? primaryLevel->MapName.GetChars() : "<none>",
		gametic,
		ClientTic,
		static_cast<unsigned long long>(summary.PredictionFaultReports));

	DebugTrace::FlushStream();
	const FString traceSnap = FStringf("%s/net_trace_snapshot.txt", bundleDir.GetChars());
	DebugTrace::SaveToFile(traceSnap.GetChars(), "net", DebugTrace::Severity::Debug);
	Net_DiagAppendManifestLine(manifest, traceSnap.GetChars());

	const char* streamPath = DebugTrace::GetStreamPath();
	if (streamPath != nullptr && *streamPath != '\0')
	{
		const FString dst = FStringf("%s/%s", bundleDir.GetChars(), Net_DiagBaseName(streamPath));
		if (Net_DiagCopyFileIfExists(streamPath, dst.GetChars()))
			Net_DiagAppendManifestLine(manifest, dst.GetChars());
	}

	const char* latestPath = DebugTrace::GetLatestStreamPath();
	if (latestPath != nullptr && *latestPath != '\0')
	{
		const FString dst = FStringf("%s/%s", bundleDir.GetChars(), Net_DiagBaseName(latestPath));
		if (Net_DiagCopyFileIfExists(latestPath, dst.GetChars()))
			Net_DiagAppendManifestLine(manifest, dst.GetChars());
	}

	const char* diagFiles[] = {
		"hcde_prediction_faults.log",
		"hcde_prediction_softwarn.log",
		"hcde_predict_metrics.csv"
	};
	for (const char* name : diagFiles)
	{
		const FString src = FStringf("%s/%s", appData.GetChars(), name);
		const FString dst = FStringf("%s/%s", bundleDir.GetChars(), name);
		if (Net_DiagCopyFileIfExists(src.GetChars(), dst.GetChars()))
			Net_DiagAppendManifestLine(manifest, dst.GetChars());
	}

	FString blackboxPath;
	if (Net_BlackboxSave(useLabel, blackboxPath))
	{
		const FString dst = FStringf("%s/%s", bundleDir.GetChars(), Net_DiagBaseName(blackboxPath.GetChars()));
		if (Net_DiagCopyFileIfExists(blackboxPath.GetChars(), dst.GetChars()))
			Net_DiagAppendManifestLine(manifest, dst.GetChars());
	}

	fclose(manifest);
	outPath = bundleDir;
	return true;
}

// INPUT_AUTH traces are emitted on every authority decision that affects
// whether a remote tic was accepted (stale-room rejects, unauthorized record
// rejects, native-apply ACK). They live under the `net.command` channel so the
// downstream tooling can splice them with NCMD_REPLAY events on the same tic.
void Net_DiagTraceInputAuthority(int clientNum, const char* event, const char* detail)
{
	if (*net_event_debug <= 0)
		return;
	DebugTrace::Markf("net.command",
		"INPUT_AUTH client=%d event=%s detail=%s gametic=%d clienttic=%d room=%u",
		clientNum,
		event != nullptr ? event : "?",
		detail != nullptr ? detail : "",
		gametic,
		ClientTic,
		unsigned(Net_GetCurrentRoomID()));
}

// Print the authoritative pose the server wrote into a snapshot for a given
// peer/player. Emits at most every 4 tics in standard mode (`net_event_debug`
// = 1) to keep the trace volume bounded, and every tic in verbose mode.
void Net_DiagTraceServerPlayerTruth(int clientNum, uint32_t serverTic, int playerNum,
	double x, double y, double z, double vx, double vy, double vz,
	int health, bool onGround, uint8_t playerState)
{
	if (*net_event_debug <= 0)
		return;
	// Per-client throttle: indexed by clientNum so each peer carries its own
	// last-trace tic, otherwise one fast client can starve every other peer's
	// trace stream. The previous implementation used a single function-static
	// uint32_t for ALL peers AND never reset it between maps - after a level
	// transition serverTic resets to small values while the static still held
	// the prior map's high tic, so `serverTic - lastTraceTic` underflowed as
	// unsigned and the throttle effectively disabled itself for the rest of
	// the session. The static signed sentinel below ("never traced yet") is
	// detected as INT64_MIN so the very first tic always emits.
	if (clientNum < 0 || clientNum >= MAXPLAYERS)
		return;
	static int64_t lastTraceTicPerClient[MAXPLAYERS] = {};
	static bool traceThrottleInitialized = false;
	if (!traceThrottleInitialized)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
			lastTraceTicPerClient[i] = INT64_MIN;
		traceThrottleInitialized = true;
	}
	const int64_t serverTicSigned = int64_t(serverTic);
	if (*net_event_debug < 2
		&& lastTraceTicPerClient[clientNum] != INT64_MIN
		&& serverTicSigned - lastTraceTicPerClient[clientNum] < 4)
	{
		return;
	}
	lastTraceTicPerClient[clientNum] = serverTicSigned;
	DebugTrace::Markf("net.snapshot",
		"SERVER_TRUTH client=%d player=%d tic=%u pos=(%.1f,%.1f,%.1f) vel=(%.2f,%.2f,%.2f) health=%d onground=%d pstate=%u",
		clientNum, playerNum, unsigned(serverTic),
		x, y, z, vx, vy, vz, health, onGround ? 1 : 0, unsigned(playerState));
}

// Trace the rising edge of BT_USE so we can correlate playsim use attempts
// with whatever the server's decision is on the same tic.
void Net_DiagTraceUseEdge(int playerNum, uint32_t buttons, bool rising)
{
	if (*net_event_debug <= 0)
		return;
	DebugTrace::Markf("playsim.use",
		"USE_EDGE player=%d rising=%d buttons=0x%08x gametic=%d clienttic=%d",
		playerNum, rising ? 1 : 0, buttons, gametic, ClientTic);
}

// Trace a use-line attempt; `useFail` is the "ouch" path where no line nor
// sector accepted the use.
void Net_DiagTraceUseLine(int playerNum, int lineIndex, bool success, bool useFail)
{
	if (*net_event_debug <= 0)
		return;
	DebugTrace::Markf("playsim.use",
		"USE_LINE player=%d line=%d success=%d usefail=%d gametic=%d",
		playerNum, lineIndex, success ? 1 : 0, useFail ? 1 : 0, gametic);
}

// Trace a line special activation. `predicted` is true on the client-side
// prediction path so a desync investigation can spot the case where the
// client predicted differently than the authority.
void Net_DiagTraceLineSpec(int playerNum, int lineIndex, int special, int tag, bool success, bool predicted)
{
	if (*net_event_debug <= 0)
		return;
	DebugTrace::Markf("playsim.linespec",
		"LINESPEC player=%d line=%d special=%d tag=%d success=%d predicted=%d gametic=%d",
		playerNum, lineIndex, special, tag, success ? 1 : 0, predicted ? 1 : 0, gametic);
}

// Trace a door movement start. Doors are a frequent source of "your client
// thinks the door is open, mine doesn't" desync, so we capture the speed and
// direction at activation time.
void Net_DiagTraceDoorEvent(int tag, int type, int direction, double speed)
{
	if (*net_event_debug <= 0)
		return;
	DebugTrace::Markf("playsim.door",
		"DOOR tag=%d type=%d dir=%d speed=%.2f gametic=%d",
		tag, type, direction, speed, gametic);
}

// Trace a sector action trigger (TriggerSectorActions) success.
void Net_DiagTraceSectorAction(int sectorIndex, int activation, bool didIt)
{
	if (*net_event_debug <= 0)
		return;
	DebugTrace::Markf("playsim.sector",
		"SECTOR sector=%d activation=%d didit=%d gametic=%d",
		sectorIndex, activation, didIt ? 1 : 0, gametic);
}

// Quick console helper: classify a diagnostic artifact by sniffing its first
// few bytes / filename and print a one-line summary plus the sidecar manifest
// when one exists. Used by `net_diag_inspect` and `net_diag_replay` to give
// the user a fast hint of what they just collected without launching the
// Python tooling.
void Net_DiagInspectFile(const char* path)
{
	if (path == nullptr || *path == '\0')
	{
		Printf(PRINT_HIGH, "Usage: net_diag_inspect <file-or-directory>\n");
		return;
	}
	FILE* f = fopen(path, "rb");
	if (f == nullptr)
	{
		Printf(PRINT_HIGH, "Could not open %s\n", path);
		return;
	}
	char header[16] = {};
	const size_t headerRead = fread(header, 1, sizeof(header), f);
	if (fseek(f, 0, SEEK_END) != 0)
	{
		fclose(f);
		Printf(PRINT_HIGH, "Could not stat %s\n", path);
		return;
	}
	const long fileSize = ftell(f);
	fclose(f);
	const bool magicBlackbox = headerRead >= 4u && memcmp(header, "HBBX", 4) == 0;
	if (magicBlackbox)
	{
		Printf(PRINT_HIGH, "HCDE black box file: %s bytes=%ld\n", path, fileSize);
		const FString sidecar = FStringf("%s.json", path);
		if (FILE* manifest = fopen(sidecar.GetChars(), "r"))
		{
			char line[512] = {};
			if (fgets(line, sizeof(line), manifest) != nullptr)
				Printf(PRINT_HIGH, "  sidecar: %s", line);
			fclose(manifest);
		}
	}
	else if (strstr(path, "manifest.txt") != nullptr || strstr(path, "hcde_diag_bundle") != nullptr)
		Printf(PRINT_HIGH, "HCDE diagnostic bundle path: %s bytes=%ld\n", path, fileSize);
	else
		Printf(PRINT_HIGH, "Generic diagnostic file: %s bytes=%ld\n", path, fileSize);
}

CCMD(net_mark)
{
	const char* label = (argv.argc() > 1) ? argv[1] : "manual";
	Net_DiagQueueMark(label);
	Net_DiagRunMarkLocally(label);
	// Authority and offline sessions never produce outbound HLIVE_CLIENT_COMMANDS
	// packets that could carry the chunk to a remote, so clear the pending bit
	// immediately. Otherwise a future netgame would replay a stale mark on its
	// first outbound input frame.
	if (!netgame || I_IsLocalHCDEServiceAuthority())
	{
		PendingMarkNetworkSend = false;
		PendingMarkLabel[0] = '\0';
	}
	Printf(PRINT_HIGH, "HCDE net mark applied: %s\n", label);
}

CCMD(net_diag_bundle)
{
	const char* label = (argv.argc() > 1) ? argv[1] : "manual";
	FString bundlePath;
	if (Net_DiagWriteBundle(label, bundlePath))
		Printf(PRINT_HIGH, "HCDE diagnostic bundle written: %s\n", bundlePath.GetChars());
	else
		Printf(PRINT_HIGH, "HCDE diagnostic bundle failed.\n");
}

// `net_diag_replay <client-bundle> [server-bundle]` is a console-side wrapper
// that surfaces a quick file inspection in-game and prints the precise
// command line the offline Python tool needs to compare two bundles.
CCMD(net_diag_replay)
{
	if (argv.argc() <= 1)
	{
		Printf(PRINT_HIGH, "Usage: net_diag_replay <client-bundle-dir> [server-bundle-dir]\n");
		Printf(PRINT_HIGH, "Offline compare: python tests/netcode_step12/replay_blackbox.py <paths...>\n");
		return;
	}
	if (argv.argc() >= 3)
		Printf(PRINT_HIGH, "Compare bundles with: python tests/netcode_step12/replay_blackbox.py %s %s\n", argv[1], argv[2]);
	Net_DiagInspectFile(argv[1]);
}

CCMD(net_diag_inspect)
{
	if (argv.argc() <= 1)
	{
		Printf(PRINT_HIGH, "Usage: net_diag_inspect <path>\n");
		return;
	}
	Net_DiagInspectFile(argv[1]);
}

CCMD(net_session_info)
{
	FHCDENetDiagProfileSummary summary = {};
	Net_DiagGetProfileSummary(summary);
	Printf(PRINT_HIGH,
		"session=%08X process=%s role=%s map=%s gametic=%d clienttic=%d faults=%llu\n",
		unsigned(DebugTrace::GetSessionId()),
		DebugTrace::GetProcessTag(),
		Net_DiagRoleName(),
		(primaryLevel != nullptr) ? primaryLevel->MapName.GetChars() : "<none>",
		gametic,
		ClientTic,
		static_cast<unsigned long long>(summary.PredictionFaultReports));
}
