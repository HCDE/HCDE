# HCDE Netcode Stage 26 Slotless Dedicated Boundary

Date: 2026-05-11

Stage 26 starts removing the hidden dedicated server player slot from the
public netcode boundary. No Odamex source was copied in this stage.

## What Changed

Dedicated server sessions now use explicit helper APIs for the remaining
reserved internal server slot:

- `I_IsServerReservedSlot`
- `I_GetFirstPlayableClientSlot`
- `I_GetVisibleMaxClients`
- `I_ToVisibleClientSlot`
- `I_ToInternalClientSlot`

Visible player counts, server-query snapshots, pregame progress, dedicated
status logging, and local prediction gating now flow through these helpers
instead of open-coded `MaxClients - 1` or direct slot-zero checks.

## Safety Rules

- The dedicated server's internal arbitrator slot remains reserved and is not
  marked as a playable `playeringame` slot.
- Public player counts exclude the reserved server slot.
- Query-player lists start at the first playable client slot.
- Prediction still exits immediately when running in the reserved server slot.
- Network packet routing still uses internal slots until the live replication
  loop no longer depends on the inherited arbitrator index.

## Current Boundary

Stage 26 removes the reserved server slot from visible/accounting paths and
centralizes the remaining internal mapping behind named APIs. This makes the
next migration smaller: call sites can move from internal slots to visible
client IDs without re-deriving the dedicated-server offset each time.

## Remaining Work

- Migrate live packet routing away from the inherited `Net_Arbitrator` player
  index.
- Replace server-owned `players[Net_Arbitrator]` state with non-player server
  session state.
- Drop the reserved internal slot once startup, sync, host migration, and live
  replication no longer need a player-backed arbitrator.
