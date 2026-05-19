# HCDE Native Invasion Mode: Full Documentation

HCDE Native Invasion is a wave-based survival game mode for Doom, built directly into the engine (`sv_gametype 4`). It provides server-authoritative control over monster spawning, wave progression, and difficulty scaling.

## 1. Server Configuration (CVars)

### Core Settings
- `sv_gametype 4`: Enables Invasion mode.
- `sv_invasionwaves` (default 8): The total number of waves in a match.
- `sv_invasionbasebudget` (default 24): The base monster count or "difficulty points" for Wave 1.
- `sv_invasionbudgetstep` (default 8): Additional budget added per wave.
- `sv_invasionperplayer` (default 6): Additional budget added for each connected player.

### Pacing and Timing
- `sv_invasionspawntime` (seconds, default 8): The maximum time allowed for spawning all monsters in a wave. If 0, the engine calculates a window based on interval and burst.
- `sv_invasionspawninterval` (seconds, default 0.35): Time between spawn bursts.
- `sv_invasionspawnburst` (default 3): Number of monsters to spawn in each interval.
- `sv_invasioncleanuptime` (seconds, default 4): Time allowed to kill remaining monsters after spawning finishes. If exceeded, the wave fails.
- `sv_invasionintermissiontime` (seconds, default 6): Time between waves.
- `sv_invasionresulttime` (seconds, default 8): Time to display the final victory or failure screen before resetting.
- `sv_invasioncountdowntime` (seconds, default 30): The initial countdown when players are "Ready."

### Map Integration
- `sv_invasionspotusemaptags` (bool, default true): If true, the director will only use spots with a specific tag for each wave, rotating through tags if multiple are present.
- `sv_invasionspotfallback` (bool, default true): If no dedicated invasion spots are found, the engine will use MapSpots, Deathmatch starts, or Player starts as fallback.

## 2. Mapping for Invasion

### Dedicated Spawn Spots
Mappers can use the `BaseMonsterInvasionSpot` or `CustomMonsterInvasionSpot` actor classes. These actors support the following arguments:

- **Arg 0 (Start Number)**: Initial number of monsters to spawn from this spot in its first wave.
- **Arg 1 (Spawn Delay)**: Delay in seconds between spawns at this specific spot.
- **Arg 2 (Round Delay)**: Initial delay in seconds before this spot starts spawning in a new wave.
- **Arg 3 (First Wave)**: The first wave number where this spot becomes active.
- **Arg 4 (Max Spawn)**: Maximum number of monsters this spot can spawn in a single wave.

### Tags and Rotation
If `sv_invasionspotusemaptags` is enabled, and a map has spots with different TIDs (Tags), the director will select one TID at the start of each wave and only spawn from those spots. This allows for multi-arena maps where the action moves from room to arena.

## 3. Modding and APIs

### ACS / ZScript Functions
The following native functions are available to scripts:
- `InvasionGetWave()`: Returns the current wave number.
- `InvasionGetMaxWaves()`: Returns the total configured waves.
- `InvasionGetState()`: Returns the current state (Waiting, Spawning, Cleanup, etc.).
- `InvasionGetActiveMonsterCount()`: Returns the number of currently alive invasion monsters.
- `InvasionIsBossWave()`: Returns true if the current wave is a boss wave.

### Custom Monster Pools
HCDE uses a built-in escalation table that limits powerful monsters like Archviles and Cyberdemons to later waves. Modders can override these pools by creating custom actors that replace the default "Spot" classes.

## 4. Dedicated Server Hosting
To host an HCDE Invasion server from the command line:
`hcde.exe -server <max_players> +sv_gametype 4 +map <mapname>`

Admins can use the built-in "Invasion Standard" preset in the HCDE Server GUI to quickly apply balanced settings.
