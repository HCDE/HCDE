<div align="center">

[<img src="branding/hcde-logo.svg" alt="HCDE logo" style="width: 100%; max-width: 1600px;" />](.)

</div>

# HCDE

HCDE is a Doom-engine project focused on multiplayer, mod compatibility, and dedicated-server workflows that pair cleanly with Doom Connector.

This README is the operator/dev reference for:

- what HCDE is and how it is structured
- how to build it
- all launch commands used for HCDE dedicated-server startup and join flow
- how the HCDE master server works (legacy + NMS1 protocol families)
- licensing (GPL-3.0-or-later + asset exceptions)
- core tech/libraries used by this project

## What HCDE is

HCDE currently ships three primary binaries from this tree:

- `hcde` - client/game executable
- `hcdeserv` - dedicated server executable
- `hcdemaster` - standalone public-server registry (master server)

HCDE keeps the master protocol boundary explicit:

- protocol facts are shared in `protocol/hcde_master_protocol.json` and `protocol/hcde_master_protocol.h`
- engine, launcher, and master-server implementations stay separate

## Repository Layout (Important Paths)

- `src/` - core engine and networking code
- `protocol/` - neutral master-protocol schema/constants
- `tools/hcdemaster/` - standalone master server source
- `docs/HCDE_MASTER_PROTOCOL.md` - master protocol contract (human-readable)
- `docs/HCDE_DOOM_CONNECTOR_LAUNCH.md` - launcher/server launch contract
- [docs/Cvars.md](docs/Cvars.md) - full list of all cvars and defaults
- `wadsrc*` - packaged game resources and compat content
- `docs/licenses/` - license reference docs

## Build

### Requirements

- CMake `>= 3.16`
- C++20 compiler
- Python `>= 3.6` (resource/packaging scripts)
- Threads package

The CMake project can use bundled/internal or system variants for some libraries (e.g., bzip2, cppdap), and links engine deps like ZMusic/ZVulkan/ZWidget/webp/lzma/abseil.

### Windows (Visual Studio) quick build

```powershell
cmake -S C:\Users\user\DoomConnector6\HCDE -B C:\Users\user\DoomConnector6\HCDE\build -G "Visual Studio 17 2022" -A x64
cmake --build C:\Users\user\DoomConnector6\HCDE\build --config Release
```

Typical outputs:

- `C:\Users\user\DoomConnector6\HCDE\build\Release\hcde.exe`
- `C:\Users\user\DoomConnector6\HCDE\build\Release\hcdeserv.exe`
- `C:\Users\user\DoomConnector6\HCDE\build\Release\hcdemaster.exe`

### Linux quick build

```bash
cmake -S /path/to/HCDE -B /path/to/HCDE/build -DCMAKE_BUILD_TYPE=Release
cmake --build /path/to/HCDE/build -j
```

## Windows Launcher Updater (Built-In)

HCDE includes a Windows updater flow in the launcher `About` tab.

Scope:

- Windows x64 runtime package updates
- Source: latest GitHub release from `bokoxthexchocobo/HCDE`
- Install model: in-place update with automatic backup and rollback path

Launcher workflow:

1. Open `About`.
2. Click `Check for updates`.
3. If a newer version is found, click `Install update`.
4. HCDE exits, updater applies the package, then restarts HCDE.

UI controls:

- `Check for updates`
- `Install update`
- `Open updater backups`
- `Open last updater log`
- bottom-bar notice button appears when an update is available or updater attention is required

Release/API behavior:

- checks `https://api.github.com/repos/bokoxthexchocobo/HCDE/releases/latest`
- compares latest tag against local `VERSIONSTR`
- prefers runtime zips matching `HCDE-<version>-windows-x64.zip`
- de-prioritizes non-runtime zips (`symbols`, `pdb`, `debug`, `source`, `src`)

Update safety guards:

- download URL must be `https`
- allowed hosts: `github.com`, `api.github.com`, `objects.githubusercontent.com`, `release-assets.githubusercontent.com`
- package must contain `hcde.exe` at payload root (or single top-level folder payload root)
- refuses update if install target resolves to filesystem root

Updater files written under `<install dir>\backups`:

- `hcde-update.lock` (active updater lock)
- `hcde-update-last-status.txt` (last run summary)
- `hcde-update-<version>-<timestamp>.log` (per-run log)
- `hcde-backup-<version>-<timestamp>.zip` (pre-update backup)

Lock/cleanup behavior:

- lock prevents concurrent updater runs
- lock is treated as stale after `120` minutes
- stale lock is auto-cleared before a new run

Rollback and recovery:

1. If update fails and rollback succeeds, relaunch HCDE and inspect `hcde-update-last-status.txt`.
2. If update fails and rollback fails, restore manually from the newest `hcde-backup-*.zip` in `backups`.
3. If updater appears stuck, close all HCDE processes and re-open launcher; stale lock cleanup runs automatically.

Manual validation script (repo utility):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\Users\user\DoomConnector6\HCDE\tools\verify-hcde-updater.ps1
```

This script validates:

- release endpoint + asset selection
- runtime package structure
- success path (backup + replace)
- failure path (rollback restore)
- stale lock cleanup path

## HCDE Launcher Protocol (Detailed Reference)

This section is the implementation-level launcher/browser contract for HCDE.
It documents how dedicated launch, master-list discovery, and per-server query
work so third-party tools can interoperate without reverse engineering.

Source of truth:

- `docs/HCDE_DOOM_CONNECTOR_LAUNCH.md`
- `docs/HCDE_MASTER_PROTOCOL.md`
- `protocol/hcde_master_protocol.json`
- `protocol/hcde_master_protocol.h`
- Doom Connector client implementation files (separate repository/worktree):
  - `DoomConnector.Client.Core/Networking/HCDEMasterService.cs`
  - `DoomConnector.Client.Core/Networking/HCDELauncherQuery.cs`

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

## HCDE Network Session Protocol (Engine Side, High-Level)

During dedicated join, HCDE negotiates and uses multiple lanes:

- connect negotiation magic: `HCD3` (service connect)
- pregame service channel: reliable service messages (`HPS_*`, seq/ack)
- live lane accounting:
  - control
  - command
  - authority
  - player snapshot
  - actor delta
  - query/registry
- actor-delta scheduling:
  - invasion actor deltas build per-client priority queues before serialization
  - `net_profile` and `net_lanes` report queue depth, priority depth, deferred depth, and top score
  - `net_relevance` reports critical/high/medium/low/dormant actor interest per client
  - far low-interest actors are skipped until their keepalive interval while protected actor state stays hot
- server simulation LOD:
  - `sv_invasionsimlod` conservatively throttles far idle invasion monsters before world ticking
  - `net_simlod` reports full/reduced/dormant actors, suspended thinkers, skipped ticks, and wake reasons
  - actors wake immediately for distance, health changes, combat targets, boss waves, projectiles, deaths, and action-priority states
- compact actor deltas:
  - modern live peers negotiate `actor-registry-v1` and `actor-delta-v2`
  - `HCDA` v2 uses registry class ids, field masks, quantized positions/velocities, and compact angles
  - invasion uses `HCDA` as the live actor stream, and coop/DM snapshots can now carry top-level shared-registry `HCDA` records
  - non-invasion `HCDA` currently establishes budgeted shared baselines without spawning or steering gameplay actors; authority-driven actor birth/apply remains a later migration step
  - the shared `HCDA` path is now the live invasion actor stream; the old invasion `HCIA` v5 fallback is retired
- projectile policy:
  - projectile deltas are ranked by baseline state, distance, viewer targeting, inbound velocity, player ownership, and keepalive age
  - near/inbound/player-relevant projectiles stay hot while distant projectile storms are allowed to thin out until keepalive or renewed relevance
  - `net_profile`, `net_relevance`, and `net_stressreport` expose projectile policy tiers, skips, keepalives, protected records, inbound shots, and player-owned shots
- lane budgets:
  - modern live peers negotiate `lane-budgets-v1`
  - protected lanes stay first: control, commands, authority headers, and player snapshots are preserved before optional actor detail
  - optional authority event replay is capped separately from actor deltas, and actor deltas use their own budget instead of consuming all remaining snapshot space
  - `net_lanes` and `net_stressreport` report lane budgets, budget clamps, deferrals, and byte counters
- authority events:
  - modern live peers negotiate `authority-events-v1`
  - `HCAV` v1 carries protected server-authored gameplay facts on the authority lane
  - invasion spawn, damage/health, and death/despawn facts now move through `HCAV`; the old invasion `HCIS` spawn fallback is retired
  - damage/health facts are interval-throttled and stale per-actor health facts are skipped when a newer damage/despawn fact is retained
  - coop/DM pickup spawn/restore and retire facts now move through `HCAV`, so item existence is protected even when shared actor detail is deferred
  - `net_profile` and `net_stressreport` break down `HCAV` pressure by spawn, damage, despawn, pickup spawn, pickup retire, and superseded retained facts
- baseline repair and catchup:
  - runtime late join and client reset paths clear stale shared actor baselines per client
  - a bounded repair window forces fresh full actor records without increasing the whole snapshot budget
  - late joiners walk retained `HCAV` spawn history forward with a per-client catchup cursor instead of relying only on the last replay tail
  - `net_profile`, `net_lanes`, and `net_stressreport` expose repair windows, reset counts, catchup records, and pending replay ids
- competitive/high-ping audit:
  - player command and player snapshot lanes stay protected before optional actor detail
  - shared `HCDA` actor deltas continue to suppress player pawns so unlagged/player correction stays isolated
  - `net_unlagged` reports latency, command/snapshot flow, player-lane budget pressure, prediction repairs, hard reconciliations, and actor-lane deferrals
- mode migration:
  - invasion monsters/projectiles are refreshed into the shared registry from their authoritative ids
  - coop and DM periodically populate the shared registry with conservative actor categories for the shared `HCDA` lane
  - `net_migration` reports the current mode scan and source counts
- stress and soak diagnostics:
  - `net_stressreport` emits one compact pressure snapshot covering world tic cost, actor queues, delta budget deferrals, relevance tiers, simulation LOD, mode migration, lane counters, and per-peer queue state
  - each stress report also writes a compact `DebugTrace` entry on the `net` channel for advanced-debugger and trace-file workflows
  - `tests/netcode_step12/netcode_step12_stress.py` runs repeatable invasion/coop/DM dedicated-server soak cases while launcher queries stay active
  - the Step 12 harness supports optional client join pressure, master-server advertising, mod-specific pressure commands, pressure presets, debug trace export via `--trace-save-dir`, JSON summaries via `--summary-dir`, and external high-ping/jitter/loss shaping metadata
- runtime debug trace streams (enabled by default via `debugtrace_enable` and `debugtrace_stream`):
  - `%LOCALAPPDATA%/hcde/hcde_trace.hcde.latest.log` for the client and `%LOCALAPPDATA%/hcde/hcde_trace.hcdeserv.latest.log` for the dedicated server
  - session-scoped files also land as `hcde_trace.<process>.<session>.log`; match the `sess=` field across client and server logs when diagnosing net stalls
  - in-game: `debugtraceopen` prints paths, `debugtrace [channel]` dumps the ring buffer, `debugtraceflush` / `debugtracesave` export on demand
- live gameplay channel types:
  - control
  - client inputs
  - server snapshots
- live control capability trailer: `HCAP` v1 bitmask, negotiated peer-to-peer and ignored by older peers
- gameplay envelope magic: `HGPL`
  - includes room id, protocol version, payload kind, game tic

Current payload families documented in code/docs include:

- client input payloads (`HCIN` family with explicit records)
- server snapshot payloads (`HCSN` family with explicit records)
- server-authored world delta stream (`HCDW`) used for validation/telemetry and live authority evolution
- shared replicated actor registry scaffold used to assign actor categories, class table ids, and reverse lookups before actor-delta v2 owns serialization

Detailed stage notes live in:

- `docs/HCDE_NETCODE_OVERHAUL.md`
- `tests/netcode_step12/README.md`
- `docs/HCDE_DOOM_CONNECTOR_LAUNCH.md`
- `docs/HCDE_NETCODE_STAGE*.md`

## Licensing (GPL-3.0-or-later)

HCDE is licensed under **GNU GPL v3 or later** (see `LICENSE`).

Important repository-specific notes already included in `LICENSE`:

- branding exceptions are documented in `branding/BRANDING-LICENSE.md`
- content under `wadsrc_bm`, `wadsrc_extra`, and `wadsrc_widepix` has separate asset/license notes:
  - `wadsrc_bm/static/license.md`
  - `wadsrc_extra/nonfree/license.md`
  - `wadsrc_widepix/static/license.md`

No warranty is provided; see GPL sections 15 and 16.

## Credits and Contributors

See `contributors.md` for the HCDE project contributor summary used by this
repository, and `CONTRIBUTORS` for the upstream historical contributor list.

## Tech Stack and Components Used to Build HCDE

Primary build/runtime stack in this repository:

- CMake build system (`>= 3.16`)
- C++20
- Python 3 scripts for resource build/staging
- Engine and support libraries integrated by this tree:
  - ZMusic
  - ZVulkan
  - ZWidget
  - Abseil
  - WebP
  - LZMA
  - bzip2
  - asmjit
  - cppdap
  - miniz
  - VPX (when available)
  - SDL2/OpenAL paths (platform-dependent in build config)
- Internal build tools:
  - `zipdir`
  - `re2c`
  - `lemon`
  - `hcdemaster`

For per-component licenses, see:

- top-level `LICENSE`
- `docs/licenses/`
- each vendored library folder under `libraries/*` (README/LICENSE files)
