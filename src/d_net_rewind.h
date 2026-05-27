#pragma once

// HCDE rewind / keyframe ring buffer
//
// Inspired by DSDA Doom's rewind feature. The server captures a full
// compressed snapshot of the level (sectors, lines, thinkers, actors,
// players, ACS, RNG state) every `net_rewind_interval` seconds and keeps
// the last `net_rewind_depth` keyframes in memory. Each keyframe is the
// same blob the savegame system would write -- we reuse FDoomSerializer +
// FCompressedBuffer so the format is battle-tested.
//
// PHASE 1: capture only.
//   - Server-side ring buffer, console diagnostics, memory accounting, and
//     per-keyframe CRC fingerprints for determinism/state-comparison tooling.
//
// PHASE 2: restore primitives.
//   - HCDERewind_RestoreKeyframe() reloads world state via the same
//     serialization machinery as savegame loads.
//   - Debug command `net_rewind_test_restore <index>` exercises the restore
//     path standalone.
//
// PHASE 4 (current): lag-compensation hit-rewind primitives.
//   - Track per-client view-tic in raw gametic units (sequenceAck * TicDup).
//   - Bracket helper: capture-current -> restore keyframe near client view
//     tic -> restore-current. Used by `net_rewind_lagcomp_probe` today,
//     intended to wrap authority hitscan/projectile resolution next.
//   - Strict superset of Odamex unlagged: rewinds monsters, projectiles,
//     and sector states, not just player positions.
//
// Memory budget rough estimate: a typical Doom 2 level snapshot is
// 50-300 KB compressed. With the default 1s interval and 10 frame depth
// the worst-case footprint is ~3 MB on the server.

#include "fs_decompress.h"
#include "zstring.h"

#include <cstddef>
#include <cstdint>

// One captured snapshot. The compressed buffers own heap memory that must be
// released via Clear()/destructor; FCompressedBuffer itself does NOT free in
// its destructor, so the keyframe wraps it with proper RAII semantics.
struct FHCDERewindKeyframe
{
	int Gametic = -1;
	uint64_t CaptureMS = 0u;
	uint32_t CaptureCostMS = 0u;	// how long the snapshot took to build
	uint32_t LevelCRC32 = 0u;
	uint32_t GlobalsCRC32 = 0u;
	uint32_t CombinedCRC32 = 0u;	// level + globals fingerprint for determinism diagnostics
	FileSys::FCompressedBuffer Level = {};
	FileSys::FCompressedBuffer Globals = {};	// RNG state + globalfreeze etc.

	FHCDERewindKeyframe() = default;
	~FHCDERewindKeyframe() { Clear(); }

	// Copying would silently double-free the heap buffers; suppress it.
	FHCDERewindKeyframe(const FHCDERewindKeyframe&) = delete;
	FHCDERewindKeyframe& operator=(const FHCDERewindKeyframe&) = delete;

	// Move transfers buffer ownership and nulls the source so the moved-from
	// keyframe's destructor / Clear() is a safe no-op. TArray<T>'s in-place
	// memmove during Delete() is also safe because the trailing slot beyond
	// Count is never accessed after the shift.
	FHCDERewindKeyframe(FHCDERewindKeyframe&& other) noexcept;
	FHCDERewindKeyframe& operator=(FHCDERewindKeyframe&& other) noexcept;

	bool Valid() const { return Gametic >= 0 && Level.mBuffer != nullptr; }
	size_t SizeBytes() const
	{
		return Level.mCompressedSize + Globals.mCompressedSize;
	}
	void Clear();
};

// Server-only. Call once per ticked gametic. Captures a keyframe whenever the
// configured interval has elapsed since the last capture.
void HCDERewind_OnServerTick();

// Force an immediate capture (used by `net_rewind_capture_now`). Returns
// false on failure (not authority / no level loaded / serializer failure).
bool HCDERewind_CaptureNow(const char* reason);

// Restore a captured keyframe into the current authority playsim.
// This is intentionally server/authority-only in Phase 2; live clients are not
// resynchronized yet, so use this as a debug/proof command before Phase 3.
bool HCDERewind_RestoreKeyframe(int index, const char* reason);

// Reset the ring buffer. Called on map change, disconnect, or via console.
void HCDERewind_Reset(const char* reason);

// Console summary; renders to PRINT_HIGH.
void HCDERewind_PrintSummary();

// How many bytes the ring buffer is consuming right now.
size_t HCDERewind_TotalBytes();

// Lookup helpers used by future phases. Return nullptr if no matching frame.
const FHCDERewindKeyframe* HCDERewind_FindByIndex(int index);
const FHCDERewindKeyframe* HCDERewind_FindNearestTic(int targetGametic);
int HCDERewind_StoredCount();

// Phase 4: server-side lag compensation metadata. The input stream reports
// the last authority snapshot sequence/tic the client had seen when it built
// commands; that is the client's approximate "view of the world".
void HCDERewind_RecordClientViewTic(int playerNum, int viewTic);
int HCDERewind_GetClientViewTic(int playerNum);

// Debug/proof primitive for Phase 4. This restores the nearest historical
// keyframe for the client's view tic, then restores the current world again.
// It does not run weapon logic yet; it proves the bracket is stable.
bool HCDERewind_TestLagCompBracket(int playerNum, const char* reason);

// Manual restore integrity check. Captures live state, restores one stored
// keyframe, restores live state, then recaptures and compares CRC fingerprints.
// index < 0 selects the newest stored keyframe.
bool HCDERewind_SelfCheck(int index, const char* reason);

// Phase 5 - Server-side lag-compensated hit resolution.
//
// Construct a HCDELagCompScope around an attacker's hit-resolution code:
//
//     {
//         HCDELagCompScope lag(attacker, "shotgun");
//         if (lag.Engaged()) { /* world is at attacker's view tic */ }
//         P_LineAttack(...);
//         // lag scope's destructor restores the current world here, then
//         // replays any captured damage events onto the live actors.
//     }
//
// The scope captures the live world, restores the historical keyframe nearest
// to the attacker's recorded view tic, runs the bracketed code (which sees
// targets at their historical positions), then on destruction restores the
// live world again. During the bracket, P_DamageMobj routes through
// HCDERewind_LagCompRecordDamage() so that damage events are captured by
// network ID rather than raw pointer (the restore pass invalidates all actor
// pointers, since the deserializer rebuilds the actor list). After the live
// restore, the recorded events are replayed against actors looked up by
// NetworkEntityManager::GetNetworkEntity, which preserves identity across the
// rewind-restore-rewind cycle.
//
// Failure modes (Engaged() returns false, no rewind happens):
//   - sv_lagcomp is disabled.
//   - Not running on the authority.
//   - No keyframe is close enough to the attacker's view tic.
//   - Capture or restore failed (logged via DebugTrace::Warningf).
//
// When Engaged() is false the scope is a no-op and P_DamageMobj behaves
// exactly as it does without lag comp. This makes the scope safe to drop
// into existing weapon code without conditional branching at the call site.
struct AActor;

class HCDELagCompScope
{
public:
	HCDELagCompScope(AActor* attacker, const char* reason);
	~HCDELagCompScope();

	HCDELagCompScope(const HCDELagCompScope&) = delete;
	HCDELagCompScope& operator=(const HCDELagCompScope&) = delete;

	bool Engaged() const { return mEngaged; }

private:
	bool mEngaged = false;
	int mPlayerNum = -1;
	FHCDERewindKeyframe mLiveKeyframe;
	int mLiveGametic = 0;
	int mSavedCaptureGametic = -1;
	uint64_t mSavedCaptureMS = 0u;
};

// Returns true while a HCDELagCompScope is active. Used by P_DamageMobj to
// decide whether to record a damage event for replay.
bool HCDERewind_LagCompActive();

// True while the scope destructor is replaying captured events onto the live
// world. P_DamageMobj must not record during replay or events nest forever.
bool HCDERewind_LagCompReplaying();

// Capture a damage event during a lag-comp scope. Stores the original
// P_DamageMobj request (requested damage + mod/flags/angle) keyed by network
// ID so it survives the live restore. Returns true when queued. The caller
// still runs DoDamageMobj in the historical bracket (throwaway world); the
// queued event is replayed once against live actors when the scope ends.
bool HCDERewind_LagCompRecordDamage(AActor* target, AActor* inflictor, AActor* source,
	int damage, int modIndex, int flags, double angleDegrees);
