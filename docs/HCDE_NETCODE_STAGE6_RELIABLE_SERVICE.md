# HCDE Netcode Stage 6 Service Sequence Envelope

Date: 2026-05-11

Stage 6 adds an ordered sequence/ACK envelope around negotiated HCDE pregame
service messages. This is still built on HCDE's current transport and does not
copy Odamex source.

## Service Header

`PRE_HCDE_SERVICE` packets now use this header:

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 1 | `NCMD_SETUP` |
| 1 | 1 | `PRE_HCDE_SERVICE` |
| 2 | 1 | `EHCDEPregameService` |
| 3 | 4 | session token |
| 7 | 4 | sender service sequence |
| 11 | 4 | latest received peer sequence ACK |
| 15 | variable | service payload |

The payload offset is now 15 bytes for every HCDE service message.

## Per-Peer State

Each `FConnection` tracks:

| Field | Purpose |
| --- | --- |
| `HCDEServiceTxSeq` | next local service sequence number |
| `HCDEServiceRxSeq` | highest peer service sequence accepted |
| `HCDEServicePeerAck` | highest local sequence acknowledged by the peer |
| `HCDEServiceDuplicateCount` | duplicate/replay packets rejected for diagnostics |

Incoming service packets are rejected when the session token is wrong, the
sequence is zero, or the sequence is older than or equal to the highest accepted
peer sequence. ACKs beyond the local sent range are logged but do not abort the
connection.

## Current Boundary

Stage 6 gives the dedicated join path ordered service packets and replay
protection. Critical pregame setup data is still resent by the existing
state-driven loops until the matching setup state or explicit ACK is observed.

This means a resend currently creates a new service sequence number. A later
reliable queue stage still needs to retain individual service messages and
retransmit the same sequence number until its ACK arrives.

Stage 7 adds that retained service queue. See
`docs/HCDE_NETCODE_STAGE7_RELIABLE_QUEUE.md`.

## Verified Smoke

The local stage smoke used:

```powershell
hcdeserv -server 1 -iwad C:\Users\User\Downloads\doom2.wad -port 10684 +sv_usemasters 0 +map MAP01
hcde -join 127.0.0.1:10684 -dedicatedjoin -netwaitsilent -iwad C:\Users\User\Downloads\doom2.wad -port 10685 -nosound -nomusic -norun
```

Observed client log:

- `HCDE service connect negotiated v1 flags=0x04`
- `Waiting for server assignment`
- `Sending player information`
- `Received HCDE console player 1`
- `Received HCDE map load`
- `Received HCDE server info`
- `Received HCDE service start`

## Remaining Work

- Add a final start-game ACK or enter-game confirmation before the dedicated
  server fully leaves pregame service mode.
- Add per-message timeout and disconnect behavior for required setup messages.
- Split live gameplay into server service messages and client command messages.
- Port or reimplement snapshot/player-state/actor-state replication.
- Move map transitions after startup onto the same server-authored service path.
