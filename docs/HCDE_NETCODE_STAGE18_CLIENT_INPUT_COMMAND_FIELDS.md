# HCDE Netcode Stage 18 Client Input Command Fields

Date: 2026-05-11

Stage 18 is the remaining-plan Stage 6 item: client-to-host input records now
carry explicit user command fields instead of inherited user-command byte
encoding. No Odamex source was copied in this stage.

## What Changed

Live `HLIV` client-command frames still carry the Stage 13 `HGPL` gameplay
envelope, the Stage 14 `HCIN` client-input payload, and the Stage 16 `HCIR`
records section. The `HCIN` payload is now version 3 and the `HCIR` records
section is now version 2.

Each `HCIR` command record now stores:

- command tic offset.
- queued event byte count and bytes. Stage 19 replaces this with explicit
  event records.
- `buttons`.
- `pitch`.
- `yaw`.
- `roll`.
- `forwardmove`.
- `sidemove`.
- `upmove`.

The sender decodes the inherited local user-command packet into explicit fields
before placing it on the wire. The host validates the `HCIR` record shape,
rebuilds a temporary inherited command packet from those fields, and then hands
that packet to the current tic parser.

## Safety Rules

- Client inputs must use `HCIN` version 3 and `HCIR` version 2.
- Command offsets must be in range and unique per player.
- Event bytes must be length-bounded and fit inside the `HCIR` body.
- User command fields have fixed sizes and cannot trail arbitrary command bytes.
- Rebuilt inherited packets are still size-checked before gameplay consumes
  them.

## Current Boundary

Stage 18 removes inherited user-command byte encoding from client-to-host live
input records. Stage 19 stores queued per-tic net events as explicit event
records, and Stage 20 replaces each event's inherited payload bytes with an
individual HCDE schema.

## Remaining Work

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
