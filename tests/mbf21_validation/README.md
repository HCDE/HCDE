# HCDE MBF21 Validation Pack

This directory contains a generated smoke-test pack for HCDE's MBF21 support.
It is intentionally small and deterministic so it can be rebuilt whenever MBF21
compatibility code changes.

## Build

Run from the HCDE repository root:

```powershell
python tests\mbf21_validation\build_mbf21_validation_pack.py
```

The script writes:

- `tests\mbf21_validation\dist\HCDE-MBF21-validation.wad`
- `tests\mbf21_validation\dist\HCDE-MBF21-validation.pk3`
- `tests\mbf21_validation\dist\HCDE-MBF21-validation-reserved-off.pk3`

## Run

Example local smoke run:

```powershell
.\build\RelWithDebInfo\hcde.exe -iwad C:\Users\user\Downloads\DOOM2.WAD -file tests\mbf21_validation\dist\HCDE-MBF21-validation.wad tests\mbf21_validation\dist\HCDE-MBF21-validation.pk3 +map MAP01
```

The ZScript validator prints `HCDE_MBF21_VALIDATION: PASS` on success and
`HCDE_MBF21_VALIDATION: FAIL` with individual error lines when a checked MBF21
runtime behavior regresses.

## Stage 1 Harness

Run the Stage 1 baseline harness from `HCDE`:

```powershell
python tests\mbf21_validation\validate_mbf21_stage1.py --rebuild-pack
```

The harness runs manifest-driven MBF21 checks and writes:

- `tests\mbf21_validation\dist\mbf21_stage1_report.json`

The JSON report includes per-case status (`passed`, `failed`, `timeout`,
`skipped`, `error`) and the output tail for fast regression triage.

## Coverage

Stage 1 validates the wiring that is easiest to regress while integrating MBF21:

- DEHACKED MBF21 codepointer aliases: `A_WeaponAlert`,
  `A_JumpIfFlagsSet`, and the HCDE MBF-friendly `A_Spawn` alias.
- MBF21 DEHACKED fields: thing groups, dropped item, fast speed, melee range,
  rip sound, frame args/MBF bits, and weapon MBF21 bits.
- OPTIONS compatibility keys: `comp_reservedlineflag`, `comp_friendlyspawn`,
  `comp_ledgeblock`, and `comp_staylift`.
- Doom-format MBF21 extended line flags, including the reserved-bit mask path.

Stage 2 adds runtime behavior checks on top of those static wiring checks:

- Friendly spawn inheritance through `HCDE_MBFSpawnItem` when
  `comp_friendlyspawn` is enabled.
- DEHACKED default propagation to live runtime defaults (`MeleeRange`,
  `FastSpeed`, and `LOGRAV` gravity behavior on `DoomImp`).
- DEHACKED dropped-item wiring on `DoomImp` defaults (`Clip` with probability
  `255`) so patch ingestion regressions are caught deterministically.

Stage 3 expands codepointer alias coverage:

- DEHACKED MBF21 alias resolution for `A_SeekerMissile` and `A_Tracer2`.
- Harness fail-fast marker for unknown codepointer parsing regressions.

Stage 4 hardens MBF21 alias/factory integrity:

- Runtime diagnostic if DEHSUPP alias count drifts from `MBFCodePointerFactories`.
- Runtime diagnostic for unknown mapped action targets.
- Harness fail markers for alias drift and unmanaged alias indices.

Stage 5 adds MBF21 group runtime validation:

- Native script accessors for `InfightingGroup`, `ProjectileGroup`, and
  `SplashGroup` (`GetInfightingGroup`, `GetProjectileGroup`,
  `GetSplashGroup`).
- Runtime validation that DEHACKED group values reach `DoomImp` defaults.
- Behavior check that same non-zero `infighting_group` blocks target switching.

Stage 6 adds live immunity-path checks for group behavior:

- Splash-group behavior check using `RadiusAttack`: same non-zero splash group
  now verifies no radius damage is applied.
- Projectile-group behavior check over multiple ticks: a spawned projectile
  against same-group target verifies no damage on impact resolution.

These checks now cover both static group assignment and runtime immunity
behavior for MBF21 groups in one harness run.

Stage 7 adds runtime steering validation for seeker codepointers:

- `A_Tracer2` now gets a live angle-correction check on a spawned tracer
  projectile with a real target.
- `A_SeekerMissile` now gets a live angle-correction check in the same harness.

This ensures Stage 3's alias mapping coverage is backed by concrete in-engine
behavior checks for both seeker actions.

Stage 8 adds dual compatibility-mode coverage for `comp_reservedlineflag`:

- The builder now emits two PK3 variants:
  - `HCDE-MBF21-validation.pk3` (OPTIONS includes `comp_reservedlineflag 1`)
  - `HCDE-MBF21-validation-reserved-off.pk3` (OPTIONS omits
    `comp_reservedlineflag`, so the harness can force it off via command line)
- The harness manifest now runs both variants in one pass.
- Runtime checks assert both:
  - the expected compatibility mode was actually applied, and
  - linedef flag translation/masking follows the correct MBF21 behavior for
    that mode.
