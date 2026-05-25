# HCDE Step 12 Netcode Stress Harness

This folder contains the first repeatable stress harness for the HCDE netcode
overhaul. It is meant to turn invasion, coop, DM, late join, launcher-query
health, and network-shaper runs into comparable logs instead of one-off manual
tests.

Script:

- `netcode_step12_stress.py`

## What It Exercises

1. Dedicated server startup for invasion, coop, and DM.
2. Launcher query responsiveness while the server is ticking.
3. Shared actor migration through `net_migration`.
4. Actor queue, relevance, lane, migration, and simulation LOD pressure through
   `net_stressreport`.
5. Optional join pressure using one or more `hcde` client processes.
6. Optional master-server advertising with `--advertise --master host:port`.
7. Optional mod-specific pressure commands through `--server-command` and
   `--periodic-command`.
8. Optional advanced-debugger trace export with `--trace-save-dir`.
9. Optional pressure presets with `--pressure-preset`.
10. Optional comparable JSON summaries with `--summary-dir`.
11. Optional stress-metrics enforcement with `--require-stress-metrics`.
12. Query-health floor checks with `--min-queries`.
13. Optional migration health checks with `--require-migration` and
   migration threshold flags.
14. Optional native live gameplay checks with `--require-native-live`, which
   parses final `net_profile` output and fails on missing native `HCIN`/`HCSN`
   traffic, encode failures, capability rejects, replay storms, or legacy
   gameplay rejects.

## Usage

Dry-run the planned cases:

```powershell
python C:\Users\user\DoomConnector6\HCDE\tests\netcode_step12\netcode_step12_stress.py --dry-run
```

Run a short invasion-only smoke:

```powershell
python C:\Users\user\DoomConnector6\HCDE\tests\netcode_step12\netcode_step12_stress.py `
  --server C:\Users\user\DoomConnector6\HCDE\build\RelWithDebInfo\hcdeserv.exe `
  --iwad C:\Games\DOOM2.WAD `
  --cases invasion `
  --duration 20 `
  --wave-pulses 2
```

Run all three broad engine cases with client join pressure:

```powershell
python C:\Users\user\DoomConnector6\HCDE\tests\netcode_step12\netcode_step12_stress.py `
  --server C:\Users\user\DoomConnector6\HCDE\build\RelWithDebInfo\hcdeserv.exe `
  --client C:\Users\user\DoomConnector6\HCDE\build\RelWithDebInfo\hcde.exe `
  --iwad C:\Games\DOOM2.WAD `
  --cases invasion coop dm `
  --client-count 2 `
  --duration 60
```

Run a native gameplay cutover smoke:

```powershell
python C:\Users\user\DoomConnector6\HCDE\tests\netcode_step12\netcode_step12_stress.py `
  --server C:\Users\user\DoomConnector6\HCDE\build\Debug\hcdeserv.exe `
  --client C:\Users\user\DoomConnector6\HCDE\build\Debug\hcde.exe `
  --iwad C:\Games\DOOM2.WAD `
  --cases coop `
  --client-count 1 `
  --duration 20 `
  --require-native-live `
  --summary-dir C:\Users\user\DoomConnector6\HCDE\traces\step12-native
```

Run against a heavy mod or projectile torture map:

```powershell
python C:\Users\user\DoomConnector6\HCDE\tests\netcode_step12\netcode_step12_stress.py `
  --server C:\Users\user\DoomConnector6\HCDE\build\RelWithDebInfo\hcdeserv.exe `
  --client C:\Users\user\DoomConnector6\HCDE\build\RelWithDebInfo\hcde.exe `
  --iwad C:\Games\DOOM2.WAD `
  --server-file C:\Mods\heavy-invasion.pk3 `
  --client-file C:\Mods\heavy-invasion.pk3 `
  --map MAP32 `
  --cases invasion `
  --client-count 4 `
  --duration 180 `
  --trace-save-dir C:\Users\user\DoomConnector6\HCDE\traces\step12 `
  --summary-dir C:\Users\user\DoomConnector6\HCDE\traces\step12 `
  --pressure-preset heavy-invasion `
  --periodic-wave-pulse `
  --server-command "net_profile_reset" `
  --periodic-command "net_stressreport"
```

For high-ping, jitter, and packet-loss passes, run the same command under the
OS/network shaper being tested and record the shape metadata with `--label`,
`--shaper`, `--rtt-ms`, `--jitter-ms`, `--loss-pct`, and optionally
`--bandwidth-kbps`, for example:

```powershell
python C:\Users\user\DoomConnector6\HCDE\tests\netcode_step12\netcode_step12_stress.py `
  --server C:\Users\user\DoomConnector6\HCDE\build\RelWithDebInfo\hcdeserv.exe `
  --iwad C:\Games\DOOM2.WAD `
  --cases invasion dm `
  --duration 120 `
  --pressure-preset competitive-highping `
  --summary-dir C:\Users\user\DoomConnector6\HCDE\traces\step12 `
  --label "150ms ping, 30ms jitter, 2pct loss" `
  --shaper clumsy `
  --rtt-ms 150 `
  --jitter-ms 30 `
  --loss-pct 2
```

Available pressure presets are `custom`, `smoke`, `broad`, `heavy-invasion`,
`projectile-storm`, and `competitive-highping`.

### Step F6 runtime soak helpers

`netcode_step12_stress.py` relies on `hcde.exe +wait <tics> +quit` to keep
synthetic clients alive. On release builds we have observed `+wait` quitting at
clienttic=17 (before the live capability handshake completes), which makes
`--require-native-live` look like a netcode failure. For Step F6 runtime
validation, two simpler drivers in this folder bypass `+wait` entirely and
keep clients alive via process lifetime:

- `f6_runtime_soak.py` -- single-server, N-client soak with explicit
  `--duration`, optional `--server-cvar`/`--periodic-server-command` (used for
  `net_extratic`, `net_ticbalance`, periodic `net_stressreport`, etc.). Writes
  `server.log`, `client_*.log`, and `summary.json` under
  `--trace-dir/<label>_<ts>_<rand>/`.
- `f6_disconnect_probe.py` -- three-phase probe (steady, abrupt SIGKILL of one
  client, late-join of another) used to confirm the native pipeline survives an
  ungraceful drop and a reconnect without escalating replay storms or leaking
  legacy NCMD.

Both drivers parse the server's final `net_profile` and emit a JSON summary,
which is the source of the Step F6 numbers in
`docs/HCDE_NETCODE_OVERHAUL.md`.

## Reading Results

Every case ends with server log output from `net_stressreport`. The high-signal
lines are:

- `world pressure`: server tic cost and active invasion pressure.
- `actor pressure`: registry size, class table size, queue maximum, and deferred
  queue work.
- `delta pressure`: packet/record volume and budget deferral.
- `relevance`: how much state was protected, skipped, or sent as keepalive.
- `sim-lod`: current full/reduced/dormant actor counts and skipped thinker work.
- `migration`: shared registry coverage by mode source and actor category.
- `pregame quarantine`: repeated malformed service traffic, quarantine activations,
  and quarantine drops.
- `peer[...]`: per-client queue, priority, skipped, keepalive, and capability
  state.

`net_stressreport` also writes a compact `DebugTrace` event on the `net`
channel. When `--trace-save-dir` is provided, the harness asks the server to run
`debugtracesave` after each case and writes a per-case trace file.

When `--summary-dir` is provided, the harness writes one JSON summary with the
label, pressure preset, network-shape metadata, final launcher snapshot, query
health, and captured stress-report lines for each case.

The harness fails if the dedicated server exits early, the launcher query
endpoint becomes unstable beyond `--max-query-failures`, final `net_stressreport`
is not observed, insufficient launcher snapshots are collected (when
`--min-queries` is used), sim-LOD metrics are missing (when `--require-sim-lod` is
used), or the required core metrics are missing (when `--require-stress-metrics` is
used), or migration telemetry is missing/inadequate (when `--require-migration` or
migration thresholds are used).

### Extra CLI options used in Stage 12

- `--require-stress-metrics`: require that the final report includes parsed
  `world pressure`, `actor pressure`, and `delta pressure` sections.
- `--min-queries N` (default 3): require at least `N` successful launcher query
  samples per case.
- `--require-sim-lod`: require sim-LOD lines from final `net_stressreport`.
- `--min-sim-lod-suspended N` (default 0): fail if invasion case suspended sim-lod count is below
  `N` in the final report (co-op/DM cases are not checked by this threshold).
- `--require-migration`: require that final `net_stressreport` includes migration metrics.
- `--min-migration-considered N`: fail if migration considered count is below `N`.
- `--min-migration-touched N`: fail if migration touched/registered count is below `N`.
- `--min-migration-source-invasion|coop|dm N`: fail if per-mode migration source count is below `N`.
- `--require-native-live`: after the final `net_stressreport`, run `net_profile`
  and require native live gameplay counters to prove the cutover path is healthy.
  This requires `--client-count >= 1` and a valid `--client`.
- `--min-native-client-input-applied N` (default `1`): minimum server-side
  `client-input native-apply` count required by `--require-native-live`.
- `--min-native-server-snapshot-built N` (default `1`): minimum server-side
  `snapshots native-built` count required by `--require-native-live`.
- `--max-native-replay-requests N` (default `0`): maximum accepted final
  `replay-req` counter.
- `--max-native-legacy-rejects N` (default `0`): maximum accepted final
  `legacy-gameplay-reject` counter.
