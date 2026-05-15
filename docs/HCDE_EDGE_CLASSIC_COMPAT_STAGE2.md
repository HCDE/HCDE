# HCDE EDGE Classic Compatibility Stage 2

Stage 2 turns the Stage 1 DDF manifest into an ordered review model. It still
does not import, translate, or execute EDGE Classic definitions.

## Added Reports

The compat patcher now resolves DDF `#INCLUDE` directives against the scanned
archive or unpacked folder and emits:

- a deterministic DDF load order
- resolved include targets
- missing include targets
- include cycles
- `#CLEARALL` reset directive locations
- duplicate DDF section definitions by category and section name

These reports exist so HCDE can avoid hidden last-writer-wins behavior when a
future adapter starts translating any DDF subset.

## Why This Stage Matters

EDGE DDF mods can split definitions across many files, reset whole definition
sets, and redefine names that already exist. HCDE needs to know which file wins
and why before mapping those definitions to actors, weapons, sounds, lines, or
sectors.

Stage 2 makes those merge decisions visible but leaves the actual behavior
unchanged.

## Workflow

Run the patcher against an EDGE-style archive or loose DDF folder:

```powershell
python tools\hcde_compat_patcher\hcde_compat_patcher.py C:\Games\EDGE\doom_ddf
```

Then read:

- `report.md` for summary checks
- `edge_validation.md` for load order, include targets, conflicts, and scripts
- `metadata.json` for structured scanner output

## Next Stage

Stage 3 is documented in `docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE3.md`. It adds an
HCDE-owned neutral DDF manifest bridge that records bridgeable properties while
keeping unsupported properties visible for later adapter decisions.
