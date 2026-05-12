# HCDE Netcode Stage 25 Client Reconciliation

Date: 2026-05-11

Stage 25 consumes the Stage 23 `HCDW` player pose stream for local client
reconciliation telemetry. No Odamex source was copied in this stage.

## What Changed

Clients now validate the local predicted player against server-authored `HCDW`
pose records and report reconciliation drift.

When the local player diverges from the server pose, the deferred correction
path records the server-authored:

- position
- velocity
- yaw
- pitch
- health
- on-ground state

The mutation path remains present but guarded off in this stage, because
applying side-band pose changes before the old lockstep checker is retired can
itself create an out-of-sync report.

## Safety Rules

- Reconciliation telemetry only runs after the `HCDW` section has passed Stage
  23 validation.
- Remote-player baseline telemetry remains handled by Stage 24.
- Local reconciliation telemetry is gated by drift or state mismatch.
- Severe local drift is detected for the future hard correction path.
- The inherited command parser still receives the same rebuilt snapshot packet
  after reconciliation telemetry is processed.

## Current Boundary

Stage 25 gives clients a first authoritative reconciliation boundary for the
local predicted player. It does not yet mutate playsim state, replay pending
local inputs after correction, nor expand reconciliation beyond the player pose
records carried by `HCDW`.

## Remaining Work

- Replay pending local inputs after server correction.
- Expand world deltas beyond player pose into actor and map-state deltas.
- Continue the Stage 26 slotless dedicated-server migration by moving live
  packet routing and server-owned state off the inherited arbitrator player.
