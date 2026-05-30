# HCDE NanoBSP Validation Harness

Roadmap board item: #4 ("NanoBSP from Woof").

This directory contains the comparison harness that must pass before
`hcde_nanobsp_loader=1` is allowed to affect map loading.

Current state:

- Upstream NanoBSP source is vendored under
  `src/nodebuilders/nanobsp/` as reference source.
- `src/d_nanobsp_loader.cpp` contains the HCDE-native adapter that emits
  vertices, segs, subsectors, and nodes into `FLevelLocals`.
- `compare_nanobsp.py` verifies the vendor files and adapter markers are
  present and writes `dist/nanobsp_compare_report.json`.
- Map cases are queued with reason `engine-run-required` until an engine
  launch wrapper captures ZDBSP-vs-NanoBSP metrics.

Required comparison fields for the engine-backed runner:

- existing builder subsector count
- NanoBSP subsector count
- existing builder seg count
- NanoBSP seg count
- line-to-seg mapping hash for both builders
- fixed line-of-sight samples for both builders
- build time for both builders

Merge rule:

`hcde_nanobsp_loader=1` may only become a default path after this harness can
compare both builders and reports matching line-of-sight results on the curated
map set.
