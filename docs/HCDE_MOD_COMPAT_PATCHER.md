# HCDE Mod Compat Patcher

`tools/hcde_compat_patcher/hcde_compat_patcher.py` is a local helper for
building reviewable patches for `hcde_mod_compat.pk3`.

The tool scans a local `.wad`, `.pk3`, `.zip`, or unpacked mod folder and writes
a candidate folder under `build/compat-candidates/<mod-slug>/`. Candidates are
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
python tools\hcde_compat_patcher\hcde_compat_patcher.py C:\Games\EDGE\doom_ddf
```

Useful output files:

- `report.md`: compatibility surfaces and candidate checks.
- `eternity_validation.md`: generated when Eternity resources are detected;
  contains HCDE launch commands, expected log lines, and EDF/EMAPINFO triage
  notes.
- `edge_validation.md`: generated when EDGE Classic resources are detected;
  contains DDF section manifests, include load order, conflict reports,
  COAL/Lua/RTS script lists, and the current report-only runtime boundary.
- `edge_manifest.json`: generated when EDGE Classic DDF resources are detected;
  contains the Stage 3 neutral DDF bridge entries with supported and manual
  properties split apart, plus duplicate property keys that need adapter
  decisions instead of silent last-writer-wins handling.
- `edge_translation/ANIMDEFS.txt` and `edge_translation/SNDINFO.txt`: generated
  when EDGE Classic animation or sound definitions can be expressed as narrow
  Stage 4 candidate translations.
- `edge_stage5_validation.md`: generated with HCDE client/server startup
  commands and log triage patterns for validating Stage 4 candidate
  translations against a specific IWAD/resource set.
- `metadata.json`: source path, SHA-256, archive entries, and scanner results.
- `hcde_engine_change_report.md` / `hcde_engine_change_report.json`: unresolved
  checks mapped to likely HCDE engine/compatibility-layer work.
- `suggested_hcde_mod_compat_entry.cpp.txt`: matcher stub for review.
- `static/decorate.txt`: empty DECORATE patch stub with TODO notes.

Optional auto-attempt mode:

```powershell
python tools\hcde_compat_patcher\hcde_compat_patcher.py C:\Mods\example.pk3 --auto-attempt
```

When `--auto-attempt` is enabled and safe transformations are available, the
candidate also includes:

- `auto_patch_attempt/<slug>.hcde-autoattempt.pk3`: a local test copy with safe
  heuristic transforms applied.
- `auto_patch_attempt/autopatch_report.json`: machine-readable transform report.
- `auto_patch_attempt/README.md`: summary of what was changed.

Current safe heuristics:

- Relocate non-asset files found in sprite/texture/sound namespaces to
  `hcde_compat/non_asset/...` inside the generated local test copy.
- Attempt to repair MAPINFO text assignments that have no clear terminator by
  inserting a trailing `;` (or replacing a trailing `,` with `;`).

This mode is for local smoke testing only. Do not commit generated third-party
content or treat auto-attempt output as approved compat code. If a candidate
still needs engine-side work, use `hcde_engine_change_report.md` to see which
HCDE areas likely need changes.

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
- EDGE Classic signals: DDF, RTS, COAL, and Lua-style resources, including
  loose DDF folders, split DDF files, and DDF lumps embedded in WADs inside
  PK3s.
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

EDGE Classic support starts at `docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE1.md`,
`docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE2.md`, and
`docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE3.md`, and
`docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE4.md`. Stage 1 lists DDF categories,
section names, reset directives, and COAL/Lua/RTS/RadTrig files. Stage 2
resolves DDF includes into load order and reports missing includes, include
cycles, and duplicate definitions. Stage 3 emits a neutral DDF manifest that
separates bridgeable properties from manual adapter decisions and flags repeated
scalar property keys. Stage 4 writes candidate `ANIMDEFS` and `SNDINFO`
translations for narrow data-only animation and sound mappings without enabling
them automatically. Stage 5 validates those candidate translations with HCDE
startup logs and records asset-sensitive blockers before anything is merged.

PK7/7z archives are not supported yet by the standard-library scanner. Unpack
or repack them as PK3 when doing compatibility analysis.
