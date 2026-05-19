# HCDE Invasion Stage 9 Validation Harness

This folder contains a practical compatibility smoke harness for Invasion Stage 9.

Script:

- `validate_invasion_stage9.py`

## What It Validates

1. `sv_gametype` query compatibility for:
   - `0` Co-op
   - `1` Deathmatch
   - `2` Team Deathmatch
   - `3` CTF fallback
   - `4` Invasion
2. Dedicated `changemap` transition (`MAP01 -> MAP02`) while query snapshots stay valid.
3. Invasion fallback spot compatibility path on classic maps using:
   - `invasion_nextwave`
   - `invasion_spots`
   - query-visible wave/spot metadata
4. Optional late-join observation (requires `hcde.exe` path).

## Usage (Windows Example)

```powershell
python HCDE/tests/invasion_stage9/validate_invasion_stage9.py `
  --server HCDE/build/RelWithDebInfo/hcdeserv.exe `
  --client HCDE/build/RelWithDebInfo/hcde.exe `
  --iwad C:/Games/DOOM2.WAD
```

If you only want dedicated/query checks, omit `--client`.

By default, the harness launches from the server binary directory. Use
`--workdir` only if your local layout requires a custom launch directory.

## Notes

- The harness talks directly to the launcher query UDP endpoint (`LAUNCHER_CHALLENGE`).
- Stage 8 appended invasion fields are treated as optional for decode compatibility.
- The script exits non-zero if any compatibility check fails.
