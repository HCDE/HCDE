# HCDE Eternity Compatibility Stage 2

Stage 2 adds an HCDE-owned parser for the compatible subset of Eternity
`EMAPINFO`.

The parser is intentionally conservative. It applies fields that map cleanly to
HCDE's existing `level_info_t` metadata and reports unsupported or invalid
properties instead of silently ignoring behavior HCDE does not implement yet.

## Load Order

`EMAPINFO` participates in normal map-info load order after the base HCDE
`MAPINFO` is parsed.

If one archive ships both `EMAPINFO` and a native `MAPINFO`, `ZMAPINFO`, or
`UMAPINFO`, HCDE skips the `EMAPINFO` from that same archive and logs the
decision. This avoids silent last-writer-wins conflicts.

## Supported Map Properties

HCDE currently applies:

- map names from `[MAPxx]` or `[ExMy]` sections
- `levelname`, `inter-levelname`, `creator`, `levelnum`, `levelpic`
- `nextlevel`, `nextsecret`, `partime`
- `enterpic`, `interpic`
- `extradata`, `acsscript`
- `disable-jump`, `aircontrol`, `gravity`
- `colormap`, `defaultenvironment`
- `doublesky`, `lightning`, `noautosequences`, `killstats`
- `music`, `intermusic`
- `skyname`, `sky2name`, `skydelta`, `sky2delta`, `unevenlight`
- `boss-specials` for `MAP07_1`, `MAP07_2`, `E1M8`, `E2M8`, `E3M8`,
  `E4M6`, and `E4M8`

Eternity fixed-point values for `aircontrol` and `gravity` are converted from
`65536 == 1.0` into HCDE's existing scalar fields.

Doom-family `music` and `intermusic` entries without a prefix are normalized to
`D_` names. Heretic entries are normalized to `MUS_` names.

## Reported But Not Yet Implemented

Stage 3 now wires EMAPINFO maps into Eternity ExtraData and XLAT support; see
`docs/HCDE_ETERNITY_COMPAT_STAGE3.md`.

Stage 2 still does not implement EMAPINFO finale semantics, EDF intermission
names, sound environment aliases, map action blocks, custom automap styles, or
advanced sector-colormap behavior. Those are reported in the startup log with
line numbers and are candidates for later compatibility stages.

Expected log shape:

```text
HCDE: parsed EMAPINFO mod.pk3:emapinfo: maps=2 properties=20 applied=16 extradata=1 missing=0 unsupported=3 invalid=1.
HCDE: EMAPINFO note: MAP01 property 'finaletype' at line 18 is not handled by the current compatibility layer.
```
