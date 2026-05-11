# HCDE Netcode Stage 21 Server Snapshot Command Records

Date: 2026-05-11

Stage 21 is the remaining-plan Stage 9 item: host-to-client server snapshot
command records now carry HCDE-owned event records and explicit user command
fields instead of inherited command/event byte blobs. No Odamex source was
copied in this stage.

## What Changed

Live `HLIV` server-snapshot frames still carry the Stage 13 `HGPL` gameplay
envelope, the Stage 15 `HCSN` server-snapshot payload, and the Stage 17 `HCSR`
records section. The `HCSN` payload is now version 3 and the `HCSR` records
section is now version 2.

Each `HCSR` command record now stores:

- command tic offset.
- event record count.
- zero or more HCDE event records.
- explicit user command fields.

Each event record stores:

- event type.
- event payload byte count.
- HCDE-owned event payload bytes from Stage 20.

The sender decodes the inherited tic command stream, validates and canonicalizes
the queued per-tic events, and stores the movement command as explicit fields.
The client validates the `HCSR` record shape, event types, event payload
schemas, and command fields before rebuilding the temporary inherited snapshot
packet for the current gameplay parser.

## Safety Rules

- Server snapshots must use `HCSN` version 3 and `HCSR` version 2.
- Command offsets must be unique and inside the advertised command tic count.
- Event types must be explicitly supported by the HCDE tic-event schema.
- Event payloads must pass the Stage 20 event-specific schema checks.
- User command fields are fixed-width and must fit the command record.
- Rebuilt inherited packets are still size-checked before gameplay consumes
  them.

## Current Boundary

Stage 21 removes inherited command/event byte blobs from host-to-client server
snapshot command records. The client still rebuilds a temporary inherited
snapshot packet after validation because gameplay command execution still
consumes the inherited parser.

## Remaining Work

- Move peer tic authority to dedicated server authority.
- Add server-authored world-state deltas, baseline repair, and client
  reconciliation.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
