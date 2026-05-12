# HCDE Netcode Stage 24 Baseline Repair

Date: 2026-05-11

Stage 24 consumes the Stage 23 `HCDW` player pose stream for baseline-repair
telemetry.
No Odamex source was copied in this stage.

## What Changed

Clients now use validated server-authored `HCDW` player pose records to detect
remote player baseline drift from the server snapshot.

For non-local players, the client validates the server-authored:

- position
- velocity
- yaw
- pitch
- health
- on-ground state

The local predicted player is not corrected in this stage. Remote correction is
also deferred while the inherited deterministic lockstep consistency checker
still owns playsim state. Drift is counted and traced so later stages can apply
the same authoritative pose stream after live replication owns the game loop.

## Safety Rules

- Baseline-repair telemetry only runs after the `HCDW` section has passed Stage
  23 validation.
- The local predicted player is not moved by this stage.
- Remote players are not moved by this stage while inherited lockstep
  consistency is active.
- Remote player drift detection is gated by a distance threshold, or by
  mismatched health/on-ground state.
- The guarded repair path remains in code for the live-replication migration,
  but it does not mutate playsim state in this boundary stage.

## Current Boundary

Stage 24 gives clients a validated server baseline for visible remote player
state. It does not yet mutate client playsim state, rewind/replay local
prediction, smooth local corrections, or expand repair beyond the player pose
records carried by `HCDW`.

## Remaining Work

- Stage 25 adds client reconciliation against local-player `HCDW` drift.
- Expand world deltas beyond player pose into actor and map-state deltas.
- Continue the Stage 26 slotless dedicated-server migration once live
  replication owns the game loop.
