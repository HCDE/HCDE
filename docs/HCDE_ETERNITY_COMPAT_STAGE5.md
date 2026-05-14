# HCDE Eternity Compatibility Stage 5

Stage 5 turns the Eternity compatibility layers into a repeatable real-mod
validation workflow.

The engine stages now cover:

- Stage 1: resource detection and startup flags
- Stage 2: supported `EMAPINFO` metadata
- Stage 3: `ExtraData` resolution and Eternity XLAT selection
- Stage 4: EDF manifest parsing and safe DoomEdNum actor binding
- Stage 5: generated validation reports and real-mod triage recipes

## Patcher Output

`tools/hcde_compat_patcher/hcde_compat_patcher.py` now builds an Eternity
validation profile when a scanned archive contains `EMAPINFO`, EDF, or
`ExtraData` resources.

Generated candidate folders can include:

- `report.md`: normal compatibility surface report plus an Eternity validation
  profile section
- `eternity_validation.md`: launch commands, expected HCDE log lines, and EDF
  thing hints for real-mod testing
- `metadata.json`: structured scanner output, including the Eternity profile
- `static/decorate.txt`: empty TODO stub for reviewed, original compat shims

The tool still does not copy mod code or assets. It only reads archive metadata
and short text signatures so the next manual compatibility patch has a focused
checklist.

## Validation Loop

For a real Eternity mod:

```powershell
python tools\hcde_compat_patcher\hcde_compat_patcher.py C:\Mods\example.pk3
```

Then open:

```text
build\compat-candidates\<mod-slug>\eternity_validation.md
```

Run the generated client and dedicated-server commands with the right IWAD path
and compare the logs against the expected HCDE lines:

```text
HCDE: Eternity compatibility resources detected:
HCDE: parsed EMAPINFO ...
HCDE: loaded ExtraData ...
HCDE: EDF compatibility parsed ...
HCDE: EDF actor binding candidates=...
```

## Triage Rules

- Native `MAPINFO`, `ZMAPINFO`, or `UMAPINFO` in the same archive wins over
  `EMAPINFO`. Do not debug missing EMAPINFO behavior until the log confirms
  which metadata path was used.
- `EMAPINFO` maps with `extradata` should log the resolved ExtraData lump and
  record counts when the map loads.
- EDF `thingtype` entries with DoomEdNums can bind only to already existing
  HCDE/ZScript actors and only when there is no registry conflict.
- EDF behavior fields such as states, health, radius, flags, sounds, damage,
  inventory, or projectile behavior are manual compat-patch work.
- Missing EDF-only actors should become narrow, original compatibility shims in
  `hcde_mod_compat.pk3` or a split mod-specific compat PK3.

## Done Criteria

A real Eternity mod is considered triaged when:

- the patcher generated an Eternity validation profile
- client startup reaches the expected Stage 1-4 log lines
- dedicated-server startup reaches the same parser and actor-binding summaries
- at least one EMAPINFO map loads far enough to resolve ExtraData, if present
- every reported missing actor, registry conflict, or unsupported EDF behavior
  has a written patch decision

That last point is important: Stage 5 is not "make every Eternity mod work by
magic." It is the repeatable bridge between automatic engine support and the
manual compatibility patches we are allowed to write under the golden rules.
