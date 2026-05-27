# Legacy Netcode Remainder Audit

This note tracks the remaining GZDoom/UZDoom-shaped networking seams after the
HCDE native gameplay overhaul. HCDE keeps the UzDoom-derived engine as the
middle compatibility/simulation core, then layers Odamex-style server authority
and HCDE-specific replication around it. DSDA-Doom is a reference for narrow
rewind, determinism, and state-comparison work, not the primary networking
model. HCDE also carries compatibility layers for other Doom-family data
and scripting systems, including Eternity-style EDF/EMAPINFO and planned or
active EDGE Classic-style DDF, Lua, and COAL support. The goal is not to delete
every old name: some structures are still shared with demos, setup handshakes,
console commands, compatibility importers, and single-player input flow.

## Current architecture boundary

- The UzDoom-derived core owns rendering, scripting compatibility, playsim
  execution, and broad mod-facing behavior.
- The networking layer reaches into that core through narrow command,
  authority-event, snapshot, prediction, and repair seams.
- Odamex is the closest model for server-authoritative multiplayer policy.
- DSDA-Doom is mainly useful for rewind/state-hash/determinism techniques.
- Eternity and EDGE Classic compatibility layers should translate foreign data
  definitions and scripts into the UzDoom-derived core instead of becoming
  separate simulation cores.
- Live gameplay traffic is expected to use HCDE native `HCIN`/`HCSN` payloads.
- Classic `NCMD_*` traffic still exists for setup, latency, level-ready, and
  control messages in `src/common/engine/i_net.h` / `i_net.cpp`.
- `DEM_*`, `Net_DoCommand`, and `usercmd_t` are still shared with demos and
  gameplay side-effect replication. They are old-shaped, but not dead.
- The level object is `FLevelLocals`, not `DLevelLocals`; no `DLevelLocals`
  references were found in the current tree.
- Replication IDs live in net registries in `src/d_net.cpp`, not as persistent
  `netid` fields on actors.

## Hard dependencies

These are compile/link dependencies. Removing or renaming them requires a
replacement interface first.

| Area | Dependency | Notes |
| --- | --- | --- |
| Main loop | `src/d_main.cpp` -> `TryRunTics()` / `Net_Initialize()` | High risk; this is the frame driver. |
| Net core | `src/d_net.cpp`, `src/d_net_*.cpp`, `src/d_net.h` | High risk; contains live protocol, prediction, diagnostics, actor replication, and tick gates. |
| Transport/setup | `src/common/engine/i_net.cpp`, `i_net.h` | High risk; UDP, connect, setup, level-ready, HCDE pregame service. |
| Shared headers | `src/doomdef.h` includes `i_net.h`; `src/playsim/d_player.h` includes `d_protocol.h` and `d_netinf.h` | Broad compile coupling. |
| Player command model | `usercmd_t`, `LocalCmds`, `ClientStates`, `FClientNetState` | Still used by demos and local command construction. |
| Build system | `src/CMakeLists.txt` lists net sources and socket libs | Removing sources without replacing callers will fail link/build. |

The broadest hard include seam is `#include "d_net.h"`: more than sixty source
files include it today, including rendering code that mostly needs
`Net_ModifyFrac()` / `Net_ModifyObjectFrac()`.

## Runtime logic dependencies

These compile everywhere, but only change behavior when `netgame`,
`multiplayer`, prediction, or authority mode is active.

| Risk | Area | Examples | Suggested direction |
| --- | --- | --- | --- |
| Low | UI/menu/status | `menu/doommenu.cpp`, `shared_sbar.cpp`, launcher widgets | Keep behavior gates, but move read-only session facts behind a small status/query interface. |
| Low | Save paths/config | `savegamemanager.cpp`, platform special paths, `gameconfigfile.cpp` | Preserve separate netgame save/config behavior unless the product model changes. |
| Medium | Actor spawn/damage rules | `p_mobj.cpp`, `p_enemy.cpp`, `p_interaction.cpp` | Compare against Odamex server-authoritative rules before removing `multiplayer` branches. |
| Medium | ACS/ZScript network awareness | `p_acs.cpp`, `events.h`, `events.cpp` | Keep compatibility until there is a mod-facing replacement event API. |
| Medium | Compatibility definition/script importers | `hcde_eternity_compat.*`, `hcde_edf.*`, `hcde_emapinfo.*`, future EDGE DDF/Lua/COAL importers | Treat as facade/translation layers, not duplicate dead code. |
| Medium | Rendering interpolation | HW/SW renderer users of `Net_Modify*` | Candidate for decoupling into a tiny prediction/interpolation header instead of including all of `d_net.h`. |
| High | Tick processing | `TryRunTics()`, `NetUpdate()`, `P_Ticker()`, `FLevelLocals::Tick()` | Do not rewrite without an automated local host/join or demo determinism test. |
| High | Handshake/session lifecycle | `i_net.cpp`, `g_game.cpp`, `g_level.cpp` | Preserve setup/control `NCMD_*` until a full session service replacement exists. |

## Ghost-code markers to watch

Use these as targeted searches when touching netcode:

```text
NCMD_                 setup/control packets; gameplay use should be suspect
DEM_                  demo/event stream; old-shaped but usually intentional
usercmd_t             player input command model
ClientTic/gametic     tick scheduling and prediction
CurrentSequence       command/snapshot window state
netgame/multiplayer   runtime behavior gates
Net_ModifyFrac        rendering/prediction include seam
EDF/EMAPINFO          Eternity compatibility importer surface
DDF/Lua/COAL          EDGE Classic compatibility importer surface
```

The live gameplay guard now logs the phrase `legacy network path hit` when a
classic gameplay path reaches a native-only HCDE boundary. That is the preferred
stub-and-log signal for finding ghost dependencies during local host/join runs.

## Refactor queue

1. **Extract read-only session facts.** Create a small interface for UI/status
   code that exposes current mode, peer count, local authority, and save policy.
   This lets low-risk UI files stop depending on `d_net.h`.
2. **Extract prediction interpolation helpers.** Move `Net_ModifyFrac()` and
   `Net_ModifyObjectFrac()` behind a narrow prediction header so renderers do
   not include the full net core.
3. **Define compatibility importer boundaries.** Keep EDF, DDF, Lua, and COAL
   support as data/script translation facades that feed the UzDoom-derived core;
   do not let them bypass the command, authority-event, snapshot, or repair seams.
4. **Protect the command/event bridge.** Keep `DEM_*` and `usercmd_t` until a
   replacement side-effect bus exists. DSDA-Doom code is most useful here for
   rewind, demo/input determinism, and state-comparison ideas, not direct
   multiplayer replacement.
5. **Compare authority behavior against Odamex.** Actor spawn, damage, pickup,
   and projectile handling should be reviewed against Odamex-style server
   authority before any `multiplayer` gameplay branches are simplified.
6. **Treat `TryRunTics()` as sacred.** The authority clock already follows the
   Odamex/Zandronum model: the server simulates on wall-clock and lagging clients
   lag themselves. Changes here need integration coverage first.

## Deletion rules

- Delete only when the caller graph is understood and tests or runtime logs show
  the path is not needed.
- Stub and log live gameplay fallbacks before removing them.
- Do not remove setup/control `NCMD_*` just because gameplay `NCMD` was retired.
- Do not remove `DEM_*` paths unless demo playback/recording and authoritative
  side-effect replication have replacement coverage.
