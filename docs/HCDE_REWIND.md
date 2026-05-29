# HCDE Rewind & Lag Compensation

This is the operator-facing summary of the rewind/lag-compensation system in
`src/d_net_rewind.{cpp,h}`. The board item is roadmap **#5** ("DSDA-Doom --
The Rewind System") and it was promoted to **Done** on 2026-05-28: phases 1
through 5 are implemented and wired into the live tic loop, the input stream,
and `P_LineAttack`.

The system is intentionally **default-off**. Capturing a full level
serializer pass once per second has a measurable cost on busy maps, so the
authority only pays it when an operator opts in.

## Tunables (CVARs)

All of these are server-side. On a dedicated server they live in `hcdeserv`'s
config; on a listen host they live in the player's config under
`<config dir>/zdoom-<user>/hcde-<port>.ini`.

| CVAR | Default | Range | Effect |
| --- | --- | --- | --- |
| `net_rewind_enable` | `0` | `0`..`1` | Master switch. `0` disables capture entirely (no memory, no per-tic cost). |
| `net_rewind_interval` | `1.0` | `0.25`..`30` seconds | How often the authority captures a keyframe. Lower = finer rewind grain at higher CPU and memory cost. |
| `net_rewind_depth` | `10` | `1`..`64` | Max keyframes retained in the ring buffer. With the default interval that's 10 seconds of history. |
| `net_rewind_max_mb` | `32` | `1`..`512` MB | Hard memory ceiling. New keyframes evict the oldest if the cap would otherwise be exceeded. |
| `sv_lagcomp` | `0` | `0`..`1` | Enables `HCDELagCompScope` on player hitscan. Requires `net_rewind_enable 1`. |
| `sv_lagcomp_max_age_tics` | `12` | `1`..`70` | Reject view tics older than this. ~12 tics at 35 Hz = ~350 ms. Higher values let high-ping clients rewind further; cap aggressively in competitive play. |

## Diagnostics (CCMDs)

| CCMD | Description |
| --- | --- |
| `net_rewind_dump` | Print the keyframe ring buffer summary, per-client view tic, and lag-comp counters. Safe to run anywhere. |
| `net_rewind_capture_now` | Force an immediate keyframe capture (authority only). Useful when reproducing rewind issues to anchor a clean baseline before the bug. |
| `net_rewind_reset` | Drop all stored keyframes and clear per-client view tics. |
| `net_rewind_test_restore <index>` | Authority-only debug: restore a specific keyframe into the live world. Live clients are NOT resynchronised by this command; treat it as an offline correctness probe. |
| `net_rewind_lagcomp_probe <player>` | Run the historical bracket round-trip for the given player without firing a weapon. Confirms the keyframe near their view tic is restorable and the live world snaps back. |
| `net_lagcomp_summary` | One-line summary of `sv_lagcomp` activity: scopes opened, scopes engaged, damage events captured/replayed/lost, current bracket depth. |

## How it works

1. **Capture (Phase 1)** – `HCDERewind_OnServerTick()` runs once per gametic
   in `d_net.cpp`. When `net_rewind_enable` is set and the configured
   interval has elapsed, it builds a compressed level snapshot (sectors,
   lines, thinkers, actors, players, ACS, RNG) plus a globals blob using the
   same serializer machinery as savegames. Frames are pushed into a ring
   buffer that respects both depth and memory caps.

2. **Restore (Phase 2)** – `HCDERewind_RestoreKeyframe()` re-loads the
   captured level state. This is intentionally authority-only because live
   clients are not yet rebroadcast a "snap to this state" delta after a
   restore; for now restore is a debug primitive plus the live anchor in
   the lag-comp bracket.

3. **Client view tic tracking (Phase 4)** – Whenever a client input record
   is processed in `d_net_snapshot_part2.cpp`,
   `HCDERewind_RecordClientViewTic()` stamps the gametic that client was
   viewing when it built the command. That tic is the bracket target.

4. **Lag-comp bracket (Phase 4 / 5)** – `HCDELagCompScope` is constructed
   around an attacker's hit-resolution code (currently `P_LineAttack`).
   Inside the scope:
   - The live world is captured into a private keyframe.
   - The keyframe nearest at-or-before the attacker's view tic is restored.
   - The hit code runs against historical positions.
   - On scope destruction, the live keyframe is restored and any
     `P_DamageMobj` calls captured during the bracket are replayed against
     live actors looked up by `NetworkEntityManager::GetNetworkEntity` (so
     identity survives the rewind/restore cycle).

   If the live restore ever fails the system logs a loud warning
   ("WORLD STUCK IN PAST"); the failure mode is recoverable on the next
   capture but should never happen in healthy operation.

## Operator runbook

- **Enable for competitive play:** set both `net_rewind_enable 1` and
  `sv_lagcomp 1` on the authority. Verify with `net_lagcomp_summary` after
  a few rounds; "engaged" should track "scopes" closely (a big gap means
  view tics aren't being recorded or are out of `sv_lagcomp_max_age_tics`).

- **Coop / casual servers:** leave both off. The rewind cost is non-trivial
  on busy invasion maps and lag-comp matters less for PvE.

- **Observability:** capture cost (`cost-avg`/`cost-max` from
  `net_rewind_dump`) is the canonical "is rewind affordable" signal. If
  cost-max climbs past a quarter of the tic budget (~7 ms at 35 Hz),
  back off `net_rewind_interval` or `net_rewind_depth`.

- **Memory:** `net_rewind_dump` reports total bytes and percentage of the
  cap. Hitting the cap evicts oldest, which can shorten the effective
  rewind window below `sv_lagcomp_max_age_tics`. If that happens, increase
  `net_rewind_max_mb` or decrease `net_rewind_depth`.

## Known limitations

- **Live clients are not resynced after a manual restore.** Phase 3 (live
  client resync after authority rewind) is intentionally not implemented;
  the system is designed for short hit-resolution brackets, not session-
  level rollback.
- **Bracket cost is paid per attacker per shot.** Hitscan-heavy weapons
  fired in groups will multiply the bracket cost. The `sv_lagcomp_max_age_tics`
  ceiling and the live-keyframe optimisation in `HCDELagCompScope` keep
  this bounded, but it's not free.
- **`HCDELagCompScope` is currently only invoked from `P_LineAttack`.**
  Projectile and rail-style attacks bypass lag-comp today; wrapping them
  is straightforward but unscoped work.

## Source map

| Concern | File / function |
| --- | --- |
| Capture loop | `src/d_net_rewind.cpp` (`HCDERewind_OnServerTick`, `BuildKeyframe`) |
| Tic-loop hook | `src/d_net.cpp` (call site immediately after `MakeConsistencies()`) |
| View-tic recording | `src/d_net_snapshot_part2.cpp` (in the per-client input apply path) |
| Hit-resolution scope | `src/playsim/p_map.cpp::P_LineAttack` (`HCDELagCompScope lagComp(...)`) |
| Damage replay | `src/d_net_rewind.cpp` (`HCDERewind_LagCompRecordDamage`, `HCDELagCompScope::~HCDELagCompScope`) |
