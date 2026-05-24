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
5. Optional per-case debug trace exports (`--trace-save-dir`).
6. Optional JSON summary output (`--summary-dir`).

## Usage (Windows Example)

```powershell
python HCDE/tests/invasion_stage9/validate_invasion_stage9.py `
  --server HCDE/build/RelWithDebInfo/hcdeserv.exe `
  --client HCDE/build/RelWithDebInfo/hcde.exe `
  --iwad C:/Games/DOOM2.WAD `
  --label "invasion compatibility suite" `
  --trace-save-dir C:/Users/user/DoomConnector6/HCDE/traces/stage9 `
  --summary-dir C:/Users/user/DoomConnector6/HCDE/traces/stage9
```

Dry-run mode (plan check only):

```powershell
python HCDE/tests/invasion_stage9/validate_invasion_stage9.py `
  --server HCDE/build/RelWithDebInfo/hcdeserv.exe `
  --iwad C:/Games/DOOM2.WAD `
  --dry-run
```

If you only want dedicated/query checks, omit `--client`.

By default, the harness launches from the server binary directory. Use
`--workdir` only if your local layout requires a custom launch directory.

## Notes

- The harness talks directly to the launcher query UDP endpoint (`LAUNCHER_CHALLENGE`).
- Stage 8 appended invasion fields are treated as optional for decode compatibility.
- The script exits non-zero if any compatibility check fails.
- For structured output, provide `--summary-dir` and `--label`.
- When `--trace-save-dir` is provided, passing cases attempt `debugtracesave` and emit `[trace]` lines with saved paths.
