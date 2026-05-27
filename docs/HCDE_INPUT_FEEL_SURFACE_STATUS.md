# HCDE Input and Feel Surface Status

This note is the Step 5 execution checkpoint for the roadmap integration plan.
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
- **Gyroscope input**: no dedicated gyroscope surface was found in this slice.
  A future implementation should convert gyro motion into the same look/move
  axes used by the command builder, not into a side channel.
- **Physics/feel**: air control, friction, bobbing, gravity, and movement
  compatibility behavior live in playsim code. These are not local presentation
  features once they can affect actor state.

## Runtime smoke check

Use the console command:

```text
hcde_input_feel_surfaces
```

The command reports the active input knobs, prediction settings, and the current
physics boundary. It is diagnostic-only and does not mutate command, prediction,
or gameplay state.

## Step 5 rules

- Route new input devices through existing event/axis processing and
  `G_BuildTiccmd()`.
- Do not bypass `LocalCmds`, prediction, authority echo, or server validation.
- Treat gyroscope support as a local input adapter, not a gameplay feature.
- Gate Doom Retro-style physics/feel changes behind explicit compatibility or
  server-authoritative decisions before enabling them in netgames.
- Keep cosmetic feel such as bob/interpolation separate from movement rules
  that change authoritative actor state.
