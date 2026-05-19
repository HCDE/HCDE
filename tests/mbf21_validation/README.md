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

## Run

Example local smoke run:

```powershell
.\build\RelWithDebInfo\hcde.exe -iwad C:\Users\user\Downloads\DOOM2.WAD -file tests\mbf21_validation\dist\HCDE-MBF21-validation.wad tests\mbf21_validation\dist\HCDE-MBF21-validation.pk3 +map MAP01
```

The ZScript validator prints `HCDE_MBF21_VALIDATION: PASS` on success and
`HCDE_MBF21_VALIDATION: FAIL` with individual error lines when a checked MBF21
runtime behavior regresses.

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
- DEHACKED dropped-item behavior by killing a probe `DoomImp` and verifying a
  nearby `Clip` count increase.

Infighting/projectile/splash groups are still included in the patch payload and
validated at parse/load time. A deeper behavior-level group interaction test is
best added in a later stage with a dedicated multi-actor combat sandbox map.
