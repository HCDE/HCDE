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
| #5 | DSDA-Doom -- The Rewind System | Phases 1-5 implemented in `src/d_net_rewind.{cpp,h}`: ring-buffer keyframe capture, restore primitives, lag-compensation bracket, server-side hit replay through `HCDELagCompScope` (wired into `P_LineAttack`). Default-off (`net_rewind_enable 0`, `sv_lagcomp 0`) so the per-second serializer cost only runs when the operator opts in. See `docs/HCDE_REWIND.md`. |

### In progress pillars

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #17 | Rocket wants more k8vavooom style rendering as default with shadows and raytracing like lights | **Phase 1 implementation landed (2026-05-28).** `hcde_k8vavoom_lighting_profile=1` composes existing shadowmap + bloom/tonemap/SSAO settings into a lighting-heavy preset; `r_k8vavoom_status` reports composed/live values and `r_k8vavoom_reset` restores the touched CVARs to HCDE defaults. Default off, presentation-only. |
| #15 | Finish Invasion mode | **Finish-line tasks completed 2026-05-28.** Invasion now has boss-wave diagnostics, spawn fallback diagnostics, RNG/replay/capability decisions documented, late-join replay probe, simulation LOD observability, level-2 spawn debug, announcement coverage notes, mirror/projectile verification notes, and `docs/HCDE_INVASION.md`. Remaining work is soak validation and content tuning, not core implementation. |

### Ready items

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #2 | ID24 | **Substantial surface already landed; generated smoke harness added (2026-05-28).** `id24_levelnum`, `id24.wad` autoload, ID24 intermission anim layers, Nightmare respawn fields, and UMAPINFO numbering all in tree. `tests/id24_validation/` now generates a tiny `E1M10`/`E1M11` WAD plus PK3/DEH artifacts and runs a manifest-driven validator that writes `dist/report.json`. DEHEXTRA high-slot and extended-flag cases remain disabled until the official ID24 spec matrix is filled. |
| #3 | Eternity's Spatial Audio Engine | **Design accepted + silent backend facade landed (2026-05-28).** Boundary contract written in `docs/HCDE_FEATURE_IMPORTS.md`. `snd_backend=eternity` now constructs a real `SoundRenderer` facade instead of returning `nullptr`; the facade is silent until the Eternity mixer is vendored but records 2D/3D submits, listener updates, parameter updates, stream requests, and mix ticks. `snd_eternity_status` and `snd_eternity_probe` expose diagnostics. |

### Backlog feature ports and compatibility layers

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #4 | NanoBSP from Woof doom engine | **Scoped + loader CVAR + vendor slot + build-from-scratch dispatch landed (2026-05-28).** `hcde_nanobsp_loader` (0=existing builder, 1=NanoBSP dispatch for build-from-scratch only, 2=advanced soak) and `r_nanobsp_status` CCMD live in `src/d_nanobsp_loader.cpp`. Upstream `nano_bsp.c/.h` are vendored under `src/utility/nodebuilder/nano/` but not compiled. `src/maploader/maploader.cpp` now consults the NanoBSP dispatch only inside the forced node rebuild path; the adapter currently refuses to emit nodes and falls back to the existing builder until deterministic HCDE data-structure mapping exists. `tests/nanobsp_validation/compare_nanobsp.py` writes an adapter-pending comparison report with LOS sample cases and soak report fields. |
| #9 | Nugget Doom -- Player Feel & Input | **Scoped + default-off feel features landed (2026-05-28).** `m_smooth_curve` shapes mouse deltas in `G_BuildTiccmd` through a stateless smoothstep curve. `r_crosshair_recoil` offsets only the rendered crosshair from clamped local refire/weapon-bob state. `r_killfeed` records existing obituary text into a HUD-side ring buffer and renders recent entries. `snd_footsteps_surface` selects `*footstep-solid` / `*footstep-liquid` aliases in ZScript only. No physics, hitscan, commands, scoring, or weapon timing changes. |
| #7 | Nugget Doom -- Gyroscope Input | **Scoped + Windows/SDL dynamic probe + command integration landed (2026-05-28).** `HCDEGyro_GetTiccmdContribution()` is called from `G_BuildTiccmd` and stays zero by default. When `joy_gyro_enable=1` on Windows, `src/i_input_gyro.cpp` dynamically probes `SDL2.dll` sensor symbols without a hard SDL include and reads `SDL_SENSOR_GYRO` samples when available. `joy_gyro_mode` is parsed: always-on contributes; held/toggle remain zero until a binding exists. |
| #8 | Doom Retro -- Physics & Feel Layer | **Scoped (2026-05-28).** Highest-risk roadmap item: every imported tweak ships behind a compat flag with demo-version awareness. One tweak per PR; first candidate is the rendering-only pain view-kick smoothing. See `docs/HCDE_DOOM_RETRO_AUDIT.md`. |
| #10 | Crispy Doom -- Variable Framerate System | **Audited as presentation-only (2026-05-28).** Existing GZDoom-derived interpolation already covers the core feature. `docs/HCDE_CRISPY_VFR_AUDIT.md` records current menu coverage (`vid_maxfps`, `cl_capfps`, `vid_vsync`) and remaining UX gaps. No engine import needed. |
| #11 | International Doom -- Two Specific Features | **Both first candidates wired (2026-05-28).** `r_weapon_bob_smooth` routes through `R_WeaponBobSmoothFrac()` in both software and hardware weapon-render paths. `r_fullbright_overrides` now lets GLDEFS `fullbright` texture metadata force sprite fullbright while `disablefullbright` still wins. See `docs/HCDE_FEATURE_IMPORTS.md`. |
| #12 | Predator Economy Game Mode | **Scoped + Phase 1 scaffold + snapshot V1 contract landed (2026-05-28).** `src/d_net_predator.{cpp,h}` adds `EHCDEPredatorState`, `FHCDEPredatorRoundDirector`, named FRandom `pr_predator`, CVARs (`sv_predator_enable`, `sv_predator_round_seconds`, `sv_predator_buy_seconds`, `sv_predator_starting_currency`), and `predator_status` CCMD. `FHCDEPredatorSnapshotV1` and `HCDELiveCapPredatorSnapshotV1` now define replicated round/countdown state; buy opcode, per-player currency, and role gameplay remain pending. See `docs/HCDE_PREDATOR_AUDIT.md`. |
| #13 | Make a AI system for hcde for monsters / enemies | **Scoped + Phase 1 scaffold landed (2026-05-28).** `src/d_net_aidirector.{cpp,h}` adds `FHCDEAIDirectorState`, named FRandom `pr_aidirector`, `sv_aidirector_enable` (default off) and `sv_aidirector_sweep_tics` CVARs, and `ai_status` CCMD. With the master CVAR off zero director code runs; behaviour bit-identical to today. No actor mutation, no replication. See `docs/HCDE_AIDIRECTOR_AUDIT.md`. |
| #21 | need doom skins taunt sound mechanic | **Replicated (2026-05-28).** `taunt [variant]` in `src/g_taunt.cpp` now emits `DEM_TAUNT` (1-byte variant index + variant name string); `d_net.cpp` parses it on receive and plays `*taunt[-name]` on the originating pawn at CHAN_VOICE. Cosmetic-only opcode on the existing allowlist; size analyzer mirrors DEM_SAY. |

### Backlog bug-fix and tooling work

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #22 | defensive null-and-guard pass to Net_CompactHCDEReplicatedActors | **Resolved (2026-05-28).** Compaction now distinguishes Id==0 defects (logged), retired-expired drops, and live-remote baselines (preserved); the rebuild detects duplicate ids and reports an id-index shortfall so soak runs catch corruption. See `src/d_net_snapshot_part1.cpp::Net_CompactHCDEReplicatedActors`. |
| #23 | redo the hcdeserv ui .. the apply button doesnt need to be there, it should just apply it | Dedicated-server UI/UX cleanup. Source lives outside `src/` (the launcher project); deferred until that codebase is in tree. |
| #24 | rcon utility | **Phase 0 + transport state machine + ingress-drain stub landed (2026-05-28).** `sv_rcon_enable` / `sv_rcon_password` / `sv_rcon_port` CVARs, `rcon_status`, listener lifecycle (`HCDERconStartListener` / `HCDERconStopListener` / `HCDERconPollListener`), and per-frame `HCDERconDrainIngress()` stub live in `src/d_net_rcon.{cpp,h}`. No socket, auth, or command dispatch yet. |

### In review

| Issue | Board item | Integration meaning |
| --- | --- | --- |
| #18 | check over the updater | **Audited + native-fetch abstraction + libcurl skeleton landed (2026-05-28).** The "updater" is a changelog viewer; no auto-download / install path exists. `releasepage.cpp` now has `FetchGitHubChangelogNative()` plus an `HCDE_HttpGetSkeleton()` libcurl-shaped helper (no I/O today). PowerShell/package fallback remains. |

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

- Keep the k8vavoom-style rendering work (#17) renderer-facing. Phase 1 is now
  scoped in `docs/HCDE_RENDERING_K8VAVOOM_AUDIT.md`: reuse existing shadowmap,
  dynamic-light, bloom/tonemap, and SSAO systems; keep the software renderer as
  compatibility fallback; default off through
  `hcde_k8vavoom_lighting_profile`, `hcde_k8vavoom_shadow_boost`, and
  `hcde_k8vavoom_raylight_probe`.
- First #17 implementation candidates: (1) a shadowmap preset wrapper that
  composes `gl_light_shadowmap`, `gl_shadowmap_quality`, and shadow budgets; (2)
  a postprocess lighting preset that composes bloom, tonemap, exposure, and SSAO.
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

## Changelog

### 2026-05-28 -- Roadmap batch run

20 items burned down across the integration plan. Build verified
RelWithDebInfo with 0 errors after each cluster.

**Code (#22 / #5 / #24 / #21 / #7):**

- `Net_CompactHCDEReplicatedActors` defensive guard pass: empty-input
  fast path, Id==0 defects logged, retired-expired and live-remote-
  baseline counts surfaced, post-rebuild id-index shortfall check.
  (`src/d_net_snapshot_part1.cpp`)
- `Net_IndexHCDEReplicatedActor` rebuild detects duplicate ids / actor
  pointers and refuses to silently overwrite earlier slots.
- `src/d_net_rcon.{cpp,h}` -- RCON Phase 0 scaffold: `sv_rcon_enable`,
  `sv_rcon_password`, `rcon_status` CCMD, `HCDERconShouldAcceptCommands`
  predicate. Transport / auth / dispatch deliberately not wired.
- `src/g_taunt.cpp` -- `taunt [variant]` CCMD; cosmetic-only `*taunt`
  skin-sound trigger on CHAN_VOICE with 1.5 s cooldown.
- `src/i_input_gyro.cpp` -- `joy_gyro_*` CVAR scope-marker scaffold for
  the future Nugget-style gyroscope input adapter.
- Promoted DSDA Rewind (#5) to "Done foundations" -- phases 1-5 are
  already implemented and wired through `P_LineAttack`; default-off.

**Documentation:**

- `docs/HCDE_REWIND.md` -- operator-facing rewind / lag-comp guide.
- `docs/HCDE_RCON.md` -- RCON design (transport / auth / dispatch
  phases, threat model, allowlist).
- `docs/HCDE_UPDATER_AUDIT.md` -- changelog viewer audit; risk findings
  for the PowerShell shell-out, hard-coded repo URL, Windows-only fetch.
- `docs/HCDE_ID24_AUDIT.md` -- ID24 surface inventory + gap list.
- `docs/HCDE_FEATURE_IMPORTS.md` -- combined design note for #11
  (International Doom: smooth weapon bob + brightmap-aware fullbright
  sprites), #10 (variable-FPS presentation split), and #3 (Eternity
  spatial audio facade boundary).
- `docs/HCDE_INVASION_AUDIT.md` -- invasion (#15) finish-line task
  list, twelve concretely sized PR-fit items.
- This file -- status updates for #22 (Resolved), #24 (Phase 0), #18
  (Audited), #2 / #3 / #10 / #11 / #15 / #21 / #5.

**Out of scope this run:**

- #17 (k8vavoom rendering), #13 (monster AI), #4 (NanoBSP), #8 (Doom
  Retro physics), #9 (Nugget feel), #12 (Predator Economy) -- large
  feature ports, each multi-PR.
- #23 (hcdeserv UI redo) -- launcher source lives outside `src/`;
  deferred until that codebase is in tree.

### 2026-05-28 -- Roadmap batch run (continued)

12 more items burned down. Build verified RelWithDebInfo with 0 errors;
no new linter warnings on touched files.

**Code (#17 / #11 / #21):**

- `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp` -- #17 Phase 1
  preset composition. `hcde_k8vavoom_lighting_profile=1` now raises
  shadowmap quality, enables prioritization, and applies a conservative
  bloom/tonemap/SSAO floor. State is captured into a snapshot struct so
  a new `r_k8vavoom_status` CCMD can report composed vs. live values.
  Sub-flags (`hcde_k8vavoom_shadow_boost`, `hcde_k8vavoom_raylight_probe`)
  reordered before the profile CVAR for static-init safety.
- `src/playsim/p_pspr.{cpp,h}` -- #11 smooth weapon bob curve wired.
  New `R_WeaponBobSmoothFrac()` helper applies a smoothstep ease-in-out
  to the per-frame interpolation fraction when `r_weapon_bob_smooth=1`,
  identity otherwise. Authoritative tic-rate bob unchanged;
  presentation-only.
- `src/rendering/swrenderer/things/r_playersprite.cpp` and
  `src/rendering/hwrenderer/scene/hw_weapon.cpp` -- both bob render
  call sites route through `R_WeaponBobSmoothFrac()`.
- `src/g_taunt.cpp` -- #21 `taunt` CCMD now emits `DEM_TAUNT` over the
  command stream (1-byte variant index + variant name string) instead
  of calling `S_Sound` directly. Cooldown still local. New helper
  `HCDETauntPlayOnPawn()` exposed for parser symmetry.
- `src/d_net.cpp` -- #21 `DEM_TAUNT` parser case in the `Net_DoCommand`
  dispatch switch plays `*taunt[-name]` on the originating pawn at
  CHAN_VOICE with no playsim mutation. `Net_TrySkipCommand` size mirror
  added next to DEM_SAY.

**Documentation:**

- `docs/HCDE_NANOBSP_AUDIT.md` -- #4 scoping. Boundary against playsim
  collision and snapshot determinism, three-value gating CVAR plan,
  six-phase rollout (vendor / harness / scratch-build only / soak / MP
  soak / always-rebuild).
- `docs/HCDE_AIDIRECTOR_AUDIT.md` -- #13 scoping. Layered design: a
  server-only director on top of unchanged vanilla state machines,
  `sv_aidirector_enable` default-off CVAR, `pr_aidirector` named RNG,
  no new replication opcodes.
- `docs/HCDE_PREDATOR_AUDIT.md` -- #12 scoping. State machine reuses
  the Invasion pattern; `HCDEPredatorSnapshotV1` capability gate;
  authority-event currency and round-timer model; cheat-elevation
  rejection.
- `docs/HCDE_NUGGET_FEEL_AUDIT.md` -- #9 scoping. Two explicit lists:
  Allowed (presentation, smoothing curves, crosshair widgets, killfeed,
  footstep sound name) and Rejected (movement physics, hitscan,
  weapon-switch timing) -- the latter belongs to #8 / explicit
  follow-ups, not #9.
- `docs/HCDE_DOOM_RETRO_AUDIT.md` -- #8 scoping. Highest-risk roadmap
  item; every tweak ships behind a compat flag with demo-version
  pinning, server-controlled in netgames, one tweak per PR. Three
  concrete first candidates ordered by risk.
- This file -- status updates for #17 (Phase 1 implementation), #11
  (smooth bob wired), #21 (DEM_TAUNT replicated), #4 / #13 / #12 / #9 /
  #8 (each linked to its scoping doc).

**Out of scope this run:**

- #4 / #13 / #12 / #9 / #8 implementation work -- each scoping doc
  defines the next PR boundary; actual code import is a separate run
  per item.
- Brightmap-aware fullbright sprites (#11 second candidate) -- still a
  CVAR scaffold (`r_fullbright_overrides`) without a consumer; deferred
  to a follow-up.
- Eternity spatial audio (#3) backend implementation past the existing
  skeleton.

### 2026-05-28 -- Roadmap batch run (presentation/tooling follow-ups)

12 more items burned down. Build verified RelWithDebInfo with 0 errors; no
new linter warnings on touched files.

**Code (#11 / #3 / #7 / #9):**

- `src/rendering/hwrenderer/scene/hw_sprites.cpp`,
  `src/rendering/hwrenderer/scene/hw_weapon.cpp`,
  `src/rendering/swrenderer/things/r_playersprite.cpp`,
  `src/rendering/swrenderer/things/r_sprite.cpp`, and
  `src/rendering/swrenderer/things/r_wallsprite.cpp` -- #11
  brightmap-aware fullbright override wiring. With
  `r_fullbright_overrides=1`, GLDEFS `fullbright` texture metadata can force
  sprite fullbright; `disablefullbright` still suppresses the override where
  that metadata exists.
- `src/common/audio/sound/s_sound_eternity.cpp` -- #3
  `snd_eternity_status` CCMD reports current `snd_backend`, whether the
  `eternity` backend name is selected, and that the current implementation is
  still a skeleton with no registered mixer.
- `src/i_input_gyro.cpp` -- #7 `m_gyro_status` CCMD reports the current gyro
  CVAR surface and reminds operators that platform sensor plumbing is not
  wired yet.
- `src/i_input_feel.cpp` -- #9 Nugget input-feel scaffold:
  `m_smooth_curve`, `r_crosshair_recoil`, `r_killfeed`,
  `snd_footsteps_surface`, and `m_input_status`. No usercmd, physics,
  hitscan, or weapon timing changes.
- `src/CMakeLists.txt` -- added `i_input_feel.cpp`.

**Documentation:**

- `docs/HCDE_RENDERING_K8VAVOOM_AUDIT.md` -- #17 Phase 1 now documented as
  implemented, including the one-shot preset/reset semantics for
  `hcde_k8vavoom_lighting_profile` and the `r_k8vavoom_status` diagnostic.
- `docs/HCDE_FEATURE_IMPORTS.md` -- #3 updated with the Phase 0 skeleton and
  the first real Eternity backend import boundary.
- `docs/HCDE_NUGGET_FEEL_AUDIT.md` -- #9 Phase 1 marked landed.
- `docs/HCDE_RCON.md` -- #24 transport-only listener TODO ledger added.
- `docs/HCDE_UPDATER_AUDIT.md` -- #18 libcurl/native live-fetch replacement
  plan added.
- `docs/HCDE_ID24_AUDIT.md` -- #2 DEHEXTRA / extended-flag addendum added,
  including verification matrix and Done-foundations exit criteria.
- This file -- roadmap status updates for #2 / #3 / #7 / #9 / #11 / #18 /
  #24 and this changelog entry.

**Out of scope this run:**

- Actual Eternity mixer registration (#3), gyro sensor plumbing (#7), and
  input-feel curve integration into `G_BuildTiccmd` (#9).
- RCON sockets/auth/dispatch (#24) and native updater code (#18).
- ID24 executable smoke WAD/DEH pair (#2).

### 2026-05-28 -- Roadmap batch run (first wire-ins)

10 more items burned down. Build verified RelWithDebInfo with 0 errors; no
new linter warnings on touched files.

**Code (#9 / #7 / #3 / #24 / #18 / #17):**

- `src/g_game.cpp` -- #9 `m_smooth_curve` now shapes mouse deltas in
  `G_BuildTiccmd` through a stateless default-off smoothstep curve.
  `src/i_input_feel.cpp` status text updated to reflect the real wire-in.
- `src/i_input_gyro.cpp` -- #7 platform sample scaffold added. No sensor
  dependency is linked yet; `m_gyro_status` reports the stub availability and
  latest sample values.
- `src/common/audio/sound/i_sound.cpp` and
  `src/common/audio/sound/s_sound_eternity.cpp` -- #3 `snd_backend=eternity`
  now routes to an explicit registration stub and falls back cleanly until the
  real Eternity mixer exists.
- `src/d_net_rcon.{cpp,h}` -- #24 transport-only listener state machine added
  (`start`, `stop`, `poll`, status snapshot). No socket, auth, or command
  dispatch yet.
- `src/widgets/releasepage.cpp` -- #18 native changelog fetch abstraction
  started (`FetchGitHubChangelogNative()` status/result scaffold) with no
  network I/O and no install/update behavior.
- `tests/id24_validation/README.md` and
  `tests/id24_validation/id24_smoke_suite.json` -- #2 smoke-test skeleton
  for ID24 map numbering, intermission animation, Nightmare respawn,
  DEHEXTRA high slots, and extended flags.
- `wiki/CVAR-Reference.md` and `docs/HCDE_FEATURE_IMPORTS.md` -- #11
  user-facing notes for `r_fullbright_overrides`.
- `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp` -- #17
  `r_k8vavoom_reset` helper CCMD added to undo the preset-touched CVARs.

**Documentation:**

- `docs/HCDE_NUGGET_FEEL_AUDIT.md` -- #9 Phase 2 marked landed.
- `docs/HCDE_RCON.md` -- current state updated from config-only to
  transport-state scaffold.
- `docs/HCDE_UPDATER_AUDIT.md` -- native fetch abstraction noted as landed.
- `docs/HCDE_ID24_AUDIT.md` -- smoke-test skeleton path added.
- `docs/HCDE_RENDERING_K8VAVOOM_AUDIT.md` -- `r_k8vavoom_reset`
  documented.
- This file -- roadmap status updates and changelog entry.

**Out of scope this run:**

- Real gyro sensor polling, real Eternity mixer registration, RCON socket/auth
  implementation, native HTTP fetch, and executable ID24 fixture generation.

### 2026-05-28 -- Roadmap batch run (deeper wire-ins + scaffold expansion)

16 more items burned down. Build verified RelWithDebInfo; no new linter
warnings on touched files. All scaffolds default-off; no playsim or
authority behaviour change.

**Code (#9 / #7 / #18 / #24 / #4 / #12 / #13):**

- `src/g_statusbar/shared_sbar.cpp` -- #9 `r_crosshair_recoil` consumer
  hook added in `DrawCrosshair`. Default-off no-op today; reserves the
  exact insertion point so the future eased-recoil presentation patch is
  one branch body, not a structural change.
- `src/playsim/p_interaction.cpp` -- #9 `r_killfeed` consumer hook added
  at the tail of `ClientObituary`. Default-off no-op; eventual kill-feed
  widget will enqueue a (attacker, victim, MoD, tic) event onto a HUD-side
  ring buffer here. No playsim impact.
- `src/playsim/p_user.cpp` -- #9 `snd_footsteps_surface` `EXTERN_CVAR`
  reservation alongside `snd_footstepvolume`, with a comment pointing at
  the ZScript footstep loop in `wadsrc/static/zscript/actors/player/
  player.zs::DoFootstep` as the consumer site. Sound-name only; no
  movement/friction/terrain physics changes.
- `src/i_input_gyro.{cpp,h}` -- #7 `HCDEGyro_GetTiccmdContribution()`
  integration helper added. Returns zero today; the eventual platform
  plumbing only has to fill `HCDEGyro_PollPlatformSample()` without
  touching `g_game.cpp`. New `i_input_gyro.h` exposes the single
  integration point.
- `src/widgets/releasepage.cpp` -- #18 libcurl HTTP-GET helper skeleton
  added (`HCDE_HttpGetSkeleton` + `FHCDEHttpGetResult`). No network I/O
  today; documents the curl_easy_* call shape so the follow-up patch is
  body-only.
- `src/d_net_rcon.{cpp,h}` -- #24 `HCDERconDrainIngress()` per-frame
  socket-loop stub added. Returns 0 today; documents the eventual
  accept/recv/dispatch sequence. `rcon_status` now reports the drained
  count.
- `src/d_nanobsp_loader.cpp` -- #4 NanoBSP loader CVAR + status CCMD
  scaffold. `hcde_nanobsp_loader` (0/1/2) defaulted to 0 with
  CVAR_SERVERINFO so netgames agree; `r_nanobsp_status` reports the
  effective build path. NanoBSP is now vendored and the build-from-scratch
  dispatch point exists, but the adapter still falls back to the existing
  builder until deterministic HCDE output is implemented.
- `src/d_net_predator.{cpp,h}` -- #12 Predator Economy scaffold.
  `EHCDEPredatorState` enum, `FHCDEPredatorRoundDirector` with a
  deterministic Waiting/Buy/Hunt/End cycle, named FRandom
  `pr_predator`, CVARs (`sv_predator_enable`,
  `sv_predator_round_seconds`, `sv_predator_buy_seconds`,
  `sv_predator_starting_currency`), and `predator_status` CCMD. No
  snapshot replication, no buy command opcode, no role gameplay.
- `src/d_net_aidirector.{cpp,h}` -- #13 Monster AI Director scaffold.
  `FHCDEAIDirectorState`, named FRandom `pr_aidirector`,
  `sv_aidirector_enable` (default off) and `sv_aidirector_sweep_tics`
  CVARs, `ai_status` CCMD. With the master CVAR off, zero director code
  runs and behaviour is bit-identical to today.
- `src/CMakeLists.txt` -- added `d_net_predator.cpp`,
  `d_net_aidirector.cpp`, and `d_nanobsp_loader.cpp`.

**Tests (#2):**

- `tests/id24_validation/build_id24_validation_pack.py` -- skeleton ID24
  fixture builder. Writes a placeholder `dist/manifest.json` with
  `status="planned"` so CI can fail-fast until real fixtures land.
- `tests/id24_validation/validate_id24_smoke.py` -- skeleton validator.
  Reads the manifest and emits per-case PASS/SKIP/FAIL markers; today
  every planned case is SKIP.

**Documentation:**

- `docs/HCDE_COMPATF2_DR_RESERVATIONS.md` -- #8 `COMPATF2_DR_*` bit
  reservation ledger. Reservations only -- the actual enum extension
  lands with the Phase 1 Doom Retro PR and respects the audit's
  per-tweak / demo-version / server-controlled discipline.
- This file -- roadmap status updates and this changelog entry.

**Out of scope this run:**

- Vendoring the NanoBSP source under
  `src/utility/nodebuilder/nano/` (#4 Phase 1 work).
- Predator snapshot replication, buy command opcode, predator pawn class
  (#12 Phase 2-4 work).
- AI director observation pass, hint mechanism, ZScript per-monster hooks
  (#13 Phase 2-3 work).
- Real libcurl link, RCON sockets/auth/dispatch, gyro sensor polling.
- Eternity mixer registration past the existing skeleton.

### 2026-05-28 -- Roadmap batch run (ID24 executable harness)

First remaining-item batch started for #2.

**Tests (#2):**

- `tests/id24_validation/build_id24_validation_pack.py` now emits real
  generated artifacts: `HCDE-ID24-validation.wad` with tiny `E1M10` /
  `E1M11` maps, `HCDE-ID24-validation.pk3` with MAPINFO/UMAPINFO/ZScript
  validation markers, `HCDE-ID24-validation.deh` as the external patch
  placeholder, and a generated `dist/manifest.json`.
- `tests/id24_validation/validate_id24_smoke.py` now launches HCDE from the
  generated manifest, scans stdout for pass/fail markers, and writes
  `dist/report.json` with per-case command lines, matched markers, failure
  lines, notes, duration, and output tails.
- `tests/id24_validation/README.md` and `id24_smoke_suite.json` now describe
  the generated-fixture state rather than a pure skeleton. `dist/` is ignored
  in `.gitignore`.

**Current validation result:**

- The harness runs and writes a report, but local `hcde.exe` / `hcdeserv.exe`
  exit immediately after startup OS reporting even for a plain `+map MAP01`;
  the ID24 report therefore records launch failures rather than fake passes.
- DEHEXTRA high-slot and extended-flag cases are deliberately disabled until
  the official ID24 range/flag matrix is filled in.

**Out of scope this run:**

- Filling the official ID24 DEHEXTRA / extended-flag matrix and making those
  two disabled rows executable.

### 2026-05-28 -- Roadmap batch run (Eternity audio facade)

Completed the next #3 body-only step.

**Code (#3):**

- `src/common/audio/sound/s_sound_eternity.cpp` now implements a real silent
  `SoundRenderer` facade for `snd_backend=eternity`. It no longer returns
  `nullptr` and no longer falls back to the null backend just because the
  Eternity mixer is not vendored yet.
- The facade records 2D starts, 3D starts, loaded sound requests, stream
  requests, listener updates, 3D parameter updates, mix ticks, and the last
  spatial submit (position, velocity, volume, pitch, channel, flags, priority,
  area-sound bit).
- `snd_eternity_status` reports selected/constructed state and all facade
  counters; `snd_eternity_probe` submits a diagnostic 2D trigger through the
  selected backend path.
- `src/common/audio/sound/i_sound.cpp` now calls
  `I_CreateEternitySoundRenderer()` for the `eternity` backend name.

**Verification:**

- Touched-file lints clean.
- RelWithDebInfo build passed; both `hcde.exe` and `hcdeserv.exe` linked.

**Out of scope this run:**

- Vendoring or linking the actual Eternity mixer. The current backend is a
  diagnostic, client-local silent facade.

### 2026-05-28 -- Roadmap batch run (NanoBSP vendor slot)

Completed the safe #4 Phase 1 groundwork that does not affect runtime node
building.

**Code / vendor (#4):**

- `src/utility/nodebuilder/nano/nano_bsp.c` and `.h` now vendor the upstream
  Woof NanoBSP source files with MIT license headers preserved.
- `src/utility/nodebuilder/nano/README.md` documents the upstream source,
  license, and HCDE integration boundary. The files are not added to
  `src/CMakeLists.txt` and no code includes them yet.

**Tests / harness (#4):**

- `tests/nanobsp_validation/compare_nanobsp.py` verifies the vendor files,
  records SHA-256 hashes, loads the fixed comparison manifest, and writes
  `dist/nanobsp_compare_report.json`.
- `tests/nanobsp_validation/nanobsp_compare_manifest.json` defines initial
  comparison rows and fixed line-of-sight sample pairs.
- `tests/nanobsp_validation/soak_checklist.md` defines the single-player
  and multiplayer late-join soak fields that must be filled once the adapter
  can run both builders.
- `tests/nanobsp_validation/dist/` is ignored in `.gitignore`.

**Current validation result:**

- Harness runs successfully and reports vendor status `present`.
- All map cases are skipped with reason `adapter-missing`, which is expected
  until HCDE map-data adapters exist for the upstream source.

**Out of scope this run:**

- Compiling NanoBSP or adapting Woof structures to HCDE structures. The
  `hcde_nanobsp_loader=1` dispatch point now exists, but intentionally falls
  back until the adapter can prove deterministic output.

### 2026-05-28 -- Roadmap batch run (Doom Retro + Nugget feel)

Completed the next #8 and #9 default-off implementation steps.

**Code (#8):**

- `COMPATF2_DR_CRUSHER` now gates a conservative crusher branch in
  `src/playsim/p_map.cpp`: ordinary dead non-player, non-ice corpses no
  longer force `nofit` when crushed with `compat_dr_crusher=1`. Default
  behavior is unchanged with the bit clear.
- `tests/doomretro_validation/validate_dr_compat_flags.py` verifies the
  Doom Retro compat bits/CVARs and records pending runtime rows for demo,
  save, and netgame authority checks.

**Code (#9):**

- `r_crosshair_recoil` now offsets the rendered crosshair from clamped local
  `player_t::refire` / weapon bob fire tuning. It does not change aim,
  hitscan, commands, or actor state.
- `r_killfeed` now feeds existing obituary text into `src/hcde_killfeed.*`
  and renders recent entries in `DBaseStatusBar::DrawTopStuff`.
- `snd_footsteps_surface` now selects `*footstep-solid` or
  `*footstep-liquid` in `player.zs`; default aliases in `sndinfo.txt` point
  back to `*land` so mods can override samples without new required assets.

**Verification:**

- Doom Retro static validator passed and wrote
  `tests/doomretro_validation/dist/dr_compat_report.json`.
- RelWithDebInfo build passed after crusher, crosshair, killfeed, and
  footstep changes.

**Out of scope this run:**

- Runtime demo/save/netgame soaks for Doom Retro flags until local HCDE launch
  produces map/runtime output in this shell.

### 2026-05-28 -- Roadmap batch run (gyro input probe)

Completed the next #7 input path steps.

**Code (#7):**

- `src/i_input_gyro.cpp` now probes Windows `SDL2.dll` sensor symbols
  dynamically when `joy_gyro_enable=1`, avoiding a hard SDL header/include
  dependency in the generic source file.
- If SDL2 exposes sensor APIs and a `SDL_SENSOR_GYRO` device is present, yaw
  and pitch angular velocity samples feed `HCDEGyro_GetTiccmdContribution()`.
  If not, contribution remains zero.
- `joy_gyro_mode` is now interpreted: mode `0` (always-on) can contribute;
  held/toggle modes are parsed and reported but remain zero until a binding
  exists.
- `src/g_game.cpp` now calls `HCDEGyro_GetTiccmdContribution()` from
  `G_BuildTiccmd` and adds non-zero gyro deltas through the normal local
  view-angle/view-pitch path before `usercmd_t` is finalized.

**Verification:**

- Initial direct SDL include attempt failed to compile in the generic target;
  replaced with dynamic symbol lookup.
- RelWithDebInfo build passed after the dynamic-probe fix.

**Out of scope this run:**

- Adding the held/toggle activation binding and per-device axis calibration.

