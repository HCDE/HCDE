# HCDE Eternity Compatibility Stage 1

Stage 1 adds an HCDE-owned Eternity compatibility module under
`src/common/engine/hcde_eternity_compat.*`.

This stage started as report-only. It does not import Eternity Engine code or
copy mod assets. Its point is to give later stages one stable place to detect
Eternity mod surfaces and expose module flags before EMAPINFO, ExtraData, XLAT,
and EDF work begins.

Stage 2 now parses a supported subset of `EMAPINFO`; see
`docs/HCDE_ETERNITY_COMPAT_STAGE2.md`. Stage 3 wires those maps into
ExtraData and XLAT; see `docs/HCDE_ETERNITY_COMPAT_STAGE3.md`. Stage 4 adds
the EDF manifest layer; see `docs/HCDE_ETERNITY_COMPAT_STAGE4.md`. Stage 5
adds the generated real-mod validation workflow; see
`docs/HCDE_ETERNITY_COMPAT_STAGE5.md`.

## Detected Resources

After the filesystem mounts IWADs and user-loaded resources, HCDE scans PWAD/PK3
content for:

- `EMAPINFO`
- `EDF` and `EDFROOT`
- files with the `.edf` extension or entries under an `edf/` folder
- `EXTRADATA` and `extradata.txt`

If any of these are present, startup logs a summary like:

```text
HCDE: Eternity compatibility resources detected: EMAPINFO=1 EDF=2 ExtraData=1.
HCDE: Eternity compatibility will parse supported EMAPINFO, ExtraData, XLAT, and EDF manifest properties; unsupported behavior still needs compat patches.
HCDE: Eternity resource: example.pk3:emapinfo
```

## Module Flags

Later stages can check these flags:

- `HCDE_ETERNITYCOMPAT_EMAPINFO`
- `HCDE_ETERNITYCOMPAT_EDF`
- `HCDE_ETERNITYCOMPAT_EXTRADATA`

Use `HCDE_EternityCompat_IsActive()` for feature gates and
`HCDE_EternityCompat_GetFlags()` for diagnostics.

## Boundary

This module follows `docs/HCDE_GOLDEN_RULES.md`: it is a neutral detection and
policy hook first. Actual compatibility behavior should be added as HCDE-owned
parsers, adapters, reports, or shims, with conflicts reported instead of silent
last-writer-wins behavior.
