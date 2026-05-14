# HCDE Netcode Stage 5 Map And Console Assignment

Date: 2026-05-11

Stage 5 moves two more inherited setup side effects into explicit HCDE service
messages for dedicated service joins:

- console-player assignment
- map-load state

No Odamex source was copied in this stage. This keeps the current HCDE
transport and payload serialization, but gives the dedicated join path clearer
server-authored setup messages before later reliable queues and snapshots are
implemented.

## New Service Messages

Stage 5 extends `PRE_HCDE_SERVICE` with:

| Type | Direction | Purpose |
| --- | --- | --- |
| `HPS_CONSOLE_PLAYER` | server to client | assigns the joining client's visible player slot |
| `HPS_MAP_LOAD` | server to client | sends start map, RNG seed, and loadgame state |
| `HPS_MAP_LOAD_ACK` | client to server | confirms map-load state was received |

The legacy `PRE_CONNECT_ACK` still carries its older fields for compatibility,
but service clients now wait for `HPS_CONSOLE_PLAYER` before calling
`Net_SetupUserInfo()` and sending player user info.

## Game Info Split

`Net_SetGameInfo()` and `Net_ReadGameInfo()` still provide the legacy combined
payload for non-service connections.

For HCDE service joins the payload is split:

| Function | Contents |
| --- | --- |
| `Net_SetMapLoadInfo` / `Net_ReadMapLoadInfo` | start map, RNG seed, loadgame argument |
| `Net_SetServerInfo` / `Net_ReadServerInfo` | server CVars |

The service `HPS_GAME_INFO` packet still carries ticdup and game id, but its
payload is now server CVars only. The map-load data is sent and ACKed
separately.

## Verified Smoke

The local stage smoke used:

```powershell
hcdeserv -server 1 -iwad C:\Users\User\Downloads\doom2.wad -port 10682 -noadvertise +map MAP01
hcde -join 127.0.0.1:10682 -dedicatedjoin -netwaitsilent -iwad C:\Users\User\Downloads\doom2.wad -port 10683 -nosound -nomusic -norun
```

Observed client log:

- `HCDE service connect negotiated v1 flags=0x04`
- `Waiting for server assignment`
- `Received HCDE console player 1`
- `Received HCDE map load`
- `Received HCDE server info`
- `Received HCDE service start`

## Remaining Work

- Stage 6 added the service sequence/ACK envelope and replay checks. See
  `docs/HCDE_NETCODE_STAGE6_RELIABLE_SERVICE.md`.
- Add retained reliable service queues that retransmit the same service message
  sequence until the peer ACKs it.
- Split live gameplay into server service messages and client command messages.
- Port or reimplement snapshot/player-state/actor-state replication.
- Move map transitions after startup onto the same server-authored service path.
- Remove the hidden dedicated server slot after gameplay authority no longer
  depends on the inherited peer setup model.
