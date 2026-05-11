# HCDE Netcode Stage 13 Gameplay Envelope

Date: 2026-05-11

Stage 13 adds an HCDE-owned gameplay envelope inside live `HLIV` command and
snapshot frames. No Odamex source was copied in this stage.

## What Changed

Live `client commands` and `server snapshot` frames now carry this inner
gameplay envelope before their temporary legacy payload:

- `HGPL` gameplay-envelope magic
- gameplay-envelope protocol version
- payload kind
- room id
- reserved flags
- sender game tic
- legacy gameplay payload bytes

The payload kinds now are:

- `reserved legacy client commands`
- `reserved legacy server snapshot`
- `client inputs`
- `server snapshot`

After validation, HCDE strips the envelope and passes only the legacy gameplay
payload into the inherited tic-command parser. This makes the live channel
schema explicit while keeping gameplay behavior stable for the next authority
stages.

## Safety Rules

- Live gameplay payloads without the `HGPL` envelope are rejected.
- Unsupported gameplay-envelope versions, unknown payload kinds, and non-zero
  envelope flags are rejected.
- Stale room ids are rejected before the legacy parser sees the payload.
- Client-command envelopes reject `NCMD_QUITTERS`; clients still leave through
  `NCMD_EXIT`, not by asking the host to disconnect arbitrary slots.
- Server-snapshot envelopes still allow legacy retransmit and quitter flags
  because the inherited parser owns those semantics until explicit deltas land.

## Current Boundary

Stage 13 creates an HCDE-owned gameplay envelope and validates direction,
payload kind, room id, and size. It does not yet replace the legacy payload with
server-authored world-state deltas.

## Remaining Work

- Stage 14 added an explicit HCDE client-input payload inside the gameplay
  envelope for client-to-host movement.
- Stage 15 added an explicit HCDE server-snapshot payload inside the gameplay
  envelope for host-to-client snapshots.
- Replace inherited peer tic authority with server-owned gameplay authority.
- Convert inherited command/event bytes inside gameplay payloads to explicit
  server-authored state deltas.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
