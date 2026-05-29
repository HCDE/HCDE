// HCDE Doom Retro presentation-only pain view smoothing scaffold.
//
// Roadmap board item: #8 ("Doom Retro physics"). This is intentionally NOT a
// compat flag because the first Doom Retro candidate is rendering-only: it may
// smooth how recent damage is presented to the local camera, but it must not
// change `player_t::damagecount`, pain timers, actor state, demo data, saves,
// snapshots, or any authoritative playsim result.

#include "c_cvars.h"
#include "c_dispatch.h"
#include "printf.h"

// Default off. Future renderer/view code may sample this when converting the
// local player's existing damagecount into a camera/view kick presentation
// curve. The authoritative damagecount value remains unchanged.
CVAR(Bool, r_view_pain_smooth, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CCMD(r_view_pain_status)
{
	Printf(PRINT_HIGH, "Doom Retro pain-view smoothing:\n");
	Printf(PRINT_HIGH, "  r_view_pain_smooth = %s\n", r_view_pain_smooth ? "on" : "off");
	Printf(PRINT_HIGH, "  implementation = CVAR scaffold only; presentation path not wired yet\n");
	Printf(PRINT_HIGH, "  boundary = local rendering only; no player damagecount, save, demo, snapshot, or playsim mutation\n");
}
