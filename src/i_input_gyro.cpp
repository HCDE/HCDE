// HCDE gyroscope input -- scope-marker scaffold.
//
// Roadmap board item: #7 ("Nugget Doom -- Gyroscope Input").
//
// This file declares the public configuration surface for a future
// gyroscope-driven aim adapter. The CVARs are intentionally consumed by
// nothing today; they exist so that:
//
//   1. The launcher menu (and tools that template configs) can list the
//      knobs without speculating on names.
//   2. A subsequent change can wire the platform input layer to read these
//      values without renaming or re-architecting the surface.
//   3. The roadmap item has a concrete file to point at as "the place to
//      finish #7" rather than a vague "find somewhere to put it".
//
// All values are local input-pipeline tunables. They MUST NOT influence
// authoritative simulation. The eventual implementation has to terminate
// in the same `usercmd_t` construction path that mouse/keyboard inputs
// use, so prediction and server authority remain unchanged.

#include "c_cvars.h"
#include "c_dispatch.h"
#include "printf.h"
#include "i_input_gyro.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

EXTERN_CVAR(Bool, joy_gyro_enable)
EXTERN_CVAR(Int, joy_gyro_mode)

namespace
{
struct HCDEGyroSample
{
	bool Available = false;
	bool PlatformQueried = false;
	const char* Provider = "none";
	int SensorCount = 0;
	float YawRate = 0.0f;   // rad/s
	float PitchRate = 0.0f; // rad/s
};

HCDEGyroSample HCDEGyro_PollPlatformSample()
{
	HCDEGyroSample sample;
	if (!joy_gyro_enable)
	{
		sample.Provider = "disabled";
		return sample;
	}

#ifdef _WIN32
	sample.PlatformQueried = true;
	sample.Provider = "windows-sdl2-dynamic-sensor";

	using SDL_WasInitFunc = unsigned int (__cdecl *)(unsigned int);
	using SDL_InitSubSystemFunc = int (__cdecl *)(unsigned int);
	using SDL_SensorUpdateFunc = void (__cdecl *)();
	using SDL_NumSensorsFunc = int (__cdecl *)();
	using SDL_SensorGetDeviceTypeFunc = int (__cdecl *)(int);
	using SDL_SensorOpenFunc = void* (__cdecl *)(int);
	using SDL_SensorGetDataFunc = int (__cdecl *)(void*, float*, int);
	using SDL_SensorCloseFunc = void (__cdecl *)(void*);

	constexpr unsigned int SDL_INIT_SENSOR_HCDE = 0x00008000u;
	constexpr int SDL_SENSOR_GYRO_HCDE = 2;

	HMODULE sdl = GetModuleHandleA("SDL2.dll");
	if (sdl == nullptr)
		sdl = LoadLibraryA("SDL2.dll");
	if (sdl == nullptr)
	{
		sample.Provider = "windows-sdl2-not-loaded";
		return sample;
	}

	auto pSDL_WasInit = reinterpret_cast<SDL_WasInitFunc>(GetProcAddress(sdl, "SDL_WasInit"));
	auto pSDL_InitSubSystem = reinterpret_cast<SDL_InitSubSystemFunc>(GetProcAddress(sdl, "SDL_InitSubSystem"));
	auto pSDL_SensorUpdate = reinterpret_cast<SDL_SensorUpdateFunc>(GetProcAddress(sdl, "SDL_SensorUpdate"));
	auto pSDL_NumSensors = reinterpret_cast<SDL_NumSensorsFunc>(GetProcAddress(sdl, "SDL_NumSensors"));
	auto pSDL_SensorGetDeviceType = reinterpret_cast<SDL_SensorGetDeviceTypeFunc>(GetProcAddress(sdl, "SDL_SensorGetDeviceType"));
	auto pSDL_SensorOpen = reinterpret_cast<SDL_SensorOpenFunc>(GetProcAddress(sdl, "SDL_SensorOpen"));
	auto pSDL_SensorGetData = reinterpret_cast<SDL_SensorGetDataFunc>(GetProcAddress(sdl, "SDL_SensorGetData"));
	auto pSDL_SensorClose = reinterpret_cast<SDL_SensorCloseFunc>(GetProcAddress(sdl, "SDL_SensorClose"));
	if (pSDL_WasInit == nullptr || pSDL_InitSubSystem == nullptr || pSDL_SensorUpdate == nullptr ||
		pSDL_NumSensors == nullptr || pSDL_SensorGetDeviceType == nullptr || pSDL_SensorOpen == nullptr ||
		pSDL_SensorGetData == nullptr || pSDL_SensorClose == nullptr)
	{
		sample.Provider = "windows-sdl2-sensor-symbols-missing";
		return sample;
	}

	if ((pSDL_WasInit(SDL_INIT_SENSOR_HCDE) & SDL_INIT_SENSOR_HCDE) == 0 &&
		pSDL_InitSubSystem(SDL_INIT_SENSOR_HCDE) != 0)
	{
		sample.Provider = "windows-sdl2-sensor-init-failed";
		return sample;
	}

	pSDL_SensorUpdate();
	const int sensorCount = pSDL_NumSensors();
	sample.SensorCount = sensorCount;
	for (int i = 0; i < sensorCount; ++i)
	{
		if (pSDL_SensorGetDeviceType(i) != SDL_SENSOR_GYRO_HCDE)
			continue;

		void* sensor = pSDL_SensorOpen(i);
		if (sensor == nullptr)
			continue;

		float values[3] = { 0.0f, 0.0f, 0.0f };
		const int ok = pSDL_SensorGetData(sensor, values, 3);
		pSDL_SensorClose(sensor);
		if (ok != 0)
			continue;

		// SDL reports gyroscope angular velocity in rad/s. Axis orientation is
		// controller/platform specific; keep the first pass conservative and
		// expose only yaw/pitch rates through the existing CVAR scales.
		sample.Available = true;
		sample.PitchRate = values[0];
		sample.YawRate = values[1];
		return sample;
	}
#else
	sample.Provider = "platform-sensor-probe-unavailable";
#endif

	return sample;
}

bool HCDEGyroModeAllowsContribution()
{
	const int mode = *joy_gyro_mode;
	if (mode <= 0)
		return true;

	// Held/toggle activation needs an input binding. Until that lands, the
	// mode is parsed and reported but contributes zero so enabling it cannot
	// surprise players or mutate view state without an explicit action.
	return false;
}
}

// Master switch. Default off so a build with no gyro hardware is
// indistinguishable from today's behaviour.
CVAR(Bool, joy_gyro_enable, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Sensitivity multipliers for the two aim axes. Defaults match
// approximately Nugget's documented baseline (degrees-per-second per
// rad/s of measured angular velocity). Adjust per-device once the
// platform layer is wired.
CVAR(Float, joy_gyro_yawscale, 2.5f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, joy_gyro_pitchscale, 2.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Deadzone in rad/s. Small involuntary motion below this threshold is
// dropped so a controller resting on a desk doesn't drift the view.
CVAR(Float, joy_gyro_deadzone, 0.05f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Activation mode. 0 = always-on, 1 = held-button, 2 = toggled-button.
// The button binding is a separate input-binding concern and lives in
// the action map; this CVAR is purely the on/off policy.
CVAR(Int, joy_gyro_mode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Reserved: invert axes for users who hold the controller upside down.
CVAR(Bool, joy_gyro_invertyaw, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, joy_gyro_invertpitch, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CCMD(m_gyro_status)
{
	const HCDEGyroSample sample = HCDEGyro_PollPlatformSample();
	Printf(PRINT_HIGH, "HCDE gyro input scaffold:\n");
	Printf(PRINT_HIGH, "  joy_gyro_enable      = %s\n", joy_gyro_enable ? "on" : "off");
	Printf(PRINT_HIGH, "  joy_gyro_yawscale    = %.3f\n", *joy_gyro_yawscale);
	Printf(PRINT_HIGH, "  joy_gyro_pitchscale  = %.3f\n", *joy_gyro_pitchscale);
	Printf(PRINT_HIGH, "  joy_gyro_deadzone    = %.3f rad/s\n", *joy_gyro_deadzone);
	Printf(PRINT_HIGH, "  joy_gyro_mode        = %d (0=always, 1=held, 2=toggle)\n", *joy_gyro_mode);
	Printf(PRINT_HIGH, "  joy_gyro_invertyaw   = %s\n", joy_gyro_invertyaw ? "on" : "off");
	Printf(PRINT_HIGH, "  joy_gyro_invertpitch = %s\n", joy_gyro_invertpitch ? "on" : "off");
	Printf(PRINT_HIGH, "  platform provider    = %s\n", sample.Provider);
	Printf(PRINT_HIGH, "  platform queried     = %s\n", sample.PlatformQueried ? "yes" : "no");
	Printf(PRINT_HIGH, "  platform sensor count = %d\n", sample.SensorCount);
	Printf(PRINT_HIGH, "  platform sensor available = %s\n", sample.Available ? "yes" : "no");
	Printf(PRINT_HIGH, "  latest platform sample = yaw %.3f rad/s, pitch %.3f rad/s\n", sample.YawRate, sample.PitchRate);
	Printf(PRINT_HIGH, "  mode contribution allowed = %s\n", HCDEGyroModeAllowsContribution() ? "yes" : "no");
	Printf(PRINT_HIGH, "  platform sensor plumbing = SDL2 sensor probe when available; otherwise zero contribution\n");
}

// Integration point for `G_BuildTiccmd`. Returns the gyro contribution to be
// added to the per-frame yaw/pitch deltas that feed `usercmd_t`. Today this
// always returns zero so the call site can land first; the eventual sensor
// implementation will replace the body without touching `g_game.cpp`.
//
// Determinism note: the contribution must be computed on the local input
// thread *before* the result becomes a recorded user command, so that demo
// playback reproduces motion bit-for-bit. We DO NOT mutate authoritative
// state here.
void HCDEGyro_GetTiccmdContribution(float& yawDelta, float& pitchDelta)
{
	yawDelta = 0.0f;
	pitchDelta = 0.0f;

	if (!joy_gyro_enable)
		return;
	if (!HCDEGyroModeAllowsContribution())
		return;

	const HCDEGyroSample sample = HCDEGyro_PollPlatformSample();
	if (!sample.Available)
		return;

	// Apply deadzone, scale, and inversion using the CVAR surface above.
	// This is intentionally inert today (sample.Available is always false),
	// but the math is shaped so the eventual platform plumbing only has to
	// fill in `HCDEGyro_PollPlatformSample`.
	const float deadzone = static_cast<float>(*joy_gyro_deadzone);
	float yaw = sample.YawRate;
	float pitch = sample.PitchRate;
	if (yaw > -deadzone && yaw < deadzone) yaw = 0.0f;
	if (pitch > -deadzone && pitch < deadzone) pitch = 0.0f;

	yaw *= static_cast<float>(*joy_gyro_yawscale);
	pitch *= static_cast<float>(*joy_gyro_pitchscale);

	if (joy_gyro_invertyaw) yaw = -yaw;
	if (joy_gyro_invertpitch) pitch = -pitch;

	yawDelta = yaw;
	pitchDelta = pitch;
}

// NOTE: the actual gyroscope sample plumbing (SDL3 sensor API on Windows,
// CoreMotion-style on macOS, evdev / udev on Linux) is intentionally
// scoped out of this scaffold. When that lands it should:
//
//   - Read raw angular velocity from the platform sensor.
//   - Apply mode (always-on vs. held vs. toggle), deadzone, scale, and
//     inversion using the CVARs above.
//   - Add the result to the per-frame yaw/pitch deltas that already feed
//     `usercmd_t`, NOT directly to actor angles.
//   - Survive demo playback gracefully: gyro contributions are part of
//     the recorded user command, so demos replay deterministically.
