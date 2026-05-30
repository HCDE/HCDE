# HCDE Doomsday Engine — Three Specific Things Audit

**Last updated:** 2026-05-30
**Status:** Phase 0-3 wired. CVAR scaffolds landed 2026-05-29; reverb regions wired to OpenAL EFX 2026-05-30; renderer implementations (FakeRadio AO) use conservative v_blend fallback.

## What #6 actually means

Roadmap board item: **#6** ("Doomsday Engine — Three Specific Things"). The user has flagged three Doomsday features that are visually/aurally distinctive enough to be worth importing as **presentation-side opt-ins**:

1. **FakeRadio lighting** — Doomsday's edge/contour shadow approximation that runs as a cheap 2D overlay around walls and steps. It is a *render-side* light-modulation pass; it does not change actor lighting, AI sight, or sector light values.
2. **Geometry-based ambient occlusion** — corner-darkening computed from sector/sidedef geometry rather than a screen-space SSAO pass. Cheaper than SSAO on lots of hardware and produces the contact-shadow look Doomsday is known for.
3. **FMOD-style reverb / audio environments** — per-sector reverb regions (verb keyword in DD's MAPINFO equivalent) so audio gets large/cathedral/tight-corridor presence without coupling to playsim.

All three are **client-local presentation** features. None of them participate in the authoritative simulation, snapshots, saves, or demos.

## Hard rules (presentation discipline)

These are non-negotiable for any #6 PR:

1. **Default off.** Every imported feature ships behind a CVAR defaulted to "off / current HCDE behaviour". Operators opt in.
2. **No actor / playsim mutation.** FakeRadio and AO read scene geometry the renderer already has; they never touch `actor.h`, `p_local.h`, sound propagation tables, or sight-test results.
3. **No replication, no save data.** Settings live in `CVAR_ARCHIVE | CVAR_GLOBALCONFIG` (per-user). Two clients with different `r_fakeradio` / `r_geom_ao` / `snd_env_reverb` settings stay in sync because the authoritative state never sees them.
4. **No demo determinism dependency.** Two clients running the same demo with different visual/audio settings must produce the same authoritative output.
5. **Hardware renderer first.** The software renderer stays stable and ignores these CVARs (or treats them as no-ops).
6. **Backend capability checks.** Any feature that needs Vulkan-only, GL-only, or FMOD-only paths probes the active backend and silently falls back to a no-op when unsupported.

## Existing in-tree surface

- **Hardware renderer scene** — `src/rendering/hwrenderer/scene/*` already produces walls/flats/sprites with per-vertex/per-fragment lighting. FakeRadio and geometry AO would slot in alongside `hw_dynlightdata.h` style data.
- **Postprocess pipeline** — `hw_postprocess.cpp` already runs SSAO/bloom/tonemap. A "geometry AO" mode is a sibling pass (or a feature flag inside the existing AO path).
- **GLDEFS** — `src/r_data/gldefs.cpp` already defines lights, glow, and brightmap rules. FakeRadio falls between dynamic lights and brightmaps in shape.
- **Audio** — `src/common/audio/sound/*` already has `S_Sound`-based listener tracking. Sector reverb regions can read sector tags or MAPINFO and feed the existing audio backend's filter API where one exists.

## CVAR surface (Phase 0)

Landing in `src/r_doomsday_features.cpp` (mirrors the layout of `src/r_view_pain_smooth.cpp` and `src/d_nanobsp_loader.cpp`):

| CVAR | Default | Meaning |
| --- | --- | --- |
| `r_fakeradio` | 0 | 0 = off (current HCDE), 1 = enable FakeRadio edge/step shadow approximation. Presentation-only. |
| `r_fakeradio_strength` | 0.5 | Strength multiplier for the FakeRadio overlay (0.0..1.0). |
| `r_geom_ao` | 0 | 0 = off (current HCDE), 1 = geometry-based corner AO. Composes with the existing `gl_ssao` pass; if both are on, geometry AO takes priority and SSAO can be reduced or disabled per future tuning. |
| `r_geom_ao_strength` | 0.4 | Strength multiplier for geometry AO. |
| `snd_env_reverb` | 1 | 1 = on (wired to OpenAL EFX), 0 = off (dry sound). Gates environmental reverb from Eternity EDF reverb blocks and sector definitions. Falls back silently to dry sound if the backend lacks EFX support. |

Diagnostic CCMD: `r_doomsday_status` reports the flags above plus backend-capability hints (whether the active sound backend exposes reverb, whether the renderer supports the AO pass, etc.).

## 2026-05-29 — first render-path fallback

`r_fakeradio` and `r_geom_ao` now feed a conservative render-side darkening
fallback in `src/rendering/2d/v_blend.cpp`. This is not the final geometry
implementation; it is the first user-visible integration point for the CVARs:

- default off
- local presentation only
- skipped while fullbright effects are active
- capped at a low opacity (`<= 0.18`) so it cannot hide gameplay
- reads only the view-blend state and existing CVARs

The future Phase 1/2 renderer work can replace this fallback with actual
FakeRadio edge/step sampling and geometry-corner AO without changing the
public CVAR names or determinism contract.

## Phased plan

- **Phase 0 (this doc).** Define the boundary. Land CVAR scaffold + diagnostic CCMD. **Landed 2026-05-29.**
- **Phase 0.5 — render fallback.** Wire `r_fakeradio` / `r_geom_ao` into the
  existing view-blend path as a conservative darkening fallback. **Landed
  2026-05-29.**
- **Phase 1 — FakeRadio.** Implement the edge/step shadow overlay in the hardware renderer. Sample sector heights / sidedef texture seams; emit a darkening multiplier in the fragment path. Default off. (Currently using v_blend fallback.)
- **Phase 2 — Geometry AO.** Add a renderer pass that computes corner darkening from BSP/sector geometry. Compose with existing `gl_ssao` (geometry AO wins where both are on). Default off. (Currently using v_blend fallback.)
- **Phase 3 — Reverb regions.** Wire `snd_env_reverb` to OpenAL EFX. Read Eternity EDF reverb definitions and EMAPINFO defaultenvironment. Map to OpenAL EFX reverb presets. **Wired 2026-05-30.**
- **Phase 4.** Soak. Verify two clients running the same demo with different visual/audio settings stay deterministic. Verify saves/demos round-trip identically with the flags either on or off.

## What this is NOT

- **Not a Doomsday port.** We are not vendoring the Doomsday renderer, audio engine, or scripting. Three specific *features* land, mapped onto HCDE's existing renderer/audio code.
- **Not a license to introduce playsim-affecting AO.** "AI sees better in lit areas" is gameplay; that belongs to #13 AI Director, not here.
- **Not a savegame format change.** All three features are client-local CVARs, no new data lands in saves or demos.

## Source map (Phase 0)

| Concern | File |
| ------- | ---- |
| CVARs + status CCMD | `src/r_doomsday_features.cpp` |
| FakeRadio/Geometry AO fallback | `src/rendering/2d/v_blend.cpp` (conservative darkening) |
| FakeRadio overlay (Phase 1) | `src/rendering/hwrenderer/scene/*` (TBD per pass) |
| Geometry AO pass (Phase 2) | `src/common/rendering/hwrenderer/postprocessing/*` (alongside SSAO) |
| Reverb regions (Phase 3) | `src/common/audio/sound/oalsound.cpp` (OpenAL EFX integration) |
| Eternity EDF reverb | `src/gamedata/hcde_edf.cpp` + `src/gamedata/hcde_emapinfo.cpp` |
| Build system | `src/CMakeLists.txt` (add `r_doomsday_features.cpp`) |

## Open questions (for the implementing PR)

1. Should FakeRadio compose with the existing dynamic-light shadow path or run as a separate pre-pass? (Composition is cheaper; pre-pass is more controllable.)
2. Geometry AO vs. existing SSAO — which wins when both are on? (Default: geometry AO wins, SSAO contribution is halved.)
3. ~~Reverb backend probing — only OpenAL EFX is widely deployable today. Should the reverb CVAR transparently no-op when the backend can't apply effects, or should it warn at startup?~~ **Answered:** `snd_env_reverb` now gates OpenAL EFX reverb. When disabled or when EFX is unavailable, the system uses the "Off" environment (dry sound). `r_doomsday_status` reports the current state.
