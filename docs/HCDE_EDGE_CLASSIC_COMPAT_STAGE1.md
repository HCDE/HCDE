# HCDE EDGE Classic Compatibility Stage 1

Stage 1 adds a report-only EDGE Classic manifest to the HCDE mod compatibility
tool. It recognizes DDF, COAL, Lua, RTS, and RadTrig resources and records what
the mod is asking for without importing third-party definitions or executing
scripts.

## Boundary

- DDF is parsed as metadata only.
- COAL, Lua, RTS, and RadTrig files are listed, not executed.
- Includes, reset directives, and definition categories are reported before any
  future runtime adapter can merge data.
- Generated candidates remain review aids under `build/compat-candidates`.

This follows the golden rules: HCDE should prefer neutral adapters first and
must not silently let overlapping mod definition systems become hidden
last-writer-wins behavior.

## Detected Resources

The scanner recognizes classic EDGE DDF lumps such as `DDFTHING`, `DDFWEAP`,
`DDFSFX`, `DDFLINE`, `DDFSECT`, `DDFLEVL`, and the other `DDF*` categories. It
also detects `.ddf` files, loose `doom_ddf/` folders from classic EDGE installs,
files under `ddf/`, and DDF lumps embedded in WADs inside PK3s.

For each detected DDF text resource, Stage 1 records:

- the DDF category, such as things, weapons, sounds, lines, or sectors
- bracketed sections, such as `[IMP]` or `[RIFLE:BASE]`
- simple property keys inside each section
- `#INCLUDE` targets
- `#VERSION` and `#CLEARALL` style directives

Script-like resources are recorded as `coal`, `lua`, `rts`, `radtrig`, or
generic script entries with their archive path and size.

## Generated Files

Run the patcher against an EDGE-style mod:

```powershell
python tools\hcde_compat_patcher\hcde_compat_patcher.py C:\Mods\example.pk3
python tools\hcde_compat_patcher\hcde_compat_patcher.py C:\Games\EDGE\doom_ddf
```

The candidate report includes an `EDGE Classic Profile` section. When EDGE
resources are found, the tool also writes `edge_validation.md`, which gives
startup commands and a triage table for DDF sections and script files.

## Next Stage Candidates

Stage 2 is documented in `docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE2.md` and adds
include/load-order/conflict reports. Stage 3 is documented in
`docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE3.md` and adds a neutral DDF manifest.
Stage 4 is documented in `docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE4.md` and adds
candidate `ANIMDEFS`/`SNDINFO` translations. Later stages should stay small and
explicit:

1. Translate a narrow subset of DDF things, weapons, and line specials
   only when the behavior can be expressed in existing HCDE systems.
2. Design a COAL/Lua/RTS sandbox or compatibility API before running script
   content.

Anything outside the supported subset should stay visible in the report so mod
compatibility work is deliberate instead of accidental.
