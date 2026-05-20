# HCDE Lighting Stage 3 - Map-Level Shadow Rules

Date: 2026-05-19

Stage 3 adds per-map shadow controls so a map or mod can declare shadow profile
intent and hard caps that are merged with Stage 2 hardware fallback limits.

## What Changed

- Added map-level fields on `level_info_t`:
  - `HcdeShadowProfileOverride` (`-1` unset, `0..7` profile)
  - `HcdeShadowQualityCap` (`-1` unset, normalized to 128..8192)
  - `HcdeShadowMaxLightsCap` (`-1` unset, clamped to 0..1024)
- Added new MAPINFO properties:
  - `hcde_shadowprofile = <int>`
  - `hcde_shadowqualitycap = <int>`
  - `hcde_shadowmaxlightscap = <int>`
- Updated render fallback path to:
  - apply/restore map-level profile override automatically
  - merge map quality cap with Stage 2 capability cap
  - merge map max-lights cap with Stage 2 budget cap

## MAPINFO Examples

```txt
map MAP01 "Entryway"
{
  hcde_shadowprofile = 3
  hcde_shadowqualitycap = 1024
  hcde_shadowmaxlightscap = 192
}
```

```txt
map MAP02 "No Override"
{
  hcde_shadowprofile = -1
  hcde_shadowqualitycap = -1
  hcde_shadowmaxlightscap = -1
}
```

## Merge Rules

- Profile:
  - If map profile override is set (`0..7`), it is applied while that map is active.
  - When leaving override maps, previous global profile is restored.
- Quality cap:
  - Effective cap = `min(device/backend cap, map cap)` when both exist.
  - If only map cap exists, map cap is used.
- Light budget cap:
  - Effective cap = `min(stage2 budget cap, map max-lights cap)` when both exist.
  - If only map cap exists, map cap is used.

## Files Updated

- `HCDE/src/gamedata/g_mapinfo.h`
- `HCDE/src/gamedata/g_mapinfo.cpp`
- `HCDE/src/rendering/hwrenderer/hw_entrypoint.cpp`

