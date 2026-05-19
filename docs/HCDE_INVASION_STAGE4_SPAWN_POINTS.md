# HCDE Invasion Stage 4: Native Spawn Points

Stage 4 promotes invasion spawn-point handling from passive compatibility shims
into server-owned map logic.

## What Stage 4 Adds

1. Native scan/build of invasion spawn spots each wave.
2. Support for classic Skulltag/Zandronum invasion spot classes
   (`BaseMonsterInvasionSpot` and descendants).
3. Map tag-aware wave routing using spot TIDs.
4. Spot-local delayed activation using map args.
5. Fallback spawn-source chain when no invasion spots exist on a map.

## Classic Spot Args Used

For classic invasion spots, Stage 4 reads the expected five map args:

- `arg0`: Start Spawn Number
- `arg1`: Spawn Delay (seconds between spawns from this spot)
- `arg2`: Round Spawn Delay (seconds from wave start before spot can spawn)
- `arg3`: First Appear Wave
- `arg4`: Max Spawn Number

Per-wave planned spawn count follows the legacy style scaling model:

- easy skills: `1.25 ^ waveAge`
- normal skill: `1.5 ^ waveAge`
- hard/nightmare: `1.6225 ^ waveAge`

then multiplied by `Start Spawn Number` and clamped by `Max Spawn Number`.

## Tag Selection

When map-tag routing is enabled (`sv_invasionspotusemaptags`):

- If a positive-TID spot group matches the current wave number, that tag is used.
- Otherwise, positive tags are sorted and selected by wave index progression.
- Tag `0` remains the untagged/default group.

## Fallback Behavior

If no invasion-classic spots are found, Stage 4 falls back in this order:

1. `MapSpot` actors (if `sv_invasionspotfallback` is enabled)
2. Deathmatch starts
3. Player starts

This keeps invasion playable on non-specialized maps.

## New Server CVars

- `sv_invasionspotusemaptags` (`true`): enable tag-based wave routing.
- `sv_invasionspotfallback` (`true`): allow mapspot fallback source scan.

## Debugging

Use:

`invasion_spots`

to print the active spot directory, planned counts, delays, tags, and fallback
status for the current wave.
