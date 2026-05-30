// HCDE Doomsday-Engine three-features scaffold.
//
// Roadmap board item: #6 ("Doomsday Engine -- Three Specific Things"). The
// three features are FakeRadio edge/step shadows, geometry-based ambient
// occlusion, and per-sector reverb regions. All three are local-client
// presentation only. None of them participate in playsim, snapshots, saves,
// or demos.
//
// This file is the Phase 0 CVAR scaffold + diagnostic CCMD. Real renderer
// and audio implementations live in their respective subsystems and gate on
// the CVARs declared here. See `docs/HCDE_DOOMSDAY_AUDIT.md` for the full
// boundary contract.

#include "c_cvars.h"
#include "c_dispatch.h"
#include "printf.h"

// FakeRadio edge/step shadow overlay. Hardware renderer only.
CVAR(Bool, r_fakeradio, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Float, r_fakeradio_strength, 0.5f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.0f) self = 0.0f;
	if (self > 1.0f) self = 1.0f;
}

// Geometry-based corner AO. Composes with existing gl_ssao when both are on.
CVAR(Bool, r_geom_ao, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Float, r_geom_ao_strength, 0.4f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.0f) self = 0.0f;
	if (self > 1.0f) self = 1.0f;
}

// Sector-tag-driven reverb regions when the active sound backend supports it.
// Falls back silently to dry sound if not.
EXTERN_CVAR(Bool, snd_env_reverb)

CCMD(r_doomsday_status)
{
	Printf(PRINT_HIGH, "\n=== HCDE Doomsday three-features scaffold ===\n");
	Printf(PRINT_HIGH, "  r_fakeradio              = %s\n", *r_fakeradio ? "on" : "off");
	Printf(PRINT_HIGH, "  r_fakeradio_strength     = %.2f\n", *r_fakeradio_strength);
	Printf(PRINT_HIGH, "  r_geom_ao                = %s\n", *r_geom_ao ? "on" : "off");
	Printf(PRINT_HIGH, "  r_geom_ao_strength       = %.2f\n", *r_geom_ao_strength);
	Printf(PRINT_HIGH, "  snd_env_reverb           = %s\n", *snd_env_reverb ? "on" : "off");
	Printf(PRINT_HIGH, "  implementation status    = FakeRadio/AO feed v_blend darkening; snd_env_reverb wired to OpenAL EFX\n");
	Printf(PRINT_HIGH, "  snd_env_reverb backend   = OpenAL EFX (requires snd_efx=1 and hardware support)\n");
	Printf(PRINT_HIGH, "  boundary                 = client-local presentation only; no playsim/save/demo/snapshot impact\n");
	Printf(PRINT_HIGH, "  see docs/HCDE_DOOMSDAY_AUDIT.md for boundaries and phased plan.\n");
	Printf(PRINT_HIGH, "==============================================\n");
}
