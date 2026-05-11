# HCDE Netcode Stage 19 Client Input Event Records

Date: 2026-05-11

Stage 19 is the remaining-plan Stage 7 item: client-to-host input records now
carry explicit event records instead of a single inherited event byte stream.
No Odamex source was copied in this stage.

## What Changed

Live `HLIV` client-command frames still carry the Stage 13 `HGPL` gameplay
envelope, the Stage 14 `HCIN` client-input payload, and the Stage 16 `HCIR`
records section. The `HCIN` payload is now version 4 and the `HCIR` records
section is now version 3.

Each `HCIR` command record now stores:

- command tic offset.
- event record count.
- zero or more event records.
- explicit user command fields from Stage 18.

Each event record stores:

- event type.
- event payload byte count.
- event payload bytes.

The sender splits the old queued event stream into typed, length-bounded event
records before placing it on the wire. The host validates event types and
record boundaries, rebuilds the temporary inherited event stream, appends the
explicit user command, and then hands the temporary packet to the current tic
parser.

## Safety Rules

- Client inputs must use `HCIN` version 4 and `HCIR` version 3.
- Event types must be known inherited demo/network event types.
- Event records may not contain `DEM_USERCMD`, `DEM_EMPTYUSERCMD`, or
  `DEM_BAD`.
- Event payload lengths must fit inside the `HCIR` body and leave room for the
  explicit user command fields.
- Rebuilt inherited packets are still size-checked before gameplay consumes
  them.

## Current Boundary

Stage 19 removed the anonymous event byte stream from client-to-host live input
records. Stage 20 now replaces the inherited payload bytes inside those event
records with HCDE-owned event payload schemas.

## Remaining Work

- Stage 20 replaced inherited payload bytes inside `HCIR` event records with
  explicit HCDE event payload schemas.
- Stage 21 replaced inherited command/event bytes inside `HCSR` command records
  with HCDE event records and explicit user command fields.
- Move peer tic authority to dedicated server authority.
- Add server-authored deltas, baseline repair, and client reconciliation.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
