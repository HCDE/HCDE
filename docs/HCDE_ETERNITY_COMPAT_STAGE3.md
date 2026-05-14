# HCDE Eternity Compatibility Stage 3

Stage 3 connects `EMAPINFO` maps to the existing Eternity ExtraData and XLAT
support already present in HCDE.

## ExtraData

When an `EMAPINFO` map uses `extradata`, HCDE now resolves the referenced lump
against the same archive first. If the same-archive lookup fails, it falls back
to the normal global lookup and reports missing references in the EMAPINFO
summary.

At map load time, ExtraData is initialized once and reports the records it
loaded:

```text
HCDE: loaded ExtraData mod.pk3:extradata.txt for map MAP01: linedefs=4 sectors=2 mapthings=1.
```

If a map references missing ExtraData, HCDE logs that explicitly instead of
silently continuing with empty records.

## XLAT

Doom-format maps defined by `EMAPINFO` now default to `xlat/eternity.txt` when
no native `translator` setting and no command-line `-xlat` override is present.
This lets Eternity linedef 270 route into ExtraData records and enables the
supported subset of Eternity's Doom-format linedef translations.

The command-line `-xlat` option still wins when the user supplies one.

## Limits

This stage does not make every Eternity linedef or portal behavior work. The
existing `xlat/eternity.txt` comments still apply: some translated specials are
partial, unsupported, or implemented differently in HCDE. Stage 3's job is to
select and diagnose the compatibility path, not to complete every gameplay
special.
