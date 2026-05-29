# HCDE ID24 Smoke-Test Skeleton

This directory is the smoke-test home for roadmap item #2 (ID24). It mirrors
the shape of `tests/mbf21_validation/`: a generated fixture pack plus a
manifest-driven validator. The current builder emits a real tiny WAD/PK3/DEH
triple and marks the DEHEXTRA/extended-flag cases disabled until the official
ID24 range/flag matrix is filled in.

## Goals

- Verify that ID24 map numbering (`id24_levelnum`) loads and sorts correctly.
- Verify that `id24.wad` support-WAD autoload remains disabled in netgames
  unless the server explicitly loads it.
- Verify ID24 intermission animation fields (`EnterAnim`, `ExitAnim`) parse
  without warning and do not affect playsim state.
- Verify Nightmare respawn fields (`MinRespawnTics`, `RespawnDice`) survive
  actor defaults, VM exposure, save serialization, and runtime respawn checks.
- Verify DEHEXTRA / extended thing, state, sprite, weapon, and flag ranges
  against the official ID24 spec.

## Planned Files

| File | Purpose |
| --- | --- |
| `build_id24_validation_pack.py` | Generates `HCDE-ID24-validation.wad`, `.pk3`, `.deh`, and `dist/manifest.json`. |
| `validate_id24_smoke.py` | Runs HCDE against the generated fixture and writes `dist/report.json`. |
| `id24_smoke_suite.json` | Source manifest of cases: map numbering, intermission art, Nightmare respawn, DEHEXTRA ranges. |
| `dist/` | Generated artifacts only; ignored by git. |

## First Fixture Cases

1. **Map numbering.** Include `E1M10` and a UMAPINFO sequence that increments
   `id24_levelnum`; assert the loader maps exits/intermission conditions to the
   ID24 number rather than old ZDoom's short `ExMy` range.
2. **Intermission animation.** Define `EnterAnim` / `ExitAnim` on one map and
   assert startup/intermission logs contain no parser warnings.
3. **Nightmare respawn.** DEH/ZScript actor with explicit `MinRespawnTics` and
   `RespawnDice`; runtime marker verifies the values reach the actor default
   and affect the respawn gate.
4. **DEHEXTRA high slots.** Use the highest ID24-promised thing/state/sprite
   slots in a tiny DEH patch. The expected result is parser acceptance plus a
   runtime marker that the thing can spawn and render.
5. **Extended flags.** For each ID24-specific flag, record whether HCDE maps it
   to an existing actor flag, adds a new one, or intentionally rejects it.

## Exit Criteria

`docs/HCDE_ID24_AUDIT.md` can move from "inventory expanded" to "Done
foundations" when this directory contains:

- A generated fixture pack. **Landed.**
- A manifest-driven validator script. **Landed.**
- A JSON report with per-case status and output tails. **Landed.**
- At least one DEH/fixture row proving the DEHEXTRA/extended-slot matrix is
  executable, not just documented. **Still pending spec diff.**
