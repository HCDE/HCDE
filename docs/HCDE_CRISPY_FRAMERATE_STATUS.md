# HCDE Crispy Framerate Status

This note is the quick-win checkpoint for GitHub issue #10. HCDE already has
fixed-tic playsim plus renderer interpolation; the Crispy-style work is exposed
as presentation presets over the existing timing controls rather than as a new
simulation loop.

## Current Surfaces

- **Original 35 Hz mode**: `hcde_crispy_framerate_preset 35` sets
  `cl_capfps=1` and caps presentation at `TICRATE`.
- **LCD-friendly 60 FPS mode**: `hcde_crispy_framerate_preset 60` keeps
  interpolation enabled and caps `vid_maxfps` at 60.
- **CRT-friendly 70 FPS mode**: `hcde_crispy_framerate_preset 70` keeps
  interpolation enabled and caps `vid_maxfps` at 70, matching the
  double-rendered 35 Hz presentation target.
- **Uncapped interpolation**: `hcde_crispy_framerate_preset uncapped` sets
  `vid_maxfps=0` with interpolation still enabled.
- **Stability guard**: all presets leave `r_ticstability=1` so frame-to-frame
  interpolation uses stable tic timing where possible.

## Runtime Smoke Check

```text
hcde_crispy_framerate_preset 60
hcde_presentation_surfaces
hcde_crispy_framerate_preset 70
hcde_presentation_surfaces
hcde_crispy_framerate_preset 35
hcde_presentation_surfaces
```

Each surface dump should report the requested `vid_maxfps`, `cl_capfps`, and the
fixed `ticrate=35` boundary.

## Rules

- These presets are presentation-only. They must never change `TICRATE`, command
  construction, net prediction, or authoritative playsim timing.
- Dedicated servers do not need the visual presets; server CPU budgeting should
  continue to use server/runtime profiling rather than renderer caps.
- Future fast mouse polling work belongs in the normal input/event pipeline and
  should not bypass `G_BuildTiccmd`.
