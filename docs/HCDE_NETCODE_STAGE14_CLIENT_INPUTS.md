# HCDE Netcode Stage 14 Client Inputs

Date: 2026-05-11

Stage 14 gives client-to-server movement packets an HCDE-owned input payload.
No Odamex source was copied in this stage.

## What Changed

Live `HLIV` client-command frames now carry:

- `HGPL` gameplay-envelope magic
- gameplay-envelope version, payload kind, room id, flags, and sender tic
- `HCIN` client-input magic
- client-input version
- restricted client control flags
- inherited routing byte
- player count
- acknowledged command and consistency sequences
- base command and consistency sequences
- command tic count and consistency tic count
- stability buffer
- explicit input byte count
- Stage 16 `HCIR` player, consistency, and command records

The host validates the `HGPL` envelope and the `HCIN` payload, then rebuilds
the temporary inherited command packet for the existing tic parser. This keeps
gameplay stable while moving client input ownership into HCDE protocol space.

## Safety Rules

- Client input must use payload kind `client inputs`.
- Client input must include the `HCIN` magic and supported input version.
- Client input rejects exit, setup, level-ready, quitter, latency, latency-ack,
  and compressed legacy control flags.
- Client input allows zero-player heartbeat packets only when they carry no
  input bytes.
- Client input rejects too many players, excessive tic counts, invalid command
  byte sizes, and packets that would expand past `MAX_MSGLEN`.
- The outer gameplay envelope still rejects stale room ids before `HCIN` is
  decoded.

## Current Boundary

Stage 14 removed the anonymous legacy blob from the client-to-host live input
path. Stage 16 now stores those inputs as explicit `HCIR` player, consistency,
and command records inside `HCIN`.

Server-to-client snapshots now use the Stage 15 `HCSN` payload inside the
`HGPL` envelope.

## Remaining Work

- Replace inherited command/event bytes inside `HCIR` command records with
  fully explicit HCDE command fields.
- Replace inherited snapshot command/event bytes inside `HCSN` with explicit
  server-authored state deltas.
- Move peer tic authority to dedicated server authority.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
