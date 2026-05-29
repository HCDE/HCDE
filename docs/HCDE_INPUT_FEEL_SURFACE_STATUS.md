# HCDE Input and Feel Surface Status

This note is the Step 10 execution checkpoint for the roadmap integration plan.
Nugget-style player feel/input and gyroscope support should feed the normal
local command path. Doom Retro-style physics and feel changes are gameplay
state changes and must be treated as server-authoritative work.

## Current surfaces

- **Command construction**: `I_StartTic()` and `D_ProcessEvents()` drain local
  input before `G_BuildTiccmd()` writes the next `usercmd_t` into `LocalCmds`.
  Netcode then sends or mirrors that command through the existing authority
  path.
- **Keyboard/mouse feel**: `cl_run`, `freelook`, `lookstrafe`, `m_forward`,
  and `m_side` are already normalized into `G_BuildTiccmd()`.
- **Controller/analog feel**: `use_joystick`, `cl_analog_run`,
  `cl_analog_straferun`, `cl_analog_sensitivity_yaw`, and
  `cl_analog_sensitivity_pitch` already terminate in the same command builder.
- **Gyroscope input**: `cl_gyro_enable`, `cl_gyro_sensitivity_yaw`,
  `cl_gyro_sensitivity_pitch`, `cl_gyro_invert_yaw`, and
  `cl_gyro_invert_pitch` control a local gyro-look adapter. Samples queued by
  `hcde_gyro_sample <yaw> <pitch>` are consumed by `G_BuildTiccmd()` and become
  normal yaw/pitch command deltas.
- **Nugget-style feel inventory**: crouch already enters the normal button path,
  `cl_doautoaim` covers existing autoaim behavior, `r_deathcamera` provides the
  death-camera presentation surface, and the existing quake specials
  (`A_Quake`, `A_QuakeEx`, `Radius_Quake`) are the engine-facing explosion
  shake APIs.
- **Physics/feel**: air control, friction, bobbing, gravity, and movement
  compatibility behavior live in playsim code. These are not local presentation
  features once they can affect actor state.

## Runtime smoke check

Use the console command:

```text
hcde_input_feel_surfaces
```

The command reports the active input knobs, prediction settings, and the current
physics boundary. It also reports gyro queued/applied sample counters so the
input adapter can be smoke-tested without adding a gameplay side channel.
It additionally inventories the existing player-feel presentation hooks for
`#9`; remaining behavior such as explicit vertical lock-on or damage flinch
still needs a separate design pass before it should be closed.

For Step 10, the minimal runtime smoke is:

```text
cl_gyro_enable 1
hcde_gyro_sample 0.25 -0.125
hcde_input_feel_surfaces
```

After the next local `G_BuildTiccmd()` call, the gyro `samples` field should show
the queued sample applied and the queued deltas should return to zero.

## Step 5 rules

- Route new input devices through existing event/axis processing and
  `G_BuildTiccmd()`.
- Do not bypass `LocalCmds`, prediction, authority echo, or server validation.
- Treat gyroscope support as a local input adapter. It must terminate in
  `G_BuildTiccmd()` yaw/pitch deltas, not in a gameplay feature or replicated
  side channel.
- Gate Doom Retro-style physics/feel changes behind explicit compatibility or
  server-authoritative decisions before enabling them in netgames.
- Keep cosmetic feel such as bob/interpolation separate from movement rules
  that change authoritative actor state.
