# HCDE Invasion Stage 8: UI and Server Tools

Stage 8 exposes invasion setup and live state through dedicated-server tools and
launcher query snapshots so admins and external clients can operate invasion
servers without source edits.

## What This Stage Adds

1. Dedicated server GUI settings now include invasion timing, wave, budget, boss,
   and spawn policy CVars in the advanced setting panel.
2. A dedicated preset (`Invasion Standard`) applies a working invasion baseline
   for hosted servers.
3. Launcher query snapshots now include invasion mode/runtime metadata:
   - game mode id and label
   - invasion state and state timer
   - wave/max/budget/spawn/cleared counters
   - boss-wave flag
   - spawn planner and fallback metadata
4. Query decode remains backward-compatible by treating appended invasion payload
   fields as optional.

## Admin-Facing Invasion CVars

The dedicated server GUI can now set:

- `sv_invasioncountdowntime`
- `sv_invasionspawntime`
- `sv_invasioncleanuptime`
- `sv_invasionintermissiontime`
- `sv_invasionresulttime`
- `sv_invasionwaves`
- `sv_invasionbasebudget`
- `sv_invasionbudgetstep`
- `sv_invasionperplayer`
- `sv_invasionspawninterval`
- `sv_invasionspawnburst`
- `sv_invasionbosswaveevery`
- `sv_invasionbossbonus`
- `sv_invasionspotusemaptags`
- `sv_invasionspotfallback`

## Verification

Local build smoke:

```powershell
cmake --build HCDE/build --config RelWithDebInfo --target hcdeserv
```

Expected result: `hcdeserv` links successfully and launches with invasion settings
visible in the dedicated server control surface.
