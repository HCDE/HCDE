# HCDE Shadow Renderer Stage 4 Tuning

Date: 2026-05-17

Stage 4 adds a conservative performance budget around HCDE's existing dynamic
light shadowmap path. It does not replace the renderer, change lighting math,
or import behavior from another engine.

## What Changed

The new archived global CVAR is:

```text
gl_shadowmap_maxlights
```

It limits how many active dynamic lights can be assigned shadowmap rows for the
current frame. The renderer still keeps the original 1024-row shadowmap texture,
but only renders the active rows needed by the current budget instead of always
drawing all 1024 rows.

The `stat shadowmap` output now reports:

- upload time
- dynamic lights processed
- dynamic lights actually shadowmapped
- shadowmap rows updated

Stage 8 extends this with eligible/dropped light counts and the active priority
mode.

## Profile Defaults

| Profile | Shadowmap budget |
| --- | ---: |
| Off | 0 |
| Performance | 128 |
| Balanced | 256 |
| Enhanced | 512 |
| Cinematic | 1024 |

`Manual` does not change the budget or any other shadow CVAR.

## Menu Path

`Options -> Display Options -> Dynamic Light Options`

The menu now includes `Shadowmapped light budget` next to the existing
shadowmap quality and filter controls.

## Compatibility Boundary

- Existing per-light shadow flags are still respected.
- Lights outside the budget are treated as unshadowmapped for that frame.
- Stage 4 keeps the existing collection order. Stage 7 adds optional priority
  sorting for nearby and larger shadowmapped lights.
- Quake-style and Doom 3-style shadows remain later clean-room stages.
