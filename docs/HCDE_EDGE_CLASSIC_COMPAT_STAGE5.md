# HCDE EDGE Classic Compatibility Stage 5

Stage 5 validates the Stage 4 candidate translations against HCDE startup logs.
It still does not enable EDGE Classic compatibility automatically.

## Generated File

EDGE Classic candidates now include:

```text
edge_stage5_validation.md
```

The file contains client and dedicated-server startup commands that load the
generated `edge_translation` folder with a chosen IWAD. It also lists log
patterns that should block a merge, such as `Script error`, `Unknown texture`,
`Unknown flat`, `Unknown sound`, `Can't find`, and `Fatal`.

## What Stage 5 Proves

Stage 5 checks whether generated `ANIMDEFS` and `SNDINFO` files are syntactically
accepted by HCDE for a specific IWAD/mod resource set.

It does not prove full gameplay compatibility because DDF things, weapons,
attacks, line specials, sectors, COAL, Lua, RTS, and RadTrig remain outside the
runtime adapter.

## First Real EDGE 1.35 Result

The first validation pass used:

```text
C:\Users\user\Downloads\Edge-1.35-win32\Edge-1.35\doom_ddf
```

The Stage 4 translation generated 23 `ANIMDEFS` entries and 119 `SNDINFO`
mappings. HCDE loaded the translation folder but stopped on an asset-sensitive
animation error when validating against the local EDGE 1.35 Doom IWADs:

```text
ANIMDEFS: Can't find SWATER1
Unknown texture SWATER4
```

That is a useful Stage 5 blocker. The generated animation set mixes definitions
that are not present in every target IWAD/resource set, so `ANIMDEFS` candidates
must be filtered or split by asset availability before they are moved into an
active compat PK3.

## Next Step

The next compatibility slice should add asset-aware filtering for generated
`ANIMDEFS` and `SNDINFO` candidates. Only after a filtered candidate starts
cleanly on both `hcde.exe -norun` and `hcdeserv.exe -norun` should it be copied
into `wadsrc_mod_compat` or a split EDGE compatibility PK3.
