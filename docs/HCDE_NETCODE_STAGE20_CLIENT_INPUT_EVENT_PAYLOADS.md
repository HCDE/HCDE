# HCDE Netcode Stage 20 Client Input Event Payloads

Date: 2026-05-11

Stage 20 is the remaining-plan Stage 8 item: client-to-host input records now
carry HCDE-owned event payload schemas instead of inherited raw event payload
bytes. No Odamex source was copied in this stage.

## What Changed

Live `HLIV` client-command frames still carry the Stage 13 `HGPL` gameplay
envelope, the Stage 14 `HCIN` client-input payload, and the Stage 16 `HCIR`
records section. The `HCIN` payload is now version 5 and the `HCIR` records
section is now version 4.

Each `HCIR` command record still stores:

- command tic offset.
- event record count.
- zero or more event records.
- explicit user command fields from Stage 18.

Each event record still starts with:

- event type.
- event payload byte count.

The event payload behind that count is now schema-owned by HCDE:

- strings are stored as 16-bit byte length plus bytes, without inherited
  null-terminated scanning.
- fixed numeric fields stay in network byte order and are accepted only when
  their command schema expects them.
- server-info changes store explicit cvar type, cvar name, and typed value.
- weapon-slot changes store weapon indices as explicit 16-bit values instead
  of inherited variable-width weapon ids.
- ACS/event argument records validate their argument counts before copying
  typed integer fields.

The host validates the event type, event record length, and event-specific
payload schema before rebuilding the temporary inherited event stream for the
current gameplay parser.

## Safety Rules

- Client inputs must use `HCIN` version 5 and `HCIR` version 4.
- Event types must be explicitly supported by the HCDE client-input event
  schema switch.
- `DEM_USERCMD`, `DEM_EMPTYUSERCMD`, `DEM_BAD`, demo-only stop markers, undone
  enum slots, and unhandled inherited commands are rejected.
- Strings must be length-bounded in the HCDE payload and are rebuilt with a
  null terminator only after validation.
- Weapon indices, cvar names, cvar value types, ZScript command byte counts,
  and script argument counts must fit their event schema.
- Rebuilt inherited packets are still size-checked before gameplay consumes
  them.

## Current Boundary

Stage 20 removes inherited event payload bytes from client-to-host live input
records. The host still rebuilds a temporary inherited event stream after
validation because gameplay command execution still consumes the inherited
parser.

## Remaining Work

- Stage 21 replaced inherited command/event bytes inside `HCSR` command records
  with HCDE event records and explicit user command fields.
- Move peer tic authority to dedicated server authority.
- Add server-authored deltas, baseline repair, and client reconciliation.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
