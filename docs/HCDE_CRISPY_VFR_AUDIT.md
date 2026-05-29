# HCDE Roadmap #10 — Crispy Variable Framerate Audit

**Last updated:** 2026-05-28  
**Status:** Audit complete. No renderer code changes are required for the
first #10 pass.

## Scope

Crispy's variable-framerate feature decouples rendering from Doom's fixed
35 Hz playsim. HCDE already inherits GZDoom-style interpolation: the render
loop may draw between tics while `TICRATE`, `usercmd_t`, demos, saves, and
netgame simulation remain fixed-rate.

The safe #10 scope is therefore menu/UX documentation and coverage, not an
engine import.

## Existing Coverage

| Area | Existing HCDE surface | Status |
| --- | --- | --- |
| Fixed authoritative tic rate | `TICRATE`, one `usercmd_t` per gametic | Covered; do not change |
| Render interpolation | `D_Render(..., interpolate)`, level interpolators, `I_GetTimeFrac()` | Covered |
| Actor/view interpolation resets | `RF_NOINTERPOLATEVIEW`, `R_ClearInterpolationPath()` | Covered |
| Weapon sprite interpolation | `PlayerBob`, psprite interpolation in SW/HW renderers | Covered |
| FPS cap UX | `vid_maxfps` slider in display options | Covered |
| Cap-to-tic UX | `cl_capfps` option in display options | Covered |
| VSync UX | `vid_vsync` option in display options | Covered |
| FPS visibility | `vid_fps` / `stat fps` | Covered, but not grouped with VFR options |

## Remaining Gaps

- There is no single "variable framerate" menu label that explains the
  relationship between `vid_maxfps`, `cl_capfps`, and `vid_vsync`.
- `vid_fps` is not presented next to the framerate controls, so users cannot
  easily enable the counter while tuning caps.
- `r_weapon_bob_smooth` and `r_view_pain_smooth` are presentation smoothing
  controls, but they are not part of a clearly named "smooth presentation"
  options group yet.
- No smoke report currently records that enabling/disabling `cl_capfps` does
  not alter demo command/tic behavior.

## Recommendation

Close the engine-import part of #10. Future work should be a small UI pass:

- Add a short display-options grouping or help text for framerate controls.
- Co-locate `vid_fps` with `vid_maxfps`, `cl_capfps`, and `vid_vsync`.
- Keep all changes presentation-only; do not alter `TICRATE`, usercmd
  production, demo serialization, or server tick cadence.
