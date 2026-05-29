# HCDE NanoBSP Vendor Slot

Roadmap board item: #4 ("NanoBSP from Woof").

This directory vendors the upstream NanoBSP source files from Woof:

- Upstream repository: <https://github.com/fabiangreffrath/woof>
- Upstream paths: `src/nano_bsp.c`, `src/nano_bsp.h`
- Upstream author/license: Andrew Apted, 2023, MIT license (license header
  preserved in each source file)

Current HCDE status:

- These files are **not** added to `src/CMakeLists.txt`.
- `hcde_nanobsp_loader=1` now reaches an HCDE adapter boundary from the
  build-from-scratch map loading path only.
- The adapter intentionally refuses to emit NanoBSP nodes until Woof-to-HCDE
  data-structure mapping and determinism comparisons are implemented.
- Runtime node output is unchanged; the existing ZDBSP-style builder remains
  the fallback.

Before wiring this into map loading, HCDE needs an adapter layer because the
upstream files target Woof data structures (`line_t`, `seg_t`, `node_t`,
`subsector_t`, zone allocation, and arena allocation). The next safe step is a
comparison harness that can run the current builder and the NanoBSP adapter on
the same tiny map inputs and diff:

- subsector count and ordering
- seg count and line mapping
- sampled line-of-sight results
- load-time measurements

Only after that harness passes should the adapter return successful NanoBSP
output instead of falling back to the existing builder.
