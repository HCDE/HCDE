# HCDE Netcode Stage 7 Retained Service Queue

Date: 2026-05-11

Stage 7 turns the HCDE pregame service sequence envelope into a retained
reliable queue. No Odamex source was copied in this stage.

## What Changed

Each negotiated HCDE connection now keeps up to 16 retained pregame service
messages. A retained message stores:

| Field | Purpose |
| --- | --- |
| service type | the `EHCDEPregameService` message kind |
| key | a small payload discriminator, such as player slot |
| sequence | the original service sequence number |
| packet bytes | the full packet payload to retransmit |
| send count | retransmit diagnostics |
| last send time | resend throttle timing |

The sender retransmits the oldest unacknowledged retained service message using
the same sequence number. The ACK field is refreshed before each resend so the
packet still carries the newest peer sequence ACK.

## ACK Behavior

The peer ACK in the service header is cumulative. When a connection receives an
ACK for sequence `N`, HCDE clears every retained local service with sequence
`<= N`.

Duplicate service messages are still rejected by the receiver, but retained ACK
messages are now resent from the queue. This avoids the deadlock where a lost
ACK made the sender repeat a packet that the receiver had already processed.

## Ordered Sending

Only the oldest retained service message is flushed at a time. Heartbeats do not
advance the service sequence while retained messages are pending, so a heartbeat
cannot jump ahead and make the peer reject an older retransmission.

## Reliable Services

Stage 7 retains these pregame service messages:

| Direction | Service |
| --- | --- |
| server to client | `HPS_CONSOLE_PLAYER` |
| server to client | `HPS_USER_INFO_ACK` |
| server to client | `HPS_MAP_LOAD` |
| server to client | `HPS_GAME_INFO` |
| server to client | `HPS_PEER_USER_INFO` |
| server to client | `HPS_START_GAME` |
| client to server | `HPS_CLIENT_USER_INFO` |
| client to server | `HPS_USER_INFO_ACK` |
| client to server | `HPS_MAP_LOAD_ACK` |
| client to server | `HPS_GAME_INFO_ACK` |

## Current Boundary

This stage makes pregame setup messages retained and retransmitted, but it does
not yet add a final start-game acknowledgement or live gameplay state
replication. `HPS_START_GAME` is retained at send time, but the host still
transitions into the game immediately after broadcasting it.

Stage 8 adds the final start acknowledgement and Stage 9 adds setup timeout
handling. See `docs/HCDE_NETCODE_STAGE8_START_ACK.md` and
`docs/HCDE_NETCODE_STAGE9_SETUP_TIMEOUTS.md`.

## Remaining Work

- Split live gameplay into server service messages and client command messages.
- Port or reimplement snapshot/player-state/actor-state replication.
- Move map transitions after startup onto the same server-authored service path.
