# HCDE Netcode Stage 12 Server Snapshots

Date: 2026-05-11

Stage 12 moves negotiated host-to-client gameplay packets onto the HCDE `HLIV`
live channel as `server snapshot` frames. No Odamex source was copied in this
stage.

## What Changed

When the dedicated host sends gameplay traffic to a client that negotiated the
HCDE service-connect extension, the packet is wrapped in a live `server
snapshot` frame:

- `HLIV` protocol header
- live transmit sequence
- live peer acknowledgement
- complete legacy host gameplay payload

The join client unwraps the payload, validates that it is a gameplay packet,
and then passes it through the existing tic-command parser. Stage 12 completes
the first live-channel transport pass in both directions without changing the
gameplay authority model yet.

## Safety Rules

- Only negotiated HCDE service peers can use the live snapshot lane.
- Only clients accept `server snapshot` frames, and only from the host slot.
- Nested `HLIV` packets are rejected.
- Pregame/control packets such as setup, latency, level-ready, exit, and
  compressed transport frames are rejected inside the live snapshot payload.
- Legacy retransmit and quitter flags remain allowed because the inherited
  gameplay parser still owns those semantics in this stage.
- Malformed payload sizes are consumed and logged instead of falling through
  into normal command parsing.

## Current Boundary

Stage 12 changes the host-to-client transport. It does not yet replace the
inherited tic parser, create true world-state deltas, or remove the hidden
dedicated server player slot.

## Remaining Work

- Stage 13 added an HCDE-owned gameplay envelope inside live command and
  snapshot frames.
- Stage 15 added an explicit `HCSN` payload for live server snapshots.
- Replace inherited peer tic authority with server-owned gameplay authority.
- Convert inherited snapshot command/event bytes to explicit server-authored
  state deltas.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
