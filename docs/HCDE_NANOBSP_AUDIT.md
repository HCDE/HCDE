# HCDE Roadmap #4 — NanoBSP (from Woof) Scoping

**Last updated:** 2026-05-28
**Status:** Scoping. No code lands as part of this audit; this document
defines the integration boundary so future PRs are bounded.

## What #4 actually means

The board item is "NanoBSP from Woof doom engine". NanoBSP is a fast,
small-footprint BSP/node builder used by Woof. The integration goal is
**not** to replace map data structures or runtime traversal logic — it is to
have a faster *load-time* node builder available for maps that ship without
GL nodes (or with stale ones) so first-load latency drops on dedicated
servers and the netgame join experience stops stalling on the host.

## Existing in-tree surface (what we already have)

- `src/utility/nodebuilder/` — the GZDoom-derived ZDBSP-style node
  builder. Files of note: `nodebuild.cpp`, `nodebuild_events.cpp`,
  `nodebuild_extract.cpp`, `nodebuild_gl.cpp`, `nodebuild_classify_nosse2.cpp`,
  `nodebuild_utility.cpp`. This is the current builder; it is invoked by
  `p_setup.cpp` / `src/maploader/glnodes.cpp` when a map either has no
  nodes or the engine elects to rebuild them.
- `compatibility.txt` flags (e.g., `rebuildnodes`) that already exist for
  problem maps.

So we already have a node builder. The #4 question is "should we swap in a
faster one for some/all cases" rather than "we have no node builder".

## Strict boundaries for the import

These are non-negotiable for any NanoBSP PR to land:

1. **Net/replay determinism.** Two clients running the same WAD MUST end
   up with identical:
     - subsector counts and ordering
     - segment-to-line mapping (for lighting/visibility hashing)
     - line-of-sight and traversal results used by playsim
   If the new builder produces different output for any input map than the
   old one, gating is required (see §"Gating"). Players in the same
   netgame must build matching nodes or refuse to join.
2. **Save/demo compatibility.** Saved games and demos do not store node
   data, but they do store subsector/sector/line indices. Any change to
   index ordering breaks loads. NanoBSP must produce indices that match
   the existing builder for a given input on the canonical path, OR the
   gating must keep saves/demos pinned to the old builder.
3. **Playsim isolation.** No NanoBSP API call may reach into:
     - `actor.h` flags
     - `p_local.h` movement helpers
     - Snapshot/replication paths in `d_net*.cpp`
   The builder runs only at map load, before any of that exists for the
   map.
4. **No gameplay collision change.** P_BlockThingsIterator,
   P_PathTraverse, and friends must keep their current semantics. NanoBSP
   may only emit a different segs/nodes/subsectors arrangement; everything
   built on top of those (line-of-sight, sound propagation, projectile
   path) must verify identical results before merge.

## Gating

The intended flag is a CVAR `hcde_nanobsp_loader` with values:

| Value | Meaning |
| ----- | ------- |
| 0 | Use the existing ZDBSP-style builder. (Default.) |
| 1 | Use NanoBSP for nodebuild-from-scratch only. Do not touch maps that
ship valid GL nodes. |
| 2 | Use NanoBSP unconditionally (advanced; soak path). |

This must be a server-controlled CVAR in netgames (CVAR_SERVERINFO or an
equivalent that gates client/server agreement). A client whose CVAR
disagrees with the server's choice must either flip to match or refuse to
join — same model as the existing nodebuild compat flags.

## Phased plan

- **Phase 0 (this doc).** Define the boundary.
- **Phase 1.** Vendor the NanoBSP source under
  `src/nodebuilders/nanobsp/` so it is present as reference source. No CVAR
  behaviour change. Add a unit-style harness that runs both builders on a
  small map set and diffs subsector/seg counts and line-of-sight results from
  a fixed sample grid. **Vendor slot and harness scaffold landed
  2026-05-28:** `tests/nanobsp_validation/compare_nanobsp.py` verifies the
  upstream files and writes an adapter-pending report; the real builder diff
  waits on the HCDE adapter.
- **Phase 2.** Add the `hcde_nanobsp_loader` CVAR (default 0). Wire it
  into map loading for the "build from scratch" path only (value 1).
  **Dispatch landed 2026-05-28:** `MapLoader` consults the NanoBSP adapter
  only after `ForceNodeBuild` is true. **Initial adapter emission landed
  2026-05-30:** the adapter now ports NanoBSP's partitioning logic onto HCDE
  arrays and emits vertices/segs/subsectors/nodes directly. Determinism
  comparisons and soak gating remain before making it the default.
  **Preflight boundary landed 2026-05-29:** the dispatch now classifies
  adapter inputs before falling back, tracking empty geometry, missing source
  arrays, polyobject presence, and the global "adapter not enabled" blocker in
  `r_nanobsp_status` and the comparison harness report.
- **Phase 3.** Soak in single-player on a curated map list (Doom 1/2,
  Plutonia, TNT, modern megawads). Compare:
     - load time
     - subsector/segment counts
     - sample-point line-of-sight matches
     - any visible rendering glitches (z-fighting around node splits)
- **Phase 4.** Multiplayer soak with `hcde_nanobsp_loader` set on the
  server. Verify late-join doesn't desync at the snapshot/index level.
- **Phase 5 (optional).** Extend to value 2 (always-rebuild) if the
  determinism criteria pass.

## What this is NOT

- Not a replacement for runtime BSP traversal in the renderer.
- Not a replacement for the ZDoom-style portal/teleport handling.
- Not a path to introduce new map *features* (slopes, 3D floors, etc.).
  Those already exist; NanoBSP is purely a faster build of the same data.
- Not a savegame/demo format change.

## Source map (current state)

| Concern | File |
| ------- | ---- |
| Node builder dispatch | `src/maploader/maploader.cpp`, `src/d_nanobsp_loader.cpp` |
| GL node loader/builder bridge | `src/maploader/glnodes.cpp` |
| Existing builder core | `src/utility/nodebuilder/nodebuild*.cpp` |
| Map data structs | `src/maploader/maploader.cpp` |

A future NanoBSP import lives next to the existing builder, not in
playsim, the renderer's runtime path, or netcode.

## Current Testing Status (2026-05-30)

### What can be tested TODAY

The dispatch scaffold is live and observable:

1. **Diagnostic CCMD:** `r_nanobsp_status` in console shows:
   - Current loader mode (`hcde_nanobsp_loader` value)
   - Effective build path (NanoBSP vs ZDBSP)
   - Dispatch request count
   - Adapter fallback count
   - Preflight pass/reject counts
   - Polyobject case count
   - Recent dispatch history (last 8 maps with geometry stats)

2. **Preflight classification and emission:** The adapter classifies inputs
   before building. Blockers tracked:
   - `empty-geometry` (zero vertices/lines/sides)
   - `missing-array` (null pointer inputs)
   - `polyobjects` (maps with polyobjects - potential compatibility issue)
   Clean inputs now attempt HCDE-native NanoBSP emission; blocked inputs still
   fall back to ZDBSP.

3. **Command-line flag:** `-forceswnodes` sets `hcde_nanobsp_loader=1` for
   testing the dispatch path without modifying config.

### What requires future implementation / validation

**The adapter layer (Phase 2 completion):**
- **Initial HCDE-native adapter landed 2026-05-30.** The runtime path no
  longer calls Woof's `BSP_BuildNodes()` directly because that source assumes
  Woof globals (`lines`, `segs`, `nodes`, `world_arena`) that HCDE does not
  expose. Instead, `src/d_nanobsp_loader.cpp` ports the NanoBSP partitioning
  logic onto HCDE's `FNodeBuilder::FLevel`/`FLevelLocals` arrays.
- Current emitted data: vertices (including split vertices), segs,
  subsectors, and nodes in the same `FLevelLocals` arrays consumed by the
  existing maploader.
- Current fallback rules remain: empty input, missing arrays, and polyobject
  maps still fall back to the existing builder.
- Determinism comparison can now begin.

**Comparison harness (Phase 3):**
- Build identical maps with both builders
- Compare subsector counts and ordering
- Compare segment-to-line mappings
- Sample point line-of-sight verification
- Load time benchmarks

### Test procedure for TODAY

1. Build HCDE with current code
2. Run with `-forceswnodes` or set `hcde_nanobsp_loader 1` in console
3. Load various maps (especially build-from-scratch maps)
4. Run `r_nanobsp_status` to verify:
   - Dispatch requests increment
   - Adapter reports either `adapter-emitted:hcde-nanobsp` or correct blockers
   - Blocked inputs fall back to ZDBSP (no crash, map loads)
   - Polyobject maps are flagged

### Safety verification

- [ ] `hcde_nanobsp_loader=0` (default): maps load normally, never touch NanoBSP path
- [ ] `hcde_nanobsp_loader=1` with existing GL nodes: skips build-from-scratch path entirely
- [ ] `hcde_nanobsp_loader=1` with missing/bad nodes: enters dispatch, adapter emits on clean maps
- [ ] Polyobject maps and invalid inputs still fall back to ZDBSP
- [ ] No crash when loading any map with any loader value
- [ ] Determinism: netgames with same loader value produce identical nodes

## Open questions (for the implementing PR, not this audit)

1. Does NanoBSP guarantee identical subsector ordering for the same input?
   If not, can we sort its output to match? If neither, we must gate
   harder.
2. Does NanoBSP support the polyobject quirks ZDBSP knows about? (See
   `nodebuild_classify_nosse2.cpp` and the `polyobjects.cpp` interactions.)
3. Memory footprint at peak vs. ZDBSP — useful or not on dedicated servers
   where simultaneous map loads happen for late joiners?
4. **NEW:** What is the performance cost of HCDE↔Woof data conversion?
   Is the conversion overhead small enough that NanoBSP's speed advantage
   is still meaningful?
