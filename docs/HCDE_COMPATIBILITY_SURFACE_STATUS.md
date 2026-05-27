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

## Runtime smoke check

Use the console command:

```text
hcde_compat_surfaces
```

The command resolves a representative ID24 actor set through `PClass::FindActor`.
That is intentionally a canonical-runtime check: imported compatibility actors
must land in the normal HCDE class registry before deeper runtime behavior is
added. MBF21 and XLAT surfaces are reported as data/script surfaces because they
are not standalone actor classes.

## Step 3 rules

- Add new compatibility imports as adapters into existing actor, map, command,
  and event concepts.
- Do not add a parallel EDF/DDF/COAL gameplay runtime before defining how it
  maps into HCDE classes, states, commands, and authority events.
- Keep presentation-only compatibility data separate from fixed-tic gameplay
  behavior.
- Any compatibility behavior that changes gameplay state must remain server
  authoritative in netgames.
