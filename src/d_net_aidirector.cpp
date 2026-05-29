// HCDE Monster AI Director scaffold -- see d_net_aidirector.h.
//
// Phase 1: scaffold and diagnostics only. This file does not touch any
// actor state, does not draw randomness on the hot path, and does not add
// any new replication. With the master CVAR off, zero director code runs.

#include "d_net_aidirector.h"

#include "actor.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "common/engine/i_net.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_levellocals.h"
#include "i_time.h"
#include "m_random.h"
#include "printf.h"
#include "tarray.h"

namespace
{
FHCDEAIDirectorState DirectorState;

struct FHCDEAIObservedMonster
{
	const AActor* Actor = nullptr;
	int GroupId = -1;
};

struct FHCDEAICluster
{
	DVector2 Center = {};
	int Count = 0;
};

struct FHCDEAIRegroupHint
{
	const AActor* Actor = nullptr;
	int GroupId = -1;
	int StoredTic = 0;
};

TArray<FHCDEAIObservedMonster> ObservedMonsters;
TArray<FHCDEAICluster> ObservedClusters;
TArray<FHCDEAIRegroupHint> RegroupHints;

// Named RNG -- mirrors `pr_invasion`. The director MUST funnel all
// randomness through this single instance so saves and demos serialize
// the RNG state deterministically (see Audit Phase 1 / hard rule 3).
FRandom pr_aidirector("AIDirector");
}

// Master switch. Server-controlled in netgames. When 0 (default) no
// director code runs and monster behaviour is bit-identical to today.
CVAR(Bool, sv_aidirector_enable, false, CVAR_ARCHIVE | CVAR_SERVERINFO)

// Frame budget in tics between Phase 2 observation sweeps. Today the
// observation pass is empty; the CVAR is reserved so Phase 2 can tune
// budget without renaming.
CUSTOM_CVAR(Int, sv_aidirector_sweep_tics, 7, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (self < 1) self = 1;
	if (self > 70) self = 70;
}

CVAR(Bool, sv_aidirector_regroup_hint, false, CVAR_ARCHIVE | CVAR_SERVERINFO)

const FHCDEAIDirectorState& HCDEAIDirectorReadState()
{
	return DirectorState;
}

bool HCDEAIDirectorShouldDriveAuthority()
{
	if (!*sv_aidirector_enable)
		return false;
	if (!I_IsLocalHCDEServiceAuthority())
		return false;
	return true;
}

static void HCDEAIDirectorObserveMonsters()
{
	const int sweepStartMs = I_msTime();
	DirectorState.LastSweepTic = primaryLevel != nullptr ? primaryLevel->time : 0;
	++DirectorState.SweepCount;
	DirectorState.MonsterCountObserved = 0;
	DirectorState.AliveMonsterCountObserved = 0;
	DirectorState.DormantMonsterCountObserved = 0;
	DirectorState.CorpseMonsterCountObserved = 0;
	DirectorState.GroupCountObserved = 0;
	DirectorState.LargestGroupSizeObserved = 0;
	DirectorState.RegroupHintsStored = 0;
	ObservedMonsters.Clear();
	ObservedClusters.Clear();
	RegroupHints.Clear();

	if (primaryLevel == nullptr)
		return;

	auto it = primaryLevel->GetThinkerIterator<AActor>();
	while (AActor* actor = it.Next())
	{
		if ((actor->flags3 & MF3_ISMONSTER) == 0)
			continue;

		++DirectorState.MonsterCountObserved;
		if (actor->flags2 & MF2_DORMANT)
		{
			++DirectorState.DormantMonsterCountObserved;
		}
		if ((actor->flags & MF_CORPSE) || actor->health <= 0)
		{
			++DirectorState.CorpseMonsterCountObserved;
		}
		else
		{
			++DirectorState.AliveMonsterCountObserved;
		}

		if (actor->health <= 0 || (actor->flags & MF_CORPSE))
			continue;

		int groupId = -1;
		const DVector2 pos = actor->Pos().XY();
		for (unsigned int i = 0; i < ObservedClusters.Size(); ++i)
		{
			if ((ObservedClusters[i].Center - pos).LengthSquared() <= 512.0 * 512.0)
			{
				groupId = int(i);
				FHCDEAICluster& cluster = ObservedClusters[i];
				cluster.Center = (cluster.Center * double(cluster.Count) + pos) / double(cluster.Count + 1);
				++cluster.Count;
				break;
			}
		}
		if (groupId < 0)
		{
			FHCDEAICluster cluster;
			cluster.Center = pos;
			cluster.Count = 1;
			groupId = int(ObservedClusters.Push(cluster));
		}

		ObservedMonsters.Push({ actor, groupId });
		if (ObservedClusters[groupId].Count > DirectorState.LargestGroupSizeObserved)
		{
			DirectorState.LargestGroupSizeObserved = ObservedClusters[groupId].Count;
		}
	}

	DirectorState.GroupCountObserved = int(ObservedClusters.Size());
	if (*sv_aidirector_regroup_hint)
	{
		for (const auto& observed : ObservedMonsters)
		{
			if (observed.GroupId >= 0 && ObservedClusters[observed.GroupId].Count >= 3)
			{
				RegroupHints.Push({ observed.Actor, observed.GroupId, DirectorState.LastSweepTic });
			}
		}
		DirectorState.RegroupHintsStored = int(RegroupHints.Size());
	}

	DirectorState.LastSweepWallclockMs = int(I_msTime() - sweepStartMs);
	if (DirectorState.LastSweepWallclockMs > DirectorState.MaxSweepWallclockMs)
	{
		DirectorState.MaxSweepWallclockMs = DirectorState.LastSweepWallclockMs;
	}
}

void HCDEAIDirectorTick()
{
	if (!HCDEAIDirectorShouldDriveAuthority())
	{
		DirectorState.AuthorityActive = false;
		DirectorState.HintsIssuedThisTic = 0;
		return;
	}

	DirectorState.AuthorityActive = true;
	DirectorState.TicksSinceStart++;
	DirectorState.HintsIssuedThisTic = 0;
	DirectorState.LastTickWallclockMs = I_msTime();

	if (DirectorState.TicksSinceStart == 1 || (DirectorState.TicksSinceStart % *sv_aidirector_sweep_tics) == 0)
	{
		HCDEAIDirectorObserveMonsters();
	}

	// Touching the named RNG here is deferred -- the audit prefers RNG draws
	// happen exactly when hints are issued, never speculatively.
	(void)pr_aidirector;
}

CCMD(ai_status)
{
	const FHCDEAIDirectorState& state = HCDEAIDirectorReadState();
	Printf(PRINT_HIGH, "\n=== HCDE AI Director ===\n");
	Printf(PRINT_HIGH, "  sv_aidirector_enable      = %s\n", *sv_aidirector_enable ? "on" : "off");
	Printf(PRINT_HIGH, "  sv_aidirector_sweep_tics  = %d\n", *sv_aidirector_sweep_tics);
	Printf(PRINT_HIGH, "  authority-drives          = %s\n",
		HCDEAIDirectorShouldDriveAuthority() ? "yes" : "no");
	Printf(PRINT_HIGH, "  authority-active          = %s\n", state.AuthorityActive ? "yes" : "no");
	Printf(PRINT_HIGH, "  ticks-since-start         = %d\n", state.TicksSinceStart);
	Printf(PRINT_HIGH, "  last-sweep-tic            = %d\n", state.LastSweepTic);
	Printf(PRINT_HIGH, "  sweep-count               = %d\n", state.SweepCount);
	Printf(PRINT_HIGH, "  last-sweep-wallclock-ms   = %d (diagnostic only)\n", state.LastSweepWallclockMs);
	Printf(PRINT_HIGH, "  max-sweep-wallclock-ms    = %d (diagnostic only)\n", state.MaxSweepWallclockMs);
	Printf(PRINT_HIGH, "  monsters-observed         = %d\n", state.MonsterCountObserved);
	Printf(PRINT_HIGH, "  monsters-alive            = %d\n", state.AliveMonsterCountObserved);
	Printf(PRINT_HIGH, "  monsters-dormant          = %d\n", state.DormantMonsterCountObserved);
	Printf(PRINT_HIGH, "  monsters-corpse           = %d\n", state.CorpseMonsterCountObserved);
	Printf(PRINT_HIGH, "  groups-observed           = %d\n", state.GroupCountObserved);
	Printf(PRINT_HIGH, "  largest-group-size        = %d\n", state.LargestGroupSizeObserved);
	Printf(PRINT_HIGH, "  sv_aidirector_regroup_hint = %s\n", *sv_aidirector_regroup_hint ? "on" : "off");
	Printf(PRINT_HIGH, "  regroup-hints-stored      = %d\n", state.RegroupHintsStored);
	Printf(PRINT_HIGH, "  deterministic-rng         = pr_aidirector named stream; draws=%d\n", state.DeterministicRngDraws);
	Printf(PRINT_HIGH, "  hints-issued-this-tic     = %d (Phase 3 work)\n", state.HintsIssuedThisTic);
	Printf(PRINT_HIGH, "  hints-issued-total        = %d\n", state.HintsIssuedTotal);
	Printf(PRINT_HIGH, "  last-tick-wallclock-ms    = %d (diagnostic only)\n", state.LastTickWallclockMs);
	Printf(PRINT_HIGH, "  phase                     = scaffold-only; no actor mutation, no replication.\n");
	Printf(PRINT_HIGH, "  see docs/HCDE_AIDIRECTOR_AUDIT.md for boundaries.\n");
	Printf(PRINT_HIGH, "========================\n");
}
