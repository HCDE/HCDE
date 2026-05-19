# HCDE Invasion Stage 9: Testing and Compatibility

Stage 9 focuses on regression safety across legacy multiplayer modes while
Invasion (`sv_gametype 4`) remains active and query-visible.

## Scope

The validation target is exactly the Stage 9 plan surface:

- co-op, deathmatch, team deathmatch, CTF fallback, and invasion query behavior
- dedicated-server startup and runtime compatibility
- launcher query stability during mode and map operations
- late-join query visibility (optional scripted path)
- old invasion-map compatibility through fallback invasion spawn spots

## Added Validation Harness

Automated script:

- `tests/invasion_stage9/validate_invasion_stage9.py`

Support file:

- `tests/invasion_stage9/README.md`

The harness starts `hcdeserv`, queries the UDP launcher endpoint, parses the
legacy snapshot + Stage 8 appended invasion fields, and enforces compatibility
assertions per scenario.

## Scenarios Covered

1. **Mode matrix query checks** (`sv_gametype 0/1/2/3/4`)
   - verifies returned mode id and mode display name
   - verifies non-invasion modes keep invasion state disabled
   - verifies invasion mode returns invasion state metadata
2. **Dedicated map transition**
   - runs `changemap` from server console stdin
   - validates query map changes from primary map to transition map
3. **Classic-map invasion fallback path**
   - runs `invasion_nextwave` and `invasion_spots`
   - validates query-visible invasion wave + spot counters become active
4. **Late join visibility (optional)**
   - requires `hcde.exe`
   - validates query player count increases during dedicated join window

## Run Command (Example)

```powershell
python HCDE/tests/invasion_stage9/validate_invasion_stage9.py `
  --server HCDE/build/RelWithDebInfo/hcdeserv.exe `
  --client HCDE/build/RelWithDebInfo/hcde.exe `
  --iwad C:/Games/DOOM2.WAD
```

## Expected Result

When all checks pass, the harness prints:

`[pass] Stage 9 compatibility harness completed successfully`

Any failed compatibility condition returns non-zero with a `[fail]` message so
CI/local packaging scripts can gate releases.
