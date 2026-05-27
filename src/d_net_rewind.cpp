// HCDE rewind / keyframe ring buffer implementation.
// See d_net_rewind.h for the design doc.

#include "d_net_rewind.h"

#include "doomstat.h"
#include "g_levellocals.h"
#include "p_setup.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "common/engine/debugtrace.h"
#include "i_time.h"
#include "i_system.h"
#include "i_interface.h"
#include "m_crc32.h"
#include "serializer_doom.h"
#include "m_random.h"
#include "tarray.h"
#include "version.h"
#include "a_sharedglobal.h"
#include "actor.h"			// AActor (Phase 5: lag-comp scope)
#include "p_local.h"		// P_DamageMobj (Phase 5: replay)
#include "d_player.h"		// players[] / player_t (Phase 5: attacker->player check)
#include "common/objects/dobject.h"

extern uint8_t globalfreeze;

#include <stdint.h>
#include <string.h>

EXTERN_CVAR(Bool, save_formatted)

extern bool insave;

namespace
{

// Capture interval in seconds. Range trimmed so a misconfigured value cannot
// blow up memory. 0.25s lower bound prevents serializer thrash; 30s upper
// bound prevents stale keyframes that can never service a recent rewind.
CUSTOM_CVAR(Float, net_rewind_interval, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SERVERINFO)
{
	if (self < 0.25f) self = 0.25f;
	if (self > 30.0f) self = 30.0f;
}

// Ring buffer depth (max keyframes retained). Default 10 keyframes at
// 1s interval = 10 seconds of rollback history.
CUSTOM_CVAR(Int, net_rewind_depth, 10, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SERVERINFO)
{
	if (self < 1) self = 1;
	if (self > 64) self = 64;
}

// Master switch. 0 disables the system entirely (no capture cost, no memory).
//
// Defaults to false: the bracket primitive (HCDERewind_TestLagCompBracket)
// is currently only invoked by `net_rewind_lagcomp_probe`, so capturing a
// full level serializer pass every second on the authority gains nothing
// for live gameplay. Enable manually when iterating on the rewind system,
// or once a future phase wires the bracket into weapon resolution.
CUSTOM_CVAR(Bool, net_rewind_enable, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SERVERINFO)
{
	if (!self)
		HCDERewind_Reset("disabled");
}

// Memory ceiling in MB. Hard cap on the ring buffer regardless of depth /
// interval; if a new keyframe would exceed this, the oldest is evicted first.
CUSTOM_CVAR(Int, net_rewind_max_mb, 32, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SERVERINFO)
{
	if (self < 1) self = 1;
	if (self > 512) self = 512;
}

TArray<FHCDERewindKeyframe> gKeyframes;
int gLastCaptureGametic = -1;
uint64_t gLastCaptureMS = 0u;
uint64_t gTotalCaptures = 0u;
uint64_t gTotalCaptureCostMS = 0u;
uint64_t gMaxCaptureCostMS = 0u;
uint64_t gEvictionsForDepth = 0u;
uint64_t gEvictionsForMemory = 0u;
int gClientViewTic[MAXPLAYERS] = {};
bool gClientViewTicValid[MAXPLAYERS] = {};
uint64_t gLagCompBracketTests = 0u;
uint64_t gLagCompBracketFailures = 0u;

bool ShouldCapture(int currentGametic)
{
	if (!*net_rewind_enable)
		return false;
	if (!I_IsLocalHCDEServiceAuthority())
		return false;
	if (gamestate != GS_LEVEL)
		return false;
	if (primaryLevel == nullptr || primaryLevel->lines.Size() == 0u || primaryLevel->sectors.Size() == 0u)
		return false;
	if (insave)
		return false;	// don't recurse into the serializer
	const int intervalTics = max<int>(int(*net_rewind_interval * float(TICRATE)), 1);
	if (gLastCaptureGametic < 0)
		return true;
	return currentGametic - gLastCaptureGametic >= intervalTics;
}

bool BuildLevelBuffer(FileSys::FCompressedBuffer& outBuf)
{
	if (primaryLevel == nullptr || !primaryLevel->info)
		return false;
	FDoomSerializer arc(primaryLevel);
	if (!arc.OpenWriter(*save_formatted))
		return false;
	SaveVersion = SAVEVER;
	primaryLevel->Serialize(arc, false);
	outBuf = arc.GetCompressedOutput();
	return outBuf.mBuffer != nullptr;
}

bool BuildGlobalsBuffer(FileSys::FCompressedBuffer& outBuf)
{
	FSerializer arc;
	if (!arc.OpenWriter(*save_formatted))
		return false;
	int ticrateValue = TICRATE;
	int leveltimeValue = primaryLevel != nullptr ? primaryLevel->time : 0;
	arc("ticrate", ticrateValue)
	   ("leveltime", leveltimeValue)
	   ("globalfreeze", globalfreeze);
	FRandom::StaticWriteRNGState(arc);
	outBuf = arc.GetCompressedOutput();
	return outBuf.mBuffer != nullptr;
}

bool BuildKeyframe(FHCDERewindKeyframe& outFrame)
{
	outFrame.Clear();
	outFrame.Gametic = gametic;
	outFrame.CaptureMS = I_msTime();
	const uint64_t startMS = I_msTime();
	const bool ok = BuildLevelBuffer(outFrame.Level) && BuildGlobalsBuffer(outFrame.Globals);
	outFrame.CaptureCostMS = uint32_t(I_msTime() - startMS);
	if (!ok)
		outFrame.Clear();
	else
	{
		outFrame.LevelCRC32 = CalcCRC32(reinterpret_cast<const uint8_t*>(outFrame.Level.mBuffer),
			unsigned(outFrame.Level.mCompressedSize));
		outFrame.GlobalsCRC32 = CalcCRC32(reinterpret_cast<const uint8_t*>(outFrame.Globals.mBuffer),
			unsigned(outFrame.Globals.mCompressedSize));
		outFrame.CombinedCRC32 = AddCRC32(outFrame.LevelCRC32,
			reinterpret_cast<const uint8_t*>(&outFrame.GlobalsCRC32), sizeof(outFrame.GlobalsCRC32));
	}
	return ok;
}

bool RestoreGlobalsBuffer(const FileSys::FCompressedBuffer& buffer)
{
	FSerializer arc;
	auto* mutableBuffer = const_cast<FileSys::FCompressedBuffer*>(&buffer);
	if (!arc.OpenReader(mutableBuffer))
		return false;
	int ticrateValue = TICRATE;
	int leveltimeValue = primaryLevel != nullptr ? primaryLevel->time : 0;
	arc("ticrate", ticrateValue)
	   ("leveltime", leveltimeValue)
	   ("globalfreeze", globalfreeze);
	if (primaryLevel != nullptr)
		primaryLevel->time = leveltimeValue;
	FRandom::StaticReadRNGState(arc);
	arc.Close();
	return true;
}

int FindNearestKeyframeIndex(int targetGametic)
{
	if (gKeyframes.Size() == 0u)
		return -1;
	int bestIndex = -1;
	int bestDelta = INT32_MAX;
	for (unsigned i = 0u; i < gKeyframes.Size(); ++i)
	{
		const int delta = gKeyframes[i].Gametic - targetGametic;
		const int absDelta = delta < 0 ? -delta : delta;
		if (absDelta < bestDelta)
		{
			bestDelta = absDelta;
			bestIndex = int(i);
		}
	}
	return bestIndex;
}

// Lag-comp wants the newest keyframe at or before the client's view tic.
// Nearest-by-absolute-delta can pick a frame *after* viewTic when capture
// intervals are sparse, which makes hitscan aim at the wrong world state.
int FindNearestKeyframeAtOrBefore(int targetGametic)
{
	if (gKeyframes.Size() == 0u)
		return -1;

	int bestIndex = -1;
	int bestGametic = INT_MIN;
	for (unsigned i = 0u; i < gKeyframes.Size(); ++i)
	{
		const int frameGametic = gKeyframes[i].Gametic;
		if (frameGametic <= targetGametic && frameGametic > bestGametic)
		{
			bestGametic = frameGametic;
			bestIndex = int(i);
		}
	}

	// View tic predates every stored frame: use the oldest snapshot we have
	// rather than failing the bracket entirely.
	if (bestIndex < 0)
		return 0;

	return bestIndex;
}

void CleanupRestoredPlayerPawns()
{
	if (primaryLevel == nullptr)
		return;

	auto it = primaryLevel->GetThinkerIterator<AActor>(NAME_PlayerPawn);
	AActor* pawn = nullptr;
	AActor* next = it.Next();
	while ((pawn = next) != nullptr)
	{
		next = it.Next();
		if (pawn->player != nullptr && pawn->player->mo != nullptr && primaryLevel->PlayerInGame(pawn->player))
			continue;

		unsigned int i;
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (primaryLevel->PlayerInGame(i)
				&& primaryLevel->Players[i] != nullptr
				&& primaryLevel->Players[i]->mo != nullptr
				&& primaryLevel->Players[i]->mo->alternative == pawn)
				break;
		}
		if (i == MAXPLAYERS)
			pawn->Destroy();
	}
}

bool RestoreLevelBuffer(const FileSys::FCompressedBuffer& buffer)
{
	if (primaryLevel == nullptr || primaryLevel->info == nullptr || !primaryLevel->info->isValid())
		return false;

	FDoomSerializer arc(primaryLevel);
	auto* mutableBuffer = const_cast<FileSys::FCompressedBuffer*>(&buffer);
	if (!arc.OpenReader(mutableBuffer))
		return false;

	primaryLevel->Serialize(arc, false);
	primaryLevel->FromSnapshot = true;
	CleanupRestoredPlayerPawns();
	arc.Close();
	return true;
}

// TArray::Delete() destructs the slot in place (via ~FHCDERewindKeyframe ->
// Clear()), then byte-shifts the trailing elements down with memmove. The
// shifted slots keep their original mBuffer pointers so no double-free occurs;
// the trailing tail-slot is no longer reachable after Count is decremented.
void EvictOldestForDepth()
{
	const int depth = max<int>(*net_rewind_depth, 1);
	while (int(gKeyframes.Size()) > depth)
	{
		gKeyframes.Delete(0);
		++gEvictionsForDepth;
	}
}

void EvictOldestForMemory(size_t pendingBytes)
{
	const size_t cap = size_t(*net_rewind_max_mb) * 1024u * 1024u;
	while (gKeyframes.Size() > 0u && (HCDERewind_TotalBytes() + pendingBytes) > cap)
	{
		gKeyframes.Delete(0);
		++gEvictionsForMemory;
	}
}

bool LagCompCaptureLiveKeyframe(FHCDERewindKeyframe& out)
{
	insave = true;
	bool ok = false;
	try { ok = BuildKeyframe(out); }
	catch (...) { ok = false; }
	insave = false;
	return ok;
}

bool LagCompRestoreLiveKeyframe(FHCDERewindKeyframe& live, int liveGametic)
{
	if (!live.Valid())
		return false;
	bool ok = false;
	insave = true;
	try
	{
		SaveVersion = SAVEVER;
		ok = RestoreLevelBuffer(live.Level) && RestoreGlobalsBuffer(live.Globals);
	}
	catch (...)
	{
		ok = false;
	}
	insave = false;
	if (ok)
		gametic = liveGametic;
	return ok;
}

void LagCompSaveCaptureTimer(int& outGametic, uint64_t& outMS)
{
	outGametic = gLastCaptureGametic;
	outMS = gLastCaptureMS;
}

void LagCompRestoreCaptureTimer(int gametic, uint64_t ms)
{
	gLastCaptureGametic = gametic;
	gLastCaptureMS = ms;
}

int LagCompFindHistoricalIndex(int viewTic)
{
	return FindNearestKeyframeAtOrBefore(viewTic);
}

int LagCompHistoricalGametic(int index)
{
	if (index < 0 || index >= int(gKeyframes.Size()))
		return -1;
	return gKeyframes[index].Gametic;
}

}	// namespace

void FHCDERewindKeyframe::Clear()
{
	Level.Clean();
	Globals.Clean();
	Gametic = -1;
	CaptureMS = 0u;
	CaptureCostMS = 0u;
	LevelCRC32 = 0u;
	GlobalsCRC32 = 0u;
	CombinedCRC32 = 0u;
}

// Move ctor: take ownership of `other`'s heap buffers and null the source so
// its destructor / Clear() is a safe no-op. mBuffer is the only owning pointer
// in FCompressedBuffer; the other fields are plain values.
FHCDERewindKeyframe::FHCDERewindKeyframe(FHCDERewindKeyframe&& other) noexcept
	: Gametic(other.Gametic)
	, CaptureMS(other.CaptureMS)
	, CaptureCostMS(other.CaptureCostMS)
	, LevelCRC32(other.LevelCRC32)
	, GlobalsCRC32(other.GlobalsCRC32)
	, CombinedCRC32(other.CombinedCRC32)
	, Level(other.Level)
	, Globals(other.Globals)
{
	other.Level.mBuffer = nullptr;
	other.Globals.mBuffer = nullptr;
	other.Gametic = -1;
	other.CaptureMS = 0u;
	other.CaptureCostMS = 0u;
	other.LevelCRC32 = 0u;
	other.GlobalsCRC32 = 0u;
	other.CombinedCRC32 = 0u;
}

FHCDERewindKeyframe& FHCDERewindKeyframe::operator=(FHCDERewindKeyframe&& other) noexcept
{
	if (this != &other)
	{
		Clear();
		Gametic = other.Gametic;
		CaptureMS = other.CaptureMS;
		CaptureCostMS = other.CaptureCostMS;
		LevelCRC32 = other.LevelCRC32;
		GlobalsCRC32 = other.GlobalsCRC32;
		CombinedCRC32 = other.CombinedCRC32;
		Level = other.Level;
		Globals = other.Globals;
		other.Level.mBuffer = nullptr;
		other.Globals.mBuffer = nullptr;
		other.Gametic = -1;
		other.CaptureMS = 0u;
		other.CaptureCostMS = 0u;
		other.LevelCRC32 = 0u;
		other.GlobalsCRC32 = 0u;
		other.CombinedCRC32 = 0u;
	}
	return *this;
}

bool HCDERewind_CaptureNow(const char* reason)
{
	if (!I_IsLocalHCDEServiceAuthority() || gamestate != GS_LEVEL || primaryLevel == nullptr)
		return false;
	if (insave)
		return false;

	insave = true;
	FHCDERewindKeyframe frame;
	bool ok = false;
	try
	{
		ok = BuildKeyframe(frame);
	}
	catch (...)
	{
		ok = false;
	}
	insave = false;
	if (!ok)
	{
		// frame's destructor releases any partial allocation.
		DebugTrace::Warningf("net.rewind",
			"capture failed reason=%s gametic=%d map=%s",
			reason != nullptr ? reason : "?", gametic,
			primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
		return false;
	}

	// Evict using the about-to-be-pushed frame's footprint as the budget delta.
	EvictOldestForMemory(frame.SizeBytes());
	EvictOldestForDepth();

	const size_t newFrameBytes = frame.SizeBytes();
	const uint32_t newFrameCostMS = frame.CaptureCostMS;
	const uint32_t newFrameLevelCRC32 = frame.LevelCRC32;
	const uint32_t newFrameGlobalsCRC32 = frame.GlobalsCRC32;
	const uint32_t newFrameCombinedCRC32 = frame.CombinedCRC32;
	gKeyframes.Push(std::move(frame));
	gLastCaptureGametic = gametic;
	gLastCaptureMS = I_msTime();
	++gTotalCaptures;
	gTotalCaptureCostMS += newFrameCostMS;
	if (newFrameCostMS > gMaxCaptureCostMS)
		gMaxCaptureCostMS = newFrameCostMS;

	// Evict again after the push in case the new frame itself blew the cap.
	EvictOldestForMemory(0u);
	EvictOldestForDepth();

	DebugTrace::Markf("net.rewind",
		"capture reason=%s gametic=%d size=%zu cost=%ums hash=%08x level=%08x globals=%08x depth=%u total-bytes=%zu",
		reason != nullptr ? reason : "tick", gametic,
		newFrameBytes, unsigned(newFrameCostMS),
		unsigned(newFrameCombinedCRC32),
		unsigned(newFrameLevelCRC32),
		unsigned(newFrameGlobalsCRC32),
		unsigned(gKeyframes.Size()), HCDERewind_TotalBytes());
	return true;
}

bool HCDERewind_RestoreKeyframe(int index, const char* reason)
{
	if (!I_IsLocalHCDEServiceAuthority() || gamestate != GS_LEVEL || primaryLevel == nullptr)
		return false;
	if (insave)
		return false;

	const FHCDERewindKeyframe* frame = HCDERewind_FindByIndex(index);
	if (frame == nullptr || !frame->Valid())
		return false;

	const uint64_t startMS = I_msTime();
	bool ok = false;
	insave = true;
	try
	{
		SaveVersion = SAVEVER;
		ok = RestoreLevelBuffer(frame->Level) && RestoreGlobalsBuffer(frame->Globals);
	}
	catch (...)
	{
		ok = false;
	}
	insave = false;

	if (!ok)
	{
		DebugTrace::Warningf("net.rewind",
			"restore failed reason=%s index=%d target-gametic=%d current-gametic=%d map=%s",
			reason != nullptr ? reason : "?", index, frame->Gametic, gametic,
			primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
		return false;
	}

	gametic = frame->Gametic;
	gLastCaptureGametic = gametic;
	gLastCaptureMS = I_msTime();
	DebugTrace::Warningf("net.rewind",
		"restore complete reason=%s index=%d gametic=%d size=%zu cost=%llums map=%s",
		reason != nullptr ? reason : "?", index, gametic, frame->SizeBytes(),
		static_cast<unsigned long long>(I_msTime() - startMS),
		primaryLevel != nullptr ? primaryLevel->MapName.GetChars() : "<none>");
	return true;
}

void HCDERewind_OnServerTick()
{
	if (!ShouldCapture(gametic))
		return;
	HCDERewind_CaptureNow("tick");
}

void HCDERewind_Reset(const char* reason)
{
	// gKeyframes.Clear() invokes ~FHCDERewindKeyframe() on every element via
	// TArray::DoDelete, which releases the compressed buffers.
	gKeyframes.Clear();
	gLastCaptureGametic = -1;
	gLastCaptureMS = 0u;
	for (int& viewTic : gClientViewTic)
		viewTic = -1;
	for (bool& valid : gClientViewTicValid)
		valid = false;
	DebugTrace::Markf("net.rewind", "reset reason=%s", reason != nullptr ? reason : "?");
}

size_t HCDERewind_TotalBytes()
{
	size_t total = 0u;
	for (const auto& frame : gKeyframes)
		total += frame.SizeBytes();
	return total;
}

const FHCDERewindKeyframe* HCDERewind_FindByIndex(int index)
{
	if (index < 0 || index >= int(gKeyframes.Size()))
		return nullptr;
	return &gKeyframes[index];
}

const FHCDERewindKeyframe* HCDERewind_FindNearestTic(int targetGametic)
{
	if (gKeyframes.Size() == 0u)
		return nullptr;
	const FHCDERewindKeyframe* best = nullptr;
	int bestDelta = INT32_MAX;
	for (const auto& frame : gKeyframes)
	{
		const int delta = frame.Gametic - targetGametic;
		const int absDelta = delta < 0 ? -delta : delta;
		if (absDelta < bestDelta)
		{
			bestDelta = absDelta;
			best = &frame;
		}
	}
	return best;
}

int HCDERewind_StoredCount()
{
	return int(gKeyframes.Size());
}

void HCDERewind_RecordClientViewTic(int playerNum, int viewTic)
{
	if (playerNum < 0 || playerNum >= MAXPLAYERS || viewTic < 0)
		return;
	// `viewTic` arrives in sequence units (gametic / TicDup). Convert back to
	// raw gametic so callers can compare directly against keyframe Gametic
	// without having to know the protocol detail. TicDup is currently pinned
	// at 1 in HCDE multiplayer, but we keep the scaling explicit so future
	// TicDup>1 deployments don't silently drift the bracket window.
	const int ticDup = max<int>(int(TicDup), 1);
	gClientViewTic[playerNum] = viewTic * ticDup;
	gClientViewTicValid[playerNum] = true;
}

int HCDERewind_GetClientViewTic(int playerNum)
{
	if (playerNum < 0 || playerNum >= MAXPLAYERS)
		return -1;
	if (!gClientViewTicValid[playerNum])
		return -1;
	return gClientViewTic[playerNum];
}

bool HCDERewind_TestLagCompBracket(int playerNum, const char* reason)
{
	++gLagCompBracketTests;
	if (!I_IsLocalHCDEServiceAuthority() || gamestate != GS_LEVEL || primaryLevel == nullptr)
	{
		++gLagCompBracketFailures;
		return false;
	}
	if (insave)
	{
		++gLagCompBracketFailures;
		return false;
	}

	const int viewTic = HCDERewind_GetClientViewTic(playerNum);
	if (viewTic < 0)
	{
		++gLagCompBracketFailures;
		DebugTrace::Warningf("net.rewind",
			"lagcomp bracket rejected reason=%s player=%d no-view-tic gametic=%d",
			reason != nullptr ? reason : "?", playerNum, gametic);
		return false;
	}

	const int historicalIndex = FindNearestKeyframeIndex(viewTic);
	if (historicalIndex < 0)
	{
		++gLagCompBracketFailures;
		DebugTrace::Warningf("net.rewind",
			"lagcomp bracket rejected reason=%s player=%d viewtic=%d no-keyframes gametic=%d",
			reason != nullptr ? reason : "?", playerNum, viewTic, gametic);
		return false;
	}

	FHCDERewindKeyframe current;
	const uint64_t startMS = I_msTime();
	insave = true;
	bool capturedCurrent = false;
	try
	{
		capturedCurrent = BuildKeyframe(current);
	}
	catch (...)
	{
		capturedCurrent = false;
	}
	insave = false;
	if (!capturedCurrent)
	{
		++gLagCompBracketFailures;
		return false;
	}

	// Snapshot the capture-timer state so we can restore it after the bracket.
	// Without this, both nested restores leave gLastCaptureGametic pointed at a
	// historical tic while gametic resumes at liveGametic, which makes the
	// next ShouldCapture() trigger a wasted snapshot every single bracket.
	const int savedLastCaptureGametic = gLastCaptureGametic;
	const uint64_t savedLastCaptureMS = gLastCaptureMS;
	const int liveGametic = gametic;
	const int historicalGametic = gKeyframes[historicalIndex].Gametic;
	const bool restoredHistorical = HCDERewind_RestoreKeyframe(historicalIndex, "lagcomp-historical");
	bool restoredCurrent = false;
	if (restoredHistorical)
	{
		// Restore the captured current frame directly. Do not push it into the
		// ring; this is only a bracket proof/restoration anchor.
		insave = true;
		try
		{
			SaveVersion = SAVEVER;
			restoredCurrent = RestoreLevelBuffer(current.Level) && RestoreGlobalsBuffer(current.Globals);
		}
		catch (...)
		{
			restoredCurrent = false;
		}
		insave = false;
		if (restoredCurrent)
			gametic = liveGametic;
	}
	// `current`'s destructor releases its compressed buffers when this frame
	// goes out of scope, so no manual Clear() is needed.

	if (!restoredHistorical || !restoredCurrent)
	{
		++gLagCompBracketFailures;
		// IMPORTANT: if `restoredHistorical` succeeded but `restoredCurrent`
		// failed, the world is now in a past state and there is no automatic
		// recovery in this primitive. The Warningf is intentionally loud so
		// the host operator can react. A future hardening pass should attempt
		// a fresh capture-and-roll-forward fallback here.
		DebugTrace::Warningf("net.rewind",
			"lagcomp bracket failed reason=%s player=%d viewtic=%d hist-index=%d hist-tic=%d live-tic=%d restored-hist=%d restored-current=%d",
			reason != nullptr ? reason : "?", playerNum, viewTic, historicalIndex,
			historicalGametic, liveGametic, restoredHistorical ? 1 : 0, restoredCurrent ? 1 : 0);
		return false;
	}

	// Restore the capture-timer to its pre-bracket value so we don't immediately
	// fire a "make-up" capture on the next tick.
	gLastCaptureGametic = savedLastCaptureGametic;
	gLastCaptureMS = savedLastCaptureMS;

	DebugTrace::Warningf("net.rewind",
		"lagcomp bracket ok reason=%s player=%d viewtic=%d hist-index=%d hist-tic=%d live-tic=%d cost=%llums",
		reason != nullptr ? reason : "?", playerNum, viewTic, historicalIndex,
		historicalGametic, liveGametic,
		static_cast<unsigned long long>(I_msTime() - startMS));
	return true;
}

bool HCDERewind_SelfCheck(int index, const char* reason)
{
	if (!I_IsLocalHCDEServiceAuthority() || gamestate != GS_LEVEL || primaryLevel == nullptr)
	{
		DebugTrace::Warningf("net.rewind",
			"selfcheck rejected reason=%s authority=%d gamestate=%d level=%d",
			reason != nullptr ? reason : "?",
			I_IsLocalHCDEServiceAuthority() ? 1 : 0,
			int(gamestate), primaryLevel != nullptr ? 1 : 0);
		return false;
	}
	if (insave)
	{
		DebugTrace::Warningf("net.rewind",
			"selfcheck rejected reason=%s already-in-save gametic=%d",
			reason != nullptr ? reason : "?", gametic);
		return false;
	}
	if (gKeyframes.Size() == 0u)
	{
		DebugTrace::Warningf("net.rewind",
			"selfcheck rejected reason=%s no-keyframes gametic=%d",
			reason != nullptr ? reason : "?", gametic);
		return false;
	}

	if (index < 0)
		index = int(gKeyframes.Size()) - 1;
	const FHCDERewindKeyframe* historical = HCDERewind_FindByIndex(index);
	if (historical == nullptr || !historical->Valid())
	{
		DebugTrace::Warningf("net.rewind",
			"selfcheck rejected reason=%s invalid-index=%d stored=%d gametic=%d",
			reason != nullptr ? reason : "?", index, int(gKeyframes.Size()), gametic);
		return false;
	}

	FHCDERewindKeyframe liveBefore;
	FHCDERewindKeyframe liveAfter;
	const uint64_t startMS = I_msTime();
	const int liveGametic = gametic;
	const int savedLastCaptureGametic = gLastCaptureGametic;
	const uint64_t savedLastCaptureMS = gLastCaptureMS;
	const int historicalGametic = historical->Gametic;
	const uint32_t historicalCRC = historical->CombinedCRC32;

	const bool capturedLive = LagCompCaptureLiveKeyframe(liveBefore);
	if (!capturedLive)
	{
		DebugTrace::Warningf("net.rewind",
			"selfcheck live-capture failed reason=%s gametic=%d",
			reason != nullptr ? reason : "?", gametic);
		LagCompRestoreCaptureTimer(savedLastCaptureGametic, savedLastCaptureMS);
		return false;
	}

	const uint32_t liveBeforeLevelCRC32 = liveBefore.LevelCRC32;
	const uint32_t liveBeforeGlobalsCRC32 = liveBefore.GlobalsCRC32;
	const uint32_t liveBeforeCombinedCRC32 = liveBefore.CombinedCRC32;

	const bool restoredHistorical = HCDERewind_RestoreKeyframe(index, "selfcheck-historical");
	bool restoredLive = false;
	bool capturedAfter = false;
	// Even a failed historical restore can leave partially deserialized state.
	// Always try to put the live capture back before reporting failure.
	restoredLive = LagCompRestoreLiveKeyframe(liveBefore, liveGametic);
	if (restoredLive)
		capturedAfter = LagCompCaptureLiveKeyframe(liveAfter);

	LagCompRestoreCaptureTimer(savedLastCaptureGametic, savedLastCaptureMS);

	if (!restoredHistorical || !restoredLive || !capturedAfter)
	{
		DebugTrace::Warningf("net.rewind",
			"selfcheck failed reason=%s index=%d hist-tic=%d live-tic=%d restored-hist=%d restored-live=%d captured-after=%d",
			reason != nullptr ? reason : "?", index, historicalGametic, liveGametic,
			restoredHistorical ? 1 : 0, restoredLive ? 1 : 0, capturedAfter ? 1 : 0);
		return false;
	}

	const bool levelMatch = liveBeforeLevelCRC32 == liveAfter.LevelCRC32;
	const bool globalsMatch = liveBeforeGlobalsCRC32 == liveAfter.GlobalsCRC32;
	const bool combinedMatch = liveBeforeCombinedCRC32 == liveAfter.CombinedCRC32;
	if (!levelMatch || !globalsMatch || !combinedMatch)
	{
		DebugTrace::Warningf("net.rewind",
			"selfcheck CRC mismatch reason=%s index=%d hist-tic=%d live-tic=%d hist-hash=%08x before=%08x/%08x/%08x after=%08x/%08x/%08x",
			reason != nullptr ? reason : "?", index, historicalGametic, liveGametic,
			unsigned(historicalCRC),
			unsigned(liveBeforeCombinedCRC32), unsigned(liveBeforeLevelCRC32), unsigned(liveBeforeGlobalsCRC32),
			unsigned(liveAfter.CombinedCRC32), unsigned(liveAfter.LevelCRC32), unsigned(liveAfter.GlobalsCRC32));
		return false;
	}

	DebugTrace::Markf("net.rewind",
		"selfcheck ok reason=%s index=%d hist-tic=%d live-tic=%d hist-hash=%08x live-hash=%08x cost=%llums",
		reason != nullptr ? reason : "?", index, historicalGametic, liveGametic,
		unsigned(historicalCRC), unsigned(liveBeforeCombinedCRC32),
		static_cast<unsigned long long>(I_msTime() - startMS));
	return true;
}

void HCDERewind_PrintSummary()
{
	Printf(PRINT_HIGH, "\n=== HCDE rewind keyframes ===\n");
	Printf(PRINT_HIGH,
		"enable=%d interval=%.2fs depth=%d max=%d MB authority=%d gamestate=%d gametic=%d\n",
		*net_rewind_enable ? 1 : 0, float(*net_rewind_interval),
		int(*net_rewind_depth), int(*net_rewind_max_mb),
		I_IsLocalHCDEServiceAuthority() ? 1 : 0,
		int(gamestate), gametic);
	const uint64_t nowMS = I_msTime();
	const uint64_t lastCaptureAgeMS = (gLastCaptureMS != 0u && nowMS >= gLastCaptureMS)
		? (nowMS - gLastCaptureMS) : 0u;
	Printf(PRINT_HIGH,
		"stored=%d total-bytes=%zu (%.2f MB) total-captures=%llu evict-depth=%llu evict-mem=%llu cost-avg=%.2fms cost-max=%llums last-capture-age=%llums\n",
		HCDERewind_StoredCount(), HCDERewind_TotalBytes(),
		double(HCDERewind_TotalBytes()) / (1024.0 * 1024.0),
		static_cast<unsigned long long>(gTotalCaptures),
		static_cast<unsigned long long>(gEvictionsForDepth),
		static_cast<unsigned long long>(gEvictionsForMemory),
		gTotalCaptures > 0u ? double(gTotalCaptureCostMS) / double(gTotalCaptures) : 0.0,
		static_cast<unsigned long long>(gMaxCaptureCostMS),
		static_cast<unsigned long long>(lastCaptureAgeMS));
	Printf(PRINT_HIGH,
		"lagcomp brackets=%llu failures=%llu\n",
		static_cast<unsigned long long>(gLagCompBracketTests),
		static_cast<unsigned long long>(gLagCompBracketFailures));
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (gClientViewTicValid[i])
			Printf(PRINT_HIGH, "  client-view player=%d tic=%d age=%d\n",
				i, gClientViewTic[i], gametic - gClientViewTic[i]);
	}

	for (unsigned i = 0u; i < gKeyframes.Size(); ++i)
	{
		const auto& frame = gKeyframes[i];
		const int ageTics = gametic - frame.Gametic;
		Printf(PRINT_HIGH,
			"  [%u] gametic=%d age=%d tics (%.2fs) size=%zu (%.1f KB) cost=%ums hash=%08x level=%08x globals=%08x\n",
			i, frame.Gametic, ageTics, double(ageTics) / double(TICRATE),
			frame.SizeBytes(), double(frame.SizeBytes()) / 1024.0,
			unsigned(frame.CaptureCostMS),
			unsigned(frame.CombinedCRC32), unsigned(frame.LevelCRC32), unsigned(frame.GlobalsCRC32));
	}
	Printf(PRINT_HIGH, "==============================\n");
}

CCMD(net_rewind_dump)
{
	HCDERewind_PrintSummary();
}

CCMD(net_rewind_capture_now)
{
	if (!I_IsLocalHCDEServiceAuthority())
	{
		Printf(PRINT_HIGH, "net_rewind_capture_now: requires authority slot.\n");
		return;
	}
	if (HCDERewind_CaptureNow("manual"))
		Printf(PRINT_HIGH, "net_rewind_capture_now: captured keyframe at gametic=%d.\n", gametic);
	else
		Printf(PRINT_HIGH, "net_rewind_capture_now: capture failed (see net.rewind trace).\n");
}

CCMD(net_rewind_reset)
{
	HCDERewind_Reset("manual");
	Printf(PRINT_HIGH, "net_rewind_reset: keyframe ring cleared.\n");
}

CCMD(net_rewind_test_restore)
{
	if (argv.argc() <= 1)
	{
		Printf(PRINT_HIGH, "usage: net_rewind_test_restore <index>\n");
		HCDERewind_PrintSummary();
		return;
	}
	if (!I_IsLocalHCDEServiceAuthority())
	{
		Printf(PRINT_HIGH, "net_rewind_test_restore: requires authority slot.\n");
		return;
	}
	const int index = atoi(argv[1]);
	const FHCDERewindKeyframe* frame = HCDERewind_FindByIndex(index);
	if (frame == nullptr || !frame->Valid())
	{
		Printf(PRINT_HIGH, "net_rewind_test_restore: invalid keyframe index %d.\n", index);
		return;
	}
	Printf(PRINT_HIGH,
		"net_rewind_test_restore: restoring index=%d gametic=%d age=%d tics. Live clients are not resynced in Phase 2.\n",
		index, frame->Gametic, gametic - frame->Gametic);
	if (HCDERewind_RestoreKeyframe(index, "manual-test"))
		Printf(PRINT_HIGH, "net_rewind_test_restore: restored to gametic=%d.\n", gametic);
	else
		Printf(PRINT_HIGH, "net_rewind_test_restore: restore failed (see net.rewind trace).\n");
}

CCMD(net_rewind_lagcomp_probe)
{
	if (argv.argc() <= 1)
	{
		Printf(PRINT_HIGH, "usage: net_rewind_lagcomp_probe <player>\n");
		HCDERewind_PrintSummary();
		return;
	}
	const int playerNum = atoi(argv[1]);
	if (!I_IsLocalHCDEServiceAuthority())
	{
		Printf(PRINT_HIGH, "net_rewind_lagcomp_probe: requires authority slot.\n");
		return;
	}
	const int viewTic = HCDERewind_GetClientViewTic(playerNum);
	if (viewTic < 0)
	{
		Printf(PRINT_HIGH, "net_rewind_lagcomp_probe: no view tic recorded for player %d yet.\n", playerNum);
		return;
	}
	Printf(PRINT_HIGH,
		"net_rewind_lagcomp_probe: player=%d viewtic=%d age=%d tics, bracketing nearest keyframe.\n",
		playerNum, viewTic, gametic - viewTic);
	if (HCDERewind_TestLagCompBracket(playerNum, "manual-probe"))
		Printf(PRINT_HIGH, "net_rewind_lagcomp_probe: bracket restore succeeded; live world restored to gametic=%d.\n", gametic);
	else
		Printf(PRINT_HIGH, "net_rewind_lagcomp_probe: bracket failed (see net.rewind trace).\n");
}

CCMD(net_rewind_selfcheck)
{
	if (!I_IsLocalHCDEServiceAuthority())
	{
		Printf(PRINT_HIGH, "net_rewind_selfcheck: requires authority slot.\n");
		return;
	}
	int index = -1;
	if (argv.argc() > 1)
		index = atoi(argv[1]);
	const int resolvedIndex = index < 0 ? HCDERewind_StoredCount() - 1 : index;
	const FHCDERewindKeyframe* frame = HCDERewind_FindByIndex(resolvedIndex);
	if (frame == nullptr || !frame->Valid())
	{
		Printf(PRINT_HIGH, "net_rewind_selfcheck: no valid keyframe at index %d.\n", resolvedIndex);
		HCDERewind_PrintSummary();
		return;
	}
	Printf(PRINT_HIGH,
		"net_rewind_selfcheck: probing index=%d gametic=%d hash=%08x, then restoring live gametic=%d.\n",
		resolvedIndex, frame->Gametic, unsigned(frame->CombinedCRC32), gametic);
	if (HCDERewind_SelfCheck(resolvedIndex, "manual-selfcheck"))
	{
		Printf(PRINT_HIGH,
			"net_rewind_selfcheck: passed; live world restored to gametic=%d with matching CRC fingerprints.\n",
			gametic);
	}
	else
	{
		Printf(PRINT_HIGH, "net_rewind_selfcheck: failed (see net.rewind trace).\n");
	}
}

// =====================================================================
// Phase 5: server-side lag-compensated hit resolution.
// =====================================================================

// Master switch. Disabled by default. Enable once the rewind keyframe ring
// is healthy (`net_rewind_enable 1`). Wraps player hitscan in a historical
// bracket and replays confirmed damage onto the live world afterward.
CVAR(Bool, sv_lagcomp, false, CVAR_SERVERINFO | CVAR_NOSAVE)

// Maximum bracket age in tics. Rejects view tics older than this so high-ping
// clients cannot rewind the whole match. ~12 tics at 35 Hz ~= 350 ms.
CUSTOM_CVAR(Int, sv_lagcomp_max_age_tics, 12, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 1) self = 1;
	if (self > TICRATE * 2) self = TICRATE * 2;
}

struct FHCDELagCompDamageEvent
{
	uint32_t TargetNetID = 0;
	uint32_t InflictorNetID = 0;
	uint32_t SourceNetID = 0;
	int RequestedDamage = 0;	// original P_DamageMobj `damage` arg (not post-armor retval)
	int ModIndex = 0;
	int Flags = 0;
	double AngleDegrees = 0.0;
};

namespace
{
int gLagCompDepth = 0;
bool gLagCompReplaying = false;
TArray<FHCDELagCompDamageEvent> gLagCompPendingEvents;
uint64_t gLagCompTotalScopes = 0u;
uint64_t gLagCompTotalEngaged = 0u;
uint64_t gLagCompTotalEventsCaptured = 0u;
uint64_t gLagCompTotalEventsReplayed = 0u;
uint64_t gLagCompTotalEventsLost = 0u;
}

bool HCDERewind_LagCompActive()
{
	return gLagCompDepth > 0;
}

bool HCDERewind_LagCompReplaying()
{
	return gLagCompReplaying;
}

bool HCDERewind_LagCompRecordDamage(AActor* target, AActor* inflictor, AActor* source,
	int damage, int modIndex, int flags, double angleDegrees)
{
	if (gLagCompDepth <= 0 || gLagCompReplaying || target == nullptr || damage <= 0)
		return false;

	const uint32_t targetID = target->GetNetworkID();
	if (targetID == 0u)
		return false;

	FHCDELagCompDamageEvent ev = {};
	ev.TargetNetID = targetID;
	ev.InflictorNetID = (inflictor != nullptr) ? inflictor->GetNetworkID() : 0u;
	ev.SourceNetID = (source != nullptr) ? source->GetNetworkID() : 0u;
	ev.RequestedDamage = damage;
	ev.ModIndex = modIndex;
	ev.Flags = flags;
	ev.AngleDegrees = angleDegrees;
	gLagCompPendingEvents.Push(ev);
	++gLagCompTotalEventsCaptured;
	return true;
}

HCDELagCompScope::HCDELagCompScope(AActor* attacker, const char* reason)
{
	if (!*sv_lagcomp || !*net_rewind_enable)
		return;
	if (!I_IsLocalHCDEServiceAuthority() || gamestate != GS_LEVEL || primaryLevel == nullptr)
		return;
	if (insave || !netgame || demoplayback)
		return;
	if (attacker == nullptr || attacker->player == nullptr)
		return;

	++gLagCompTotalScopes;

	mPlayerNum = int(attacker->player - players);
	if (mPlayerNum < 0 || mPlayerNum >= MAXPLAYERS)
		return;

	const int viewTic = HCDERewind_GetClientViewTic(mPlayerNum);
	if (viewTic < 0)
		return;
	const int age = gametic - viewTic;
	if (age < 0 || age > int(*sv_lagcomp_max_age_tics))
		return;

	if (gLagCompDepth > 0)
		return;

	const int historicalIndex = LagCompFindHistoricalIndex(viewTic);
	if (historicalIndex < 0)
		return;

	gLagCompPendingEvents.Clear();
	LagCompSaveCaptureTimer(mSavedCaptureGametic, mSavedCaptureMS);
	mLiveGametic = gametic;

	if (!LagCompCaptureLiveKeyframe(mLiveKeyframe))
	{
		DebugTrace::Warningf("net.rewind",
			"lagcomp scope live-capture failed reason=%s player=%d gametic=%d",
			reason != nullptr ? reason : "?", mPlayerNum, gametic);
		return;
	}

	if (!HCDERewind_RestoreKeyframe(historicalIndex, "lagcomp-scope"))
	{
		DebugTrace::Warningf("net.rewind",
			"lagcomp scope historical-restore failed reason=%s player=%d viewtic=%d hist-index=%d",
			reason != nullptr ? reason : "?", mPlayerNum, viewTic, historicalIndex);
		mLiveKeyframe.Clear();
		return;
	}

	mEngaged = true;
	++gLagCompDepth;
	++gLagCompTotalEngaged;

	DebugTrace::Markf("net.rewind",
		"lagcomp scope engaged reason=%s player=%d viewtic=%d age=%d hist-index=%d hist-tic=%d live-tic=%d",
		reason != nullptr ? reason : "?", mPlayerNum, viewTic, age, historicalIndex,
		LagCompHistoricalGametic(historicalIndex), mLiveGametic);
}

HCDELagCompScope::~HCDELagCompScope()
{
	if (!mEngaged)
		return;

	const bool restored = LagCompRestoreLiveKeyframe(mLiveKeyframe, mLiveGametic);
	mLiveKeyframe.Clear();
	LagCompRestoreCaptureTimer(mSavedCaptureGametic, mSavedCaptureMS);

	if (!restored)
	{
		DebugTrace::Warningf("net.rewind",
			"lagcomp scope LIVE RESTORE FAILED player=%d events=%d - WORLD STUCK IN PAST",
			mPlayerNum, int(gLagCompPendingEvents.Size()));
		gLagCompPendingEvents.Clear();
		--gLagCompDepth;
		mEngaged = false;
		return;
	}

	if (gLagCompPendingEvents.Size() > 0u)
	{
		gLagCompReplaying = true;
		for (const auto& ev : gLagCompPendingEvents)
		{
			AActor* target = static_cast<AActor*>(NetworkEntityManager::GetNetworkEntity(ev.TargetNetID));
			if (target == nullptr || (target->ObjectFlags & OF_EuthanizeMe))
			{
				++gLagCompTotalEventsLost;
				continue;
			}
			AActor* inflictor = static_cast<AActor*>(NetworkEntityManager::GetNetworkEntity(ev.InflictorNetID));
			AActor* source = static_cast<AActor*>(NetworkEntityManager::GetNetworkEntity(ev.SourceNetID));
			// Replay with the original requested damage so armor/type modifiers
			// run exactly once on the live world. Recording post-armor retval
			// and feeding it back in as `damage` would double-apply armor.
			const int replayed = P_DamageMobj(target, inflictor, source, ev.RequestedDamage,
				FName(ENamedName(ev.ModIndex)), ev.Flags, DAngle::fromDeg(ev.AngleDegrees));
			if (replayed > 0)
				++gLagCompTotalEventsReplayed;
		}
		gLagCompReplaying = false;
	}
	gLagCompPendingEvents.Clear();

	--gLagCompDepth;
	mEngaged = false;
}

CCMD(net_lagcomp_summary)
{
	Printf(PRINT_HIGH,
		"sv_lagcomp=%d max-age=%d tics scopes=%llu engaged=%llu events captured=%llu replayed=%llu lost=%llu depth=%d\n",
		*sv_lagcomp ? 1 : 0, int(*sv_lagcomp_max_age_tics),
		static_cast<unsigned long long>(gLagCompTotalScopes),
		static_cast<unsigned long long>(gLagCompTotalEngaged),
		static_cast<unsigned long long>(gLagCompTotalEventsCaptured),
		static_cast<unsigned long long>(gLagCompTotalEventsReplayed),
		static_cast<unsigned long long>(gLagCompTotalEventsLost),
		gLagCompDepth);
}
