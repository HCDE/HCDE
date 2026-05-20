# HCDE Invasion Stage 1: Mode Identity Hardening

Stage 1 establishes `sv_gametype 4` as a native Invasion mode identity and
removes outdated "temporary coop fallback" messaging from runtime paths.

## What Stage 1 Changes

1. Removed legacy runtime message that stated Invasion was still using
   cooperative rules as a temporary fallback.
2. Replaced that path with explicit identity comments:
   - Invasion is mode `4`
   - legacy deathmatch/teamplay flags stay disabled for Invasion mode so DM/TDM
     rule gates are not accidentally applied.
3. Updated cutscene countdown path comments to reflect current behavior:
   Invasion uses its own pacing cvar (`sv_invasioncountdowntime`) as a native
   mode path, not a temporary bridge.

## Files Updated

- `src/g_game.cpp`
- `src/d_net.cpp`

## Stage 1 Boundary

Stage 1 is identity hardening only. It does not change wave director tuning,
spawn spot logic, or victory/failure semantics. Those remain in later stages.

## Validation Notes

- Confirmed stale fallback strings are removed from the updated sources.
- Full local `hcdeserv` build was attempted; build stopped on an unrelated
  existing renderer compile error in `src/rendering/hwrenderer/hw_entrypoint.cpp`
  (`std::min` template mismatch). Stage 1 changes themselves are isolated to
  invasion mode identity comments/behavior messaging paths.
