# HCDE EDGE Classic Compatibility Stage 4

Stage 4 adds the first narrow translation slice on top of the neutral DDF
manifest. It still does not enable translated resources automatically.

## Translated Candidate Types

The compat patcher now writes candidate files under:

```text
edge_translation/
```

Current outputs:

- `ANIMDEFS.txt` for DDF `animations` entries with simple `TYPE`, `FIRST`,
  `LAST` or `SEQUENCE`, and `SPEED` values
- `SNDINFO.txt` for DDF `sounds` entries with `LUMP_NAME` or `LUMP`

These files are generated from normalized manifest data, not copied DDF source.
They must be reviewed before anything is moved into `wadsrc_mod_compat`.

## Boundary

Stage 4 deliberately avoids:

- thing, weapon, attack, line, and sector behavior
- state-machine translation
- sound priority, singularity, and loop semantics beyond direct name-to-lump
  mappings
- COAL, Lua, RTS, and RadTrig execution

Those surfaces stay visible in `edge_manifest.json` and `edge_validation.md`
for later adapter decisions.

## Review Workflow

Run the patcher:

```powershell
python tools\hcde_compat_patcher\hcde_compat_patcher.py C:\Games\EDGE\doom_ddf
```

Review:

- `edge_manifest.json` for source properties and manual fields
- `edge_translation/ANIMDEFS.txt` for generated animation definitions
- `edge_translation/SNDINFO.txt` for generated sound mappings

Only after that review should a maintainer copy narrow HCDE-owned compatibility
definitions into `wadsrc_mod_compat` or a split compatibility PK3.

## Next Stage

Stage 5 is documented in `docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE5.md`. It is a
real-mod validation pass for the Stage 4 candidate outputs: load the generated
translations beside a selected IWAD/mod resource set, compare startup logs, and
record which DDF categories are ready for an actual runtime adapter.
