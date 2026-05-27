# HCDE Roadmap Integration Plan

This plan mirrors the public HCDE Kanban project board and treats its major
items as part of the engine integration roadmap. The board contains both feature
work and bug-fix/maintenance work; they should not be merged into one generic
"future ideas" bucket.

Project board source:

- <https://github.com/users/bokoxthexchocobo/projects/2>

## Architecture target

HCDE is a compatibility-layer engine, not a single-source-port fork. The
intended shape is:

```text
UzDoom-derived middle core
  - playsim, renderer, scripting, mod-facing runtime, engine services

Compatibility importers
  - ZDoom/UzDoom formats and behavior
  - Eternity-style EDF/EMAPINFO
  - EDGE Classic-style DDF, Lua, and COAL
  - ID24 and MBF21 feature surfaces

Multiplayer and diagnostics
  - Odamex-style server authority and session behavior
  - HCDE-native command, snapshot, authority-event, and repair lanes
  - DSDA-inspired rewind, state hashing, and determinism tools

Feature/feel layers
  - selected rendering, physics, input, audio, BSP, AI, and gameplay systems
    imported as facades into the canonical HCDE runtime
```

The UzDoom-derived core remains the canonical runtime. Imported systems should
translate into that core rather than becoming parallel simulation engines.

## Board status snapshot

Snapshot source: GitHub project item list visible on 2026-05-27.

### Done foundations

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #14 | ODAMEX Netcode baked in | Treat as the completed foundation for server-authoritative multiplayer. Follow-up work should harden and decouple it rather than reintroduce lockstep gameplay. |
| #1 | Full MBF21 compliance | Treat MBF21 as a supported compatibility surface that future gameplay/importer changes must preserve. |
| #19 | check console for bugs - i keep getting stuck | Completed bug-fix item; keep regressions covered by startup/console smoke checks when available. |

### In progress pillars

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #17 | Rocket wants more k8vavooom style rendering as default with shadows and raytracing like lights | Rendering modernization pillar. Keep visual upgrades behind renderer-facing facades so they do not leak into playsim/network determinism. |
| #15 | Finish Invasion mode | Multiplayer gameplay stress test. Invasion should exercise actor replication, authority events, prediction repair, and server simulation load. |
| #5 | DSDA-Doom -- The Rewind System | Rewind/diagnostics pillar. Use DSDA concepts for history buffers, state comparison, and debugging; do not treat DSDA as the primary network model. |

### Ready items

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #2 | ID24 | Compatibility surface. Needs clear mapping into HCDE's canonical runtime and interaction checks with MBF21/ZDoom behavior. |
| #3 | Eternity's Spatial Audio Engine | Audio feature import. Keep as an engine-service facade; verify multiplayer/dedicated behavior does not depend on client-only audio state. |

### Backlog feature ports and compatibility layers

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #4 | NanoBSP from Woof doom engine | Map/rendering infrastructure candidate. Needs isolation from gameplay collision and network-relevant map state until validated. |
| #9 | Nugget Doom -- Player Feel & Input | Input/feel layer. Must feed the command pipeline without bypassing prediction or server authority. |
| #7 | Nugget Doom -- Gyroscope Input | Input device feature. Should terminate in normal local command construction. |
| #8 | Doom Retro -- Physics & Feel Layer | Gameplay feel candidate. High desync risk; compare against authoritative server behavior before enabling in netgames. |
| #10 | Crispy Doom -- Variable Framerate System | Presentation/timing feature. Must stay presentation-side and avoid changing fixed-tic playsim semantics. |
| #11 | International Doom -- Two Specific Features | Compatibility/UX feature imports; classify each feature as rendering, input, gameplay, or data compatibility before implementation. |
| #12 | Predator Economy Game Mode | Gameplay mode. Should use authority events and replicated mode state instead of ad hoc client state. |
| #13 | Make a AI system for hcde for monsters / enemies | Gameplay/AI layer. High risk for prediction, savegames, and multiplayer determinism; keep server authoritative. |
| #21 | need doom skins taunt sound mechanic | Player cosmetic/audio feature. Keep cosmetic sound triggering separate from authoritative gameplay effects. |

### Backlog bug-fix and tooling work

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #22 | defensive null-and-guard pass to Net_CompactHCDEReplicatedActors | Netcode hardening. Shared actor registry compaction must not invalidate live replication IDs or per-client baselines. |
| #23 | redo the hcdeserv ui .. the apply button doesnt need to be there, it should just apply it | Dedicated-server UI/UX cleanup. Keep UI changes separate from server runtime state transitions. |
| #24 | rcon utility | Server administration tooling. Should use a narrow auth/command channel and avoid coupling to gameplay packet lanes. |

### In review

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #18 | check over the updater | Maintenance/release infrastructure. Keep separate from engine runtime changes. |

## Roadmap grouping by subsystem

### Networking and authority

- Preserve the Odamex-style authority foundation from #14.
- Finish Invasion mode (#15) as the multiplayer stress test for monsters,
  projectiles, actor deltas, late join, and server load.
- Harden actor registry compaction (#22) before broadening replicated actor
  categories.
- Add RCON (#24) as admin tooling, not as a gameplay packet extension.

### Rewind, observability, and determinism

- Implement the DSDA-inspired rewind work (#5) as history/state tooling.
- Add state hash or state-comparator hooks around authoritative tic boundaries.
- Use rewind/state comparison to debug divergence in Invasion, AI, physics, and
  imported compatibility behavior.

### Compatibility importers

- Preserve MBF21 compliance (#1).
- Add ID24 (#2) as another compatibility surface.
- Treat Eternity EDF/EMAPINFO, EDGE Classic DDF/Lua/COAL, and other imported
  definitions as facades into the UzDoom-derived core.
- Avoid parallel runtime semantics for imported formats unless there is a
  deliberate adapter that maps them into HCDE's command/event/snapshot model.

### Rendering, timing, and presentation

- Keep the k8vavoom-style rendering work (#17) renderer-facing.
- Evaluate NanoBSP (#4) as infrastructure with strict boundaries around map
  collision and gameplay state.
- Keep Crispy-style variable framerate (#10) presentation-side. The fixed tic
  simulation must remain authoritative.

### Input, player feel, and physics

- Route Nugget player-feel/input work (#9) and gyroscope input (#7) into normal
  command construction.
- Treat Doom Retro physics/feel (#8) as multiplayer-sensitive. Any gameplay
  physics changes need server-authoritative validation and netgame gating.

### Gameplay systems

- Predator Economy mode (#12), monster/enemy AI (#13), and skin taunt sounds
  (#21) should use authority events, replicated mode state, and cosmetic/event
  separation as appropriate.

### Maintenance and release tooling

- Keep updater review (#18), console bug regressions (#19), hcdeserv UI cleanup
  (#23), and RCON (#24) tracked as maintenance/tooling work, not feature-port
  experiments.

## Implementation rules

- Board items are roadmap inputs. Do not drop them from planning just because
  they come from different Doom-family ports.
- Classify each item before implementation: compatibility importer, engine
  service, presentation layer, command input, gameplay logic, networking,
  diagnostics, or maintenance.
- Anything that changes gameplay state must pass through the canonical HCDE
  runtime and respect server authority.
- Presentation improvements must not change fixed-tic playsim semantics.
- Imported data/script systems should translate into existing runtime concepts
  before adding new parallel state.
- Bug-fix cards should be handled with focused verification and should not be
  bundled with large feature-port refactors unless they are direct prerequisites.
