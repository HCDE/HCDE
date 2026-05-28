# HCDE International Doom Surface Status

This note is the Step 11 execution checkpoint for the roadmap integration plan.
International Doom's two requested imports are classified before implementation:
night vision is server-gated presentation, and idle breathing is local cosmetic
presentation.

## Current surfaces

- **Night-vision visor**: `sv_hcde_nightvision_allowed` is the authoritative
  serverinfo gate. Clients opt into the visual effect with
  `cl_hcde_nightvision`; `cl_hcde_nightvision_lowlight` limits the effect to
  sectors at or below a light threshold, and `cl_hcde_nightvision_alpha`
  controls the green post-process blend strength.
- **Idle bobbing/breathing**: `cl_hcde_idle_breathing` adds a subtle local
  breathing offset to weapon bob only while the local command and actor velocity
  are idle. `cl_hcde_idle_breathing_amount` and
  `cl_hcde_idle_breathing_speed` control the cosmetic motion.
- **Authority boundary**: neither feature changes actor movement, damage,
  inventory, visibility checks, or command serialization. Night vision can be
  enabled by server policy; idle breathing remains local presentation.

## Runtime smoke check

Use the console command:

```text
hcde_international_surfaces
```

Minimal night-vision smoke:

```text
sv_hcde_nightvision_allowed 1
cl_hcde_nightvision 1
cl_hcde_nightvision_lowlight -1
hcde_international_surfaces
```

Minimal idle-breathing smoke:

```text
cl_hcde_idle_breathing 1
hcde_international_surfaces
```

## Step 11 rules

- Keep night vision in the renderer/post-process path; do not make it a gameplay
  targeting or visibility mechanic.
- Keep the server gate explicit so multiplayer modes can allow or deny the
  visual effect consistently.
- Keep idle breathing cosmetic and local. It must not alter player viewheight,
  position, command input, prediction repair, or authoritative playsim state.
