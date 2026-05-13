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

- Stage 27 migrated live packet routing behind explicit HCDE authority-route
  helpers.
- Stage 28 moved dedicated-server-owned settings authority state into
  non-player server session state.
- Stage 29 moved HCDE service/live authority lookups behind a separate service
  authority identity boundary.
- Stage 30 moved the packet-size, receive, disconnect, and level-start boundary
  onto that service authority identity.
- Stage 31 moved packet building and tic balancing onto service-authority
  helpers.
- Stage 32 moved local quit and host-migration packet emission onto
  service-authority helpers.
- Stage 33 moved read-only status text, cutscene ready ownership, and ping
  visibility onto service-authority helpers.
- Drop the reserved internal slot once startup, sync, host migration, and live
  replication no longer need a player-backed arbitrator.
