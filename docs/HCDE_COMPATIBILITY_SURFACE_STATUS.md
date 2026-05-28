# HCDE Compatibility Surface Status

This note is the Step 3 execution checkpoint for the roadmap integration plan.
It keeps compatibility imports classified as surfaces that map into HCDE's
canonical runtime, not separate gameplay engines.

## Current surfaces

- **MBF21**: core compatibility actions and constants are present in
  `wadsrc/static/zscript/actors/mbf21.zs` and related constants. ID24 actors use
  these actions, so MBF21 remains a prerequisite compatibility layer.
- **ID24**: Doom-family actors, weapons, ammo, ambient objects, and dynamic lights
  are present under `wadsrc/static/zscript/actors/doom/id24/` and
  `wadsrc_lights/static/filter/doom.id/gldefs.txt`.
- **Eternity**: line translation support is data-driven through
  `wadsrc/static/xlat/eternity.txt`, including EMAPINFO-oriented line mapping.
- **EDGE**: EDGE linetypes are represented in `wadsrc/static/xlat/base.txt`; map
  data flags also expose EDGE-facing surfaces through `mapdata.zs`.

## Step 7 runtime smoke check

Use the console command:

```text
hcde_compat_surfaces
```

The command resolves the full in-tree ID24 actor/weapon surface through
`PClass::FindActor`, grouped as weapons, ammo, monsters, projectiles,
decorations, gore, and ambient actors. Weapon and ammo entries also validate
their expected canonical base classes (`Weapon` and `Ammo`). This is
intentionally a canonical-runtime check: imported compatibility actors must land
in the normal HCDE class registry before deeper runtime behavior is added. MBF21
and XLAT surfaces are reported as data/script surfaces because they are not
standalone actor classes.

Known-good Step 7 target:

```text
command: hcde_compat_surfaces
expected: summary: missing=0 id24-checked=60
```

## Step 8 runtime smoke check

The same command also validates the MBF21/Dehacked action surface. It checks the
standard `dehsupp.txt` MBF21 action mappings against the canonical `Actor` and
`Weapon` action symbols implemented in `wadsrc/static/zscript/actors/mbf21.zs`.

Known-good Step 8 target:

```text
command: hcde_compat_surfaces
expected: summary: missing=0 id24-checked=60 mbf21-actions=23
```

## Step 9 runtime smoke check

The same command also validates the built-in Eternity/EDGE XLAT surface by
reading the packaged translator/data lumps and checking for the entries that
represent the high-risk compatibility categories:

- Eternity ExtraData line indirection (`xlat/eternity.txt`, line 270).
- Eternity 3D midtextures, sector portals, line portals, slopes, and sector
  initialization.
- EDGE 3D floors, translucent lines, texture scrollers, and MBF21 scroll
  extensions.
- Shared mapdata flags/types used by EDGE thin floors and Eternity linked
  portals.

Known-good Step 9 target:

```text
command: hcde_compat_surfaces
expected: summary: missing=0 id24-checked=60 mbf21-actions=23 xlat-tokens=12 total-checked=95
```

Tool-level regression coverage:

```text
python -m unittest tools.hcde_compat_patcher.tests.test_hcde_compat_patcher
```

That suite includes synthetic Eternity `EMAPINFO`/ExtraData/EDF scans and EDGE
DDF/COAL/RTS scans, plus embedded-WAD and include-order cases. The local
`wadsrc_mod_compat/pink_valley_eng` package remains the in-tree Pink Valley
compatibility checkpoint for map transition/event-handler packaging.

## Step 3 rules

- Add new compatibility imports as adapters into existing actor, map, command,
  and event concepts.
- Do not add a parallel EDF/DDF/COAL gameplay runtime before defining how it
  maps into HCDE classes, states, commands, and authority events.
- Keep presentation-only compatibility data separate from fixed-tic gameplay
  behavior.
- Any compatibility behavior that changes gameplay state must remain server
  authoritative in netgames.
- ID24 coverage should fail loudly if any in-tree ID24 ZScript actor class is
  missing from the canonical runtime registry or if weapon/ammo classes stop
  deriving from their expected base classes.
- MBF21/Dehacked coverage should fail loudly if any standard MBF21 Dehacked
  action alias loses its `Actor`/`Weapon` runtime action symbol.
- Eternity/EDGE coverage should fail loudly if packaged XLAT/mapdata support
  drops the translator tokens needed by representative ExtraData, portal,
  slope, 3D-floor, translucent-line, and scroller maps.
