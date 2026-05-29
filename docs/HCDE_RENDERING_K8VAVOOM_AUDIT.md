# HCDE k8vavoom-Style Rendering Audit

Roadmap board item: **#17** ("Rocket wants more k8vavooom style rendering
as default with shadows and raytracing like lights").

This is the scoping document for turning that broad request into reviewable
renderer work. The target is a modernized visual profile inspired by
k8vavoom's lighting-heavy presentation, but imported as HCDE renderer features
rather than as a parallel engine.

## Current Renderer Surface

| Area | Existing HCDE surface | Notes |
| --- | --- | --- |
| Hardware scene renderer | `src/rendering/hwrenderer/scene/*` | Walls, flats, sprites, BSP traversal, portals, decals, sky, fake flats. |
| Dynamic lights | `src/common/rendering/hwrenderer/data/hw_dynlightdata.h`, `src/r_data/gldefs.cpp` | GLDEFS-driven light definitions already exist. |
| Shadow maps | `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp`, `gl_light_shadowmap`, `gl_shadowmap_quality`, `gl_shadowmap_maxlights`, `hcde_shadow_autofallback`, `hcde_shadow_autobudget` | Already has a shadowmap pipeline and HCDE-specific budget/fallback knobs. |
| Postprocess | `hw_postprocess.cpp`, `gl_bloom`, `gl_tonemap`, `gl_exposure`, `gl_ssao` | Bloom, tonemap, SSAO, lens, exposure are already in tree. |
| Vulkan / GL shader stack | `common/rendering/vulkan/*`, `common/rendering/gl/*` | Shader infrastructure exists; new visual experiments should enter here, not playsim. |
| Software renderer | `src/rendering/swrenderer/*` | Keep compatibility fallback. k8vavoom-style work targets hardware renderer only. |
| Brightmaps / fullbright | `autoloadbrightmaps`, `r_fullbright_overrides` scaffold | Presentation-only, no network relevance. |

## Import Boundaries

- **Presentation only.** Shadows, ray-style lighting approximations, bloom,
  SSAO, and fullbright overrides must not change actor state, collision,
  visibility used by AI, sound propagation, RNG, or savegame contents.
- **Renderer-facing facade.** New k8vavoom-inspired toggles live behind HCDE
  CVARs and feed existing renderer code paths. Do not fork map loading or
  playsim light definitions.
- **Hardware renderer first.** The software renderer stays stable and can
  ignore the new CVARs unless a simple no-op is needed.
- **Default-off while experimental.** New visual presets are opt-in until
  performance and compatibility are measured.
- **Demo/multiplayer deterministic.** Two clients may choose different visual
  settings while receiving the same authoritative snapshots and producing the
  same demos.

## Existing CVARs To Reuse

| CVAR | Purpose |
| --- | --- |
| `gl_light_shadowmap` | Master shadowmap switch. |
| `gl_shadowmap_quality` | Shadowmap resolution. |
| `gl_shadowmap_maxlights` | Light budget cap. |
| `gl_shadowmap_prioritize` | Prioritize lights for shadowmap budget. |
| `hcde_shadow_autofallback` | HCDE automatic fallback when shadowmap support is unsuitable. |
| `hcde_shadow_autobudget` | HCDE adaptive shadow budget. |
| `hcde_shadow_autobudget_targetms` | Target upload/pass cost for adaptive budget. |
| `gl_bloom`, `gl_bloom_amount` | Bloom. |
| `gl_tonemap`, `gl_exposure` | Tonemapping / exposure. |
| `gl_ssao`, `gl_ssao_strength`, `gl_ssao_radius` | Ambient occlusion. |

## New Scaffold

`src/common/rendering/hwrenderer/data/hw_shadowmap.cpp` now contains these
default-off roadmap CVARs and the first preset implementation:

| CVAR | Meaning |
| --- | --- |
| `hcde_k8vavoom_lighting_profile` | Master profile selector. `0` = disabled/current HCDE behaviour, `1` = experimental lighting-heavy preset. When set to `1`, this now composes existing shadowmap and postprocess CVARs. |
| `hcde_k8vavoom_shadow_boost` | Allows the preset to prefer a higher shadow quality floor when supported. |
| `hcde_k8vavoom_raylight_probe` | Placeholder for ray-style light probe experiments. Does not perform raytracing yet. |
| `r_k8vavoom_status` | Diagnostic CCMD that prints the last composed preset snapshot and the current live renderer CVAR values. |
| `r_k8vavoom_reset` | Helper CCMD that disables the profile/sub-flags and resets the touched renderer CVARs to HCDE defaults (`gl_light_shadowmap=0`, `gl_shadowmap_prioritize=1`, `gl_shadowmap_quality=512`, `gl_bloom=0`, `gl_tonemap=0`, `gl_ssao=0`). |

The profile remains default-off and presentation-only. Enabling it is a
one-shot preset apply: it raises selected CVARs to conservative floors but does
not remember or restore previous user values when the profile is disabled.
Use `r_k8vavoom_status` to inspect the active values. Use `r_k8vavoom_reset`
for the common "undo the preset" path.

## Phase 1 Implemented

1. **Shadowmap preset wrapper.** When `hcde_k8vavoom_lighting_profile=1`,
   the renderer enables `gl_light_shadowmap`, enables
   `gl_shadowmap_prioritize`, and raises `gl_shadowmap_quality` to a floor of
   `512` (`1024` with `hcde_k8vavoom_shadow_boost=1`). This composes existing
   shadow code instead of adding a new renderer path.

2. **Postprocess lighting preset.** When the profile is enabled, it enables
   `gl_bloom`, raises `gl_tonemap` from `0` to `1`, and raises `gl_ssao` from
   `0` to `1`. Existing user-selected nonzero tonemap/SSAO values are not
   lowered.

3. **Diagnostics.** `r_k8vavoom_status` prints both the last composed snapshot
   and the live CVAR values so operators can see whether later manual edits
   changed the preset output.

## Deferred / High-Risk Work

- Real raytracing or ray queries. This requires backend capability probing,
  shader work, and likely Vulkan-specific code. Track as a separate #17 phase.
- Renderer-visible light changes driven by actor AI or gameplay logic. Rejected
  unless represented as existing dynamic-light definitions.
- Any map geometry acceleration structure that touches collision or movement.
  That belongs to #4 NanoBSP, not #17 rendering.

## Raylight / Raytrace Experiment Boundary

`hcde_k8vavoom_raylight_probe` remains a probe flag only. The next acceptable
experiment is a renderer-local light visibility sample that:

- runs only in the hardware renderer after the scene has already been built
- reads draw-side light/geometry buffers but never writes playsim actors,
  sectors, lines, RNG, saves, demos, or snapshots
- is guarded by backend capability checks and silently falls back to the
  existing dynamic-light path
- has a budget counter in `r_k8vavoom_status` before any visual default changes

Out of bounds for this roadmap step: gameplay-visible ray queries, changing
AI sight/sound, replacing BSP/node traversal, or requiring Vulkan raytracing
for the default renderer.

## Smoke / Tuning Log

### 2026-05-28

- Attempted local render launch:
  `build/RelWithDebInfo/hcde.exe -iwad doom2.wad +map map01 +hcde_k8vavoom_lighting_profile 1 +wait 5 +quit`.
- The process printed startup/version information and exited before a useful
  frame sample in this environment.
- Preset tuning was therefore made conservatively: the base profile now keeps
  `gl_shadowmap_quality` at the HCDE reset/default floor (`512`), while
  `hcde_k8vavoom_shadow_boost=1` raises the floor to `1024`. Bloom, tonemap,
  and SSAO remain opt-in through the profile and still do not change defaults.

## Done Criteria For #17 Phase 1

- CVAR scaffold lands and builds.
- Shadowmap and postprocess preset composition is implemented.
- `r_k8vavoom_status` reports composed and live renderer CVAR state.
- Roadmap explicitly lists the implemented Phase 1 behavior.
- Documentation states the determinism/multiplayer rules and reset semantics.
- No default visual change until a follow-up patch intentionally enables the
  preset.
