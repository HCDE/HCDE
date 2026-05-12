# HCDE Netcode Stage 23 Server World Deltas

Date: 2026-05-11

Stage 23 is the first server-authored world-state delta slice after dedicated
input authority. No Odamex source was copied in this stage.

## What Changed

Live `HCSN` server snapshots are now version 4 and include an `HCDW` world-delta
section after the existing `HCSR` command records.

The first `HCDW` section carries one server-authored player pose record for each
player represented in the snapshot:

- player slot
- pose flags
- health
- position X/Y/Z
- velocity X/Y/Z
- yaw
- pitch

Clients validate the `HCDW` section separately from command records before
rebuilding the temporary inherited snapshot packet. The section is read-only for
this stage; baseline repair and reconciliation will consume these authoritative
deltas in later stages.

## Safety Rules

- Server snapshots must use `HCSN` version 4.
- The `HCDW` section must use version 1 and zero flags.
- The `HCDW` player set must exactly match the `HCSR` snapshot player set.
- Player records reject duplicate slots, unknown pose flags, non-finite numeric
  values, and records for players not present in the snapshot.
- The inherited command parser still receives the same rebuilt packet as before.

## Current Boundary

Stage 23 creates a server-authored world-state lane without applying corrections
to client simulation yet. This keeps compatibility stable while giving baseline
repair and reconciliation a validated authoritative input stream.

## Remaining Work

- Stage 24 consumes `HCDW` deltas for remote-player baseline repair.
- Add client reconciliation against the validated server pose stream.
- Expand world deltas beyond player pose into actor and map-state deltas.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
