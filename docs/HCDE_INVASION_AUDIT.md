# HCDE Invasion Mode Audit

Roadmap board item: **#15** ("Finish Invasion mode"). The board lists this
as "In progress" but the implementation footprint is already large.
This audit captures what is shipping, what is open, and a concrete
finish-line task list.

## What's already in tree

| Concern | Status | Notes |
| --- | --- | --- |
| Authority state machine | Implemented | `EInvasionState { DISABLED, WAITING, COUNTDOWN, SPAWNING, CLEANUP, INTERMISSION }`, `Net_GetInvasionState()` and friends in `d_net.h`. |
| Wave director | Implemented | `FInvasionWaveDirector` with current wave, max waves, budget, spawned/cleared counters. |
| Spawn directory | Implemented | `FInvasionSpawnDirectory` with classic + fallback sources, registered spawn spots, plan budgets. |
| Replicated invasion actor lane | Implemented | `FInvasionReplicatedActorRef` with separate id/ptr indexes, action-state tracking, mirror/visual fallback for high-ping clients. |
| Native snapshot | Implemented | `HCDEInvasionSnapshotV1` and V2 with capability gating (`HCDELiveCapInvasionSnapshotV2`), 52-byte header, payload budget, action history. |
| ACS API | Implemented | `ACSF_InvasionGetState` / `Wave` / `MaxWaves` / `Budget` / `Spawned` / `Cleared` / `ActiveMonsterCount` / `IsBossWave` / etc., plus Skulltag-compatible LSPEC6 opcodes. |
| Simulation LOD | Implemented | `EHCDEInvasionSimulationLOD { FULL, REDUCED, DORMANT }` with `sv_invasionsimlod`, suspended/allowed/skipped counters, wake distance/health hooks. |
| Late-join repair | Implemented | Replays pending invasion spawn events, snapshot baseline repair, rebuild on cap exhaustion. |
| Diagnostic surface | Implemented | `net_actorstats`, `net_profile`, `net_stressreport` all expose invasion counters; `sv_invasiondebug` mirrors invasion state changes to the HUD. |

The investment here is substantial — easily the largest single subsystem
the roadmap calls out. "Finish" means closing specific gaps, not
building from scratch.

## Finish-line task list

Each item below is sized to fit in a single PR.

### Mechanic completeness

1. **Wave director boss-cycle audit.** `IsBossWave` exists, but verify
   the director seeds boss waves at the documented cadence (every Nth
   wave, configurable). Add a test scene that walks 10 waves and asserts
   the boss flag fires at the expected indices.
2. **Spawn fallback decision log.** `Net_GetInvasionSpawnFallbackSource()`
   returns the source enum but the *why* (no classic spots / blocked /
   wave-cleared) isn't currently reported. Add a per-fallback diag line
   so soak runs can graph fallback rates.
3. **`InvasionWipeGraceTics` policy doc.** The grace window for "all
   players dead before declaring failure" exists. Document the policy
   (default value, when it ticks, how rejoins reset it) so server ops
   can tune it.
4. **Director RNG seed plumbing.** `pr_invasion` is a seeded RNG. Verify
   the seed is captured in savegames AND in net rewind keyframes so a
   restored snapshot resumes wave selection deterministically.

### Multiplayer correctness

5. **Late-join replay scope check.** Confirm
   `InvasionPendingSpawnEvents` correctly bounds replay length per
   late-joiner. A large mid-wave drop should not crash the replay loop
   on rejoin. Add an explicit test in the existing
   `tests/netcode_step12/` harness.
6. **Snapshot V2 capability fallback path.** `HCDELiveCapInvasionSnapshotV2`
   gates the V2 header. Today a peer that lacks V2 hits
   `server-snapshot-invasion-capability-missing` as a hard fail. Decide:
   reject the connection, or fall back to V1? Document the chosen path.
7. **Simulation LOD soak.** `sv_invasionsimlod` switches monsters
   between FULL / REDUCED / DORMANT. Run a soak with 200+ active
   invasion actors and confirm the LOD distribution matches expectation
   under both `net_stressreport` and observed visual fidelity for live
   peers.

### Operator UX

8. **`sv_invasiondebug` levels.** Today level 1 mirrors important
   state/spawn/result lines. Add level 2 for per-spawn detail and
   document the levels in `docs/HCDE_INVASION.md` (which doesn't exist
   yet).
9. **Wave outcome announcement.** `InvasionAnnouncementState` and
   `InvasionAnnouncementWave` track per-client deduped announcements.
   Verify the localisation strings exist for all 6 state transitions
   and that countdown second-by-second ("3...2...1...") is gated by
   `InvasionAnnouncementLastCountdownSecond`.
10. **Operator-facing doc.** Write `docs/HCDE_INVASION.md` covering:
    - CVAR list (`sv_invasion*`, `net_*` invasion-relevant)
    - State machine with allowed transitions
    - Spawn fallback decision tree
    - Diagnostic CCMD list
    - Tuning guidance for wave budget vs. simulation LOD

### Bug-fix candidates worth confirming

11. `InvasionMirrorVisualFallback*` constants are conservative defaults
    (8 units/tic, 1.10x multiplier, 12 max). Spot-check that high-ping
    actors actually converge on their authoritative positions within a
    couple of tics of acknowledgement; a runaway mirror is a known
    risk in this style of fallback.
12. `HCDEInvasionProjectileMirrorMaxAgeTics = TICRATE * 3` (3 seconds).
    Verify no projectile class has natural lifetime longer than 3
    seconds without authoritative replication, otherwise it
    silently disappears for spectators / mirrors.

## What is NOT on the finish-line list

- **Brand-new wave templates.** Authoring is a content task, not an
  engine task. The director already supports configurable waves.
- **A dedicated invasion lobby / queue UI.** Out of scope for #15;
  belongs with hcdeserv UI work (#23) if anywhere.
- **Spectator fly-cam during invasion.** Distinct feature; not
  required to "finish" invasion as a mode.

## Recommendation

Once items 1-10 land (and 11-12 are signed off as either fixed or
tracked), promote #15 to "Done foundations" alongside #14 and #5.
The current "in progress" status reflects polish work, not core
implementation.

## Soak / probe log

### 2026-05-28 — late-join replay probe

Command:

```text
python tests/netcode_step12/f6_invasion_latejoin_replay.py --server build/RelWithDebInfo/hcdeserv.exe --client build/RelWithDebInfo/hcde.exe --iwad doom2.wad --duration 25
```

Result:

- `replay events observed: 0`
- `crash markers: 0`
- `pending-event bound ok: True`
- Probe exit: passed

Interpretation: the short late-join/drop/rejoin probe did not reproduce a
replay crash or unbounded pending-event window. A longer 200+ actor soak is
still recommended for simulation LOD tuning.
