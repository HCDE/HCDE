// HCDE NanoBSP loader scaffold.
//
// Roadmap board item: #4 ("NanoBSP from Woof"). This file ONLY exposes the
// gating CVAR and a status CCMD so the eventual integration has a stable
// configuration surface and an observable diagnostic path. There is no
// NanoBSP code linked here yet -- vendoring lives behind Phase 1 of
// `docs/HCDE_NANOBSP_AUDIT.md`.
//
// Hard rules carried over from the audit:
//
//   - Net/replay determinism. Two clients on the same WAD must produce
//     identical subsector ordering, segment-to-line mapping, and
//     line-of-sight results. If a future NanoBSP build cannot guarantee
//     that, the loader must keep value 0 (existing builder) for that map.
//   - Save/demo compatibility. Subsector indices are referenced by
//     gameplay paths; any reordering breaks saves. Hence the value 1 path
//     is "build-from-scratch only" -- maps with valid GL nodes are
//     untouched.
//   - Playsim isolation. The builder runs at map load; it must not reach
//     into actor flags, p_local helpers, or snapshot/replication.
//
// Today this file is observability-only. Wiring it into `p_setup.cpp`
// dispatch is Phase 2 work.

#include "d_nanobsp_loader.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "printf.h"

// Loader selector. Server-controlled in netgames so all clients agree on
// the build path; CVAR_SERVERINFO ensures the value participates in the
// existing server-info handshake the way other compat flags already do.
//
//   0 = Use the existing ZDBSP-style builder. (Default.)
//   1 = Use NanoBSP for build-from-scratch maps only (no valid GL nodes).
//   2 = Use NanoBSP unconditionally (advanced soak path).
CUSTOM_CVAR(Int, hcde_nanobsp_loader, 0, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (self < 0)
		self = 0;
	else if (self > 2)
		self = 2;
}

namespace
{
int NanoBSPDispatchRequests = 0;
int NanoBSPAdapterFallbacks = 0;
}

// True iff the map loader should consult the NanoBSP dispatch path for a
// forced rebuild. The map loader only calls this from the build-from-scratch
// branch, so mode 1 never affects maps with valid existing nodes.
bool HCDENanoBSPShouldUseForBuildFromScratch()
{
	const int mode = *hcde_nanobsp_loader;
	return mode == 1 || mode == 2;
}

bool HCDENanoBSPBuildFromScratch(
	FNodeBuilder::FLevel& leveldata,
	TArray<FNodeBuilder::FPolyStart>& polyspots,
	TArray<FNodeBuilder::FPolyStart>& anchors,
	bool buildGLNodes,
	FLevelLocals& level)
{
	++NanoBSPDispatchRequests;

	// The vendored NanoBSP source still targets Woof data structures. Keep the
	// runtime dispatch point wired, but refuse to emit nodes until an adapter can
	// prove deterministic seg/subsector ordering against the comparison harness.
	++NanoBSPAdapterFallbacks;
	(void)leveldata;
	(void)polyspots;
	(void)anchors;
	(void)buildGLNodes;
	(void)level;
	return false;
}

CCMD(r_nanobsp_status)
{
	const int mode = *hcde_nanobsp_loader;
	const char* modeName = "existing-zdbsp";
	if (mode == 1) modeName = "nanobsp-build-from-scratch";
	else if (mode == 2) modeName = "nanobsp-always";

	Printf(PRINT_HIGH, "\n=== HCDE NanoBSP loader ===\n");
	Printf(PRINT_HIGH, "  hcde_nanobsp_loader = %d (%s)\n", mode, modeName);
	Printf(PRINT_HIGH, "  effective-build-path = %s\n",
		HCDENanoBSPShouldUseForBuildFromScratch() ? "nanobsp" : "existing-zdbsp");
	Printf(PRINT_HIGH, "  dispatch-requests    = %d\n", NanoBSPDispatchRequests);
	Printf(PRINT_HIGH, "  adapter-fallbacks    = %d\n", NanoBSPAdapterFallbacks);
	Printf(PRINT_HIGH, "  vendoring-status      = source present; HCDE adapter not yet enabled\n");
	Printf(PRINT_HIGH, "  determinism-harness   = comparison/report scaffold present\n");
	Printf(PRINT_HIGH, "  see docs/HCDE_NANOBSP_AUDIT.md for boundaries.\n");
	Printf(PRINT_HIGH, "==========================\n");
}
