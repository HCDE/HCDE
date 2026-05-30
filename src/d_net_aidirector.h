#pragma once

// HCDE Monster AI Director scaffold.
//
// Roadmap board item: #13 ("Monster AI System"). This is Phase 1 scaffold
// only -- the director CVAR, a tick stub, observability counters, and a
// status CCMD. No hints are issued, no playsim state is touched, and no
// new replication path is added. With `sv_aidirector_enable = 0` (the
// default) zero director code runs and behaviour is bit-identical.
//
// Hard rules from `docs/HCDE_AIDIRECTOR_AUDIT.md`:
//
//   - Server-authoritative end-to-end. Clients never tick the director.
//   - Director mutates actors only via existing playsim APIs; no bypass.
//   - Determinism: all randomness goes through the named FRandom
//     `pr_aidirector` so saves and demos round-trip identically.
//   - No new client-side AI replication path. Hints reuse the existing
//     actor snapshot stream or are not visible to clients at all.

#include <cstdint>

struct FHCDEAIDirectorState
{
	bool   AuthorityActive = false;
	int    TicksSinceStart = 0;
	int    LastTickWallclockMs = 0;     // diagnostic only
	int    LastSweepTic = 0;
	int    SweepCount = 0;
	int    LastSweepWallclockMs = 0;
	int    MaxSweepWallclockMs = 0;
	int    MonsterCountObserved = 0;
	int    AliveMonsterCountObserved = 0;
	int    DormantMonsterCountObserved = 0;
	int    CorpseMonsterCountObserved = 0;
	int    GroupCountObserved = 0;
	int    LargestGroupSizeObserved = 0;
	int    RegroupHintsStored = 0;
	int    DeterministicRngDraws = 0;
	int    HintsIssuedThisTic = 0;
	int    HintsIssuedTotal = 0;
	int    HintsConsumedTotal = 0;      // bumps when a chase call applies a regroup target
};

class AActor;

const FHCDEAIDirectorState& HCDEAIDirectorReadState();

// Per-tic step. Phase 1 implementation:
//   - Returns immediately if the local instance is not the authority.
//   - Returns immediately if `sv_aidirector_enable` is 0.
//   - Otherwise advances `TicksSinceStart` and updates an empty observation
//     pass so `ai_status` can show motion. No actor mutation, no RNG draws.
void HCDEAIDirectorTick();

// True iff the director is enabled AND this instance is the authority.
bool HCDEAIDirectorShouldDriveAuthority();

// Phase 3 read-only hint accessor. Returns true if `actor` currently has a
// stored regroup hint, populating `outGroupId` and `outStoredTic`. Otherwise
// returns false and leaves the outputs unchanged. Calling this DOES NOT
// mutate the hint store -- a future PR may add a paired "consume" API once
// the A_Chase callsite is ready.
//
// Today this is purely observational: even when it returns true, no monster
// behaviour changes. The function exists so the eventual Phase 3 actor hook
// has a stable interface to call before the playsim wiring lands.
bool HCDEAIDirectorReadRegroupHint(const AActor* actor, int* outGroupId, int* outStoredTic);

// Phase 3 consume hook. Returns true and populates outputs if a hint was
// available, then increments `HintsConsumedTotal`. This is the API the
// eventual A_Chase patch will call. With `sv_aidirector_regroup_hint=0`
// (default) no hints are stored, so this always returns false.
//
// Even with the CVAR on, returning true does NOT mutate the actor or its
// state -- the caller is responsible for any movement decision. This API
// is the "hand the hint to the call site" boundary.
bool HCDEAIDirectorConsumeRegroupHint(const AActor* actor, int* outGroupId, int* outStoredTic);

// Phase 3 movement hook. Returns a deterministic XY regroup target for
// `actor` and increments `HintsConsumedTotal` when a target is returned. The
// caller still uses normal playsim movement helpers; this function only hands
// back a point to bias the existing chase direction.
bool HCDEAIDirectorConsumeRegroupTarget(const AActor* actor, double* outX, double* outY, int* outGroupId, int* outStoredTic);
