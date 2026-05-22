# HCDE Lighting Stage 6 - UDMF Dynamic-Light Key Completion

Date: 2026-05-21

Stage 6 finalizes mapper-facing UDMF support for per-light shadow behavior and
runtime light shading overrides, then validates behavior across OpenGL and
Vulkan.

## Supported UDMF Thing Keys

For dynamic-light map things (for example `PointLight`, DoomEdNum `9800`):

- `light_noshadowmap` (bool)
- `light_dontlightactors` (bool)
- `light_dontlightmap` (bool)
- `light_softshadowradius` (float)
- `light_linearity` (float)
- `light_shadowminquality` (int)

## Runtime Semantics

- Boolean keys can explicitly force `true` or `false`, overriding class defaults.
- `light_shadowminquality` accepts either:
  - tier-style values (`1..8`), mapped to texture sizes (`1->256`, `2->512`, `3->1024`, ...), or
  - direct quality values (`256`, `512`, `1024`, `2048`, `4096`, `8192`).
- Lights below their minimum required quality are excluded from shadowmap row
  assignment.

## Stage 6 Fixes

During verification, one ordering bug was found and fixed:

- UDMF per-light overrides were set at map-thing spawn time, but `PointLight`
  attaches its internal `FDynamicLight` in `PostBeginPlay`.
- Result: `light_shadowminquality` and other per-light runtime overrides could
  be missed.

Fix:

- Cache UDMF per-light overrides on `AActor`.
- Apply cached values when dynamic lights are attached in `AttachLight(...)`.
- Preserve these cached values through save/load serialization.

## Verification Matrix (Step 5/6)

Using the UDMF validation map (`LTEST01`) with 4 point lights:

- 1 baseline
- 1 `light_noshadowmap=true`
- 1 actor-only lighting override (`light_dontlightactors`)
- 1 quality-gated light (`light_shadowminquality=3`, i.e. min `1024`)

Results:

- OpenGL at `gl_shadowmap_quality=512`:
  - `processed=4`, `eligible=2`, `quality_gated=1`
- OpenGL at `gl_shadowmap_quality=1024`:
  - `processed=4`, `eligible=3`, `quality_gated=0`
- Vulkan at `gl_shadowmap_quality=512`:
  - `processed=4`, `eligible=2`, `quality_gated=1`
- Vulkan at `gl_shadowmap_quality=1024`:
  - `processed=4`, `eligible=3`, `quality_gated=0`

This confirms that `light_noshadowmap` and `light_shadowminquality` are both
active and backend-consistent.

## Files Updated

- `HCDE/src/playsim/actor.h`
- `HCDE/src/playsim/a_dynlight.cpp`
- `HCDE/src/playsim/p_mobj.cpp`
- `HCDE/src/rendering/hwrenderer/hw_entrypoint.cpp`

