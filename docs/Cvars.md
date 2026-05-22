# HCDE CVAR Reference

Generated: 2026-05-22 21:37:07 UTC

This reference combines live runtime CVAR output and source-level metadata.

## Coverage

- Total runtime CVARs discovered: **449**
- Set/get tested runtime CVARs: **176**
- Successful get responses: **176**
- Missing get responses: **0**
- Unexpected parser/runtime lines during sweep: **0**

## Flag Legend

- Position 1: `A` = archived, space = not archived
- Position 2: `U` = userinfo, `S` = serverinfo, `C` = auto/custom, space = local/general
- Position 3: `-` = write-protected, `L` = latched, `*` = unsettable auto cvar, space = writable
- Position 4: `M` = modified/session-marked
- Position 5: `X` = ignored/hidden from normal flow

## Invasion CVARs

These are the invasion controls defined in source.

### `sv_invasionbasebudget`

- Description: Base monster budget each wave starts with.
- Default: `24`
- Valid range/shape: `>= 1`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionbossbonus`

- Description: Extra budget added during boss waves.
- Default: `20`
- Valid range/shape: `>= 0`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionbosswaveevery`

- Description: Boss wave cadence (e.g. 5 = every 5th wave, 0 = never).
- Default: `5`
- Valid range/shape: `>= 0`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionbudgetstep`

- Description: Budget increase applied per wave number.
- Default: `8`
- Valid range/shape: `>= 0`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasioncleanuptime`

- Description: Seconds allowed for cleanup phase after spawning ends.
- Default: `4`
- Valid range/shape: `>= 0`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasioncountdowntime`

- Description: Seconds before wave 1 starts ("Prepare for invasion" countdown).
- Default: `30`
- Valid range/shape: `>= 0`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionintermissiontime`

- Description: Seconds between completed waves before the next wave starts.
- Default: `6`
- Valid range/shape: `>= 0`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionperplayer`

- Description: Additional budget per extra active player.
- Default: `6`
- Valid range/shape: `>= 0`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionresulttime`

- Description: Seconds to keep the final victory/failure state visible.
- Default: `8`
- Valid range/shape: `>= 0`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionspawnburst`

- Description: Maximum monsters spawned per spawn tick burst.
- Default: `3`
- Valid range/shape: `>= 1`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionspawninterval`

- Description: Seconds between spawn ticks while wave spawning is active.
- Default: `0.35`
- Valid range/shape: `>= 0.05`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionspawntime`

- Description: Wave spawn window length in seconds before cleanup phase.
- Default: `8`
- Valid range/shape: `>= 0`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionspotfallback`

- Description: Fallback to generic spawning when tagged invasion spots cannot be used.
- Default: `1`
- Valid range/shape: `bool`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionspotusemaptags`

- Description: Use map-tagged invasion spot routing when available.
- Default: `1`
- Valid range/shape: `bool`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_invasionwaves`

- Description: Maximum number of invasion waves in a run.
- Default: `8`
- Valid range/shape: `1..255`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `sv_usemapsettingswavelimit`

- Description: If enabled, map-defined invasion `wavelimit` metadata (CMPGNINF/MAPINFO) overrides `sv_invasionwaves` when present.
- Default: `1`
- Valid range/shape: `bool`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `wavelimit`

- Description: Legacy Skulltag compatibility override for invasion waves. `0` disables the override; `1..255` forces that wave count.
  Online gameplay with map-authored limits locks this unless running offline (`netgame` false), where the legacy override is allowed.
- Default: `0`
- Valid range/shape: `0..255`
- Present in runtime snapshot: No (not in this runtime snapshot)

### `duellimit`

- Description: Legacy Skulltag/Skulltag compatibility override for `fraglimit` (duel/frags map limit). `0` disables the override; `1..255` forces that frag limit.
  Online gameplay with map-authored `duellimit` (from CMPGNINF) locks this unless running offline (`netgame` false), where the legacy override is allowed.
- Default: `0`
- Valid range/shape: `0..255`
- Present in runtime snapshot: No (not in this runtime snapshot)

## Full Runtime CVAR Catalog

### `chase_dist`

- Description: Likely controls chase dist.
- Current value: `90`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `chase_height`

- Description: Likely controls chase height.
- Current value: `-8`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `cl_centerbobonfire`

- Description: Likely controls centerbobonfire behavior for client.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `cl_deathcam`

- Description: Likely controls deathcam behavior for client.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `cl_movebob`

- Description: Likely controls movebob behavior for client.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `cl_predictpickup`

- Description: Likely controls predictpickup behavior for client.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `cl_predictsectors`

- Description: Likely controls predictsectors behavior for client.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `cl_waddownloaddir`

- Description: Likely controls waddownloaddir behavior for client.
- Current value: ``
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_allowdropoff`

- Description: Likely controls allowdropoff behavior for co-op.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `co_avoidhazards`

- Description: Likely controls avoidhazards behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_blockmapfix`

- Description: Likely controls blockmapfix behavior for co-op.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `co_boomphys`

- Description: Likely controls boomphys behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_fineautoaim`

- Description: Likely controls fineautoaim behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_fixweaponimpacts`

- Description: Likely controls fixweaponimpacts behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_friend_distance`

- Description: Likely controls friend distance behavior for co-op.
- Current value: `128`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_friend_helpertype`

- Description: Likely controls friend helpertype behavior for co-op.
- Current value: ``
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_friend_ledgejumping`

- Description: Likely controls friend ledgejumping behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_friend_playerhelpers`

- Description: Likely controls friend playerhelpers behavior for co-op.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `co_globalsound`

- Description: Likely controls globalsound behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_helpfriends`

- Description: Likely controls helpfriends behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_mbfphys`

- Description: Likely controls mbfphys behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_monsterbacking`

- Description: Likely controls monsterbacking behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_monsterfriction`

- Description: Likely controls monsterfriction behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_monstersclimbsteep`

- Description: Likely controls monstersclimbsteep behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_nosilentspawns`

- Description: Likely controls nosilentspawns behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_novileghosts`

- Description: Likely controls novileghosts behavior for co-op.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `co_pursuit`

- Description: Likely controls pursuit behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_realactorheight`

- Description: Likely controls realactorheight behavior for co-op.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `co_removesoullimit`

- Description: Likely controls removesoullimit behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_staylift`

- Description: Likely controls staylift behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_zdoomammo`

- Description: Likely controls zdoomammo behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_zdoomphys`

- Description: Likely controls zdoomphys behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `co_zdoomsound`

- Description: Likely controls zdoomsound behavior for co-op.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `configver`

- Description: Likely controls configver.
- Current value: `12020`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `ctf_flagathometoscore`

- Description: Likely controls ctf flagathometoscore.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `ctf_flagtimeout`

- Description: Likely controls ctf flagtimeout.
- Current value: `10`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `ctf_manualreturn`

- Description: Likely controls ctf manualreturn.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `debug_disconnect`

- Description: Likely controls debug disconnect.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `developer`

- Description: Likely controls developer.
- Current value: `0`
- Raw flag field: (none)
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_ctf_notouchreturn`

- Description: Likely controls ctf notouchreturn behavior for gameplay.
- Current value: `0`
- Raw flag field: "A  L"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `g_gametypename`

- Description: Likely controls gametypename behavior for gameplay.
- Current value: ``
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_horde_extralife`

- Description: Likely controls horde extralife behavior for gameplay.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `g_horde_goalhp`

- Description: Likely controls horde goalhp behavior for gameplay.
- Current value: `8`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_horde_maxtotalhp`

- Description: Likely controls horde maxtotalhp behavior for gameplay.
- Current value: `10`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_horde_mintotalhp`

- Description: Likely controls horde mintotalhp behavior for gameplay.
- Current value: `4`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_horde_resurrect`

- Description: Likely controls horde resurrect behavior for gameplay.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `g_horde_spawnempty_max`

- Description: Likely controls horde spawnempty max behavior for gameplay.
- Current value: `3`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_horde_spawnempty_min`

- Description: Likely controls horde spawnempty min behavior for gameplay.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_horde_spawnfull_max`

- Description: Likely controls horde spawnfull max behavior for gameplay.
- Current value: `6`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_horde_spawnfull_min`

- Description: Likely controls horde spawnfull min behavior for gameplay.
- Current value: `2`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_horde_waves`

- Description: Likely controls horde waves behavior for gameplay.
- Current value: `5`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_lives`

- Description: Likely controls lives behavior for gameplay.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `g_lives_jointimer`

- Description: Likely controls lives jointimer behavior for gameplay.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_postroundtime`

- Description: Likely controls postroundtime behavior for gameplay.
- Current value: `3`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_preroundreset`

- Description: Likely controls preroundreset behavior for gameplay.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_preroundtime`

- Description: Likely controls preroundtime behavior for gameplay.
- Current value: `5`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_resetinvonexit`

- Description: Likely controls resetinvonexit behavior for gameplay.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_roundlimit`

- Description: Likely controls roundlimit behavior for gameplay.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_rounds`

- Description: Likely controls rounds behavior for gameplay.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `g_sides`

- Description: Likely controls sides behavior for gameplay.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `g_spawninv`

- Description: Likely controls spawninv behavior for gameplay.
- Current value: `default`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_thingfilter`

- Description: Likely controls thingfilter behavior for gameplay.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `g_winlimit`

- Description: Likely controls winlimit behavior for gameplay.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `g_winnerstays`

- Description: Likely controls winnerstays behavior for gameplay.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_actor_bridge_blocked`

- Description: Likely controls hcde acs adapter actor bridge blocked.
- Current value: `30`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_actor_bridge_required`

- Description: Likely controls hcde acs adapter actor bridge required.
- Current value: `30`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_actor_bridge_surfaces`

- Description: Likely controls hcde acs adapter actor bridge surfaces.
- Current value: `actor-spawn,actor-class-query,inventory-class,weapon-class,user-fields,network-identity`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_blocked_features`

- Description: Likely controls hcde acs adapter blocked features.
- Current value: `actor-bridge-runtime,actor-bridge-network`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_client_only`

- Description: Likely controls hcde acs adapter client only.
- Current value: `31`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_decisions`

- Description: Likely controls hcde acs adapter decisions.
- Current value: `unknown-surface,legacy-runtime-handoff,reject-online-policy,reject-missing-actor-bridge,reject-runtime-disabled,execute-reviewed`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_deny_policy`

- Description: Likely controls hcde acs adapter deny policy.
- Current value: `11`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_functions`

- Description: Likely controls hcde acs adapter functions.
- Current value: `112`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_inventory_entries`

- Description: Likely controls hcde acs adapter inventory entries.
- Current value: `265`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_legacy_handoff`

- Description: Likely controls hcde acs adapter legacy handoff.
- Current value: `193`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_manifest_protocol`

- Description: Likely controls hcde acs adapter manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_missing_features`

- Description: Likely controls hcde acs adapter missing features.
- Current value: `98304`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_opcodes`

- Description: Likely controls hcde acs adapter opcodes.
- Current value: `153`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_runtime`

- Description: Likely controls hcde acs adapter runtime.
- Current value: `diagnostics-only`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_runtime_diagnostics`

- Description: Likely controls hcde acs adapter runtime diagnostics.
- Current value: `0`
- Raw flag field: (none)
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_runtime_executed`

- Description: Likely controls hcde acs adapter runtime executed.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_runtime_mode`

- Description: Likely controls hcde acs adapter runtime mode.
- Current value: `1`
- Raw flag field: (none)
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_server_context_denied`

- Description: Likely controls hcde acs adapter server context denied.
- Current value: `42`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_server_only`

- Description: Likely controls hcde acs adapter server only.
- Current value: `83`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_acs_adapter_status`

- Description: Likely controls hcde acs adapter status.
- Current value: `staged-modern-acs-adapter-diagnostics-only`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_action_dispatch_class_names`

- Description: Likely controls hcde actor bridge action dispatch class names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_action_dispatch_classes`

- Description: Likely controls hcde actor bridge action dispatch classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_action_hydration_classes`

- Description: Likely controls hcde actor bridge action hydration classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_action_review_classes`

- Description: Likely controls hcde actor bridge action review classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_actor_fields`

- Description: Likely controls hcde actor bridge actor fields.
- Current value: `type,info,state,flags,flags2,flags3,oflags,netid,tid,baseline,target,tracer`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_decorate_required`

- Description: Likely controls hcde actor bridge decorate required.
- Current value: `18`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_default_hydration_classes`

- Description: Likely controls hcde actor bridge default hydration classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_denied_surfaces`

- Description: Likely controls hcde actor bridge denied surfaces.
- Current value: `16`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_feature_names`

- Description: Likely controls hcde actor bridge feature names.
- Current value: `actor-bridge-metadata`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_features`

- Description: Likely controls hcde actor bridge features.
- Current value: `16384`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_guarded_class_names`

- Description: Likely controls hcde actor bridge guarded class names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_hydration_class_names`

- Description: Likely controls hcde actor bridge hydration class names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_hydration_classes`

- Description: Likely controls hcde actor bridge hydration classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_identity_classes`

- Description: Likely controls hcde actor bridge identity classes.
- Current value: `5`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_inventory_fields`

- Description: Likely controls hcde actor bridge inventory fields.
- Current value: `player_t weaponowned,ammo,maxammo,readyweapon,pendingweapon,gitem_t flags,offset,quantity`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_inventory_hydration_classes`

- Description: Likely controls hcde actor bridge inventory hydration classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_manifest_protocol`

- Description: Likely controls hcde actor bridge manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_metadata_ready_classes`

- Description: Likely controls hcde actor bridge metadata ready classes.
- Current value: `9`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_mobjinfo_fields`

- Description: Likely controls hcde actor bridge mobjinfo fields.
- Current value: `type,doomednum,spawnstate,spawnhealth,gibhealth,seestate,seesound,reactiontime,attacksound,painstate,painchance,painsound,meleestate,missilestate,deathstate,xdeathstate,deathsound,speed,radius,height,cdheight,mass,damage,activesound,flags,flags2,raisestate,translucency,name,altspeed,meleerange,infightinggroup,projectilegroup,splashgroup,flags3,ripsound,droppeditem`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_modern_acs_required`

- Description: Likely controls hcde actor bridge modern acs required.
- Current value: `9`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_network_guarded_classes`

- Description: Likely controls hcde actor bridge network guarded classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_network_identity_classes`

- Description: Likely controls hcde actor bridge network identity classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_networked_surface_names`

- Description: Likely controls hcde actor bridge networked surface names.
- Current value: `spawn-map-editor-number,actor-replacement,inventory-class,weapon-info,player-class-defaults,network-identity,acs-class-api,dropitem-spawn`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_networked_surfaces`

- Description: Likely controls hcde actor bridge networked surfaces.
- Current value: `8`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_parent_links`

- Description: Likely controls hcde actor bridge parent links.
- Current value: `8`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_playerclass_hydration_classes`

- Description: Likely controls hcde actor bridge playerclass hydration classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_public_safe_classes`

- Description: Likely controls hcde actor bridge public safe classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_registered_class_names`

- Description: Likely controls hcde actor bridge registered class names.
- Current value: `Actor,Inventory,Weapon,PlayerPawn,HCDEPreviewDecorateActor,HCDEPreviewDecorateReplacement,HCDEPreviewDecorateInventoryItem,HCDEPreviewDecorateWeapon,HCDEPreviewDecoratePlayerPawn`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_registered_classes`

- Description: Likely controls hcde actor bridge registered classes.
- Current value: `9`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_replacement_hydration_classes`

- Description: Likely controls hcde actor bridge replacement hydration classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_replicated_surfaces`

- Description: Likely controls hcde actor bridge replicated surfaces.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_required_surface_names`

- Description: Likely controls hcde actor bridge required surface names.
- Current value: `actor-class-name,actor-parent-chain,mobjinfo-core-defaults,mobjinfo-flags,state-storage,state-label-map,action-function-dispatch,sprite-name-table,sound-name-table,spawn-map-editor-number,actor-replacement,inventory-class,weapon-info,player-class-defaults,network-identity,save-load-identity,acs-class-api,zscript-class-api,client-presentation,dropitem-spawn`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_required_surfaces`

- Description: Likely controls hcde actor bridge required surfaces.
- Current value: `20`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_root_classes`

- Description: Likely controls hcde actor bridge root classes.
- Current value: `1`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_runtime`

- Description: Likely controls hcde actor bridge runtime.
- Current value: `metadata-only`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_runtime_mode`

- Description: Likely controls hcde actor bridge runtime mode.
- Current value: `1`
- Raw flag field: (none)
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_runtime_mutation_class_names`

- Description: Likely controls hcde actor bridge runtime mutation class names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_runtime_mutation_classes`

- Description: Likely controls hcde actor bridge runtime mutation classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_runtime_surfaces`

- Description: Likely controls hcde actor bridge runtime surfaces.
- Current value: `13`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_saveload_guarded_classes`

- Description: Likely controls hcde actor bridge saveload guarded classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_saveload_identity_classes`

- Description: Likely controls hcde actor bridge saveload identity classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_saveload_surfaces`

- Description: Likely controls hcde actor bridge saveload surfaces.
- Current value: `1`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_state_fields`

- Description: Likely controls hcde actor bridge state fields.
- Current value: `statenum,sprite,frame,tics,action,nextstate,misc1,misc2,args,flags`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_state_hydration_classes`

- Description: Likely controls hcde actor bridge state hydration classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_status`

- Description: Likely controls hcde actor bridge status.
- Current value: `runtime-hydration-disabled`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_surface_names`

- Description: Likely controls hcde actor bridge surface names.
- Current value: `actor-class-name,actor-parent-chain,mobjinfo-core-defaults,mobjinfo-flags,state-storage,state-label-map,action-function-dispatch,sprite-name-table,sound-name-table,spawn-map-editor-number,actor-replacement,inventory-class,weapon-info,player-class-defaults,network-identity,save-load-identity,acs-class-api,zscript-class-api,client-presentation,dropitem-spawn`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_surfaces`

- Description: Likely controls hcde actor bridge surfaces.
- Current value: `20`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_weapon_fields`

- Description: Likely controls hcde actor bridge weapon fields.
- Current value: `ammotype,upstate,downstate,readystate,atkstate,flashstate,droptype,ammouse,minammo,flags,ammopershot,internalflags`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_weapon_hydration_classes`

- Description: Likely controls hcde actor bridge weapon hydration classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_actor_bridge_zscript_required`

- Description: Likely controls hcde actor bridge zscript required.
- Current value: `19`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_consumer_count`

- Description: Likely controls hcde addon consumer consumer count.
- Current value: `5`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_dependency_edges`

- Description: Likely controls hcde addon consumer dependency edges.
- Current value: `8`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_framework_family_count`

- Description: Likely controls hcde addon consumer framework family count.
- Current value: `6`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_kinds`

- Description: Likely controls hcde addon consumer kinds.
- Current value: `phase-group,phase-group,phase-group,phase-group,package-group`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_manifest_protocol`

- Description: Likely controls hcde addon consumer manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_metadata_only_families`

- Description: Likely controls hcde addon consumer metadata only families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_metadata_only_family_names`

- Description: Likely controls hcde addon consumer metadata only family names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_names`

- Description: Likely controls hcde addon consumer names.
- Current value: `reference-only,metadata-only,runtime-ready,runtime-enabled,package-discovery`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_package_discovery_families`

- Description: Likely controls hcde addon consumer package discovery families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_package_discovery_family_names`

- Description: Likely controls hcde addon consumer package discovery family names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_records`

- Description: Likely controls hcde addon consumer records.
- Current value: `reference-only:2[compatibility-contract,edge-classic]|metadata-only:2[decorate,actor-bridge]|runtime-ready:2[edf,modern-acs]|runtime-enabled:0[]|package-discovery:2[edf,edge-classic]`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_reference_only_families`

- Description: Likely controls hcde addon consumer reference only families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_reference_only_family_names`

- Description: Likely controls hcde addon consumer reference only family names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_runtime`

- Description: Likely controls hcde addon consumer runtime.
- Current value: `runtime-ready`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_runtime_enabled_families`

- Description: Likely controls hcde addon consumer runtime enabled families.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_runtime_enabled_family_names`

- Description: Likely controls hcde addon consumer runtime enabled family names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_runtime_ready_families`

- Description: Likely controls hcde addon consumer runtime ready families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_runtime_ready_family_names`

- Description: Likely controls hcde addon consumer runtime ready family names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_status`

- Description: Likely controls hcde addon consumer status.
- Current value: `staged-addon-consumer-runtime-ready`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_consumer_statuses`

- Description: Likely controls hcde addon consumer statuses.
- Current value: `staged-addon-consumer-reference-only,staged-addon-consumer-metadata-only,staged-addon-consumer-runtime-ready,staged-addon-consumer-runtime-enabled,staged-addon-consumer-reference-only`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_capability_namespaces`

- Description: Likely controls hcde addon framework capability namespaces.
- Current value: `hcde.contract,hcde.edf,hcde.acs,hcde.decorate,hcde.actor,hcde.edgeclassic`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_dependency_edges`

- Description: Likely controls hcde addon framework dependency edges.
- Current value: `8`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_family_count`

- Description: Likely controls hcde addon framework family count.
- Current value: `6`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_family_dependencies`

- Description: Likely controls hcde addon framework family dependencies.
- Current value: `compatibility-contract:none,edf:hcde.contract,modern-acs:hcde.contract,hcde.actor,decorate:hcde.contract,hcde.actor,hcde.acs,actor-bridge:hcde.contract,edge-classic:hcde.contract`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_family_kinds`

- Description: Likely controls hcde addon framework family kinds.
- Current value: `contract-root,package-discovery,runtime-adapter,runtime-bridge,runtime-bridge,reference-discovery`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_family_names`

- Description: Likely controls hcde addon framework family names.
- Current value: `compatibility-contract,edf,modern-acs,decorate,actor-bridge,edge-classic`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_family_phases`

- Description: Likely controls hcde addon framework family phases.
- Current value: `reference-only,runtime-ready,runtime-ready,metadata-only,metadata-only,reference-only`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_family_statuses`

- Description: Likely controls hcde addon framework family statuses.
- Current value: `staged-reference-only,staged-runtime-disabled,staged-modern-acs-adapter-diagnostics-only,parsed-registry-runtime-disabled,runtime-hydration-disabled,staged-coal-runtime-disabled`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_manifest_protocol`

- Description: Likely controls hcde addon framework manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_metadata_only_families`

- Description: Likely controls hcde addon framework metadata only families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_metadata_only_family_names`

- Description: Likely controls hcde addon framework metadata only family names.
- Current value: `decorate,actor-bridge`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_package_discovery_families`

- Description: Likely controls hcde addon framework package discovery families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_package_discovery_family_names`

- Description: Likely controls hcde addon framework package discovery family names.
- Current value: `edf,edge-classic`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_reference_only_families`

- Description: Likely controls hcde addon framework reference only families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_reference_only_family_names`

- Description: Likely controls hcde addon framework reference only family names.
- Current value: `compatibility-contract,edge-classic`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_runtime`

- Description: Likely controls hcde addon framework runtime.
- Current value: `runtime-ready`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_runtime_enabled_families`

- Description: Likely controls hcde addon framework runtime enabled families.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_runtime_enabled_family_names`

- Description: Likely controls hcde addon framework runtime enabled family names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_runtime_ready_families`

- Description: Likely controls hcde addon framework runtime ready families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_runtime_ready_family_names`

- Description: Likely controls hcde addon framework runtime ready family names.
- Current value: `edf,modern-acs`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_addon_framework_status`

- Description: Likely controls hcde addon framework status.
- Current value: `staged-addon-framework-runtime-ready`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_base_engine`

- Description: Likely controls hcde base engine.
- Current value: `hcde`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_acs_candidates`

- Description: Likely controls hcde class discovery acs candidates.
- Current value: `6`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_blocked_features`

- Description: Likely controls hcde class discovery blocked features.
- Current value: `actor-bridge-runtime,actor-bridge-network`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_candidate_names`

- Description: Likely controls hcde class discovery candidate names.
- Current value: `acs-actor-spawn,acs-actor-class-query,acs-inventory-class,acs-weapon-class,acs-user-fields,acs-network-identity,decorate-actor-class,decorate-spawn-map,decorate-replacement,decorate-inventory-class,decorate-weapon-class,decorate-player-class,decorate-state-action,decorate-dropitem,decorate-client-presentation,zscript-reflection-class,zscript-native-thunk-class`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_candidates`

- Description: Likely controls hcde class discovery candidates.
- Current value: `17`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_decorate_candidates`

- Description: Likely controls hcde class discovery decorate candidates.
- Current value: `9`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_manifest_protocol`

- Description: Likely controls hcde class discovery manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_max_surfaces`

- Description: Likely controls hcde class discovery max surfaces.
- Current value: `10`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_metadata_accepted`

- Description: Likely controls hcde class discovery metadata accepted.
- Current value: `17`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_missing_feature_candidates`

- Description: Likely controls hcde class discovery missing feature candidates.
- Current value: `17`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_missing_features`

- Description: Likely controls hcde class discovery missing features.
- Current value: `98304`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_parser_required`

- Description: Likely controls hcde class discovery parser required.
- Current value: `17`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_required_features`

- Description: Likely controls hcde class discovery required features.
- Current value: `114688`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_runtime`

- Description: Likely controls hcde class discovery runtime.
- Current value: `metadata-only`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_runtime_blocked`

- Description: Likely controls hcde class discovery runtime blocked.
- Current value: `17`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_status`

- Description: Likely controls hcde class discovery status.
- Current value: `metadata-registry-runtime-disabled`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_class_discovery_zscript_candidates`

- Description: Likely controls hcde class discovery zscript candidates.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_compat_profile`

- Description: Likely controls hcde compat profile.
- Current value: `hcde-base`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_contract_phase`

- Description: Likely controls hcde contract phase.
- Current value: `reference-only`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_contract_protocol`

- Description: Likely controls hcde contract protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_contract_status`

- Description: Likely controls hcde contract status.
- Current value: `staged-reference-only`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_acs_bridge_required`

- Description: Likely controls hcde decorate acs bridge required.
- Current value: `9`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_action_dispatch_class_names`

- Description: Likely controls hcde decorate action dispatch class names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_action_dispatch_classes`

- Description: Likely controls hcde decorate action dispatch classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_action_families`

- Description: Likely controls hcde decorate action families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_actor_bridge_required`

- Description: Likely controls hcde decorate actor bridge required.
- Current value: `14`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_actor_families`

- Description: Likely controls hcde decorate actor families.
- Current value: `1`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_denied_definitions`

- Description: Likely controls hcde decorate denied definitions.
- Current value: `14`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_feature_names`

- Description: Likely controls hcde decorate feature names.
- Current value: `decorate-metadata`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_features`

- Description: Likely controls hcde decorate features.
- Current value: `2048`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_flag_families`

- Description: Likely controls hcde decorate flag families.
- Current value: `1`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_identity_classes`

- Description: Likely controls hcde decorate identity classes.
- Current value: `5`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_inventory_entries`

- Description: Likely controls hcde decorate inventory entries.
- Current value: `17`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_manifest_protocol`

- Description: Likely controls hcde decorate manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_parsed_class_names`

- Description: Likely controls hcde decorate parsed class names.
- Current value: `HCDEPreviewDecorateActor,HCDEPreviewDecorateReplacement,HCDEPreviewDecorateInventoryItem,HCDEPreviewDecorateWeapon,HCDEPreviewDecoratePlayerPawn`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_parsed_classes`

- Description: Likely controls hcde decorate parsed classes.
- Current value: `5`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_property_families`

- Description: Likely controls hcde decorate property families.
- Current value: `1`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_reference_sources`

- Description: Likely controls hcde decorate reference sources.
- Current value: `11`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_registered_class_names`

- Description: Likely controls hcde decorate registered class names.
- Current value: `HCDEPreviewDecorateActor,HCDEPreviewDecorateReplacement,HCDEPreviewDecorateInventoryItem,HCDEPreviewDecorateWeapon,HCDEPreviewDecoratePlayerPawn`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_registered_classes`

- Description: Likely controls hcde decorate registered classes.
- Current value: `5`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_replacement_families`

- Description: Likely controls hcde decorate replacement families.
- Current value: `1`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_replicated_definitions`

- Description: Likely controls hcde decorate replicated definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_runtime`

- Description: Likely controls hcde decorate runtime.
- Current value: `metadata-only`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_runtime_mode`

- Description: Likely controls hcde decorate runtime mode.
- Current value: `1`
- Raw flag field: (none)
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_runtime_mutation_class_names`

- Description: Likely controls hcde decorate runtime mutation class names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_runtime_mutation_classes`

- Description: Likely controls hcde decorate runtime mutation classes.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_state_families`

- Description: Likely controls hcde decorate state families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_status`

- Description: Likely controls hcde decorate status.
- Current value: `parsed-registry-runtime-disabled`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_decorate_zscript_required`

- Description: Likely controls hcde decorate zscript required.
- Current value: `4`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_admission_blocked_definitions`

- Description: Likely controls hcde edf admission blocked definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_admission_deferred_definitions`

- Description: Likely controls hcde edf admission deferred definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_admission_ready_definitions`

- Description: Likely controls hcde edf admission ready definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_deferred_definitions`

- Description: Likely controls hcde edf deferred definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_denied_definitions`

- Description: Likely controls hcde edf denied definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_errors`

- Description: Likely controls hcde edf errors.
- Current value: `0`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_feature_names`

- Description: Likely controls hcde edf feature names.
- Current value: `none`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_features`

- Description: Likely controls hcde edf features.
- Current value: `0`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_manifest_protocol`

- Description: Likely controls hcde edf manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_manual_review_definitions`

- Description: Likely controls hcde edf manual review definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_parsed_definitions`

- Description: Likely controls hcde edf parsed definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_processed_sources`

- Description: Likely controls hcde edf processed sources.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_replicated_definitions`

- Description: Likely controls hcde edf replicated definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_root_sources`

- Description: Likely controls hcde edf root sources.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_runtime`

- Description: Likely controls hcde edf runtime.
- Current value: `admission-ready`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_status`

- Description: Likely controls hcde edf status.
- Current value: `staged-runtime-admission-ready`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edf_warnings`

- Description: Likely controls hcde edf warnings.
- Current value: `0`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_admission_blocked_modules`

- Description: Likely controls hcde edgeclassic coal admission blocked modules.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_admission_deferred_modules`

- Description: Likely controls hcde edgeclassic coal admission deferred modules.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_admission_ready_modules`

- Description: Likely controls hcde edgeclassic coal admission ready modules.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_runtime_admission_enabled`

- Description: Likely controls hcde edgeclassic coal runtime admission enabled.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_runtime_application_enabled`

- Description: Likely controls hcde edgeclassic coal runtime application enabled.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_runtime_bridge_enabled`

- Description: Likely controls hcde edgeclassic coal runtime bridge enabled.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_runtime_mode`

- Description: Likely controls hcde edgeclassic coal runtime mode.
- Current value: `disabled`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_scripts`

- Description: Likely controls hcde edgeclassic coal scripts.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_sources`

- Description: Likely controls hcde edgeclassic coal sources.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_coal_status`

- Description: Likely controls hcde edgeclassic coal status.
- Current value: `staged-coal-runtime-disabled`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_consumer_count`

- Description: Likely controls hcde edgeclassic consumer count.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_consumer_kinds`

- Description: Likely controls hcde edgeclassic consumer kinds.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_consumer_names`

- Description: Likely controls hcde edgeclassic consumer names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_consumer_records`

- Description: Likely controls hcde edgeclassic consumer records.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_anonymous_definitions`

- Description: Likely controls hcde edgeclassic ddf parser anonymous definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_definitions`

- Description: Likely controls hcde edgeclassic ddf parser definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_errors`

- Description: Likely controls hcde edgeclassic ddf parser errors.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_files`

- Description: Likely controls hcde edgeclassic ddf parser files.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_named_definitions`

- Description: Likely controls hcde edgeclassic ddf parser named definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_numbered_definitions`

- Description: Likely controls hcde edgeclassic ddf parser numbered definitions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_sections`

- Description: Likely controls hcde edgeclassic ddf parser sections.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_state_blocks`

- Description: Likely controls hcde edgeclassic ddf parser state blocks.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_template_directives`

- Description: Likely controls hcde edgeclassic ddf parser template directives.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_parser_warnings`

- Description: Likely controls hcde edgeclassic ddf parser warnings.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_scripts`

- Description: Likely controls hcde edgeclassic ddf scripts.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_ddf_sources`

- Description: Likely controls hcde edgeclassic ddf sources.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_discovered_roots`

- Description: Likely controls hcde edgeclassic discovered roots.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_discovery_record_count`

- Description: Likely controls hcde edgeclassic discovery record count.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_discovery_record_hashes`

- Description: Likely controls hcde edgeclassic discovery record hashes.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_discovery_record_names`

- Description: Likely controls hcde edgeclassic discovery record names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_discovery_records`

- Description: Likely controls hcde edgeclassic discovery records.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_document_roots`

- Description: Likely controls hcde edgeclassic document roots.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_feature_names`

- Description: Likely controls hcde edgeclassic feature names.
- Current value: `reference-only`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_features`

- Description: Likely controls hcde edgeclassic features.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_indexed_lookup_check_count`

- Description: Likely controls hcde edgeclassic indexed lookup check count.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_indexed_lookup_consistent`

- Description: Likely controls hcde edgeclassic indexed lookup consistent.
- Current value: `1`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_indexed_lookup_miss_count`

- Description: Likely controls hcde edgeclassic indexed lookup miss count.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_indexed_record_count`

- Description: Likely controls hcde edgeclassic indexed record count.
- Current value: `-7.98239e+06`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_indexed_record_kinds`

- Description: Likely controls hcde edgeclassic indexed record kinds.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_indexed_record_names`

- Description: Likely controls hcde edgeclassic indexed record names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_indexed_record_paths`

- Description: Likely controls hcde edgeclassic indexed record paths.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_indexed_records`

- Description: Likely controls hcde edgeclassic indexed records.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_core_modules`

- Description: Likely controls hcde edgeclassic lua core modules.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_entry_modules`

- Description: Likely controls hcde edgeclassic lua entry modules.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_errors`

- Description: Likely controls hcde edgeclassic lua errors.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_functions`

- Description: Likely controls hcde edgeclassic lua functions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_local_declarations`

- Description: Likely controls hcde edgeclassic lua local declarations.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_local_functions`

- Description: Likely controls hcde edgeclassic lua local functions.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_modules`

- Description: Likely controls hcde edgeclassic lua modules.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_requires`

- Description: Likely controls hcde edgeclassic lua requires.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_returns`

- Description: Likely controls hcde edgeclassic lua returns.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_setmetatable_calls`

- Description: Likely controls hcde edgeclassic lua setmetatable calls.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_sources`

- Description: Likely controls hcde edgeclassic lua sources.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_support_sources`

- Description: Likely controls hcde edgeclassic lua support sources.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_lua_warnings`

- Description: Likely controls hcde edgeclassic lua warnings.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_manifest_protocol`

- Description: Likely controls hcde edgeclassic manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_package_roots`

- Description: Likely controls hcde edgeclassic package roots.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_reference_docs`

- Description: Likely controls hcde edgeclassic reference docs.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_reference_files`

- Description: Likely controls hcde edgeclassic reference files.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_reference_root_resolved`

- Description: Likely controls hcde edgeclassic reference root resolved.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_runtime`

- Description: Likely controls hcde edgeclassic runtime.
- Current value: `reference-only`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_source_branch`

- Description: Likely controls hcde edgeclassic source branch.
- Current value: `master`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_source_map_count`

- Description: Likely controls hcde edgeclassic source map count.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_source_map_kinds`

- Description: Likely controls hcde edgeclassic source map kinds.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_source_map_names`

- Description: Likely controls hcde edgeclassic source map names.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_source_map_records`

- Description: Likely controls hcde edgeclassic source map records.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_source_repository`

- Description: Likely controls hcde edgeclassic source repository.
- Current value: `https://github.com/edge-classic/EDGE-classic.git`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_source_revision`

- Description: Likely controls hcde edgeclassic source revision.
- Current value: `766453d5981d6653ba1c642e780d58f677082523`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_source_roots`

- Description: Likely controls hcde edgeclassic source roots.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_edgeclassic_status`

- Description: Likely controls hcde edgeclassic status.
- Current value: `staged-reference-only`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_engine`

- Description: Likely controls hcde engine.
- Current value: `HCDE`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_feature_names`

- Description: Likely controls hcde feature names.
- Current value: `hcde-base,unlagged,client-prediction,dc-stats,script-profiles`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_manifest_protocol`

- Description: Likely controls hcde manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_consistency_issues`

- Description: Likely controls hcde release shape consistency issues.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_consumer_count`

- Description: Likely controls hcde release shape consumer count.
- Current value: `5`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_consumer_status`

- Description: Likely controls hcde release shape consumer status.
- Current value: `staged-addon-consumer-runtime-ready`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_dependency_edges`

- Description: Likely controls hcde release shape dependency edges.
- Current value: `8`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_framework_family_count`

- Description: Likely controls hcde release shape framework family count.
- Current value: `6`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_framework_status`

- Description: Likely controls hcde release shape framework status.
- Current value: `staged-addon-framework-runtime-ready`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_manifest_protocol`

- Description: Likely controls hcde release shape manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_package_discovery_families`

- Description: Likely controls hcde release shape package discovery families.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_ready_signals`

- Description: Likely controls hcde release shape ready signals.
- Current value: `2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_status`

- Description: Likely controls hcde release shape status.
- Current value: `staged-release-shape-ready`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_release_shape_summary`

- Description: Likely controls hcde release shape summary.
- Current value: `framework=staged-addon-framework-runtime-ready consumer=staged-addon-consumer-runtime-ready issues=0 ready=2`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_required_features`

- Description: Likely controls hcde required features.
- Current value: `31`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_script_denied_bindings`

- Description: Likely controls hcde script denied bindings.
- Current value: `0`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_script_feature_names`

- Description: Likely controls hcde script feature names.
- Current value: `modern-acs,acs-inventory,zscript-metadata,zscript-bytecode,zscript-runtime,zscript-mutation`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_script_features`

- Description: Likely controls hcde script features.
- Current value: `63`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_script_manifest_protocol`

- Description: Likely controls hcde script manifest protocol.
- Current value: `1`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_script_profile`

- Description: Likely controls hcde script profile.
- Current value: `zscript-safe`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_script_replicated_fields`

- Description: Likely controls hcde script replicated fields.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_script_runtime`

- Description: Likely controls hcde script runtime.
- Current value: `server-authoritative`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `hcde_script_status`

- Description: Likely controls hcde script status.
- Current value: `zscript-safe-runtime-ready`
- Raw flag field: "S-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `join_password`

- Description: Likely controls join password.
- Current value: ``
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `language`

- Description: Likely controls language.
- Current value: `auto`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `log_fulltimestamps`

- Description: Likely controls log fulltimestamps.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `log_packetdebug`

- Description: Likely controls log packetdebug.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `lookspring`

- Description: Likely controls lookspring.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `net_rcvbuf`

- Description: Likely controls rcvbuf behavior for network.
- Current value: `131072`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `net_sndbuf`

- Description: Likely controls sndbuf behavior for network.
- Current value: `131072`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `port`

- Description: Likely controls port.
- Current value: `10666`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `r_softinvulneffect`

- Description: Likely controls r softinvulneffect.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `rcon_password`

- Description: Likely controls rcon password.
- Current value: ``
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_aircontrol`

- Description: Server setting: Air Control
- Current value: `0.00390625`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_allowcheats`

- Description: Likely controls allowcheats behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_allowexit`

- Description: Likely controls allowexit behavior for server.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_allowfov`

- Description: Likely controls allowfov behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_allowjump`

- Description: Likely controls allowjump behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_allowmovebob`

- Description: Likely controls allowmovebob behavior for server.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_allowpwo`

- Description: Likely controls allowpwo behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_allowredscreen`

- Description: Likely controls allowredscreen behavior for server.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_allowshowspawns`

- Description: Likely controls allowshowspawns behavior for server.
- Current value: `1`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_allowtargetnames`

- Description: Likely controls allowtargetnames behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_allowwidescreen`

- Description: Likely controls allowwidescreen behavior for server.
- Current value: `1`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_banfile`

- Description: Likely controls banfile behavior for server.
- Current value: `banlist.json`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_banfile_reload`

- Description: Likely controls banfile reload behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_coinflip`

- Description: Likely controls callvote coinflip behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_forcespec`

- Description: Likely controls callvote forcespec behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_forcestart`

- Description: Likely controls callvote forcestart behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_fraglimit`

- Description: Likely controls callvote fraglimit behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_kick`

- Description: Likely controls callvote kick behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_lives`

- Description: Likely controls callvote lives behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_map`

- Description: Likely controls callvote map behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_nextmap`

- Description: Likely controls callvote nextmap behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_randcaps`

- Description: Likely controls callvote randcaps behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_randmap`

- Description: Likely controls callvote randmap behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_randpickup`

- Description: Likely controls callvote randpickup behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_restart`

- Description: Likely controls callvote restart behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_scorelimit`

- Description: Likely controls callvote scorelimit behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_callvote_timelimit`

- Description: Likely controls callvote timelimit behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_clientcount`

- Description: Likely controls clientcount behavior for server.
- Current value: `0`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_countdown`

- Description: Likely controls countdown behavior for server.
- Current value: `5`
- Raw flag field: "A  L"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_curmap`

- Description: Likely controls curmap behavior for server.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_curpwad`

- Description: Likely controls curpwad behavior for server.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_dmfarspawn`

- Description: Likely controls dmfarspawn behavior for server.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_doubleammo`

- Description: Likely controls doubleammo behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_downloadsites`

- Description: Likely controls downloadsites behavior for server.
- Current value: ``
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_email`

- Description: Likely controls email behavior for server.
- Current value: `email@domain.com`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_emptyfreeze`

- Description: Likely controls emptyfreeze behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_emptyreset`

- Description: Likely controls emptyreset behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_endmapscript`

- Description: Likely controls endmapscript behavior for server.
- Current value: ``
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_fastmonsters`

- Description: Likely controls fastmonsters behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_flooddelay`

- Description: Likely controls flooddelay behavior for server.
- Current value: `1.5`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_forcerespawn`

- Description: Likely controls forcerespawn behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_forcerespawntime`

- Description: Likely controls forcerespawntime behavior for server.
- Current value: `30`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_forcewater`

- Description: Likely controls forcewater behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_fragexitswitch`

- Description: Likely controls fragexitswitch behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_fraglimit`

- Description: Likely controls fraglimit behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_freelook`

- Description: Likely controls freelook behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_friendlyfire`

- Description: Likely controls friendlyfire behavior for server.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_friendlymonsterfire`

- Description: Likely controls friendlymonsterfire behavior for server.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_gametype`

- Description: Server setting: Game Type
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_globalspectatorchat`

- Description: Likely controls globalspectatorchat behavior for server.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_gravity`

- Description: Server setting: Gravity
- Current value: `800`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_hostname`

- Description: Server setting: Hostname
- Current value: `Untitled HCDE Server`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_infiniteammo`

- Description: Likely controls infiniteammo behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_intermissionlimit`

- Description: Likely controls intermissionlimit behavior for server.
- Current value: `10`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_itemrespawntime`

- Description: Likely controls itemrespawntime behavior for server.
- Current value: `30`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_itemsrespawn`

- Description: Likely controls itemsrespawn behavior for server.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_keepkeys`

- Description: Likely controls keepkeys behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_maxclients`

- Description: Likely controls maxclients behavior for server.
- Current value: `4`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_maxcorpses`

- Description: Likely controls maxcorpses behavior for server.
- Current value: `200`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_maxplayers`

- Description: Server setting: Max Players
- Current value: `4`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_maxplayersperteam`

- Description: Likely controls maxplayersperteam behavior for server.
- Current value: `3`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_maxrate`

- Description: Likely controls maxrate behavior for server.
- Current value: `200`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_maxunlagtime`

- Description: Likely controls maxunlagtime behavior for server.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_monsterdamage`

- Description: Likely controls monsterdamage behavior for server.
- Current value: `1`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_monstershealth`

- Description: Likely controls monstershealth behavior for server.
- Current value: `1`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_monstersrespawn`

- Description: Likely controls monstersrespawn behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_motd`

- Description: Server setting: MOTD
- Current value: `Welcome to HCDE`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_natport`

- Description: Server setting: NAT Port
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_nextmap`

- Description: Likely controls nextmap behavior for server.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_nomonsters`

- Description: Likely controls nomonsters behavior for server.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_playerbeacons`

- Description: Likely controls playerbeacons behavior for server.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_respawnsuper`

- Description: Likely controls respawnsuper behavior for server.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_scorelimit`

- Description: Likely controls scorelimit behavior for server.
- Current value: `5`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_sharekeys`

- Description: Likely controls sharekeys behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_showplayerpowerups`

- Description: Likely controls showplayerpowerups behavior for server.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_shufflemaplist`

- Description: Likely controls shufflemaplist behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_skill`

- Description: Likely controls skill behavior for server.
- Current value: `3`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_spawndelaytime`

- Description: Likely controls spawndelaytime behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_splashfactor`

- Description: Likely controls splashfactor behavior for server.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_startmapscript`

- Description: Likely controls startmapscript behavior for server.
- Current value: ``
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_startwadscript`

- Description: Likely controls startwadscript behavior for server.
- Current value: ``
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_teamsinplay`

- Description: Likely controls teamsinplay behavior for server.
- Current value: `2`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_teamspawns`

- Description: Likely controls teamspawns behavior for server.
- Current value: `1`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_ticbuffer`

- Description: Likely controls ticbuffer behavior for server.
- Current value: `1`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_timelimit`

- Description: Likely controls timelimit behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_unblockfriendly`

- Description: Likely controls unblockfriendly behavior for server.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_unblockplayers`

- Description: Likely controls unblockplayers behavior for server.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_upnp`

- Description: Likely controls upnp behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_upnp_description`

- Description: Likely controls upnp description behavior for server.
- Current value: ``
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_upnp_discovertimeout`

- Description: Likely controls upnp discovertimeout behavior for server.
- Current value: `2000`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_upnp_externalip`

- Description: Likely controls upnp externalip behavior for server.
- Current value: ``
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_upnp_internalip`

- Description: Likely controls upnp internalip behavior for server.
- Current value: `192.168.1.120`
- Raw flag field: "-"
- Archive: No
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: No (write-protected in flag map)
- Set/get result: Not applicable
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_usemasters`

- Description: Likely controls usemasters behavior for server.
- Current value: `0`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_vote_countabs`

- Description: Likely controls vote countabs behavior for server.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_vote_majority`

- Description: Likely controls vote majority behavior for server.
- Current value: `0.5`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_vote_speccall`

- Description: Likely controls vote speccall behavior for server.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_vote_specvote`

- Description: Likely controls vote specvote behavior for server.
- Current value: `1`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_vote_timelimit`

- Description: Likely controls vote timelimit behavior for server.
- Current value: `30`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_vote_timeout`

- Description: Likely controls vote timeout behavior for server.
- Current value: `60`
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_warmup`

- Description: Likely controls warmup behavior for server.
- Current value: `0`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_warmup_autostart`

- Description: Likely controls warmup autostart behavior for server.
- Current value: `1`
- Raw flag field: "A  L"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_weapondamage`

- Description: Likely controls weapondamage behavior for server.
- Current value: `1`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `sv_weapondrop`

- Description: Likely controls weapondrop behavior for server.
- Current value: `0`
- Raw flag field: "A S"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

### `sv_weaponstay`

- Description: Likely controls weaponstay behavior for server.
- Current value: `1`
- Raw flag field: "A SL"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: Yes (latched message observed during set pass)
- Write-protected message observed in sweep: No

### `waddirs`

- Description: Likely controls waddirs.
- Current value: ``
- Raw flag field: "A"
- Archive: Yes
- Scope/type: Local/General
- Mutability: Writable now
- Modified flag (`M`): No
- Ignored flag (`X`): No
- Set/get tested: Yes (set/get sweep)
- Set/get result: OK
- Latched behavior observed: No
- Write-protected message observed in sweep: No

