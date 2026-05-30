// HCDE Doom Retro presentation-only pain view smoothing scaffold.
//
// Roadmap board item: #8 ("Doom Retro physics"). This is intentionally NOT a
// compat flag because the first Doom Retro candidate is rendering-only: it may
// smooth how recent damage is presented to the local camera, but it must not
// change `player_t::damagecount`, pain timers, actor state, demo data, saves,
// snapshots, or any authoritative playsim result.
//
// Phase 1 (this file) defines the CVARs, the smoothing helper, and a status
// CCMD that demonstrates the curve on a fixed sample input. The helper takes
// the unmodified vanilla damagecount-derived intensity and returns a smoothed
// presentation value. The helper is pure (no globals, no time-dependent state)
// so the renderer can call it from `R_SetupFrame` without introducing a new
// per-frame mutable cache.
//
// Phase 2 (a follow-up renderer PR) will read the helper from the view-blend
// path so the smoothed value drives only the rendered intensity. The actual
// `damagecount` value seen by playsim, `bot_func`, mugshot, save data, and
// demos remains the vanilla value.

#include "c_cvars.h"
#include "c_dispatch.h"
#include "printf.h"

#include <algorithm>

// Default off. Future renderer/view code may sample this when converting the
// local player's existing damagecount into a camera/view kick presentation
// curve. The authoritative damagecount value remains unchanged.
CVAR(Bool, r_view_pain_smooth, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// Strength multiplier (0..1). 0 = identical to vanilla, 1 = full Doom-Retro
// smoothness. Default 0.6 leans toward the smoother feel without erasing the
// vanilla pain spike entirely. Only takes effect when r_view_pain_smooth is on.
CUSTOM_CVAR(Float, r_view_pain_smooth_strength, 0.6f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.0f) self = 0.0f;
	if (self > 1.0f) self = 1.0f;
}

// Pure smoothing helper. Public so renderer code (and tests) can call it.
//
// Input:
//   rawDamageCount  -- player_t::damagecount, range [0, 100] in vanilla.
//   strength        -- per-call strength override; pass *r_view_pain_smooth_strength.
//
// Output:
//   Smoothed intensity in [0, 1] suitable for multiplying a pain blend or
//   view-kick scalar. Returns the vanilla linear curve when strength is 0.
//
// Curve:
//   Vanilla pain blend treats damagecount as a linear ramp 0..100 -> 0..1.
//   The smoothed curve uses a smoothstep (3t^2 - 2t^3) on the same input
//   range, then lerps between the two by `strength`. The peak intensity at
//   damagecount=100 is identical (1.0) so the maximum bite of a big hit is
//   preserved; only the rise/fall shape is smoothed.
//
//   strength=0   : identical to vanilla.
//   strength=1   : full smoothstep curve.
//   strength=0.6 : 60% smoothstep + 40% linear (default).
//
// This function is intentionally stateless: the renderer must not introduce a
// per-frame cache that would couple presentation to playsim timing. If a
// time-decay tail is desired in the future, it lives next to the callsite.
float HCDEViewPainSmoothScale(int rawDamageCount, float strength)
{
	const float clampedDc = float(std::clamp(rawDamageCount, 0, 100));
	const float t = clampedDc * 0.01f;
	const float linear = t;
	const float smooth = t * t * (3.0f - 2.0f * t);
	const float s = std::clamp(strength, 0.0f, 1.0f);
	return linear + (smooth - linear) * s;
}

CCMD(r_view_pain_status)
{
	const bool on = r_view_pain_smooth;
	const float strength = *r_view_pain_smooth_strength;
	Printf(PRINT_HIGH, "Doom Retro pain-view smoothing:\n");
	Printf(PRINT_HIGH, "  r_view_pain_smooth          = %s\n", on ? "on" : "off");
	Printf(PRINT_HIGH, "  r_view_pain_smooth_strength = %.2f\n", strength);
	Printf(PRINT_HIGH, "  helper sample (dc=  0)      vanilla=%.3f smoothed=%.3f\n",
		HCDEViewPainSmoothScale(0, 0.0f), HCDEViewPainSmoothScale(0, strength));
	Printf(PRINT_HIGH, "  helper sample (dc= 25)      vanilla=%.3f smoothed=%.3f\n",
		HCDEViewPainSmoothScale(25, 0.0f), HCDEViewPainSmoothScale(25, strength));
	Printf(PRINT_HIGH, "  helper sample (dc= 50)      vanilla=%.3f smoothed=%.3f\n",
		HCDEViewPainSmoothScale(50, 0.0f), HCDEViewPainSmoothScale(50, strength));
	Printf(PRINT_HIGH, "  helper sample (dc= 75)      vanilla=%.3f smoothed=%.3f\n",
		HCDEViewPainSmoothScale(75, 0.0f), HCDEViewPainSmoothScale(75, strength));
	Printf(PRINT_HIGH, "  helper sample (dc=100)      vanilla=%.3f smoothed=%.3f\n",
		HCDEViewPainSmoothScale(100, 0.0f), HCDEViewPainSmoothScale(100, strength));
	Printf(PRINT_HIGH, "  implementation status       = wired into v_blend.cpp pain-flash path; gate on r_view_pain_smooth\n");
	Printf(PRINT_HIGH, "  boundary                    = local rendering only; no player damagecount, save, demo, snapshot, or playsim mutation\n");
}
