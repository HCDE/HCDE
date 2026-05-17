# HCDE Shadow Renderer Stage 5 - Quake-Style Projected Shadows

Stage 5 adds a clean-room Quake-style shadow mode built on HCDE's existing actor
sprite-shadow projection path. It does not import renderer code or algorithms
from another Doom source port.

## User-facing controls

- `hcde_shadowprofile 6` applies the Quake-style preset.
- `r_actorspriteshadowstyle 0` keeps classic HCDE sprite-shadow flattening.
- `r_actorspriteshadowstyle 1` uses the harder, flatter Quake-style projection.

Stage 6 adds another style value for the later Doom 3-style profile; this stage
only defines the Quake-style value.

The Dynamic Lights menu now exposes `Sprite shadow style` with `Classic` and
`Quake-style` values, and the profile selector includes `Quake-style`.

## Preset behavior

The Quake-style profile favors low-cost actor grounding over dynamic-light
shadowmaps:

- Turns dynamic light shadowmaps off by setting `gl_light_shadowmap` false and
  `gl_shadowmap_maxlights` to `0`.
- Forces actor sprite shadows on for monsters and players with
  `r_actorspriteshadow 2`.
- Uses a flatter projection via `r_actorspriteshadowstyle 1`.
- Uses a strong hard shadow with `r_actorspriteshadowalpha 0.70` and
  `r_actorspriteshadowfadeheight 0`.

## Renderer notes

Hardware and software renderers both call the same helper functions in
`r_utility.cpp` for sprite-shadow flattening and alpha. Keeping this logic
centralized prevents the two renderers from drifting apart as more shadow
profiles are added.
