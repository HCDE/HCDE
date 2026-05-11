# HCDE Netcode Stage 3 Connect Boundary

Date: 2026-05-11

Stage 3 starts replacing the old UZDoom room handshake with an HCDE service
connect boundary. It does not yet port Odamex snapshots, reliable service
message queues, or server-owned actor replication.

## Upstream Reference

Odamex separates connection setup from gameplay replication:

- `server/src/sv_main.cpp` accepts connect packets, validates a token, reads
  user info, sends the console player id, sends map load data, and only then
  moves the client into the live game.
- `common/svc_message.cpp` and `common/svc_message.h` define server-to-client
  service messages such as load-map, console-player, user-info,
  connect-client, mobj updates, and player state.
- `common/i_net.h` keeps client command markers such as user-info, movement,
  ack, and netcmd separate from server service messages.

No Odamex source was copied in this stage. The active HCDE implementation keeps
the current packet transport, then negotiates a small HCDE service-connect
extension so later stages have a stable protocol hook.

## HCDE Service Connect Extension

For launcher/dedicated joins, HCDE appends this extension after the
null-terminated password in the existing `PRE_CONNECT` packet:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | ASCII magic `HCD3` |
| 4 | 1 | service connect version, currently `1` |
| 5 | 1 | client flags |

Client flags:

| Flag | Meaning |
| --- | --- |
| `0x01` | client was launched with `-dedicatedjoin` |
| `0x02` | client suppresses the old room UI with `-netwaitsilent` |
| `0x04` | client expects a server-authoritative flow |

The server replies in `PRE_CONNECT_ACK` with:

| Offset | Size | Field |
| --- | --- | --- |
| 9 | 1 | ack flags |
| 10 | 1 | service connect version, when service flag is set |
| 11 | 1 | server flags, when service flag is set |

Ack flags:

| Flag | Meaning |
| --- | --- |
| `0x01` | hidden dedicated server slot is active |
| `0x02` | HCDE service connect was negotiated |
| `0x04` | server-authoritative flow is active |

## Current Behavior

- Dedicated clients launched with `-dedicatedjoin` or `-netwaitsilent` send the
  extension.
- Dedicated servers accept the extension, log the negotiated version and flags,
  and mark that client connection as an HCDE service connection.
- Dedicated servers still accept legacy setup connects for now, but log a
  warning so Doom Connector argument mistakes are visible.
- Unsupported service-connect versions are rejected before user info or game
  info exchange.
- The internal connection flow is now named server/client auth/sync instead of
  room flow.

## Remaining Work

- Stage 4 added HCDE pregame service message wrappers for dedicated service
  joins. See `docs/HCDE_NETCODE_STAGE4_SERVICE_MESSAGES.md`.
- Move map-load and console-player assignment into server-authored messages.
- Add reliable server-to-client queues and client-to-server command markers.
- Port snapshot/player-state/actor-state replication.
- Remove the old room window from dedicated multiplayer launch paths entirely,
  then keep it only for any legacy peer-host mode that survives.
