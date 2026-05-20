# HCDE Lighting Stage 5 - Quake-Style Profile

Date: 2026-05-19

Stage 5 formalizes a Quake-style low-cost grounding profile on top of the
existing actor sprite-shadow projection path.

## What Changed

- Added explicit preset command:
  - `hcde_quakeshadows`
- Kept the existing Stage 3 profile slot:
  - `hcde_shadowprofile 6`

## Preset Behavior

The Quake-style profile favors projected actor grounding over dynamic-light
shadowmaps:

- `gl_light_shadowmap = false`
- `gl_shadowmap_maxlights = 0`
- `r_actorspriteshadow = 2`
- `r_actorspriteshadowstyle = 1`
- `r_actorspriteshadowalpha = 0.70`
- `r_actorspriteshadowfadeheight = 0`

This remains clean-room and uses HCDE's existing renderer systems.

## Stage Interop

- Stage 2 capability fallback is still active for maps/profiles that use
  dynamic light shadowmaps.
- Stage 3 map rules can force this profile per map with:
  - `hcde_shadowprofile = 6`
- Stage 4 adaptive budget is effectively bypassed for this profile because
  light shadowmaps are disabled.

## Files Updated

- `HCDE/src/menu/doommenu.cpp`

