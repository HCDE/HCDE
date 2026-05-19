# HCDE Invasion Stage 3: Wave Director

Stage 3 introduces a deterministic, server-authoritative wave director that
drives invasion progression data before native actor spawning is fully wired.

## Core Behavior

1. Waves are planned with deterministic budgets per wave.
2. Spawn pacing is controlled by server-side interval and burst settings.
3. Cleanup/intermission/victory/failure transitions are state-machine driven.
4. Wave metadata is exposed in query snapshots for launchers and web clients.

## New Wave Director CVars

- `sv_invasionwaves` (default `8`)
- `sv_invasionbasebudget` (default `24`)
- `sv_invasionbudgetstep` (default `8`)
- `sv_invasionperplayer` (default `6`)
- `sv_invasionspawninterval` (default `0.35`)
- `sv_invasionspawnburst` (default `3`)
- `sv_invasionbosswaveevery` (default `5`, `0` disables boss-wave marking)
- `sv_invasionbossbonus` (default `20`)

Existing stage timing CVars from Stage 2 remain active:

- `sv_invasioncountdowntime`
- `sv_invasionspawntime`
- `sv_invasioncleanuptime`
- `sv_invasionintermissiontime`
- `sv_invasionresulttime`

## Wave Metadata Surface

Server snapshots now carry:

- `InvasionWave`
- `InvasionMaxWaves`
- `InvasionWaveBudget`
- `InvasionWaveSpawned`
- `InvasionWaveCleared`
- `InvasionWaveFlags` (`bit0 = boss wave`)

This metadata is appended after prior optional query fields for compatibility.

## Notes For Stage 4 And Stage 5

Stage 3 intentionally models wave progression and budget accounting without
native invasion spot spawning yet. Stage 4 should map invasion spawn locations,
and Stage 5 should replace virtual wave-spawn accounting with real actor spawn
and kill tracking.
