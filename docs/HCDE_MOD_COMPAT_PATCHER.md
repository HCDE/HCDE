# HCDE Mod Compat Patcher

`tools/hcde_compat_patcher/hcde_compat_patcher.py` is a local helper for
building reviewable patches for `hcde_mod_compat.pk3`.

The tool scans a local `.wad`, `.pk3`, or `.zip` mod archive and writes a
candidate folder under `build/compat-candidates/<mod-slug>/`. Candidates are
not active game resources. They are reports, provenance metadata, and empty
patch stubs for a human to review before changing `wadsrc_mod_compat`.

## Golden-Rule Boundary

- Do not copy third-party mod source or assets into HCDE unless the mod license
  explicitly allows it.
- Use generated candidates as notes and TODOs, not as automatically approved
  imports.
- Keep the original mod outside the repository unless its provenance and
  license have been audited.
- Prefer HCDE-owned compatibility shims that target behavior, not repackaged
  mod content.

## Basic Use

```powershell
python tools\hcde_compat_patcher\hcde_compat_patcher.py C:\Mods\example.pk3
```

Useful output files:

- `report.md`: compatibility surfaces and candidate checks.
- `eternity_validation.md`: generated when Eternity resources are detected;
  contains HCDE launch commands, expected log lines, and EDF/EMAPINFO triage
  notes.
- `metadata.json`: source path, SHA-256, archive entries, and scanner results.
- `suggested_hcde_mod_compat_entry.cpp.txt`: matcher stub for review.
- `static/decorate.txt`: empty DECORATE patch stub with TODO notes.

To use a candidate:

1. Confirm the source mod license and provenance.
2. Read `report.md` and identify the actual HCDE behavior mismatch.
3. Write original compatibility code in `wadsrc_mod_compat`.
4. Add a narrow matcher in `src/common/engine/hcde_mod_compat.cpp`.
5. Build and test with the original mod loaded normally.

## Current Scanner Checks

The scanner detects:

- DECORATE-style files and actor replacements.
- ZScript files, which usually need manual review.
- MAPINFO/UMAPINFO/ZMAPINFO/EMAPINFO metadata.
- Eternity Engine signals: EDF, EDFROOT, ExtraData, and EMAPINFO.
- EDGE Classic signals: DDF, RTS, COAL, and Lua-style resources.
- DeHackEd/BEX data.
- ACS/input-related strings that may assume local player zero.
- `A_RailAttack`, because HCDE already has a dedicated-server compatibility
  case in this area.

Eternity and EDGE Classic findings are intentionally marked as manual layer
work. HCDE does not yet have complete compatibility layers for those ports, so
the generated candidate should guide design and testing rather than produce an
automatic patch.

HCDE's built-in Eternity layer starts in
`docs/HCDE_ETERNITY_COMPAT_STAGE1.md` and
`docs/HCDE_ETERNITY_COMPAT_STAGE2.md`, with ExtraData/XLAT wiring in
`docs/HCDE_ETERNITY_COMPAT_STAGE3.md`, EDF manifest support in
`docs/HCDE_ETERNITY_COMPAT_STAGE4.md`, and the generated real-mod validation
workflow in `docs/HCDE_ETERNITY_COMPAT_STAGE5.md`. It detects Eternity
resources at startup, applies a supported subset of `EMAPINFO`, routes
Doom-format EMAPINFO maps through Eternity XLAT by default, and safely binds
EDF DoomEdNums to already-ported HCDE/ZScript actors when there is no registry
conflict. EDF actor behavior and advanced EMAPINFO properties remain manual
compatibility work.

PK7/7z archives are not supported yet by the standard-library scanner. Unpack
or repack them as PK3 when doing compatibility analysis.
