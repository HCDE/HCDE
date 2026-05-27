# HCDE Presentation Surface Status

This note is the Step 4 execution checkpoint for the roadmap integration plan.
Rendering, timing, and presentation work must remain outside authoritative
fixed-tic gameplay unless a later phase explicitly routes the behavior through
the canonical HCDE runtime.

## Current surfaces

- **Dynamic lighting and shadow maps**: hardware-renderer shadow-map controls
  already exist (`gl_light_shadowmap`, `gl_shadowmap_quality`,
  `gl_shadowmap_filter`, `gl_shadowmap_maxlights`, `gl_shadowmap_prioritize`).
  HCDE-specific safety knobs also exist (`hcde_shadow_autofallback`,
  `hcde_shadow_autobudget`).
- **Actor sprite shadows**: `r_actorspriteshadow`,
  `r_actorspriteshadowstyle`, `r_actorspriteshadowdist`, and related alpha/fade
  controls are presentation-side actor rendering surfaces.
- **Dynamic lights**: `r_dynlights`, `gl_lights`, and the light actor data in
  the PK3 resources are renderer-facing surfaces.
- **Variable framerate / interpolation**: `cl_capfps`, `vid_vsync`,
  `vid_maxfps`, and `r_ticstability` affect frame pacing and interpolation.
  They must not change the fixed-tic simulation rate.

## Runtime smoke check

Use the console command:

```text
hcde_presentation_surfaces
```

The command reports the current render/timing knobs and prints the fixed
`TICRATE` boundary. It is intentionally diagnostic-only: it does not change
renderer state or gameplay state.

## Step 4 rules

- Keep k8vavoom-style lighting/shadow work behind renderer-facing controls.
- Treat NanoBSP/Woof-style BSP work as infrastructure until collision and
  gameplay-facing map state boundaries are explicitly validated.
- Keep Crispy-style variable framerate behavior presentation-side. The
  authoritative playsim remains fixed-tic.
- If a rendering feature needs gameplay data, read from the canonical runtime;
  do not introduce parallel simulation state for rendering convenience.
