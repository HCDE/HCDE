# HCDE Netcode Stage 16 Client Input Records

Date: 2026-05-11

Stage 16 is the remaining-plan Stage 4 item: client-to-host input payloads now
carry explicit HCDE input records inside `HCIN`. No Odamex source was copied in
this stage.

## What Changed

Live `HLIV` client-command frames still carry the Stage 13 `HGPL` gameplay
envelope and Stage 14 `HCIN` client-input payload. The `HCIN` payload is now
version 2 and its body contains an `HCIR` records section:

- `HCIR` records magic.
- records version.
- player record count.
- per-player record header with player id, consistency count, and command
  count.
- explicit consistency records, each with offset and consistency value.
- explicit command records, each with offset, byte length, and command/event
  bytes. Stage 18 replaces the command half of this record with explicit
  command fields.

The host validates the `HCIR` records, reconstructs the temporary inherited
tic-command packet, and then hands that packet to the existing parser. This
keeps gameplay stable while removing the anonymous input byte stream from the
wire format.

## Safety Rules

- Client inputs must use `HCIN` version 2 and include the `HCIR` body.
- The `HCIR` player count must match the `HCIN` player count.
- Player ids must be valid and unique in the payload.
- Per-player consistency and command counts must match the packet-level tic
  counts.
- Consistency and command offsets must be in range and unique per player.
- Command records must declare a non-empty byte length before command/event
  bytes.
- Reconstructed packets are still size-checked with the inherited parser before
  gameplay consumes them.

## Current Boundary

Stage 16 makes client input records explicit on the wire. Stage 18 now stores
the user command itself as explicit fields, but queued per-tic net events remain
as a bounded event byte stream until those event commands get their own schema.

## Remaining Work

- Stage 18 replaced inherited user-command bytes inside `HCIR` command records
  with explicit HCDE command fields.
- Stage 19 replaced inherited event bytes inside `HCIR` command records with
  explicit HCDE event records.
- Stage 20 replaced inherited payload bytes inside `HCIR` event records with
  explicit HCDE event payload schemas.
- Stage 17 added explicit `HCSR` records inside `HCSN` server snapshots.
- Stage 21 replaced inherited command/event bytes inside `HCSR` command records
  with HCDE event records and explicit user command fields.
- Move peer tic authority to dedicated server authority.
- Add server-authored deltas, baseline repair, and client reconciliation.
