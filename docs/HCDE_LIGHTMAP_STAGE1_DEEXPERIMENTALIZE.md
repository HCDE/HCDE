# HCDE Lightmap Stage 1 - Default-On Lightmap Loading

Date: 2026-05-19

Stage 1 removes the old experimental gate around baked `LIGHTMAP` lump loading
so lightmaps are now enabled by default.

## What Changed

- Removed the `-enablelightmaps` requirement in map loading.
- Added an explicit opt-out switch: `-nolightmaps`.
- Removed the startup warning that labeled lightmaps as experimental.
- Removed the stale `-enablelightmaps` launcher argument from HCDE client
  singleplayer/join templates.

## Runtime Behavior

- If a map/mod does not include a `LIGHTMAP` lump, behavior is unchanged.
- If a map/mod includes a valid `LIGHTMAP` lump, HCDE loads and uses it by
  default.
- Use `-nolightmaps` to disable baked lightmaps for compatibility triage.

## Files Updated

- `HCDE/src/maploader/maploader.cpp`
- `avalonia/DoomConnector.Avalonia.Shared/Engines/HCDE.json`

