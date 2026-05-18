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

## How HCDE Dedicated Multiplayer Works

HCDE dedicated flow is intentionally a two-process model:

1. start `hcdeserv` as the authority
2. join it with `hcde` using dedicated join flags

This is the launch shape Doom Connector expects.

## Dedicated Server Launch Commands (Complete HCDE Operator Set)

This section documents the server-launch commands used by HCDE dedicated operation from this codebase.

### Required server launch arguments

| Argument | Purpose |
|---|---|
| `-server <slots>` | Enables dedicated server mode with visible player slot count |
| `-iwad <path>` | IWAD to run (e.g. `doom2.wad`) |
| `+map <mapname>` | Startup map (recommended for dedicated startup) |

### Common optional dedicated arguments

| Argument | Purpose |
|---|---|
| `-port <udpPort>` | Alternate game UDP port (default multiplayer port is `5029` unless overridden) |
| `-file <mods...>` | Load one or more PWAD/PK3 files |
| `-deh <patch.deh>` | Load DeHackEd patch |
| `-bex <patch.bex>` | Load BEX patch |
| `-password <text>` | Require a network password for join |
| `-host <slots>` | Start a listen-server host inside `hcde` (non-dedicated flow) |
| `-advertise` | Force master advertising on for this run |
| `-noadvertise` | Force master advertising off for this run |
| `-master <host[:port]>` | Add master endpoint at startup (repeatable) |

### Related join/client launch arguments

| Argument | Purpose |
|---|---|
| `-join <host:port>` | Join a host/dedicated server |
| `-dedicatedjoin` | Join using HCDE dedicated reserved-authority-slot behavior |
| `-netwaitsilent` | Hide old connection/session pregame window |

### Canonical examples

Dedicated private server:

```powershell
hcdeserv -server 8 -iwad C:\Games\doom2.wad -port 10666 +map MAP01
```

Dedicated public server with master advertising:

```powershell
hcdeserv -server 8 -iwad C:\Games\doom2.wad -port 10666 -advertise -master hcde.servebeer.com:15000 +map MAP01
```

Dedicated join client:

```powershell
hcde -join 127.0.0.1:10666 -dedicatedjoin -iwad C:\Games\doom2.wad -file C:\Mods\example.pk3
```

### Runtime master-control console commands (server side)

These are not process-launch flags, but are part of dedicated operation:

- `advertise [host[:port] ...]`
- `unadvertise`
- `addmaster <host[:port]>`
- `delmaster <host[:port]>`
- `masters`

## Standalone Master Server (`hcdemaster`)

`hcdemaster` is a standalone UDP registry process. It does not link against HCDE gameplay engine code.

### Run

```powershell
hcdemaster --bind 0.0.0.0 --port 15000 --ttl 180
```

### Options

| Option | Meaning |
|---|---|
| `--bind <ipv4>` | Bind address (default `0.0.0.0`) |
| `--port <port>` / `-p <port>` | UDP port (default `15000`) |
| `--ttl <seconds>` | Entry TTL (default `180`) |
| `--max-packets <count>` | Optional max packet loop count |
| `--quiet` / `-q` | Suppress normal logging |

## Master Protocols

Source of truth:

- `protocol/hcde_master_protocol.json`
- `protocol/hcde_master_protocol.h`
- `docs/HCDE_MASTER_PROTOCOL.md`

### Legacy discovery protocol (still supported)

- Byte order: little-endian
- Default master port: `15000`
- Server heartbeat packet: 6 bytes
  - marker `5560020` (`uint32 LE`)
  - game port (`uint16 LE`)
- Launcher list query packet: 4 bytes
  - marker `777123` (`uint32 LE`)
- List response packet:
  - marker `777123` (`uint32 LE`)
  - count (`uint16 LE`)
  - repeated entries: IPv4 (4 bytes) + port (`uint16 LE`)

### NMS1 advertiser protocol (current registration flow)

- Header magic: `NMS1`
- Protocol version: `1`
- Header size: `16` bytes
- Byte order: big-endian for multi-byte numeric values
- Core flow:
  1. `ChallengeRequest`
  2. `ChallengeResponse`
  3. `Register`
  4. `RegisterAck` (returns entry token)
  5. periodic `Heartbeat` / `HeartbeatAck`
  6. `Unregister` / `UnregisterAck` on shutdown or endpoint change
- Current HCDE server advertiser behavior:
  - `protocolFamily` defaults to `raw`
  - `queryPort` is currently sent as the same value as `gamePort`

Message types:

- `1` challengeRequest
- `2` challengeResponse
- `3` register
- `4` registerAck
- `5` heartbeat
- `6` heartbeatAck
- `7` unregister
- `8` unregisterAck
- `9` listRequest
- `10` listResponse
- `11` error

### Boundary rules

HCDE explicitly allows sharing protocol facts/schemas/constants, and explicitly disallows sharing implementation internals (socket loops, storage logic, launcher behavior, gameplay code) across projects.

## HCDE Network Session Protocol (Engine Side, High-Level)

During dedicated join, HCDE negotiates and uses multiple lanes:

- connect negotiation magic: `HCD3` (service connect)
- pregame service channel: reliable service messages (`HPS_*`, seq/ack)
- live gameplay channel types:
  - control
  - client inputs
  - server snapshots
- gameplay envelope magic: `HGPL`
  - includes room id, protocol version, payload kind, game tic

Current payload families documented in code/docs include:

- client input payloads (`HCIN` family with explicit records)
- server snapshot payloads (`HCSN` family with explicit records)
- server-authored world delta stream (`HCDW`) used for validation/telemetry and live authority evolution

Detailed stage notes live in:

- `docs/HCDE_DOOM_CONNECTOR_LAUNCH.md`
- `docs/HCDE_NETCODE_STAGE*.md`

## Licensing (GPL-3.0-or-later)

HCDE is licensed under **GNU GPL v3 or later** (see `LICENSE`).

Important repository-specific notes already included in `LICENSE`:

- branding exceptions are documented in `branding/BRANDING-LICENSE.md`
- content under `wadsrc_bm`, `wadsrc_extra`, and `wadsrc_widepix` has separate asset/license notes:
  - `wadsrc_bm/static/license.md`
  - `wadsrc_extra/static/license.md`
  - `wadsrc_widepix/static/license.md`

No warranty is provided; see GPL sections 15 and 16.

## Credits and Contributors

See `CONTRIBUTORS` for the maintained contributor list.

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
