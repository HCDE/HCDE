# HCDE Master Protocol

HCDE uses a separate master server process for public discovery. The master
server does not link against HCDE engine code; it only speaks UDP with the
dedicated server and launcher/browser clients.

## Boundary Rules

This file is the approved shared contract for HCDE public-server discovery.
Doom Connector may implement these packets in its own code, and the standalone
master server may implement the matching server side, but neither program should
copy HCDE engine implementation files or link against HCDE engine libraries.

Allowed shared material is limited to protocol facts: packet marker values,
field layouts, byte order, ports, time-to-live rules, and neutral generated
constants or schemas. Socket loops, storage logic, launcher behavior, engine
headers, and gameplay state remain implementation details of their own program.

The neutral machine-readable protocol file is
`protocol/hcde_master_protocol.json`. The protocol-only C++ constants in
`protocol/hcde_master_protocol.h` mirror that JSON data and contain no socket,
storage, launcher, engine, or gameplay implementation.

## UDP Port

Default master port: `15000`.

## Protocol Generations

HCDE keeps the legacy 6-byte heartbeat documented below for compatibility
while the dedicated server uses the NMS1 advertiser protocol for current master
server registration, heartbeat refreshes, and unregisters.

## Dedicated Server Startup

Dedicated servers advertise only when `sv_usemasters` is enabled. Use
`-master host[:port]` for startup master addresses:

```powershell
hcdeserv -server 1 -port 10666 -iwad doom2.wad +set sv_usemasters 1 -master hcde.servebeer.com +map MAP01
```

`-master` may be repeated. If no startup or archived masters are configured,
HCDE falls back to the built-in default master list. The `addmaster` console
command is still available at runtime and in archived configs, but launchers
should prefer `-master` because dedicated-server startup does not run delayed
console commands.

## Legacy Server Heartbeat

`hcdeserv` sends this packet to each configured master when `sv_usemasters` is
enabled:

| Offset | Size | Endian | Field |
|--------|------|--------|-------|
| 0 | 4 | little | heartbeat marker `5560020` |
| 4 | 2 | little | public game server UDP port |

The master records the sender IP plus the supplied game port. Entries expire
after the master's configured TTL.

## Legacy Launcher List Query

A browser client sends:

| Offset | Size | Endian | Field |
|--------|------|--------|-------|
| 0 | 4 | little | list query marker `777123` |

The master replies:

| Offset | Size | Endian | Field |
|--------|------|--------|-------|
| 0 | 4 | little | response marker `777123` |
| 4 | 2 | little | server count |
| 6 | 6 * count | mixed | repeated IPv4 bytes followed by little-endian port |

The per-server launcher query is a different protocol and uses network byte
order. Keep these two wire formats separate unless both sides are versioned.

## NMS1 Advertiser Packets

NMS1 packets use a fixed 16-byte header followed by TLV fields. Multi-byte
integer values are big-endian.

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | ASCII magic `NMS1` |
| 4 | 1 | protocol version `1` |
| 5 | 1 | message type |
| 6 | 2 | flags, currently `0` |
| 8 | 4 | request id |
| 12 | 2 | payload length |
| 14 | 2 | reserved, currently `0` |

Dedicated-server advertising uses protocol family `raw` unless a future HCDE
wire family is defined.

### Message Types

| Value | Name | Direction |
|-------|------|-----------|
| 1 | `ChallengeRequest` | server to master |
| 2 | `ChallengeResponse` | master to server |
| 3 | `Register` | server to master |
| 4 | `RegisterAck` | master to server |
| 5 | `Heartbeat` | server to master |
| 6 | `HeartbeatAck` | master to server |
| 7 | `Unregister` | server to master |
| 8 | `UnregisterAck` | master to server |
| 9 | `ListRequest` | browser to master |
| 10 | `ListResponse` | master to browser |
| 11 | `Error` | master to caller |

### Server Advertiser Fields

| Field id | Name | Type | Used by |
|----------|------|------|---------|
| 1 | `purpose` | u8 | `ChallengeRequest` |
| 2 | `challenge_issued_unix` | u32 | `Register` |
| 3 | `challenge_token` | 32 bytes | `Register` |
| 16 | `protocol_family` | UTF-8 text, max 32 bytes | `Register`, `Heartbeat`, `Unregister` |
| 17 | `game_port` | u16 | `Register`, `Heartbeat`, `Unregister` |
| 18 | `query_port` | u16 | `Register` |
| 19 | `entry_token` | 32 bytes | `Heartbeat`, `Unregister` |
| 20 | `current_players` | u16 | `Register`, `Heartbeat` |
| 21 | `max_players` | u16 | `Register`, `Heartbeat` |
| 22 | `server_flags` | u32 | `Register`, `Heartbeat` |
| 24 | `build_label` | UTF-8 text, max 64 bytes | `Register` |
| 25 | `display_name` | UTF-8 text, max 96 bytes | `Register` |

Registration starts with `ChallengeRequest` using purpose `1`
(`Registration`). The master replies with a challenge token. `Register` sends
the challenge fields plus the server endpoint data. The master replies with an
entry token. Later `Heartbeat` and `Unregister` packets identify the endpoint
with `protocol_family`, `game_port`, and that entry token.

`HeartbeatAck` returns `ttl_seconds`. If `Heartbeat` receives `stale_entry` or
`token_invalid`, the advertiser must discard the entry token and repeat
challenge/register.

`Unregister` should be sent before shutdown, before removing a configured
master, and before re-registering under a different advertised game port. A
successful `UnregisterAck` has an empty payload.

The `masters` console command reports each configured master plus current NMS1
advertiser state. Registered entries show the advertised port, estimated TTL
remaining, and seconds since the last successful register/heartbeat refresh;
pending entries show the last NMS1 error or timeout when one is known.
