// HCDE Nugget-style player feel/input scaffold.
//
// Roadmap board item: #9 ("Nugget Doom -- Player Feel & Input").
//
// This file declares the default-off configuration surface described in
// `docs/HCDE_NUGGET_FEEL_AUDIT.md`. `m_smooth_curve` is consumed by
// `G_BuildTiccmd` for stateless mouse-delta shaping when explicitly enabled;
// the remaining CVARs are stable names for follow-up presentation patches.
//
// Boundary: these are local input/presentation tunables only. Anything that
// changes movement physics, hitscan, weapon timing, or actor state belongs to
// #8 Doom Retro physics (or another explicit gameplay task), not #9.

#include "c_cvars.h"
#include "c_dispatch.h"
#include "printf.h"

// 0 = linear/current behaviour, 1 = eased curve. Future patches must apply
// this before raw input becomes `usercmd_t`, never by mutating actor angles.
CUSTOM_CVAR(Int, m_smooth_curve, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
}

// Render-only widgets. Default off until the HUD consumers land.
CVAR(Bool, r_crosshair_recoil, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, r_killfeed, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Sound-name selection only. The eventual implementation may choose
// `*footstep-<surface>` aliases, but it must not change movement friction,
// collision, or any authoritative floor semantics.
CVAR(Bool, snd_footsteps_surface, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CCMD(m_input_status)
{
	Printf(PRINT_HIGH, "HCDE Nugget input-feel scaffold:\n");
	Printf(PRINT_HIGH, "  m_smooth_curve       = %d (0=linear, 1=eased)\n", *m_smooth_curve);
	Printf(PRINT_HIGH, "  r_crosshair_recoil   = %s\n", r_crosshair_recoil ? "on" : "off");
	Printf(PRINT_HIGH, "  r_killfeed           = %s\n", r_killfeed ? "on" : "off");
	Printf(PRINT_HIGH, "  snd_footsteps_surface = %s\n", snd_footsteps_surface ? "on" : "off");
	Printf(PRINT_HIGH, "  wiring = m_smooth_curve shapes mouse deltas only when enabled; no physics, hitscan, or weapon timing changes\n");
}
