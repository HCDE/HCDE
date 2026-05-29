// HCDE gyroscope input integration surface.
//
// Roadmap board item: #7 ("Nugget Doom -- Gyroscope Input").
//
// This header exposes the single integration point that the per-frame
// command-builder calls (currently in `G_BuildTiccmd`). The body lives in
// `src/i_input_gyro.cpp` and is zero by default. On Windows, when
// `joy_gyro_enable` is set, the implementation dynamically probes SDL2
// sensor symbols and reads `SDL_SENSOR_GYRO` samples if available.
//
// Hard rule: the function must produce its result on the local input
// thread BEFORE the value becomes part of the recorded `usercmd_t`. It
// must not mutate authoritative actor state.

#pragma once

// Returns the gyroscope contribution to per-frame yaw/pitch deltas, in
// the same units the rest of the input pipeline already uses for mouse
// look. Returns zeros when disabled, when no platform sensor is available,
// or when held/toggle mode is selected without a binding.
void HCDEGyro_GetTiccmdContribution(float& yawDelta, float& pitchDelta);
