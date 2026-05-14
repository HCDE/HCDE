# HCDE Eternity Compatibility Stage 4

Stage 4 adds an HCDE-owned EDF manifest layer. It reads loaded Eternity EDF
resources, follows local includes, reports unsupported actor behavior, and
bridges only the safe DoomEdNum cases into HCDE's existing actor registry.

## What It Parses

The parser looks for user-loaded `EDFROOT`, `EDF`, `.edf` files, and entries
under an `edf/` folder. When an EDF root exists, HCDE treats that as the entry
point and follows `include` statements against the same archive first.

Supported manifest data:

- `thingtype` and `thingdelta` block names
- Alfheim-style thing headers with parent, DoomEdNum, and DeHackEd number
- body fields `doomednum`, `dehackednum`, `spawnid`, `inherits`, and
  `compatname`
- frame, sound, sprite, terrain, damage type, and other known EDF block counts
  for diagnostics

`stdinclude` and `userinclude` are reported but not imported. HCDE uses native
actors plus explicit compatibility patches instead of copying Eternity's base
EDF implementation.

## Actor Binding

After HCDE loads actor classes and normal MAPINFO DoomEdNums, the EDF layer can
bind a DoomEdNum to an actor only when all of these are true:

- the EDF thing has a DoomEdNum
- an HCDE or ZScript actor exists with the EDF thing name, or with its
  `compatname`
- the DoomEdNum is not already owned by another actor or special mapthing
- no later EDF definition replaces that same thing name
- no other active EDF thing claims the same DoomEdNum

When those checks pass, startup logs:

```text
HCDE: EDF mapped DoomEdNum 54321 to actor DoomImp from thingtype EdfSmokeImp.
```

When they fail, HCDE reports the reason instead of overwriting the registry.
This follows the no-silent-last-writer-wins rule in
`docs/HCDE_GOLDEN_RULES.md`.

## Limits

Stage 4 does not convert EDF actor behavior into ZScript. Fields such as
`states`, `radius`, `height`, `health`, flags, sounds, damage, inventory, and
projectile behavior are counted as unsupported behavior and should become
explicit `hcde_mod_compat.pk3` patches when a mod needs them.

This stage is still useful because it lets maps and ExtraData reach already
ported actors by number while telling us exactly which EDF-defined actors still
need manual compatibility work.

Stage 5 adds the real-mod validation workflow that consumes these diagnostics;
see `docs/HCDE_ETERNITY_COMPAT_STAGE5.md`.

## Verification

Tested commands:

```powershell
cmake --build build --target hcdeserv --config RelWithDebInfo -- /m:4
cmake --build build --target zdoom --config RelWithDebInfo -- /m:4
build\RelWithDebInfo\hcde.exe -norun -errorlog build\compat-smoke-stage4\parse.log -iwad C:\Users\User\Downloads\DOOM2.WAD -file build\compat-smoke-stage4\eternity-stage4-smoke.pk3
build\RelWithDebInfo\hcdeserv.exe -norun -errorlog build\compat-smoke-stage4\server-parse.log -iwad C:\Users\User\Downloads\DOOM2.WAD -file build\compat-smoke-stage4\eternity-stage4-smoke.pk3 -nosound -nomusic
```

Expected startup shape:

```text
HCDE: EDF compatibility parsed roots=1 includes=1 duplicate_includes=0 thingtypes=2 thingdeltas=0 doomednums=2 spawnids=1 dehacked=0 inherited=1 compatnames=1 behavior=2 frames=0 sounds=0 sprites=0 other_blocks=0 unknown_blocks=0 properties=7 unsupported_props=3 invalid=0 missing_includes=0.
HCDE: EDF mapped DoomEdNum 54321 to actor DoomImp from thingtype EdfSmokeImp.
HCDE: EDF actor binding candidates=2 applied=1 existing=0 missing_actors=1 registry_conflicts=0 edf_conflicts=0 duplicate_names=0 unsupported_behavior=2.
```

## References

- Eternity EDF overview: https://eternity.youfailit.net/wiki/EDF
- Eternity EDF basics and free-form syntax: https://eternity.youfailit.net/wiki/EDF_basics
- Eternity thingtype reference: https://eternity.youfailit.net/wiki/EDF_thing_reference
