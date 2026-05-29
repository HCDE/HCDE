# HCDE Invasion Mode — Operator Guide

This document is the operator-facing reference for Invasion mode
(roadmap board item #15). It covers the CVAR surface, state machine,
spawn logic, diagnostics, and tuning knobs so a server admin can run a
stable, observable invasion server without reading the source.

## Quick start

On a dedicated server, the minimal viable config is:

```
sv_gametype 4
sv_invasionwaves 8
sv_invasioncountdowntime 30
sv_invasionspawntime 8
sv_invasionbudgetbase 40
sv_invasionbudgetstep 8
sv_invasionperplayer 6
sv_invasionbosswaveevery 5
sv_invasionsimlod 1
```

Launch with `-dedicated -host 8` (or however many players you expect) and
an invasion map. The server will cycle through waves, announce
countdowns, and expose diagnostics via `net_stressreport` and the
`net_invasion_*` family of CCMDs.

## CVAR surface (serverinfo)

All of these are `CVAR_SERVERINFO | CVAR_NOSAVE` unless noted.

| CVAR | Default | Meaning |
| --- | --- | --- |
| `sv_invasioncountdowntime` | 30 s | Seconds before a wave starts (countdown phase). |
| `sv_invasionspawntime` | 8 s | Fallback window used to compute spawn pacing when `sv_invasionspawnburst` is active. |
| `sv_invasionspawnburst` | 3 | How many monsters to spawn per spawn tick during the burst phase. |
| `sv_invasionmaxactive` | 0 (unlimited) | Hard cap on simultaneously alive invasion monsters. |
| `sv_invasionbudgetbase` | 40 | Base monster budget for wave 1. |
| `sv_invasionbudgetstep` | 8 | Additional budget per wave after the first. |
| `sv_invasionperplayer` | 6 | Extra budget per human participant (adds to the wave budget). |
| `sv_invasionbosswaveevery` | 5 | Boss wave every Nth wave (0 = disabled). |
| `sv_invasionbossbonus` | 20 | Extra budget granted on boss waves. |
| `sv_invasionspotusemaptags` | false | If true, respect map-defined spot tags for wave-specific spawns. |
| `sv_invasionspotfallback` | true | Allow fallback to deathmatch / player starts when no classic invasion spots exist. |
| `sv_invasionsimlod` | true | Enable simulation LOD (FULL / REDUCED / DORMANT) for distant or numerous monsters. |
| `sv_invasionsimlodfullrange` | 2048 | Distance (units) inside which monsters stay FULL. |
| `sv_invasionsimlodreducedrange` | 4096 | Distance inside which monsters may be REDUCED. |
| `sv_invasionsimlodreducedinterval` | 5 tics | Think interval when REDUCED. |
| `sv_invasionsimloddormantinterval` | 3 s | Think interval when DORMANT. |
| `sv_invasiondebug` | 0 | 0 = off, 1 = important state/spawn/result lines, 2 = per-spawn detail (class, spot source, wave, budget). |
| `sv_invasionexitonvictory` | true | Dedicated servers exit the map on victory (clients stay to watch the end screen). |

## State machine

```
DISABLED → WAITING → COUNTDOWN → SPAWNING → CLEANUP → INTERMISSION
                                              ↑
                                              └──→ VICTORY (final wave)
```

- **WAITING**: Server is waiting for the first participant or for the
  countdown to be manually started.
- **COUNTDOWN**: "Prepare for invasion: N" announcements, N seconds.
- **SPAWNING**: Monsters are spawned in bursts; budget is consumed.
- **CLEANUP**: Wave is active; monsters are alive; wave-cleared tracking runs.
- **INTERMISSION**: "Next wave in: N" announcements; players can shop /
  prepare.
- **VICTORY**: All waves complete; exit or stay depending on
  `sv_invasionexitonvictory`.

Transitions are driven by `Net_SetInvasionState`. The announcement
deduplication (`InvasionAnnouncementState` / `Wave` / `LastCountdownSecond`)
prevents spammy HUD messages.

## Spawn fallback decision tree

When `Net_RebuildInvasionSpawnSpots` finds no classic invasion spots:

1. If `sv_invasionspotfallback` is false → no spawns (wave may be empty).
2. Try map spots (`INVSPAWN_MAPSPOT`).
3. Fall back to deathmatch starts (`INVSPAWN_DEATHMATCH`).
4. Fall back to player starts (`INVSPAWN_PLAYERSTART`).

The decision and the resulting source are logged at debug level 1 and
appear in `net_stressreport` / `net_invasionstatus`. The per-fallback
diagnostic line (added during the #15 audit) records *why* fallback was
used ("no-classic-spots;used-mapspot", etc.).

## Diagnostics

| CCMD | Output |
| --- | --- |
| `net_stressreport` | One-line compact report + multi-line breakdown including `lod=full/reduced/dormant`, invasion wave/state, actor pressure, etc. |
| `net_invasionstatus` | Human-readable snapshot of current wave, budget, spawned/cleared, active count, boss flag, fallback state. |
| `net_invasion_simlod` | Detailed LOD ranges, current distribution, per-actor LOD state for suspended actors. |
| `net_invasion_bosswave_test` | Prints the boss-wave pattern for the next 10 waves under the current `sv_invasionbosswaveevery`. |
| `net_actorstats` | Actor registry compaction and per-category counts (includes invasion-tracked). |

`sv_invasiondebug >= 1` mirrors important state changes and spawn results
to the console/HUD. `>= 2` adds per-spawn detail (class, spot source,
wave, remaining budget).

## Tuning guidance

- **Wave length vs. player count**: Increase `sv_invasionperplayer` for
  larger lobbies so the wave does not feel empty. Decrease for small
  co-op groups.
- **Spawn pacing**: `sv_invasionspawnburst` controls burst size;
  `sv_invasionspawntime` is a fallback window. Larger bursts + shorter
  interval = more "swarm" feel; smaller bursts = steadier pressure.
- **Simulation LOD**: Enable (`sv_invasionsimlod 1`) on busy maps.
  Tune `sv_invasionsimlodfullrange` / `reducedrange` so that monsters
  the players can actually see stay FULL. Watch the `lod=` triplet in
  `net_stressreport` during a 200+ actor soak.
- **Boss waves**: `sv_invasionbosswaveevery=5` gives a boss every 5th
  wave with `sv_invasionbossbonus` extra budget. Set to 0 to disable.
- **Fallback spam**: If many waves fall back to player starts, either
  add more map spots or lower expectations for that map. The diagnostic
  line tells you exactly which fallback path was taken.

## Recommended soak for a new map

1. `net_stressreport` once per minute; watch `lod=`, `invasion_active`,
   `world_avg_ms`.
2. `net_invasion_simlod` mid-wave to see the distribution.
3. `net_invasionstatus` after each wave to confirm budget scaling and
   boss flags.
4. If `lod=` shows too many DORMANT monsters that players can still see,
   lower `sv_invasionsimlodfullrange`.

When the LOD distribution, spawn pacing, and announcement cadence all
feel right under load, the invasion experience is production-ready.

## Latest Probe Result

On 2026-05-28, `tests/netcode_step12/f6_invasion_latejoin_replay.py` was run
for 25 seconds against local `hcdeserv.exe` / `hcde.exe` with `doom2.wad`.
The probe passed with no crash markers and a bounded pending-event window.
No replay event lines were observed in that short run, so a longer high-actor
soak remains useful for LOD tuning.
