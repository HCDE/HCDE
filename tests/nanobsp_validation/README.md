# HCDE NanoBSP Validation Harness

Roadmap board item: #4 ("NanoBSP from Woof").

This directory contains the comparison harness that must pass before
`hcde_nanobsp_loader=1` is allowed to affect map loading.

Current state:

- Upstream NanoBSP source is vendored under
  `src/utility/nodebuilder/nano/`.
- The source is not compiled and no runtime behavior changes.
- `compare_nanobsp.py` verifies the vendor files are present and writes
  `dist/nanobsp_compare_report.json`.
- Until the HCDE adapter exists, every map case is reported as skipped with
  reason `adapter-missing`.

Required comparison fields once the adapter lands:

- existing builder subsector count
- NanoBSP subsector count
- existing builder seg count
- NanoBSP seg count
- line-to-seg mapping hash for both builders
- fixed line-of-sight samples for both builders
- build time for both builders

Merge rule:

`hcde_nanobsp_loader=1` may only be wired into the build-from-scratch node path
after this harness can compare both builders and reports matching line-of-sight
results on the curated map set.
