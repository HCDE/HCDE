# HCDE Netcode Stage 11 Client Commands

Date: 2026-05-11

Stage 11 moves negotiated client-to-host gameplay command packets onto the
HCDE `HLIV` live channel. No Odamex source was copied in this stage.

## What Changed

When a client negotiated the HCDE service-connect extension, command packets
sent from that client to the host are wrapped in a live `client commands`
frame:

- `HLIV` protocol header
- live transmit sequence
- live peer acknowledgement
- complete legacy command-packet payload

The dedicated host unwraps the payload, validates that it is a gameplay command
packet, and then passes it through the existing tic-command parser. This keeps
the gameplay parser stable while moving the wire boundary toward the dedicated
server model.

## Safety Rules

- Only negotiated HCDE service peers can use the live command lane.
- Only the host accepts `client commands` frames.
- Nested `HLIV` packets are rejected.
- Pregame/control packets such as setup, latency, level-ready, exit, and
  compressed transport frames are rejected inside the live command payload.
- Malformed payload sizes are consumed and logged instead of falling through
  into normal command parsing.

## Current Boundary

Stage 11 changes the client-to-host transport. It does not yet replace the
inherited tic parser, server-authored snapshots, or the hidden dedicated server
player slot.

## Remaining Work

- Stage 12 moves host-to-client gameplay traffic onto `server snapshot` frames.
- Replace inherited peer tic authority with server-owned gameplay authority.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
