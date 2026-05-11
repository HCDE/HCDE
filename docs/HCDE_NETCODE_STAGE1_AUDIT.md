# HCDE Netcode Stage 1 Audit

Date: 2026-05-11
Tree: `C:\Users\User\HCDE`
Baseline commit inspected: `35f69f4fc8c3b4dd12189d98fdfd2340d622d2f6`

This audit maps what HCDE currently ships for multiplayer, where UZDoom-era
session code is still active, and which Odamex-style pieces still need to be
ported before HCDE behaves like a real separated client/server engine.

## Ground Rules

- HCDE remains GPL-3.0-or-later.
- Doom Connector, HCDE, and the master server remain separate programs.
- The master server communicates by documented network protocol only.
- Imported code must keep upstream notices and provenance.
- Odamex is a source/reference boundary until code is intentionally ported.

## Current HCDE Multiplayer Shape

| Area | Current state | Main files |
| --- | --- | --- |
| Client executable | Built as `hcde.exe` from the full game source set. | `src/CMakeLists.txt`, `src/d_main.cpp` |
| Server executable | Built as `hcdeserv.exe`, but from the same `GAME_SOURCES` as the client with `HCDE_DEDICATED_SERVER=1`. It is not yet a minimal Odamex-style server target. | `src/CMakeLists.txt` |
| Startup gating | `-server` skips several video/UI paths, startup screen paths, and delayed startup commands. The server still runs through the shared UZDoom/GZDoom initialization flow. | `src/d_main.cpp` |
| Pregame transport | Uses the existing UZDoom room/session handshake: `PRE_CONNECT`, `PRE_CONNECT_ACK`, `PRE_GAME_INFO`, `PRE_GO`, session tokens, and room flow states. | `src/common/engine/i_net.cpp` |
| Dedicated server mode | Adds `-server`, `-netwaitsilent`, and `-dedicatedjoin` behavior, hides the old session window in those paths, and reserves the arbitrator as a hidden server slot. | `src/common/engine/i_net.cpp`, `src/d_net.cpp` |
| Gameplay sync | Still uses deterministic tic command synchronization, `Net_Arbitrator`, `NetworkClients`, `ClientStates`, heartbeat tics, consistency checks, and level-start acks. | `src/d_net.cpp` |
| Old lobby UI | Still present and callable for normal `-host` flows. It is suppressed for dedicated/silent join flows, not removed. | `src/widgets/netstartwindow.cpp` |
| Server browser query | HCDE custom query snapshot exists, with a compatibility check for Odamex query-tag style probes. It is not the Odamex server protocol. | `src/common/engine/i_net.h`, `src/common/engine/i_net.cpp` |
| Master heartbeat | HCDE custom heartbeat/list protocol, separate process, defaulting to `hcde.servebeer.com`. | `src/common/engine/sv_master.cpp`, `tools/hcdemaster`, `docs/HCDE_MASTER_PROTOCOL.md` |

## Odamex Reference Shape

The local upstream reference at `C:\Users\User\odamex-upstream` has a much
clearer split:

| Odamex area | Representative files |
| --- | --- |
| Dedicated server target | `server/CMakeLists.txt`, `server/src/sv_main.cpp`, `server/src/d_main.cpp` |
| Client target | `client/CMakeLists.txt` |
| Master target | `master/CMakeLists.txt`, `master/main.cpp` |
| Server protocol and replication | `server/src/sv_main.cpp`, `server/src/sv_level.cpp`, `server/src/sv_game.cpp`, `common/svc_message.cpp`, `common/svc_map.cpp`, `common/p_snapshot.cpp` |
| Server discovery/query | `server/src/sv_master.cpp`, `server/src/sv_sqp.cpp`, `server/src/sv_sqpold.cpp` |

HCDE currently does not contain a live port of those Odamex server/client
replication systems. The visible Odamex touchpoint in active HCDE code is the
query tag constant used to accept Odamex-shaped discovery probes.

## What Is Already Useful

- `hcdeserv.exe` exists as a separate binary target and can run with a console
  subsystem on Windows.
- `-server` now avoids the worst client-only startup paths and suppresses the
  old session window.
- `-dedicatedjoin` prevents the hidden server/arbitrator slot from appearing as
  a normal in-game player.
- Doom Connector can query servers through the HCDE query snapshot path.
- `hcdemaster` is a separate standalone discovery process, which matches the
  golden-rule boundary.

## Main Gaps

1. `hcdeserv.exe` is still structurally a client build with server gates, not a
   server-only program.
2. Connection setup is still the old room/session handshake. It must become an
   Odamex-style connect/auth/load-map/join flow without the room state machine.
3. Gameplay sync is still deterministic peer/arbitrator tic sync. It must gain
   server-owned state, authoritative joins, snapshots/service messages, and
   client-side prediction boundaries.
4. The old `NetStartWindow` lobby still exists and can return if a launch path
   misses `-netwaitsilent`, `-join`, or `-dedicatedjoin`.
5. Master discovery is HCDE-custom and documented, but not Odamex SQP. That is
   acceptable for Doom Connector, but it should be named as HCDE protocol.
6. No live Odamex source import was found in HCDE's active multiplayer files.
   Any future import needs explicit provenance notes and retained notices.

## Stage 2 Target

Stage 2 should make the dedicated server boundary explicit without replacing
gameplay replication yet:

1. Add a small HCDE server runtime boundary in code, separate from the client
   launch path.
2. Make `hcdeserv` fail loudly if it attempts to create client UI, renderer
   windows, input devices, or startup overlays.
3. Keep the current game-source build if needed, but isolate server-only startup
   decisions in one small adapter instead of scattered `Args->CheckParm(-server)`
   checks.
4. Add diagnostic logging that reports server mode, visible player slots,
   hidden server slot status, port, IWAD, selected map command, master setting,
   and whether the old room UI is suppressed.
5. Document the expected Doom Connector launch contract:
   `hcdeserv -server <slots> -iwad <iwad> -port <port> +map <map>` and
   `hcde -join <host:port> -dedicatedjoin -netwaitsilent -iwad <iwad>`.

Stage 2 should stop before importing Odamex `sv_main.cpp` or `svc_message.cpp`.
That port needs its own stage because it touches gameplay authority, actor
state, map state, ACS/script events, and prediction.

