# HCDE Shadow Renderer Stage 6 - Doom 3-Style Hard Shadows

Stage 6 adds a clean-room Doom 3-style shadow profile using HCDE's existing
dynamic light shadowmap and actor sprite-shadow systems. It does not import
renderer code, shadow volume code, or algorithms from Doom 3 or another source
port.

## User-facing controls

- `hcde_shadowprofile 7` applies the Doom 3-style preset.
- `r_actorspriteshadowstyle 2` selects the stronger Doom 3-style actor grounding
  projection.

The Dynamic Lights menu now exposes `Doom 3-style` in the HCDE shadow profile
selector and in the sprite shadow style selector.

## Preset behavior

The Doom 3-style profile favors crisp, high-contrast shadows:

- Enables dynamic light shadowmaps with `gl_light_shadowmap true`.
- Uses `gl_shadowmap_quality 4096`.
- Uses nearest filtering with `gl_shadowmap_filter 0` for harder edges.
- Allows the full Stage 4 budget with `gl_shadowmap_maxlights 1024`.
- Forces actor sprite shadows on for monsters and players with
  `r_actorspriteshadow 2`.
- Uses stronger actor grounding with `r_actorspriteshadowalpha 0.85` and
  `r_actorspriteshadowfadeheight 0`.

## Compatibility boundary

This is a profile and tuning stage. HCDE still uses its current shadowmap path
and projected actor shadows. Real stencil shadow volumes, full per-pixel Doom 3
lighting, and renderer replacement work remain out of scope for this stage.
