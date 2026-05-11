# HCDE Netcode Stage 17 Server Snapshot Records

Date: 2026-05-11

Stage 17 is the remaining-plan Stage 5 item: host-to-client server snapshots now
carry explicit HCDE snapshot records inside `HCSN`. No Odamex source was copied
in this stage.

## What Changed

Live `HLIV` server-snapshot frames still carry the Stage 13 `HGPL` gameplay
envelope and Stage 15 `HCSN` server-snapshot payload. The `HCSN` payload is now
version 2 and its body contains an `HCSR` records section:

- `HCSR` records magic.
- records version.
- player record count.
- per-player record header with player id, latency, consistency count, and
  command count.
- explicit consistency records, each with offset and consistency value.
- explicit command records, each with offset, byte length, and command/event
  bytes.

The client validates the `HCSR` records, reconstructs the temporary inherited
snapshot packet, and then hands that packet to the existing parser. This keeps
gameplay stable while removing the anonymous snapshot byte stream from the wire
format.

## Safety Rules

- Server snapshots must use `HCSN` version 2 and include the `HCSR` body.
- The `HCSR` player count must match the `HCSN` player count.
- Player ids must be valid and unique in the payload.
- Per-player consistency and command counts must match the packet-level tic
  counts.
- Consistency and command offsets must be in range and unique per player.
- Command records must declare a non-empty byte length before command/event
  bytes.
- Quitter payloads must still match the inherited quitter flag and embedded
  quitter-count byte.
- Reconstructed packets are still size-checked with the inherited parser before
  gameplay consumes them.

## Current Boundary

Stage 17 made server snapshot records explicit on the wire. Stage 21 now
replaces the inherited command/event bytes inside each `HCSR` command record
with HCDE event records and explicit user command fields.

## Remaining Work

- Stage 18 replaced inherited user-command bytes inside `HCIR` command records
  with explicit HCDE command fields.
- Stage 19 replaced inherited event bytes inside `HCIR` command records with
  explicit HCDE event records.
- Stage 20 replaced inherited payload bytes inside `HCIR` event records with
  explicit HCDE event payload schemas.
- Stage 21 replaced inherited command/event bytes inside `HCSR` command records
  with HCDE event records and explicit user command fields.
- Move peer tic authority to dedicated server authority.
- Add server-authored deltas, baseline repair, and client reconciliation.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
