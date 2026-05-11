# HCDE Netcode Stage 15 Server Snapshot Payload

Date: 2026-05-11

Stage 15 is the remaining-plan Stage 3 item: host-to-client gameplay snapshots
now use an HCDE-owned payload shape instead of an anonymous wrapped legacy blob.
No Odamex source was copied in this stage.

## What Changed

Live `HLIV` server-snapshot frames now carry:

- `HGPL` gameplay-envelope magic, version, payload kind, room id, flags, and
  sender tic.
- `HCSN` server-snapshot magic.
- server-snapshot payload version.
- restricted inherited control flags.
- inherited routing byte.
- player count.
- acknowledged command and consistency sequences.
- explicit quitter byte count.
- base command and consistency sequences.
- command tic count and consistency tic count.
- stability buffer.
- explicit snapshot body byte count.
- quitter bytes and Stage 17 `HCSR` records.

The receiver validates the `HGPL` envelope and the `HCSN` payload, then rebuilds
the temporary inherited snapshot packet for the existing tic parser. This keeps
the current gameplay path stable while making the server snapshot protocol
explicit and inspectable.

## Safety Rules

- Server snapshots must use payload kind `server snapshot`.
- Server snapshots must include the `HCSN` magic and supported snapshot version.
- Server snapshots reject exit, setup, level-ready, latency, latency-ack, and
  compressed inherited control flags.
- Server snapshots allow inherited retransmit and quitter flags because the
  inherited parser still owns those semantics.
- Quitter payloads must have both the inherited quitter flag and a byte count
  that matches the embedded quitter-count byte.
- Server snapshots reject too many players, excessive tic counts, invalid
  snapshot byte sizes, and packets that would expand past `MAX_MSGLEN`.
- The rebuilt inherited packet is size-checked with the existing parser before
  gameplay consumes it.

## Current Boundary

Stage 15 removes the anonymous legacy blob from the host-to-client live snapshot
path. Stage 17 stores the snapshot body as explicit `HCSR` records. Stage 21
now replaces inherited command/event bytes inside each command record with HCDE
event records and explicit user command fields.

## Remaining Work

- Stage 16 added explicit `HCIR` records inside `HCIN` client inputs.
- Stage 17 added explicit `HCSR` records inside `HCSN` server snapshots.
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
