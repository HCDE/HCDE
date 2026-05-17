# HCDE Shadow Renderer Stage 2 QA

Date: 2026-05-17
Build tested: `C:\Users\user\DoomConnector6\HCDE\build\RelWithDebInfo\hcde.exe`
Runtime archive: `C:\Users\user\DoomConnector6\HCDE\build\hcde.pk3`

Stage 2 validates the Stage 1 enhanced shadow controls before adding more
renderer behavior. The goal is to make sure the preset path is stable across
plain Doom II and the local compatibility stress mods before moving into
modder-facing controls.

## Launch Pattern

Each smoke test used the local Windows build, Doom II as the IWAD, Vulkan, the
enhanced shadow preset command, and a short timed MAP01 launch:

```powershell
.\build\RelWithDebInfo\hcde.exe -iwad C:\Users\user\Downloads\DOOM2.WAD -window -width 640 -height 480 -file <mods> +logfile <log> +hcde_enhancedshadows +map map01 +wait 70 +quit
```

## Test Matrix

| Case | Add-ons | Result |
| --- | --- | --- |
| Doom II baseline | none | Passed. Preset applied, Vulkan initialized, no fatal errors. |
| Complex Doom | `complex-doom.v26a2.pk3`, `complex-doom.v26a2-nopush-v6.pk3` | Passed. Preset applied, Vulkan initialized, no fatal errors. Existing duplicate class/editor warnings remain. |
| Brutal Doom v22 test | `brutalv22test4.pk3` | Passed. Preset applied, Vulkan initialized, no fatal errors. Existing actor/drop/decal warnings remain. |
| The Island | `theisland_full.pk3` | Passed. Preset applied, Vulkan initialized, no fatal errors. Existing MAPINFO/texture/drop warnings remain. |
| Aliens Eradication | `ALIENS_ERADICATION_TC_2_0.pk3`, `ERADICATION_MAPSET_2_0.wad` | Passed. Preset applied, Vulkan initialized, no fatal errors. |
| Monsters and Addons clean patch | `Monstersandaddonsv2-hcde-clean.pk3` | Passed. Preset applied, Vulkan initialized, no fatal errors. Existing compatibility warnings remain noisy. |

## Logs

Logs were written under `C:\Users\user\Downloads`:

- `hcde-shadowqa-doom2-baseline.log`
- `hcde-shadowqa-complex-doom.log`
- `hcde-shadowqa-brutalv22.log`
- `hcde-shadowqa-the-island.log`
- `hcde-shadowqa-aliens-eradication.log`
- `hcde-shadowqa-monsters-addons-clean.log`

Summary counts:

| Log | Fatal errors | Script warnings |
| --- | ---: | ---: |
| `hcde-shadowqa-doom2-baseline.log` | 0 | 0 |
| `hcde-shadowqa-complex-doom.log` | 0 | 15 |
| `hcde-shadowqa-brutalv22.log` | 0 | 0 |
| `hcde-shadowqa-the-island.log` | 0 | 0 |
| `hcde-shadowqa-aliens-eradication.log` | 0 | 0 |
| `hcde-shadowqa-monsters-addons-clean.log` | 0 | 441 |

## Findings

- The enhanced shadow preset is reachable from startup commands and applies
  successfully in every tested case.
- Vulkan initializes in every tested case.
- No smoke test produced a renderer fatal, VM abort, or startup crash.
- The loudest issues are still mod compatibility warnings, not shadow renderer
  failures.
- Stage 2 does not add Quake-style or Doom 3-style shadow behavior. Those stay
  reserved for the later clean-room shadow stages.

## Stage 3 Input

Stage 3 can safely focus on modder-facing controls and defaults:

1. Add a compact HCDE shadow quality/profile setting instead of requiring users
   to tune multiple advanced CVARs by hand.
2. Keep the Stage 1 preset as a fast recovery path.
3. Avoid treating script/parser warnings as renderer failures unless they block
   level startup or asset loading.
