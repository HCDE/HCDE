# HCDE Lighting Stage 4 - Adaptive Shadow Budget

Date: 2026-05-19

Stage 4 adds an optional adaptive budget controller for dynamic-light
shadowmaps. It keeps the Stage 2 capability caps and Stage 3 map rules, then
auto-tunes the active per-frame shadowmapped light count based on observed
shadow upload cost.

## What Changed

- Added adaptive budget CVARs:
  - `hcde_shadow_autobudget` (bool, default `false`)
  - `hcde_shadow_autobudget_targetms` (float, default `1.20`)
  - `hcde_shadow_autobudget_minlights` (int, default `64`)
  - `hcde_shadow_autobudget_step` (int, default `32`)
- Added runtime budget telemetry fields to shadow stats:
  - hard cap (`BudgetHardCap`)
  - runtime cap (`BudgetRuntimeCap`)
  - adaptive mode flag (`BudgetAdaptiveEnabled`)
- Updated `stat shadowmap` output with adaptive budget visibility:
  - `cap=<runtime>/<hard>`
  - `adaptive=on|off`
- Updated light-row assignment to honor runtime adaptive cap in addition to
  configured `gl_shadowmap_maxlights`.

## Runtime Behavior

When `hcde_shadow_autobudget` is enabled and dynamic light shadowmaps are on:

- Stage 2/3 still compute the hard cap first.
- The controller uses the previous shadow upload time (`stat shadowmap upload`)
  against `hcde_shadow_autobudget_targetms`.
- If upload time is above `target * 1.20`, runtime cap is reduced by
  `hcde_shadow_autobudget_step`.
- If upload time is below `target * 0.70`, runtime cap is increased by
  `hcde_shadow_autobudget_step`.
- Runtime cap stays clamped between:
  - `hcde_shadow_autobudget_minlights` (bounded by map/device/user caps), and
  - the effective hard cap/user budget ceiling.

When disabled, behavior is unchanged from Stage 2/3.

## Files Updated

- `HCDE/src/common/rendering/hwrenderer/data/hw_shadowmap.h`
- `HCDE/src/common/rendering/hwrenderer/data/hw_shadowmap.cpp`
- `HCDE/src/rendering/hwrenderer/hw_entrypoint.cpp`

