# HCDE Invasion Stage 2: Native State Machine

This stage adds a native invasion phase state machine for `sv_gametype 4` so
the engine tracks mode flow with server-owned states even before the wave
director exists.

## Implemented States

- `disabled`
- `waiting`
- `countdown`
- `spawning`
- `cleanup`
- `intermission`
- `victory`
- `failure`

## Current Transition Rules

1. Mode is `disabled` when `sv_gametype != 4`.
2. Mode becomes `waiting` when `sv_gametype` switches to `4`.
3. `Net_StartCutscene()` moves invasion to `countdown`.
4. `Net_AdvanceCutscene()` moves invasion to `spawning`.
5. `spawning -> cleanup -> intermission -> waiting` is timer-driven.
6. `invasion_victory` and `invasion_failure` force result states.
7. `victory` and `failure` return to `waiting` after a timer.

All state changes are produced by the local service authority tick path
(`TryRunTics` world-tic loop), which keeps phase timing deterministic for the
authoritative session host.

## New CVars

- `sv_invasionspawntime` (seconds)
- `sv_invasioncleanuptime` (seconds)
- `sv_invasionintermissiontime` (seconds)
- `sv_invasionresulttime` (seconds)

Existing countdown cvar remains:

- `sv_invasioncountdowntime` (seconds)

## Query Surface

Server query snapshots now include:

- `InvasionState`
- `InvasionStateTics`
- `InvasionStateName`

and append invasion phase text to `SessionState` while in invasion mode.

This is intentionally non-breaking for older query readers because these fields
are appended at the packet tail after previously-added optional fields.
