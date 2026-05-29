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
	int    HintsIssuedThisTic = 0;      // 0 in Phase 1
	int    HintsIssuedTotal = 0;        // 0 in Phase 1
};

const FHCDEAIDirectorState& HCDEAIDirectorReadState();

// Per-tic step. Phase 1 implementation:
//   - Returns immediately if the local instance is not the authority.
//   - Returns immediately if `sv_aidirector_enable` is 0.
//   - Otherwise advances `TicksSinceStart` and updates an empty observation
//     pass so `ai_status` can show motion. No actor mutation, no RNG draws.
void HCDEAIDirectorTick();

// True iff the director is enabled AND this instance is the authority.
bool HCDEAIDirectorShouldDriveAuthority();
