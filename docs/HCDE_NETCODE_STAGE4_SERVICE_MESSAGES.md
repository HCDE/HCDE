# HCDE Netcode Stage 4 Pregame Service Messages

Date: 2026-05-11

Stage 4 moves the dedicated HCDE pregame exchange behind explicit service
message types after stage 3 service-connect negotiation succeeds. This keeps
legacy UZDoom host/join packets available for non-service connections while
giving dedicated server joins a cleaner server-authored protocol boundary.

No Odamex source was copied in this stage. The implementation uses HCDE packet
wrappers over the existing transport so later stages can replace the payloads
with server-authored map load, client command, snapshot, and replication data.

## Packet Wrapper

Negotiated service traffic uses the existing `NCMD_SETUP` command with
`PRE_HCDE_SERVICE`.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | `NCMD_SETUP` |
| 1 | 1 | `PRE_HCDE_SERVICE` |
| 2 | 1 | HCDE pregame service type |
| 3 | 4 | session token |
| 7 | variable | service payload |

The session token is still checked on every service packet. A client must first
negotiate the stage 3 `HCD3` service-connect extension before the host accepts
these service messages from that client.

## Service Types

| Type | Direction | Purpose |
| --- | --- | --- |
| `HPS_CLIENT_USER_INFO` | client to server | sends the joining player's user info |
| `HPS_USER_INFO_ACK` | both | acknowledges user info for one client slot |
| `HPS_GAME_INFO` | server to client | sends ticdup, game id, and game info payload |
| `HPS_GAME_INFO_ACK` | client to server | acknowledges game info |
| `HPS_PEER_USER_INFO` | server to client | sends visible peer user info |
| `HPS_HEARTBEAT` | both | keeps the pregame connection alive |
| `HPS_START_GAME` | server to client | starts the joined client |

## Current Behavior

- Dedicated clients launched with `-dedicatedjoin -netwaitsilent` negotiate
  HCDE service connect and then use service messages for user info, game info,
  peer info, heartbeat, and start-game.
- Legacy `PRE_USER_INFO`, `PRE_GAME_INFO`, `PRE_USER_INFO_ACK`,
  `PRE_GAME_INFO_ACK`, `PRE_HEARTBEAT`, and `PRE_GO` are still used for
  unnegotiated connections.
- Dedicated server start can now reach the client through
  `HPS_START_GAME`; smoke logs show `Received HCDE service start`.
- The hidden dedicated server slot remains in place until later stages replace
  the inherited UZDoom slot accounting and game startup assumptions.

## Verified Smoke

The local stage smoke used:

```powershell
hcdeserv -server 1 -iwad C:\Users\User\Downloads\doom2.wad -port 10680 -noadvertise +map MAP01
hcde -join 127.0.0.1:10680 -dedicatedjoin -netwaitsilent -iwad C:\Users\User\Downloads\doom2.wad -port 10681 -nosound -nomusic -norun
```

Observed result:

- Server logged `HCDE service connect v1 flags=0x07`.
- Client logged `HCDE service connect negotiated v1 flags=0x04`.
- Client logged `Received HCDE service start`.

## Remaining Work

- Stage 5 added explicit server-authored map-load and console-player service
  messages. See `docs/HCDE_NETCODE_STAGE5_MAP_ASSIGNMENT.md`.
- Split live gameplay traffic into server service messages and client command
  messages instead of relying on the old tic sync path.
- Add reliable queues, sequencing, and replay protection for service messages.
- Port or reimplement snapshot/player-state/actor-state replication.
- Remove the hidden server slot once server authority no longer depends on the
  old peer setup model.
