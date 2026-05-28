# Launcher Protocol

This section is the implementation-level launcher/browser contract for HCDE.
It documents how dedicated launch, master-list discovery, and per-server query
work so third-party tools can interoperate without reverse engineering.

Source of truth:

- `docs/HCDE_MASTER_PROTOCOL.md`
- `protocol/hcde_master_protocol.json`
- `protocol/hcde_master_protocol.h`

### 1) Roles and Process Model

HCDE dedicated multiplayer is a two-process model:

1. `hcdeserv` runs authority/gameplay state.
2. `hcde` joins that server as a normal client (`-dedicatedjoin` path).

Launchers should treat these as separate lifecycle units:

- authority process lifecycle: create, monitor, advertise, shutdown
- local client lifecycle: join, disconnect, relaunch/retry

### 2) Dedicated Launch Arguments

Required server arguments:

| Argument | Purpose |
|---|---|
| `-server <slots>` | Dedicated mode with visible player slots |
| `-iwad <path>` | Base IWAD |
| `+map <mapname>` | Initial map |

Common optional server arguments:

| Argument | Purpose |
|---|---|
| `-port <udpPort>` | Game UDP port (default `5029` unless overridden) |
| `-file <mods...>` | Load PWAD/PK3 package list |
| `-deh <patch.deh>` | Load DeHackEd patch |
| `-bex <patch.bex>` | Load BEX patch |
| `-password <text>` | Join password |
| `-advertise` | Force master advertising on |
| `-noadvertise` | Force master advertising off |
| `-master <host[:port]>` | Add startup master endpoint (repeatable) |

Join/client arguments:

| Argument | Purpose |
|---|---|
| `-join <host:port>` | Join target server |
| `-dedicatedjoin` | Use dedicated join behavior and slot mapping |
| `-netwaitsilent` | Suppress legacy pregame wait UI |

Canonical examples:

```powershell
# Private dedicated
hcdeserv -server 8 -iwad C:\Games\doom2.wad -port 10666 +map MAP01

# Public dedicated
hcdeserv -server 8 -iwad C:\Games\doom2.wad -port 10666 -advertise -master hcde.servebeer.com:15000 +map MAP01

# Client join
hcde -join 127.0.0.1:10666 -dedicatedjoin -iwad C:\Games\doom2.wad -file C:\Mods\example.pk3
```

Runtime master control console commands:

- `advertise [host[:port] ...]`
- `unadvertise`
- `addmaster <host[:port]>`
- `delmaster <host[:port]>`
- `masters`

### 3) Transport, Byte Order, and Separation Rules

HCDE launcher-facing networking uses UDP everywhere, but it is split into
distinct protocols that must not be mixed:

| Path | Purpose | Byte order |
|---|---|---|
| Legacy master discovery | old list/heartbeat compatibility | little-endian |
| NMS1 master protocol | current advertise + paged list flow | big-endian for multi-byte ints |
| Per-server launcher query | live server status/details | big-endian for multi-byte ints |

Boundary rule: share packet facts/constants only. Do not copy socket/event-loop
or gameplay implementation across projects.

### 4) Master Discovery: Legacy Compatibility Packets

Legacy server heartbeat (`hcdeserv -> master`), 6 bytes:

| Offset | Size | Endian | Field |
|---|---:|---|---|
| 0 | 4 | LE | heartbeat marker `5560020` |
| 4 | 2 | LE | advertised game port |

Legacy launcher list query (`browser -> master`), 4 bytes:

| Offset | Size | Endian | Field |
|---|---:|---|---|
| 0 | 4 | LE | list marker `777123` |

Legacy list response (`master -> browser`):

| Offset | Size | Endian | Field |
|---|---:|---|---|
| 0 | 4 | LE | response marker `777123` |
| 4 | 2 | LE | server count |
| 6 | `6 * count` | mixed | `IPv4(4)` + `port(u16 LE)` repeated |

### 5) Master Discovery: NMS1/v1 (Current Path)

#### 5.1 Header (16 bytes)

| Offset | Size | Field |
|---|---:|---|
| 0 | 4 | ASCII magic `NMS1` |
| 4 | 1 | protocol version (`1`) |
| 5 | 1 | message type |
| 6 | 2 | flags (`0`) |
| 8 | 4 | request id |
| 12 | 2 | payload length |
| 14 | 2 | reserved (`0`) |

All multi-byte numeric fields are big-endian.

#### 5.2 Message Types

| Value | Name | Direction |
|---:|---|---|
| 1 | `ChallengeRequest` | client/server -> master |
| 2 | `ChallengeResponse` | master -> client/server |
| 3 | `Register` | server -> master |
| 4 | `RegisterAck` | master -> server |
| 5 | `Heartbeat` | server -> master |
| 6 | `HeartbeatAck` | master -> server |
| 7 | `Unregister` | server -> master |
| 8 | `UnregisterAck` | master -> server |
| 9 | `ListRequest` | browser -> master |
| 10 | `ListResponse` | master -> browser |
| 11 | `Error` | master -> caller |

#### 5.3 Browser List Query Flow

1. Send `ChallengeRequest` with `purpose=ListQuery (2)`.
2. Receive `ChallengeResponse` with `challenge_issued_unix` + `challenge_token`.
3. Send paged `ListRequest` with:
   - challenge fields
   - `protocol_family` (normally `raw`)
   - `cursor` (start at `0`)
   - `page_size` (launcher currently uses `3`)
4. Receive `ListResponse` pages until response `cursor=0`.

#### 5.4 NMS1 Field IDs Used by Browser/Advertiser

| Field id | Name | Type |
|---:|---|---|
| 1 | `purpose` | u8 |
| 2 | `challenge_issued_unix` | u32 |
| 3 | `challenge_token` | 32 raw bytes |
| 16 | `protocol_family` | UTF-8 text (1..32) |
| 17 | `game_port` | u16 |
| 18 | `query_port` | u16 |
| 19 | `entry_token` | 32 raw bytes |
| 20 | `current_players` | u16 |
| 21 | `max_players` | u16 |
| 22 | `server_flags` | u32 |
| 24 | `build_label` | UTF-8 text, max 64 |
| 25 | `display_name` | UTF-8 text, max 96 |
| 26 | `game_name` | UTF-8 text, max 64 |
| 27 | `map_name` | UTF-8 text, max 64 |
| 31 | `cursor` | u32 |
| 32 | `page_size` | u16 |
| 33 | `entry_count` | u16 |
| 100 | `error_code` | u16 |
| 101 | `error_text` | UTF-8 text |
| 200 | `entries` | packed endpoint records |
| 201 | `entry_metadata` | optional packed metadata records (launcher extension) |

#### 5.5 `entries` Record Format (Field 200)

Per entry:

| Field | Size | Notes |
|---|---:|---|
| address family | 1 | `4` (IPv4) or `6` (IPv6) |
| address bytes | 4 or 16 | raw IP bytes |
| game port | 2 | u16 BE |
| query port | 2 | u16 BE |
| current players | 2 | u16 BE |
| max players | 2 | u16 BE |
| server flags | 4 | u32 BE |
| ttl remaining | 2 | u16 BE |

`entry_count` must match the number of records parsed.

#### 5.6 `entry_metadata` Format (Field 201)

For each entry (same order as field 200), the browser parses:

- `build_label`: length-prefixed UTF-8 (1-byte length, max 64)
- `display_name`: length-prefixed UTF-8 (max 96)
- `game_name`: length-prefixed UTF-8 (max 64)
- `map_name`: length-prefixed UTF-8 (max 64)

Reject metadata containing embedded NULs, oversized lengths, or control chars.

### 6) Per-Server Query Protocol (Raw Launcher Query)

This query is against the server endpoint itself (usually `query_port` from
NMS1; currently equal to `game_port` in HCDE advertiser behavior).

#### 6.1 Request Packet

Send 4 bytes:

| Offset | Size | Endian | Field |
|---|---:|---|---|
| 0 | 4 | BE | launcher challenge `777123` |

#### 6.2 Response Packet

| Offset | Size | Endian | Field |
|---|---:|---|---|
| 0 | 4 | BE | response code (`5560020` = accepted) |
| 4 | 4 | BE | query time (server-side value) |
| 8+ | var | | null-terminated UTF-8 strings and fixed fields below |

After code/time:

1. `hostName` (Z-string, UTF-8, NUL-terminated)
2. `playerCount` (u8)
3. `maxPlayers` (u8)
4. `mapName` (Z-string)
5. `sessionState` (Z-string)
6. `deathmatch` (u8 bool)
7. `skill` (u8)
8. `teamplay` (u8 bool)
9. `timeLeft` (u16 BE)
10. `fragLimit` (u16 BE)
11. `version` (Z-string)
12. `gitHash` (Z-string)
13. `playerCountWire` (u8; if present and different, overrides earlier count)
14. repeated player records:
    - `name` (Z-string)
    - `ping` (u16 BE)
    - `frags` (i16 BE)
    - `kills` (i16 BE)
    - `deaths` (i16 BE)
15. optional trailing `gameName` (Z-string)

If response code is not `5560020`, treat as rejected and do not parse payload.

### 7) Launcher State Machine (Recommended)

#### 7.1 Browser Path

1. Resolve master host/port.
2. NMS1 challenge request.
3. Paginated list requests until `cursor=0`.
4. For each endpoint, run per-server query with timeout.
5. Merge results:
   - live query success -> detailed row
   - live query timeout/fail -> fallback row from master metadata

#### 7.2 Host + Local Join Path

1. Validate files/paths/ports.
2. Launch `hcdeserv` with chosen advertise policy.
3. Wait for socket-ready window (or bounded retry loop).
4. Launch local `hcde -join ... -dedicatedjoin`.
5. Monitor both processes; when authority exits, terminate/reconcile local join.

### 8) Timeouts and Error Handling

Recommended behavior (matches current tools):

- master query timeout: ~5000 ms
- per-server query timeout: ~2500 ms
- ignore packets from wrong IP/port for the in-flight request
- require matching request id on NMS1 responses
- reject malformed/truncated payloads immediately
- if heartbeat returns `stale_entry` or `token_invalid`, re-run challenge/register

### 9) Standalone Master Server (`hcdemaster`)

`hcdemaster` is a standalone UDP registry process and does not link HCDE gameplay.

Current implementation note: the `tools/hcdemaster` binary in this repository
implements the legacy heartbeat/list query path. Use an NMS1-capable master
service for challenge/register/heartbeat/list v1 packet flows.

Run:

```powershell
hcdemaster --bind 0.0.0.0 --port 15000 --ttl 180
```

Options:

| Option | Meaning |
|---|---|
| `--bind <ipv4>` | Bind address (default `0.0.0.0`) |
| `--port <port>` / `-p <port>` | UDP port (default `15000`) |
| `--ttl <seconds>` | Entry TTL (default `180`) |
| `--max-packets <count>` | Optional max packet loop count |
| `--quiet` / `-q` | Suppress normal logging |
