# HCDE Netcode Stage 10 Live Channel

Date: 2026-05-11

Stage 10 adds a versioned HCDE live-game packet lane for negotiated HCDE
service peers. No Odamex source was copied in this stage.

## What Changed

Live HCDE packets now ride inside the normal game UDP path with a distinct
`HLIV` frame header instead of using `NCMD_SETUP` after pregame.

The frame currently carries:

- protocol version
- message type
- transmit sequence
- peer acknowledgement
- payload bytes

The initial message types are:

- `control`
- `client commands`
- `server snapshot`

Only `control` is sent in this stage. It carries diagnostic timing and slot
state once per second so both sides can prove the live lane is active without
changing gameplay state yet.

## Safety Rules

- HCDE live frames are accepted only from peers that negotiated the HCDE
  service-connect extension.
- Unsupported protocol versions are consumed and logged instead of falling
  through into legacy tic parsing.
- Duplicate or replayed sequence numbers are consumed and ignored.
- Unknown message types are consumed and logged.

## Current Boundary

Stage 10 creates the live transport boundary and diagnostics. It does not yet
apply client commands or server snapshots to gameplay.

## Remaining Work

- Stage 11 moves client command traffic onto `client commands` frames.
- Move server-authored state onto `server snapshot` frames.
- Replace inherited peer tic authority with server-owned gameplay authority.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
