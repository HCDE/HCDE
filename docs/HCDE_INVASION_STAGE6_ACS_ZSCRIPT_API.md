# HCDE Invasion Stage 6: ACS and ZScript API

Stage 6 exposes invasion runtime state to scripts and adds a controlled action
entry point for wave flow commands.

## ZScript API (`Object` static natives)

Read-only state:

- `Object.InvasionGetState()` -> `int` (`EInvasionState`)
- `Object.InvasionGetStateTics()` -> `int`
- `Object.InvasionGetWave()` -> `int`
- `Object.InvasionGetMaxWaves()` -> `int`
- `Object.InvasionGetWaveBudget()` -> `int`
- `Object.InvasionGetWaveSpawned()` -> `int`
- `Object.InvasionGetWaveCleared()` -> `int`
- `Object.InvasionGetActiveMonsterCount()` -> `int`
- `Object.InvasionIsBossWave()` -> `bool`
- `Object.InvasionGetSpawnSpotCount()` -> `int`
- `Object.InvasionGetActiveSpawnSpotCount()` -> `int`
- `Object.InvasionGetSpawnPlanBudget()` -> `int`
- `Object.InvasionGetSpawnActiveTag()` -> `int`
- `Object.InvasionIsSpawnUsingFallback()` -> `bool`
- `Object.InvasionGetSpawnFallbackSource()` -> `int`

Controlled write:

- `Object.InvasionControl(int action)` -> `bool`

Action values:

- `0`: no-op / invalid
- `1`: next wave (`INVCTL_NEXTWAVE`)
- `2`: force victory (`INVCTL_VICTORY`)
- `3`: force failure (`INVCTL_FAILURE`)

## ACS API (`CallFunction` indices)

Custom ACS function ids were added in the `19700+` range:

- `ACSF_InvasionGetState`
- `ACSF_InvasionGetStateTics`
- `ACSF_InvasionGetWave`
- `ACSF_InvasionGetMaxWaves`
- `ACSF_InvasionGetWaveBudget`
- `ACSF_InvasionGetWaveSpawned`
- `ACSF_InvasionGetWaveCleared`
- `ACSF_InvasionGetActiveMonsterCount`
- `ACSF_InvasionIsBossWave`
- `ACSF_InvasionGetSpawnSpotCount`
- `ACSF_InvasionGetActiveSpawnSpotCount`
- `ACSF_InvasionGetSpawnPlanBudget`
- `ACSF_InvasionGetSpawnActiveTag`
- `ACSF_InvasionIsSpawnUsingFallback`
- `ACSF_InvasionGetSpawnFallbackSource`
- `ACSF_InvasionControlAction` (argument: action int)

## Authority and Safety

`InvasionControl` paths are server-authoritative:

- requires local HCDE service authority
- requires invasion mode enabled (`sv_gametype 4`)

When either condition is not met, control calls return `false`/`0` and do not
mutate state.
