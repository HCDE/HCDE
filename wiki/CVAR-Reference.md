# HCDE CVAR Reference

Generated: 2026-05-27 02:04:19 UTC

This reference combines source-defined CVAR inventory with the imported runtime audit snapshot.

## Coverage

- Source CVAR definitions discovered: **1029** unique / **1036** total macro definitions
- Source-defined CVARs absent from imported runtime snapshot: **1005**
- Runtime-only CVARs from imported snapshot: **425**
- Total runtime CVARs in imported snapshot: **449**
- Set/get tested runtime CVARs: **176**
- Successful get responses: **176**
- Missing get responses: **0**
- Unexpected parser/runtime lines during sweep: **0**
- Runtime baseline CSV: `C:\path\to\HCDE\tmp_cvar_baseline.csv`
- Set/get report: `C:\path\to\HCDE\tmp_cvar_setpass_report.json`

> Note: the source catalog is regenerated from the current checkout. The runtime snapshot is imported from the audit files above, so entries marked absent may still be valid source CVARs that were not visible in that older runtime capture.

## Flag Legend

- Position 1: `A` = archived, space = not archived
- Position 2: `U` = userinfo, `S` = serverinfo, `C` = auto/custom, space = local/general
- Position 3: `-` = write-protected, `L` = latched, `*` = unsettable auto cvar, space = writable
- Position 4: `M` = modified/session-marked
- Position 5: `X` = ignored/hidden from normal flow

## HCDE Server, Invasion, and Netcode CVARs

These are the high-value controls for invasion, net diagnostics, compatibility, and heavy-load cleanup.

### `duellimit`

- Description: Legacy Skulltag compatibility value for duel limit metadata.
- Source default: `0`
- Valid range/shape: `0..255`
- Source: `src/d_net_invasion.cpp:113`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `net_hcde_native_only`

- Description: Requires HCDE-native networking/capability paths for multiplayer sessions.
- Source default: `true`
- Valid range/shape: `n/a`
- Source: `src/d_net.cpp:306`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `net_predict_debug`

- Description: Controls HCDE prediction diagnostics: off, CSV sampling, and/or on-screen/debug trace output depending on level.
- Source default: `3`
- Valid range/shape: `n/a`
- Source: `src/d_net.cpp:207`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `net_predict_debug_interval`

- Description: Tic interval used by prediction CSV/debug sampling.
- Source default: `15`
- Valid range/shape: `n/a`
- Source: `src/d_net.cpp:215`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `net_predict_softwarn_ack_lag`

- Description: Soft warning threshold for client ack lag during prediction diagnostics.
- Source default: `3`
- Valid range/shape: `n/a`
- Source: `src/d_net.cpp:262`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `net_predict_softwarn_mirror_delta`

- Description: Soft warning threshold for invasion mirror drift during prediction diagnostics.
- Source default: `2`
- Valid range/shape: `n/a`
- Source: `src/d_net.cpp:289`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `net_predict_softwarn_passive_storm`

- Description: Soft warning threshold for passive update storms during prediction diagnostics.
- Source default: `5`
- Valid range/shape: `n/a`
- Source: `src/d_net.cpp:297`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_corpsefilter`

- Description: Selects which corpse queues sv_corpsequeuesize trims: 0 off, 1 monsters, 2 players, 3 both.
- Source default: `1`
- Valid range/shape: `0..3`
- Source: `src/g_cvars.cpp:176`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_corpsequeuesize`

- Description: Maximum queued corpses retained by corpse cleanup; used with sv_corpsefilter.
- Source default: `64`
- Valid range/shape: `>= 0`
- Source: `src/g_cvars.cpp:184`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionbasebudget`

- Description: Base monster budget each wave starts with.
- Source default: `24`
- Valid range/shape: `>= 1`
- Source: `src/d_net_invasion.cpp:123`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionbossbonus`

- Description: Extra budget added during boss waves.
- Source default: `20`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:158`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionbosswaveevery`

- Description: Boss wave cadence (e.g. 5 = every 5th wave, 0 = never).
- Source default: `5`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:153`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionbudgetstep`

- Description: Budget increase applied per wave number.
- Source default: `8`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:128`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasioncleanuptime`

- Description: Seconds allowed for cleanup phase after spawning ends.
- Source default: `4.0f`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:81`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasioncountdowntime`

- Description: Seconds before wave 1 starts ("Prepare for invasion" countdown).
- Source default: `30.0f`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:68`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasiondebug`

- Description: Server setting: Invasion Debug
- Source default: `0`
- Valid range/shape: `n/a`
- Source: `src/d_net.cpp:188`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionexitonvictory`

- Description: Server setting: Invasion Exit Victory
- Source default: `true`
- Valid range/shape: `n/a`
- Source: `src/d_net_invasion.cpp:96`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionintermissiontime`

- Description: Seconds between completed waves before the next wave starts.
- Source default: `6.0f`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:86`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionmaxactive`

- Description: Optional cap for active invasion monsters. 0 disables the cap; positive values are clamped by the engine.
- Source default: `0`
- Valid range/shape: `0 or 1..1024`
- Source: `src/d_net_invasion.cpp:148`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionperplayer`

- Description: Additional budget per extra active player.
- Source default: `6`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:133`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionresulttime`

- Description: Seconds to keep the final victory/failure state visible.
- Source default: `8.0f`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:91`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionsimlod`

- Description: Enables server-side simulation LOD for invasion monsters so distant actors think less often under heavy load.
- Source default: `true`
- Valid range/shape: `bool`
- Source: `src/d_net_invasion.cpp:169`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionsimloddormantinterval`

- Description: Think interval in tics for dormant distant invasion simulation.
- Source default: `TICRATE * 3`
- Valid range/shape: `>= 1 tic`
- Source: `src/d_net_invasion.cpp:190`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionsimlodfullrange`

- Description: Distance within which invasion monsters keep full-rate simulation.
- Source default: `2048.0f`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:172`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionsimlodreducedinterval`

- Description: Think interval in tics for reduced-rate invasion simulation.
- Source default: `5`
- Valid range/shape: `>= 1 tic`
- Source: `src/d_net_invasion.cpp:184`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionsimlodreducedrange`

- Description: Distance within which invasion monsters use reduced-rate simulation before becoming dormant.
- Source default: `4096.0f`
- Valid range/shape: `>= sv_invasionsimlodfullrange`
- Source: `src/d_net_invasion.cpp:178`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionspawnburst`

- Description: Maximum monsters spawned per spawn tick burst.
- Source default: `3`
- Valid range/shape: `>= 1`
- Source: `src/d_net_invasion.cpp:143`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionspawninterval`

- Description: Seconds between spawn ticks while wave spawning is active.
- Source default: `0.35f`
- Valid range/shape: `>= 0.05`
- Source: `src/d_net_invasion.cpp:138`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionspawntime`

- Description: Wave spawn window length in seconds before cleanup phase.
- Source default: `8.0f`
- Valid range/shape: `>= 0`
- Source: `src/d_net_invasion.cpp:76`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionspotfallback`

- Description: Fallback to generic spawning when tagged invasion spots cannot be used.
- Source default: `true`
- Valid range/shape: `bool`
- Source: `src/d_net_invasion.cpp:166`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionspotusemaptags`

- Description: Restrict native invasion spots by map thing TID/tag. Keep disabled for Skulltag/Zandronum map compatibility; the spot arguments already control wave timing.
- Source default: `false`
- Valid range/shape: `bool`
- Source: `src/d_net_invasion.cpp:163`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_invasionwaves`

- Description: Maximum number of invasion waves in a run.
- Source default: `8`
- Valid range/shape: `1..255`
- Source: `src/d_net_invasion.cpp:99`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `sv_usemapsettingswavelimit`

- Description: If enabled, map-defined invasion wavelimit metadata overrides sv_invasionwaves when present.
- Source default: `true`
- Valid range/shape: `bool`
- Source: `src/d_net_invasion.cpp:120`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

### `wavelimit`

- Description: Legacy Skulltag compatibility override for invasion waves. 0 disables the override; 1..255 forces that wave count.
- Source default: `0`
- Valid range/shape: `0..255`
- Source: `src/d_net_invasion.cpp:106`
- Present in runtime snapshot: No (not in this runtime snapshot)
- Runtime snapshot value: `n/a`

## Source-Defined CVAR Catalog

This section is generated from CVAR, CUSTOM_CVAR, CVARD, CUSTOM_CVARD, and named CVAR macros in src/.

### `addrocketexplosion`

- Description: Likely controls addrocketexplosion.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_mobj.cpp:135`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_auto_arpeggio`

- Description: Likely controls adl auto arpeggio.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:107`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_bank`

- Description: Likely controls adl bank.
- Type: `Int`
- Source default: `14`
- Source flags: `CVAR_ARCHIVE | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:77`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_chan_alloc`

- Description: Likely controls adl chan alloc.
- Type: `Int`
- Source default: `0 /*ADLMIDI_ChanAlloc_AUTO*/`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:102`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_chips_count`

- Description: Likely controls adl chips count.
- Type: `Int`
- Source default: `6`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:57`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_custom_bank`

- Description: Likely controls adl custom bank.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_VIRTUAL | CVAR_SYSTEM_ONLY`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:92`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_emulator_id`

- Description: Likely controls adl emulator id.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_fullpan`

- Description: Likely controls adl fullpan.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:72`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_gain`

- Description: Likely controls adl gain.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:112`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_run_at_pcm_rate`

- Description: Likely controls adl run at pcm rate.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:67`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_use_custom_bank`

- Description: Likely controls adl use custom bank.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:82`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_use_genmidi`

- Description: Likely controls adl use genmidi.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:87`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `adl_volume_model`

- Description: Likely controls adl volume model.
- Type: `Int`
- Source default: `0 /*ADLMIDI_VolumeModel_AUTO*/`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:97`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `aimdebug`

- Description: Likely controls aimdebug.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_map.cpp:3882`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `allcheats`

- Description: Likely controls allcheats.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/st_stuff.cpp:296`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `allowsingleplayerscripts`

- Description: Likely controls allowsingleplayerscripts.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_acs.cpp:10957`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `alwaysapplydmflags`

- Description: Server setting: Apply DM Flags Always
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:140`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_backcolor`

- Description: Likely controls am backcolor.
- Type: `Color`
- Source default: `0x6c5440`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:269`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_cdwallcolor`

- Description: Likely controls am cdwallcolor.
- Type: `Color`
- Source default: `0x4c3820`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:276`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_cheat`

- Description: Likely controls am cheat.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:133`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_colorset`

- Description: Likely controls am colorset.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:159`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_customcolors`

- Description: Likely controls am customcolors.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:160`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_drawmapback`

- Description: Likely controls am drawmapback.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:162`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_efwallcolor`

- Description: Likely controls am efwallcolor.
- Type: `Color`
- Source default: `0x665555`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:277`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_emptyspacemargin`

- Description: Likely controls am emptyspacemargin.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:168`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_fdwallcolor`

- Description: Likely controls am fdwallcolor.
- Type: `Color`
- Source default: `0x887058`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:275`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_followplayer`

- Description: Likely controls am followplayer.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:191`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_gridcolor`

- Description: Likely controls am gridcolor.
- Type: `Color`
- Source default: `0x8b5a2b`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:279`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_interlevelcolor`

- Description: Likely controls am interlevelcolor.
- Type: `Color`
- Source default: `0xff0000`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:284`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_intralevelcolor`

- Description: Likely controls am intralevelcolor.
- Type: `Color`
- Source default: `0x0000ff`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:283`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_linealpha`

- Description: Likely controls am linealpha.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:119`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_lineantialiasing`

- Description: Likely controls am lineantialiasing.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:121`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_linethickness`

- Description: Likely controls am linethickness.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:120`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_lockedcolor`

- Description: Likely controls am lockedcolor.
- Type: `Color`
- Source default: `0x007800`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:282`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_map_secrets`

- Description: Likely controls am map secrets.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:161`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_markcolor`

- Description: Likely controls am markcolor.
- Type: `Int`
- Source default: `CR_GREY`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:198`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_markfont`

- Description: Likely controls am markfont.
- Type: `String`
- Source default: `DEFAULT_FONT_NAME`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:197`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_notseencolor`

- Description: Likely controls am notseencolor.
- Type: `Color`
- Source default: `0x6c6c6c`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:281`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovcdwallcolor`

- Description: Likely controls am ovcdwallcolor.
- Type: `Color`
- Source default: `0x008844`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:304`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovefwallcolor`

- Description: Likely controls am ovefwallcolor.
- Type: `Color`
- Source default: `0x008844`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:302`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_overlay`

- Description: Likely controls am overlay.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:144`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovfdwallcolor`

- Description: Likely controls am ovfdwallcolor.
- Type: `Color`
- Source default: `0x008844`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:303`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovinterlevelcolor`

- Description: Likely controls am ovinterlevelcolor.
- Type: `Color`
- Source default: `0xffff00`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:307`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovlockedcolor`

- Description: Likely controls am ovlockedcolor.
- Type: `Color`
- Source default: `0x008844`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:301`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovotherwallscolor`

- Description: Likely controls am ovotherwallscolor.
- Type: `Color`
- Source default: `0x008844`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:300`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovportalcolor`

- Description: Likely controls am ovportalcolor.
- Type: `Color`
- Source default: `0x004022`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:318`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovsecretsectorcolor`

- Description: Likely controls am ovsecretsectorcolor.
- Type: `Color`
- Source default: `0x00ffff`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:308`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovsecretwallcolor`

- Description: Likely controls am ovsecretwallcolor.
- Type: `Color`
- Source default: `0x008844`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:298`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovsectorfillalpha`

- Description: Likely controls am ovsectorfillalpha.
- Type: `Float`
- Source default: `0.2f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:317`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovsectorfillcolor`

- Description: Likely controls am ovsectorfillcolor.
- Type: `Color`
- Source default: `0x000000`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:316`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovspecialwallcolor`

- Description: Likely controls am ovspecialwallcolor.
- Type: `Color`
- Source default: `0xffffff`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:299`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovtelecolor`

- Description: Likely controls am ovtelecolor.
- Type: `Color`
- Source default: `0xffff00`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:306`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovthingcolor`

- Description: Likely controls am ovthingcolor.
- Type: `Color`
- Source default: `0xe88800`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:310`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovthingcolor_citem`

- Description: Likely controls am ovthingcolor citem.
- Type: `Color`
- Source default: `0xe88800`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:315`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovthingcolor_friend`

- Description: Likely controls am ovthingcolor friend.
- Type: `Color`
- Source default: `0xe88800`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:311`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovthingcolor_item`

- Description: Likely controls am ovthingcolor item.
- Type: `Color`
- Source default: `0xe88800`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:314`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovthingcolor_monster`

- Description: Likely controls am ovthingcolor monster.
- Type: `Color`
- Source default: `0xe88800`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:312`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovthingcolor_ncmonster`

- Description: Likely controls am ovthingcolor ncmonster.
- Type: `Color`
- Source default: `0xe88800`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:313`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovunexploredsecretcolor`

- Description: Likely controls am ovunexploredsecretcolor.
- Type: `Color`
- Source default: `0x00ffff`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:309`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovunseencolor`

- Description: Likely controls am ovunseencolor.
- Type: `Color`
- Source default: `0x00226e`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:305`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovwallcolor`

- Description: Likely controls am ovwallcolor.
- Type: `Color`
- Source default: `0x00ff00`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:297`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_ovyourcolor`

- Description: Likely controls am ovyourcolor.
- Type: `Color`
- Source default: `0xfce8d8`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:296`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_portalcolor`

- Description: Likely controls am portalcolor.
- Type: `Color`
- Source default: `0x404040`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:294`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_portaloverlay`

- Description: Likely controls am portaloverlay.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:192`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_rotate`

- Description: Likely controls am rotate.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:143`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_secretsectorcolor`

- Description: Likely controls am secretsectorcolor.
- Type: `Color`
- Source default: `0xff00ff`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:285`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_secretwallcolor`

- Description: Likely controls am secretwallcolor.
- Type: `Color`
- Source default: `0x000000`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:272`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_sectorfillalpha`

- Description: Likely controls am sectorfillalpha.
- Type: `Float`
- Source default: `0.4f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:293`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_sectorfillcolor`

- Description: Likely controls am sectorfillcolor.
- Type: `Color`
- Source default: `0x4e3621`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:292`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showalllines`

- Description: Likely controls am showalllines.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:126`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showcluster`

- Description: Likely controls am showcluster.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:57`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showepisode`

- Description: Likely controls am showepisode.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:56`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showgrid`

- Description: Likely controls am showgrid.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:193`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showitems`

- Description: Likely controls am showitems.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:155`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showkeys`

- Description: Likely controls am showkeys.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:163`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showkeys_always`

- Description: Likely controls am showkeys always.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:166`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showlevelname`

- Description: Likely controls am showlevelname.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:158`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showmaplabel`

- Description: Likely controls am showmaplabel.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:114`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showmonsters`

- Description: Likely controls am showmonsters.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:154`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showsecrets`

- Description: Likely controls am showsecrets.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:153`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showsubsector`

- Description: Likely controls am showsubsector.
- Type: `Int`
- Source default: `-1`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:123`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showthingsprites`

- Description: Likely controls am showthingsprites.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:165`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showtime`

- Description: Likely controls am showtime.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:156`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showtotaltime`

- Description: Likely controls am showtotaltime.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:157`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_showtriggerlines`

- Description: Likely controls am showtriggerlines.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:164`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_specialwallcolor`

- Description: Likely controls am specialwallcolor.
- Type: `Color`
- Source default: `0xffffff`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:273`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_textured`

- Description: Likely controls am textured.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:118`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_thingcolor`

- Description: Likely controls am thingcolor.
- Type: `Color`
- Source default: `0xfcfcfc`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:278`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_thingcolor_citem`

- Description: Likely controls am thingcolor citem.
- Type: `Color`
- Source default: `0xfcfcfc`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:291`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_thingcolor_friend`

- Description: Likely controls am thingcolor friend.
- Type: `Color`
- Source default: `0xfcfcfc`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:287`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_thingcolor_item`

- Description: Likely controls am thingcolor item.
- Type: `Color`
- Source default: `0xfcfcfc`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:290`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_thingcolor_monster`

- Description: Likely controls am thingcolor monster.
- Type: `Color`
- Source default: `0xfcfcfc`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:288`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_thingcolor_ncmonster`

- Description: Likely controls am thingcolor ncmonster.
- Type: `Color`
- Source default: `0xfcfcfc`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:289`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_thingrenderstyles`

- Description: Likely controls am thingrenderstyles.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:122`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_tswallcolor`

- Description: Likely controls am tswallcolor.
- Type: `Color`
- Source default: `0x888888`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:274`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_unexploredsecretcolor`

- Description: Likely controls am unexploredsecretcolor.
- Type: `Color`
- Source default: `0xff00ff`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:286`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_wallcolor`

- Description: Likely controls am wallcolor.
- Type: `Color`
- Source default: `0x2c1808`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:271`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_xhaircolor`

- Description: Likely controls am xhaircolor.
- Type: `Color`
- Source default: `0x808080`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:280`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_yourcolor`

- Description: Likely controls am yourcolor.
- Type: `Color`
- Source default: `0xfce8d8`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:270`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `am_zoomdir`

- Description: Likely controls am zoomdir.
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/am_map.cpp:194`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `anonstats_enabled411`

- Description: Likely controls anonstats enabled411.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_anonstats.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `anonstats_host`

- Description: Likely controls anonstats host.
- Type: `String`
- Source default: `"gzstats.drdteam.org"`
- Source flags: `CVAR_NOSET`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_anonstats.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `anonstats_port`

- Description: Likely controls anonstats port.
- Type: `Int`
- Source default: `80`
- Source flags: `CVAR_NOSET`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_anonstats.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `autoaim`

- Description: Likely controls autoaim.
- Type: `Float`
- Source default: `35.f`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:47`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `autoloadbrightmaps`

- Description: Likely controls autoloadbrightmaps.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:506`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `autoloadlights`

- Description: Likely controls autoloadlights.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:507`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `autoloadwidescreen`

- Description: Likely controls autoloadwidescreen.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:508`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `autosavecount`

- Description: Likely controls autosavecount.
- Type: `Int`
- Source default: `4`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:414`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `autosavenum`

- Description: Likely controls autosavenum.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_NOSET|CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:410`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `blood_fade_scalar`

- Description: Likely controls blood fade scalar.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/2d/v_blend.cpp:43`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `bot_allowspy`

- Description: Likely controls bot allowspy.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:408`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `bot_next_color`

- Description: Likely controls bot next color.
- Type: `Int`
- Source default: `11`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/bots/b_bot.cpp:144`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `bottomskew`

- Description: Likely controls bottomskew.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_walls.cpp:2131`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chase_dist`

- Description: Likely controls chase dist.
- Type: `Float`
- Source default: `90.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_map.cpp:5666`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `90`

### `chase_height`

- Description: Likely controls chase height.
- Type: `Float`
- Source default: `-8.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_map.cpp:5665`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `-8`

### `chasedemo`

- Description: Likely controls chasedemo.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:288`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chat_substitution`

- Description: Likely controls chat substitution.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:111`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro0`

- Description: Likely controls chatmacro0.
- Type: `String`
- Source default: `"No"`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:95`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro1`

- Description: Likely controls chatmacro1.
- Type: `String`
- Source default: `"I'm ready to kick butt!"`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:86`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro2`

- Description: Likely controls chatmacro2.
- Type: `String`
- Source default: `"I'm OK."`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:87`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro3`

- Description: Likely controls chatmacro3.
- Type: `String`
- Source default: `"I'm not looking too good!"`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:88`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro4`

- Description: Likely controls chatmacro4.
- Type: `String`
- Source default: `"Help!"`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:89`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro5`

- Description: Likely controls chatmacro5.
- Type: `String`
- Source default: `"You suck!"`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:90`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro6`

- Description: Likely controls chatmacro6.
- Type: `String`
- Source default: `"Next time, scumbag..."`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:91`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro7`

- Description: Likely controls chatmacro7.
- Type: `String`
- Source default: `"Come here!"`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:92`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro8`

- Description: Likely controls chatmacro8.
- Type: `String`
- Source default: `"I'll take care of it."`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:93`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `chatmacro9`

- Description: Likely controls chatmacro9.
- Type: `String`
- Source default: `"Yes"`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:94`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_analog_run`

- Description: Likely controls analog run behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:330`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_analog_sensitivity_pitch`

- Description: Likely controls analog sensitivity pitch behavior for client.
- Type: `Float`
- Source default: `0.6f`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:329`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_analog_sensitivity_yaw`

- Description: Likely controls analog sensitivity yaw behavior for client.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:328`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_analog_straferun`

- Description: Likely controls analog straferun behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:331`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_bbannounce`

- Description: Likely controls bbannounce behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/bbannouncer.cpp:63`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_blockcheats`

- Description: Likely controls blockcheats behavior for client.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_cmds.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_bloodsplats`

- Description: Likely controls bloodsplats behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_map.cpp:67`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_bloodtype`

- Description: Likely controls bloodtype behavior for client.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_mobj.cpp:137`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_capfps`

- Description: Likely controls capfps behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_framebuffer.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_custominvulmapcolor1`

- Description: Likely controls custominvulmapcolor1 behavior for client.
- Type: `Color`
- Source default: `0x00001a`
- Source flags: `CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/r_data/colormaps.cpp:41`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_custominvulmapcolor2`

- Description: Likely controls custominvulmapcolor2 behavior for client.
- Type: `Color`
- Source default: `0xa6a67a`
- Source flags: `CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/r_data/colormaps.cpp:46`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_customizeinvulmap`

- Description: Likely controls customizeinvulmap behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/r_data/colormaps.cpp:37`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_debug_monster_proximity`

- Description: Likely controls debug monster proximity behavior for client.
- Type: `Int`
- Source default: `768`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:281`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_debugprediction`

- Description: Likely controls debugprediction behavior for client.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_CHEAT`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:207`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_defaultconfiguration`

- Description: Likely controls defaultconfiguration behavior for client.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_bind.cpp:879`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_doautoaim`

- Description: Likely controls doautoaim behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_map.cpp:69`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_doubleclickthreshold`

- Description: Likely controls doubleclickthreshold behavior for client.
- Type: `Int`
- Source default: `250`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_bind.cpp:139`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_gfxlocalization`

- Description: Likely controls gfxlocalization behavior for client.
- Type: `Int`
- Source default: `3`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/menu/doommenu.cpp:1614`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_hcde_predict_dedicated`

- Description: Likely controls hcde predict dedicated behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:83`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_maxdecals`

- Description: Likely controls maxdecals behavior for client.
- Type: `Int`
- Source default: `1024`
- Source flags: `CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:195`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_missiledecals`

- Description: Likely controls missiledecals behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_mobj.cpp:134`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_net_prediction_lead`

- Description: Likely controls net prediction lead behavior for client.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:254`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_noboldchat`

- Description: Likely controls noboldchat behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:197`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_nochatsound`

- Description: Likely controls nochatsound behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:198`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_nointros`

- Description: Likely controls nointros behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:523`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_noprediction`

- Description: Likely controls noprediction behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:80`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_oldfreelooklimit`

- Description: Likely controls oldfreelooklimit behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/r_swrenderer.cpp:49`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_predict_lerpscale`

- Description: Likely controls predict lerpscale behavior for client.
- Type: `Float`
- Source default: `0.05f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:84`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_predict_lerpthreshold`

- Description: Likely controls predict lerpthreshold behavior for client.
- Type: `Float`
- Source default: `2.00f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:85`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_predict_max`

- Description: Likely controls predict max behavior for client.
- Type: `Int`
- Source default: `24`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:119`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_predict_specials`

- Description: Likely controls predict specials behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:78`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_pufftype`

- Description: Likely controls pufftype behavior for client.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_mobj.cpp:136`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_restartondeath`

- Description: Likely controls restartondeath behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:293`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_rockettrails`

- Description: Likely controls rockettrails behavior for client.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_effect.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_rubberband_limit`

- Description: Likely controls rubberband limit behavior for client.
- Type: `Float`
- Source default: `756.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:108`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_rubberband_minmove`

- Description: Likely controls rubberband minmove behavior for client.
- Type: `Float`
- Source default: `20.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:103`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_rubberband_scale`

- Description: Likely controls rubberband scale behavior for client.
- Type: `Float`
- Source default: `0.3f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:87`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_rubberband_threshold`

- Description: Likely controls rubberband threshold behavior for client.
- Type: `Float`
- Source default: `32.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:98`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_run`

- Description: Likely controls run behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:320`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_scaleweaponfov`

- Description: Likely controls scaleweaponfov behavior for client.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:244`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_showchat`

- Description: Likely controls showchat behavior for client.
- Type: `Int`
- Source default: `CHAT_GLOBAL`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:199`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_showmultikills`

- Description: Likely controls showmultikills behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_interaction.cpp:65`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_showsecretmessage`

- Description: Likely controls showsecretmessage behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_spec.cpp:594`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_showsprees`

- Description: Likely controls showsprees behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_interaction.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_spreaddecals`

- Description: Likely controls spreaddecals behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:136`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_stannounce`

- Description: Likely controls stannounce behavior for client.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/bbannouncer.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `cl_waitforsave`

- Description: Likely controls waitforsave behavior for client.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:291`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `classic_scaling_factor`

- Description: Likely controls classic scaling factor.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/2d/v_2ddrawer.cpp:39`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `classic_scaling_pixelaspect`

- Description: Likely controls classic scaling pixelaspect.
- Type: `Float`
- Source default: `1.2f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/2d/v_2ddrawer.cpp:40`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `classicflight`

- Description: Likely controls classicflight.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:61`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `color`

- Description: Likely controls color.
- Type: `Color`
- Source default: `0x40cf00`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:49`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `colorset`

- Description: Likely controls colorset.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_anybossdeath`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_ANYBOSSDEATH`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:919`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_avoidhazard`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_AVOID_HAZARDS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:941`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_badangles`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_BADANGLES`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:930`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_boomscroll`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_BOOMSCROLL`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:913`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_checkswitchrange`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_CHECKSWITCHRANGE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:937`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_crossdropoff`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_CROSSDROPOFF`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:918`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_dehhealth`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_DEHHEALTH`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:910`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_dropoff`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_DROPOFF`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:912`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_emulatemikoportals`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_EMULATEMIKOPORTALS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:947`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_explode1`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_EXPLODE1`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:938`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_explode2`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_EXPLODE2`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:939`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_fdteleport`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_FDTELEPORT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:945`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_floormove`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_FLOORMOVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:931`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_hitscan`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_HITSCAN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:926`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_invisibility`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_INVISIBILITY`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:914`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_light`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_LIGHT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:927`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_limitpain`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_LIMITPAIN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:900`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_maskedmidtex`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_MASKEDMIDTEX`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:929`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_mbfmonstermove`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_MBFMONSTERMOVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:922`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_minotaur`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_MINOTAUR`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:920`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_missileclip`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_MISSILECLIP`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:917`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_multiexit`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_MULTIEXIT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:934`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_mushroom`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_MUSHROOM`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:921`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_noblockfriends`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_NOBLOCKFRIENDS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:924`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_nodoorlight`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_NODOORLIGHT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:907`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_nofriendlyspawn`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_NOFRIENDLYSPAWN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:949`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_nombf21`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_NOMBF21`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:943`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_nopassover`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_NO_PASSMOBJ`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:902`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_notossdrops`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_NOTOSSDROPS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:905`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_novdolllockmsg`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_NOVDOLLLOCKMSG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:946`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_pointonline`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_POINTONLINE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:933`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_polyobj`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_POLYOBJ`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:928`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_pushwindow`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_PUSHWINDOW`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:936`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_railing`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_RAILING`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:940`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_ravenscroll`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_RAVENSCROLL`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:908`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_reservedlineflag`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_RESERVEDLINEFLAG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:948`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_sectorsounds`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_SECTORSOUNDS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:916`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_shortTex`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_SHORTTEX`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:898`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_silentinstantfloors`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_SILENT_INSTANT_FLOORS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:915`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_silentpickup`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_SILENTPICKUP`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:901`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_soundcutoff`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_SOUNDCUTOFF`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:932`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_soundslots`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_MAGICSILENCE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:903`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_soundtarget`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_SOUNDTARGET`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:909`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_spritesort`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_SPRITESORT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:925`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_stairs`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_STAIRINDEX`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:899`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_stayonlift`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_STAYONLIFT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:942`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_teleport`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_TELEPORT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:935`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_trace`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_TRACE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:911`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_useblocking`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_USEBLOCKING`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:906`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_vileghosts`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_VILEGHOSTS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:923`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_voodoozombies`

- Description: Flag alias backed by compatflags2.
- Type: `Flag`
- Source default: `compatflags2`
- Source flags: `COMPATF2_VOODOO_ZOMBIES`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:944`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compat_wallrun`

- Description: Flag alias backed by compatflags.
- Type: `Flag`
- Source default: `compatflags`
- Source flags: `COMPATF_WALLRUN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:904`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compatflags`

- Description: Server setting: Raw Compat Flags
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE|CVAR_SERVERINFO | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:808`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compatflags2`

- Description: Server setting: Raw Compat Flags 2
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE|CVAR_SERVERINFO | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:816`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `compatmode`

- Description: Server setting: Compat Mode
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:825`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_4bitansi`

- Description: Likely controls con 4bitansi.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/sdl/i_system.cpp:66`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_alpha`

- Description: Likely controls con alpha.
- Type: `Float`
- Source default: `0.75f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:133`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_buffersize`

- Description: Likely controls con buffersize.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:76`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_centernotify`

- Description: Likely controls con centernotify.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_notifybuffer.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_ctrl_d`

- Description: Likely controls con ctrl d.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:150`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_debugoutput`

- Description: Likely controls con debugoutput.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_system.cpp:109`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_midtime`

- Description: Likely controls con midtime.
- Type: `Float`
- Source default: `3.f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/hudmessages.cpp:869`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_notablist`

- Description: Likely controls con notablist.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_tabcomplete.cpp:63`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_notifylines`

- Description: Likely controls con notifylines.
- Type: `Int`
- Source default: `4`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_notifybuffer.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_notifytime`

- Description: Likely controls con notifytime.
- Type: `Float`
- Source default: `3.f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_notifybuffer.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_printansi`

- Description: Likely controls con printansi.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/sdl/i_system.cpp:65`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_pulsetext`

- Description: Likely controls con pulsetext.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_notifybuffer.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_quick_home_end`

- Description: Use HOME/END keys to scroll when cursor is at start/end of line already
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:139`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_scale`

- Description: Likely controls con scale.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:128`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_scaletext`

- Description: Likely controls con scaletext.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_notifybuffer.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `con_stackident`

- Description: Likely controls con stackident.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_cmds.cpp:63`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `consoleendoom`

- Description: Likely controls consoleendoom.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/startscreen/endoom.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshair`

- Description: Likely controls crosshair.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:112`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshair_offset_x`

- Description: Likely controls crosshair offset x.
- Type: `Float`
- Source default: `0.`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:79`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshair_offset_y`

- Description: Likely controls crosshair offset y.
- Type: `Float`
- Source default: `0.`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:80`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshaircolor`

- Description: Likely controls crosshaircolor.
- Type: `Color`
- Source default: `0xff0000`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:47`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshaircolorFull`

- Description: Likely controls crosshaircolorFull.
- Type: `Color`
- Source default: `0x00ff00`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:48`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshaircolorMax`

- Description: Likely controls crosshaircolorMax.
- Type: `Color`
- Source default: `0x7f7fff`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:49`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshaircolors`

- Description: 0: basic, 1: show health, 2: show health bonus, 3: inverted
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshairforce`

- Description: Likely controls crosshairforce.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:113`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshairgrow`

- Description: grow crosshair upon pickup
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshairhascolor`

- Description: Likely controls crosshairhascolor.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_HIDDEN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshairon`

- Description: Likely controls crosshairon.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:111`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshairscale`

- Description: changes the size of the crosshair
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:63`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `crosshairshowshealth`

- Description: Likely controls crosshairshowshealth.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_HIDDEN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `deathmatch`

- Description: Likely controls deathmatch.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO|CVAR_LATCH`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:287`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debug_languages`

- Description: Likely controls debug languages.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/stringtable.cpp:39`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debuganimated`

- Description: Likely controls debuganimated.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/gamedata/textures/animations.cpp:189`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debugtrace_capacity`

- Description: Likely controls debugtrace capacity.
- Type: `Int`
- Source default: `16384`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/debugtrace.cpp:47`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debugtrace_enable`

- Description: Likely controls debugtrace enable.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/debugtrace.cpp:43`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debugtrace_filter`

- Description: Likely controls debugtrace filter.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/debugtrace.cpp:44`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debugtrace_minseverity`

- Description: Likely controls debugtrace minseverity.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/debugtrace.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debugtrace_stats`

- Description: Likely controls debugtrace stats.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/debugtrace.cpp:46`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debugtrace_stream`

- Description: Likely controls debugtrace stream.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/debugtrace.cpp:48`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debugtrace_stream_rotate_count`

- Description: Likely controls debugtrace stream rotate count.
- Type: `Int`
- Source default: `4`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/debugtrace.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `debugtrace_stream_rotate_mb`

- Description: Likely controls debugtrace stream rotate mb.
- Type: `Int`
- Source default: `10`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/debugtrace.cpp:49`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultaddonfiles`

- Description: Likely controls defaultaddonfiles.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultargs`

- Description: Likely controls defaultargs.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultiwad`

- Description: Likely controls defaultiwad.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:57`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetaddress`

- Description: Likely controls defaultnetaddress.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:67`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetaltdm`

- Description: Likely controls defaultnetaltdm.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:66`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetargs`

- Description: Likely controls defaultnetargs.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:61`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetextratic`

- Description: Likely controls defaultnetextratic.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:72`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetgamemode`

- Description: Likely controls defaultnetgamemode.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:65`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnethostport`

- Description: Likely controls defaultnethostport.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:63`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnethostteam`

- Description: Likely controls defaultnethostteam.
- Type: `Int`
- Source default: `255`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:70`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetiwad`

- Description: Likely controls defaultnetiwad.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetjoinport`

- Description: Likely controls defaultnetjoinport.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetjointeam`

- Description: Likely controls defaultnetjointeam.
- Type: `Int`
- Source default: `255`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:71`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetpage`

- Description: Likely controls defaultnetpage.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:69`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetplayers`

- Description: Likely controls defaultnetplayers.
- Type: `Int`
- Source default: `8`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetsavefile`

- Description: Likely controls defaultnetsavefile.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:73`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `defaultnetticdup`

- Description: Likely controls defaultnetticdup.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `dehload`

- Description: Likely controls dehload.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/gamedata/d_dehacked.cpp:3210`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `demo_compress`

- Description: Likely controls demo compress.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:318`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `developer`

- Description: Likely controls developer.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:143`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `dimamount`

- Description: Likely controls dimamount.
- Type: `Float`
- Source default: `0.8f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/menu/doommenu.cpp:376`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `dimcolor`

- Description: Likely controls dimcolor.
- Type: `Color`
- Source default: `0x000000`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/menu/doommenu.cpp:387`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `disableautoload`

- Description: Likely controls disableautoload.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:505`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `disableautosave`

- Description: Likely controls disableautosave.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:411`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `disablecrashlog`

- Description: Likely controls disablecrashlog.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_main.cpp:626`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `displaynametags`

- Description: Likely controls displaynametags.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:309`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `dlg_musicvolume`

- Description: Likely controls dlg musicvolume.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/p_conversation.cpp:218`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `dlg_vgafont`

- Description: Likely controls dlg vgafont.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/p_conversation.cpp:67`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `dmflags`

- Description: Server setting: Raw DM Flags
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:641`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `dmflags2`

- Description: Server setting: Raw DM Flags 2
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:717`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `dmflags3`

- Description: Server setting: Raw DM Flags 3
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:787`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `duellimit`

- Description: Legacy Skulltag compatibility value for duel limit metadata.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR_NAMED`
- Ref symbol: `duellimit_compat`
- Source: `src/d_net_invasion.cpp:113`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `dumpspawnedthings`

- Description: Likely controls dumpspawnedthings.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_mobj.cpp:7039`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `eaxedit_test`

- Description: Likely controls eaxedit test.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/s_reverbedit.cpp:57`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `enablescriptscreenshot`

- Description: Likely controls enablescriptscreenshot.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:292`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_chorus`

- Description: Likely controls fluid chorus.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:161`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_chorus_depth`

- Description: Likely controls fluid chorus depth.
- Type: `Float`
- Source default: `8.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:222`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_chorus_level`

- Description: Likely controls fluid chorus level.
- Type: `Float`
- Source default: `1.2f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:211`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_chorus_speed`

- Description: Likely controls fluid chorus speed.
- Type: `Float`
- Source default: `0.3f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:216`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_chorus_type`

- Description: Likely controls fluid chorus type.
- Type: `Int`
- Source default: `0/*FLUID_CHORUS_DEFAULT_TYPE*/`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:227`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_chorus_voices`

- Description: Likely controls fluid chorus voices.
- Type: `Int`
- Source default: `3`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:206`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_gain`

- Description: Likely controls fluid gain.
- Type: `Float`
- Source default: `0.5`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:142`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_interp`

- Description: Likely controls fluid interp.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:171`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_lib`

- Description: Likely controls fluid lib.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL | CVAR_SYSTEM_ONLY`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:132`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_patchset`

- Description: Likely controls fluid patchset.
- Type: `String`
- Source default: `GAMENAMELOWERCASE`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL | CVAR_SYSTEM_ONLY`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:137`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_reverb`

- Description: Likely controls fluid reverb.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:156`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_reverb_damping`

- Description: Likely controls fluid reverb damping.
- Type: `Float`
- Source default: `0.23f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:191`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_reverb_level`

- Description: Likely controls fluid reverb level.
- Type: `Float`
- Source default: `0.57f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:201`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_reverb_roomsize`

- Description: Likely controls fluid reverb roomsize.
- Type: `Float`
- Source default: `0.61f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:186`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_reverb_width`

- Description: Likely controls fluid reverb width.
- Type: `Float`
- Source default: `0.76f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:196`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_samplerate`

- Description: Likely controls fluid samplerate.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:176`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_threads`

- Description: Likely controls fluid threads.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:181`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fluid_voices`

- Description: Likely controls fluid voices.
- Type: `Int`
- Source default: `128`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:166`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `forcewater`

- Description: Likely controls forcewater.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/p_setup.cpp:754`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fov`

- Description: Likely controls fov.
- Type: `Float`
- Source default: `90.f`
- Source flags: `CVAR_ARCHIVE | CVAR_USERINFO | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:133`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fraglimit`

- Description: Server setting: Frag Limit
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:461`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `freelook`

- Description: Likely controls freelook.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:321`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `fviewbob`

- Description: Likely controls fviewbob.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:56`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gamma`

- Description: Likely controls gamma.
- Type: `Float`
- Source default: `GAMMA_DEFAULT`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR_NAMED`
- Ref symbol: `vid_gamma_compat`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:102`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `genblockmap`

- Description: Likely controls genblockmap.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/maploader/maploader.cpp:63`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gender`

- Description: Likely controls gender.
- Type: `String`
- Source default: `"neutral"`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:53`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gennodes`

- Description: Likely controls gennodes.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/maploader/maploader.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_aalines`

- Description: Likely controls gl aalines.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/hw_draw2d.cpp:41`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_bandedswlight`

- Description: Likely controls gl bandedswlight.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_drawinfo.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_billboard_faces_camera`

- Description: Likely controls gl billboard faces camera.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:81`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_billboard_mode`

- Description: Likely controls gl billboard mode.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:80`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_billboard_particles`

- Description: Likely controls gl billboard particles.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:83`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_bloom`

- Description: Likely controls gl bloom.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:31`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_bloom_amount`

- Description: Likely controls gl bloom amount.
- Type: `Float`
- Source default: `1.4f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:32`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_breaksec`

- Description: Likely controls gl breaksec.
- Type: `Int`
- Source default: `-1`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_flats.cpp:44`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_brightfog`

- Description: Likely controls gl brightfog.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_level.cpp:124`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_cachenodes`

- Description: Likely controls gl cachenodes.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:138`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_cachetime`

- Description: Likely controls gl cachetime.
- Type: `Float`
- Source default: `0.6f`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:139`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_control_tear`

- Description: Likely controls gl control tear.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/gl_sysfb.cpp:103`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_coronas`

- Description: Likely controls gl coronas.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_drawinfo.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_custompost`

- Description: Likely controls gl custompost.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess.cpp:901`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_customshader`

- Description: Likely controls gl customshader.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hw_material.cpp:28`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_debug`

- Description: Likely controls gl debug.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/sdl/sdlglvideo.cpp:78`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_debug_breakpoint`

- Description: Likely controls gl debug breakpoint.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/gl/gl_debug.cpp:39`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_debug_level`

- Description: Likely controls gl debug level.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/gl/gl_debug.cpp:31`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_distfog`

- Description: Likely controls gl distfog.
- Type: `Int`
- Source default: `70`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_lighting.cpp:36`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_dither_bpc`

- Description: Likely controls gl dither bpc.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/gl/gl_postprocess.cpp:46`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_enhanced_nightvision`

- Description: Likely controls gl enhanced nightvision.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_lighting.cpp:28`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_enhanced_nv_stealth`

- Description: Likely controls gl enhanced nv stealth.
- Type: `Int`
- Source default: `3`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_drawinfo.cpp:48`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_es`

- Description: Likely controls gl es.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/sdl/sdlglvideo.cpp:82`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_exposure_base`

- Description: Likely controls gl exposure base.
- Type: `Float`
- Source default: `0.35f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:39`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_exposure_min`

- Description: Likely controls gl exposure min.
- Type: `Float`
- Source default: `0.35f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:38`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_exposure_scale`

- Description: Likely controls gl exposure scale.
- Type: `Float`
- Source default: `1.3f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:37`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_exposure_speed`

- Description: Likely controls gl exposure speed.
- Type: `Float`
- Source default: `0.05f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:40`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_finishbeforeswap`

- Description: Likely controls gl finishbeforeswap.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/gl/gl_framebuffer.cpp:251`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_fogmode`

- Description: Likely controls gl fogmode.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:32`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_fuzztype`

- Description: Likely controls gl fuzztype.
- Type: `Int`
- Source default: `8`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:84`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_fxaa`

- Description: Likely controls gl fxaa.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_interpolate_model_frames`

- Description: Likely controls gl interpolate model frames.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/r_data/models.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_lens`

- Description: Likely controls gl lens.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:48`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_lens_chromatic`

- Description: Likely controls gl lens chromatic.
- Type: `Float`
- Source default: `1.12f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_lens_k`

- Description: Likely controls gl lens k.
- Type: `Float`
- Source default: `-0.12f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_lens_kcube`

- Description: Likely controls gl lens kcube.
- Type: `Float`
- Source default: `0.1f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_light_models`

- Description: Likely controls gl light models.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/hw_models.cpp:36`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_light_particles`

- Description: Likely controls gl light particles.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/hw_dynlightdata.cpp:36`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_light_shadowmap`

- Description: Likely controls gl light shadowmap.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_light_sprites`

- Description: Likely controls gl light sprites.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/hw_dynlightdata.cpp:35`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_lightadditivesurfaces`

- Description: Likely controls gl lightadditivesurfaces.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_level.cpp:132`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_lightmode`

- Description: Select lighting mode. 2 is vanilla accurate, 1 is accurate to the ZDoom software renderer and 0 is a less demanding non-shader implementation
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/g_level.cpp:153`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_lights`

- Description: Likely controls gl lights.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:164`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_maplightmode`

- Description: Likely controls gl maplightmode.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_NOINITCALL | CVAR_CHEAT`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_level.cpp:148`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_mask_sprite_threshold`

- Description: Likely controls gl mask sprite threshold.
- Type: `Float`
- Source default: `0.5f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_drawinfo.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_mask_threshold`

- Description: Likely controls gl mask threshold.
- Type: `Float`
- Source default: `0.5f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_drawinfo.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_menu_blur`

- Description: Likely controls gl menu blur.
- Type: `Float`
- Source default: `-1.0f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:98`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_mirror_envmap`

- Description: Likely controls gl mirror envmap.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_mirrors`

- Description: Likely controls gl mirrors.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:41`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_multisample`

- Description: Likely controls gl multisample.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/gl/gl_renderbuffers.cpp:38`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_multithread`

- Description: Likely controls gl multithread.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_bsp.cpp:44`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_no_skyclear`

- Description: Likely controls gl no skyclear.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_drawinfo.cpp:47`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_noskyboxes`

- Description: Likely controls gl noskyboxes.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sky.cpp:33`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_notexturefill`

- Description: Likely controls gl notexturefill.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_level.cpp:140`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_paltonemap_powtable`

- Description: Likely controls gl paltonemap powtable.
- Type: `Float`
- Source default: `2.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:88`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_paltonemap_reverselookup`

- Description: Likely controls gl paltonemap reverselookup.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:93`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_particles_style`

- Description: Likely controls gl particles style.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:79`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_pipeline_depth`

- Description: Likely controls gl pipeline depth.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_plane_reflection`

- Description: Likely controls gl plane reflection.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_portals`

- Description: Likely controls gl portals.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:40`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_precache`

- Description: Likely controls gl precache.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:197`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_render_flats`

- Description: Likely controls gl render flats.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_bsp.cpp:232`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_render_things`

- Description: Likely controls gl render things.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_bsp.cpp:230`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_render_walls`

- Description: Likely controls gl render walls.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_bsp.cpp:231`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_satformula`

- Description: Likely controls gl satformula.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:179`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_scale_viewport`

- Description: Likely controls gl scale viewport.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_framebuffer.cpp:48`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_sclipfactor`

- Description: Likely controls gl sclipfactor.
- Type: `Float`
- Source default: `1.8f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:78`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_sclipthreshold`

- Description: Likely controls gl sclipthreshold.
- Type: `Float`
- Source default: `10.0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:77`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_seamless`

- Description: Likely controls gl seamless.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:43`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_shadowmap_filter`

- Description: Likely controls gl shadowmap filter.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:200`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_shadowmap_maxlights`

- Description: Likely controls gl shadowmap maxlights.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp:93`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_shadowmap_prioritize`

- Description: Likely controls gl shadowmap prioritize.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp:65`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_shadowmap_quality`

- Description: Likely controls gl shadowmap quality.
- Type: `Int`
- Source default: `512`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp:112`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_sort_textures`

- Description: Likely controls gl sort textures.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_drawinfo.cpp:46`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_sprite_blend`

- Description: Likely controls gl sprite blend.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:73`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_spriteclip`

- Description: Likely controls gl spriteclip.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_ssao`

- Description: Likely controls gl ssao.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_ssao_bias`

- Description: Likely controls gl ssao bias.
- Type: `Float`
- Source default: `0.2f`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:76`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_ssao_blur`

- Description: Likely controls gl ssao blur.
- Type: `Float`
- Source default: `16.0f`
- Source flags: `0`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:78`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_ssao_debug`

- Description: Likely controls gl ssao debug.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:75`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_ssao_exponent`

- Description: Likely controls gl ssao exponent.
- Type: `Float`
- Source default: `1.8f`
- Source flags: `0`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:83`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_ssao_portals`

- Description: Likely controls gl ssao portals.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_ssao_radius`

- Description: Likely controls gl ssao radius.
- Type: `Float`
- Source default: `80.0f`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:77`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_ssao_strength`

- Description: Likely controls gl ssao strength.
- Type: `Float`
- Source default: `0.7f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture`

- Description: Likely controls gl texture.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_drawinfo.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_filter`

- Description: changes the texture filtering settings
- Type: `Int`
- Source default: `6`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:191`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_filter_anisotropic`

- Description: changes the OpenGL texture anisotropy setting
- Type: `Float`
- Source default: `16.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:186`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresize_fonts`

- Description: Flag alias backed by gl_texture_hqresize_targets.
- Type: `Flag`
- Source default: `gl_texture_hqresize_targets`
- Source flags: `4`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:76`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresize_maxinputsize`

- Description: Likely controls gl texture hqresize maxinputsize.
- Type: `Int`
- Source default: `512`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresize_mt_height`

- Description: Likely controls gl texture hqresize mt height.
- Type: `Int`
- Source default: `4`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:87`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresize_mt_width`

- Description: Likely controls gl texture hqresize mt width.
- Type: `Int`
- Source default: `16`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:81`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresize_multithread`

- Description: Likely controls gl texture hqresize multithread.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:79`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresize_skins`

- Description: Flag alias backed by gl_texture_hqresize_targets.
- Type: `Flag`
- Source default: `gl_texture_hqresize_targets`
- Source flags: `8`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:77`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresize_sprites`

- Description: Flag alias backed by gl_texture_hqresize_targets.
- Type: `Flag`
- Source default: `gl_texture_hqresize_targets`
- Source flags: `2`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:75`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresize_targets`

- Description: Likely controls gl texture hqresize targets.
- Type: `Int`
- Source default: `15`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresize_textures`

- Description: Flag alias backed by gl_texture_hqresize_targets.
- Type: `Flag`
- Source default: `gl_texture_hqresize_targets`
- Source flags: `1`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresizemode`

- Description: Likely controls gl texture hqresizemode.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_texture_hqresizemult`

- Description: Likely controls gl texture hqresizemult.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_tonemap`

- Description: Likely controls gl tonemap.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_usecolorblending`

- Description: Likely controls gl usecolorblending.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:72`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_weapon_purelightlevel`

- Description: Makes the lighting on weapon sprites (or models) purely match the sector's light level you're standing in
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_weapon.cpp:49`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gl_weaponlight`

- Description: Likely controls gl weaponlight.
- Type: `Int`
- Source default: `8`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_lighting.cpp:27`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gles_force_glsl_v100`

- Description: Likely controls gles force glsl v100.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/gles/gles_system.cpp:24`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gles_glsl_precision`

- Description: Likely controls gles glsl precision.
- Type: `Int`
- Source default: `2`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/gles/gles_shader.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gles_max_lights_per_surface`

- Description: Likely controls gles max lights per surface.
- Type: `Int`
- Source default: `32`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/gles/gles_system.cpp:25`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gles_use_mapped_buffer`

- Description: Likely controls gles use mapped buffer.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/gles/gles_system.cpp:23`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gme_stereodepth`

- Description: Likely controls gme stereodepth.
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:502`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gus_memsize`

- Description: Likely controls gus memsize.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:361`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `gus_patchdir`

- Description: Likely controls gus patchdir.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL | CVAR_SYSTEM_ONLY`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:351`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_compat`

- Description: haptic feedback compatibility level
- Type: `Int`
- Source default: `HAPTCOMPAT_MATCH`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:140`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_debug`

- Description: print diagnostics for haptic feedback
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:138`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_do_action`

- Description: allow haptic feedback for player doing things
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:149`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_do_damage`

- Description: allow haptic feedback for things hurting player
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:148`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_do_menus`

- Description: allow haptic feedback for menus
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:146`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_do_world`

- Description: allow haptic feedback for things acting on player
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:147`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_strength`

- Description: Translate linear haptics to audio taper
- Type: `Int`
- Source default: `10`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:119`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_strength_hf`

- Description: high frequency motor fine-control
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:103`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_strength_lf`

- Description: low frequency motor fine-control
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:98`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_strength_lt`

- Description: left trigger motor fine-control
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:108`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `haptics_strength_rt`

- Description: right trigger motor fine-control
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_haptics.cpp:113`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_hud_debug`

- Description: Likely controls hcde hud debug.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:184`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_lag_hud`

- Description: Likely controls hcde lag hud.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:196`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_shadow_autobudget`

- Description: Likely controls hcde shadow autobudget.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp:67`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_shadow_autobudget_minlights`

- Description: Likely controls hcde shadow autobudget minlights.
- Type: `Int`
- Source default: `64`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp:77`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_shadow_autobudget_step`

- Description: Likely controls hcde shadow autobudget step.
- Type: `Int`
- Source default: `32`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp:85`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_shadow_autobudget_targetms`

- Description: Likely controls hcde shadow autobudget targetms.
- Type: `Float`
- Source default: `1.20f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp:69`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_shadow_autofallback`

- Description: Likely controls hcde shadow autofallback.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_shadowmap.cpp:66`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_shadow_forcealllights`

- Description: Likely controls hcde shadow forcealllights.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/hw_entrypoint.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_shadowprofile`

- Description: applies HCDE grouped shadow settings. 0 = manual, 1 = off, 2 = performance, 3 = balanced, 4 = enhanced, 5 = cinematic, 6 = quake-style, 7 = doom3-style
- Type: `Int`
- Source default: `HCDE_SHADOWPROFILE_DOOM3`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/menu/doommenu.cpp:802`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hcde_startup_profile`

- Description: Likely controls hcde startup profile.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/scripting/thingdef.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_althud`

- Description: Likely controls hud althud.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:47`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_althud_forceinternal`

- Description: Likely controls hud althud forceinternal.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:97`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_althudscale`

- Description: Likely controls hud althudscale.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:46`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_ammo_order`

- Description: Likely controls hud ammo order.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_ammo_red`

- Description: Likely controls hud ammo red.
- Type: `Int`
- Source default: `25`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:65`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_ammo_yellow`

- Description: Likely controls hud ammo yellow.
- Type: `Int`
- Source default: `50`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:66`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_armor_green`

- Description: Likely controls hud armor green.
- Type: `Int`
- Source default: `100`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:73`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_armor_red`

- Description: Likely controls hud armor red.
- Type: `Int`
- Source default: `25`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:71`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_armor_yellow`

- Description: Likely controls hud armor yellow.
- Type: `Int`
- Source default: `50`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:72`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_aspectscale`

- Description: enables aspect ratio correction for the status bar
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:73`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_berserk_health`

- Description: Likely controls hud berserk health.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:75`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_health_green`

- Description: Likely controls hud health green.
- Type: `Int`
- Source default: `100`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:70`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_health_red`

- Description: Likely controls hud health red.
- Type: `Int`
- Source default: `25`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_health_yellow`

- Description: Likely controls hud health yellow.
- Type: `Int`
- Source default: `50`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:69`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_oldscale`

- Description: Likely controls hud oldscale.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:78`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_scale`

- Description: Likely controls hud scale.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:76`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_scalefactor`

- Description: changes the hud scale
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/statusbar/base_sbar.cpp:66`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showammo`

- Description: Likely controls hud showammo.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showangles`

- Description: Likely controls hud showangles.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:76`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showitems`

- Description: Likely controls hud showitems.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showlag`

- Description: Likely controls hud showlag.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showmonsters`

- Description: Likely controls hud showmonsters.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showscore`

- Description: Likely controls hud showscore.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showsecrets`

- Description: Likely controls hud showsecrets.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showstats`

- Description: Likely controls hud showstats.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:53`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showtime`

- Description: Likely controls hud showtime.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showtimestat`

- Description: Likely controls hud showtimestat.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_showweapons`

- Description: Likely controls hud showweapons.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:55`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_swaphealtharmor`

- Description: Likely controls hud swaphealtharmor.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:67`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_timecolor`

- Description: Likely controls hud timecolor.
- Type: `Int`
- Source default: `CR_GOLD`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:61`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hud_toggled`

- Description: Likely controls hud toggled.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:576`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hudcolor_ltim`

- Description: Likely controls hudcolor ltim.
- Type: `Int`
- Source default: `CR_ORANGE`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:80`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hudcolor_statnames`

- Description: Likely controls hudcolor statnames.
- Type: `Int`
- Source default: `CR_RED`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:84`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hudcolor_stats`

- Description: Likely controls hudcolor stats.
- Type: `Int`
- Source default: `CR_GREEN`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:85`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hudcolor_time`

- Description: Likely controls hudcolor time.
- Type: `Int`
- Source default: `CR_RED`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:79`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hudcolor_titl`

- Description: Likely controls hudcolor titl.
- Type: `Int`
- Source default: `CR_YELLOW`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:78`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hudcolor_ttim`

- Description: Likely controls hudcolor ttim.
- Type: `Int`
- Source default: `CR_GOLD`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:81`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hudcolor_xyco`

- Description: Likely controls hudcolor xyco.
- Type: `Int`
- Source default: `CR_GREEN`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:82`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hw_2dmip`

- Description: Likely controls hw 2dmip.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/hw_draw2d.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hw_force_cambbpref`

- Description: Likely controls hw force cambbpref.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:82`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `hw_npottest`

- Description: Likely controls hw npottest.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_walls.cpp:202`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `i_discordrpc`

- Description: Likely controls i discordrpc.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:513`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `i_display_new_release`

- Description: Show changelog upon update
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/d_iwad.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `i_exit_on_not_found`

- Description: Exits game if a specified file is not found
- Type: `Int`
- Source default: `REQUIRE_DEFAULT`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/utility/findfile.cpp:38`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `I_FriendlyWindowTitle`

- Description: Likely controls I FriendlyWindowTitle.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:519`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `i_is_new_release`

- Description: Likely controls i is new release.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_HIDDEN`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/d_iwad.cpp:56`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `i_loadsupportwad`

- Description: Load id24.wad
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/d_iwad.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `i_pauseinbackground`

- Description: Likely controls i pauseinbackground.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/s_sound.cpp:41`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `i_searchdistributors`

- Description: Search storefront intallations for IWADS
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/d_iwad.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `i_soundinbackground`

- Description: Likely controls i soundinbackground.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/s_sound.cpp:40`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `i_timescale`

- Description: Likely controls i timescale.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_NOINITCALL | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:417`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `idmypos`

- Description: Likely controls idmypos.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:119`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `in_mouse`

- Description: Likely controls in mouse.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_mouse.cpp:157`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `infighting`

- Description: Likely controls infighting.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/gamedata/d_dehacked.cpp:63`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `inter_classic_scaling`

- Description: Likely controls inter classic scaling.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/intermission/intermission.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `inter_subtitles`

- Description: Likely controls inter subtitles.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/cutscenes/screenjob.cpp:43`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `invertmouse`

- Description: Likely controls invertmouse.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/d_event.cpp:47`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `invertmousex`

- Description: Likely controls invertmousex.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/d_event.cpp:48`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `joy_axespolling`

- Description: Likely controls joy axespolling.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/cocoa/i_joystick.cpp:1356`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `joy_dinput`

- Description: Likely controls joy dinput.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_dijoy.cpp:285`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `joy_ps2raw`

- Description: Likely controls joy ps2raw.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_rawps2.cpp:234`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `joy_xinput`

- Description: Likely controls joy xinput.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_xinput.cpp:203`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `joykey_stop_conflict`

- Description: Detect joypad/keyboard conflicts, dropping events as needed. Useful for handheld PCs such as the SteamDeck. -1: auto-detect, 0: disabled, 1: detected, 2: forced
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/sdl/i_input.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `k_allowfullscreentoggle`

- Description: Likely controls k allowfullscreentoggle.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_input.cpp:119`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `k_mergekeys`

- Description: Likely controls k mergekeys.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_keyboard.cpp:113`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `language`

- Description: Likely controls language.
- Type: `String`
- Source default: `"auto"`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:81`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `auto`

### `language_debug_maxlen`

- Description: Likely controls language debug maxlen.
- Type: `Int`
- Source default: `64`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/stringtable.cpp:40`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `log_vgafont`

- Description: Likely controls log vgafont.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:77`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `longsavemessages`

- Description: Likely controls longsavemessages.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:290`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `lookspring`

- Description: Likely controls lookspring.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:3136`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `1`

### `lookstrafe`

- Description: Likely controls lookstrafe.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:322`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_blockcontrollers`

- Description: Likely controls m blockcontrollers.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:55`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_cleanscale`

- Description: Likely controls m cleanscale.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_forward`

- Description: Likely controls m forward.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:323`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_hidepointer`

- Description: Likely controls m hidepointer.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_mouse.cpp:155`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_pitch`

- Description: Likely controls m pitch.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:603`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_quickexit`

- Description: Likely controls m quickexit.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/messagebox.cpp:34`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_sensitivity_x`

- Description: Likely controls m sensitivity x.
- Type: `Float`
- Source default: `2.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/d_event.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_sensitivity_y`

- Description: Likely controls m sensitivity y.
- Type: `Float`
- Source default: `2.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/d_event.cpp:46`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_show_backbutton`

- Description: Likely controls m show backbutton.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_showinputgrid`

- Description: Likely controls m showinputgrid.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_side`

- Description: Likely controls m side.
- Type: `Float`
- Source default: `2.f`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:324`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_simpleoptions`

- Description: Likely controls m simpleoptions.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/menu/doommenu.cpp:96`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_simpleoptions_view`

- Description: Likely controls m simpleoptions view.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/menu/doommenu.cpp:97`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_swapbuttons`

- Description: Likely controls m swapbuttons.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/i_mouse.cpp:391`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_tooltip_alpha`

- Description: Likely controls m tooltip alpha.
- Type: `Float`
- Source default: `0.6f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:85`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_tooltip_capratio`

- Description: Likely controls m tooltip capratio.
- Type: `Float`
- Source default: `4.0/3.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_tooltip_delay`

- Description: Likely controls m tooltip delay.
- Type: `Float`
- Source default: `9.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:75`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_tooltip_lines`

- Description: Likely controls m tooltip lines.
- Type: `Int`
- Source default: `3`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:70`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_tooltip_small`

- Description: Likely controls m tooltip small.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:69`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_tooltip_speed`

- Description: Likely controls m tooltip speed.
- Type: `Float`
- Source default: `3.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:80`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_use_mouse`

- Description: Likely controls m use mouse.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `m_yaw`

- Description: Likely controls m yaw.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:604`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `map_point_coordinates`

- Description: Likely controls map point coordinates.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_hud.cpp:88`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `maxviewpitch`

- Description: Likely controls maxviewpitch.
- Type: `Float`
- Source default: `90.f`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:1258`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `menu_overscroll`

- Description: Number of lines you can scroll past the bottom of a menu
- Type: `Int`
- Source default: `8`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/optionmenu.cpp:32`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `menu_resolution_custom_height`

- Description: Likely controls menu resolution custom height.
- Type: `Int`
- Source default: `480`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/resolutionmenu.cpp:31`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `menu_resolution_custom_width`

- Description: Likely controls menu resolution custom width.
- Type: `Int`
- Source default: `640`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/resolutionmenu.cpp:30`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `midi_config`

- Description: Likely controls midi config.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL | CVAR_SYSTEM_ONLY`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:341`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `midi_dmxgus`

- Description: Likely controls midi dmxgus.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:346`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `midi_voices`

- Description: Likely controls midi voices.
- Type: `Int`
- Source default: `32`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:356`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `midskew`

- Description: Likely controls midskew.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_walls.cpp:2130`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mod_autochip`

- Description: Likely controls mod autochip.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:544`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mod_autochip_scan_threshold`

- Description: Likely controls mod autochip scan threshold.
- Type: `Int`
- Source default: `12`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:559`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mod_autochip_size_force`

- Description: Likely controls mod autochip size force.
- Type: `Int`
- Source default: `100`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:549`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mod_autochip_size_scan`

- Description: Likely controls mod autochip size scan.
- Type: `Int`
- Source default: `500`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:554`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mod_dumb_mastervolume`

- Description: Likely controls mod dumb mastervolume.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:564`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mod_interp`

- Description: Likely controls mod interp.
- Type: `Int`
- Source default: `2/*DUMB_LQ_CUBIC*/`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:539`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mod_preferred_player`

- Description: Likely controls mod preferred player.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music.cpp:87`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mod_samplerate`

- Description: Likely controls mod samplerate.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:524`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mod_volramp`

- Description: Likely controls mod volramp.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:534`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mouse_capturemode`

- Description: Likely controls mouse capturemode.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:3139`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `movebob`

- Description: Likely controls movebob.
- Type: `Float`
- Source default: `0.25f`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:55`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `msg`

- Description: Filters HUD message by importance
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVARD_NAMED`
- Ref symbol: `msglevel`
- Source: `src/common/console/c_console.cpp:182`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `msg0color`

- Description: Likely controls msg0color.
- Type: `Int`
- Source default: `CR_UNTRANSLATED`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:184`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `msg1color`

- Description: Likely controls msg1color.
- Type: `Int`
- Source default: `CR_GOLD`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:189`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `msg2color`

- Description: Likely controls msg2color.
- Type: `Int`
- Source default: `CR_GRAY`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:194`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `msg3color`

- Description: Likely controls msg3color.
- Type: `Int`
- Source default: `CR_GREEN`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:199`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `msg4color`

- Description: Likely controls msg4color.
- Type: `Int`
- Source default: `CR_GREEN`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:204`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `msgmidcolor`

- Description: Likely controls msgmidcolor.
- Type: `Int`
- Source default: `CR_UNTRANSLATED`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:209`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `msgmidcolor2`

- Description: Likely controls msgmidcolor2.
- Type: `Int`
- Source default: `CR_BROWN`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/console/c_console.cpp:214`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mus_calcgain`

- Description: Likely controls mus calcgain.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music.cpp:85`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mus_enabled`

- Description: enables/disables music
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/i_music.cpp:86`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `mus_usereplaygain`

- Description: Likely controls mus usereplaygain.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music.cpp:86`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `name`

- Description: descr
- Type: `String`
- Source default: `"Player"`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:48`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `nametagcolor`

- Description: Likely controls nametagcolor.
- Type: `Int`
- Source default: `CR_GOLD`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:317`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_adaptive_lead`

- Description: Likely controls adaptive lead behavior for network.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_movement_diag.cpp:34`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_adaptive_lead_guard`

- Description: Likely controls adaptive lead guard behavior for network.
- Type: `Int`
- Source default: `3`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_movement_diag.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_adaptive_lead_max`

- Description: Likely controls adaptive lead max behavior for network.
- Type: `Int`
- Source default: `6`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_movement_diag.cpp:46`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_adaptive_lead_min`

- Description: Likely controls adaptive lead min behavior for network.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_movement_diag.cpp:38`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_blackbox_record`

- Description: Likely controls blackbox record behavior for network.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_blackbox.cpp:35`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_blackbox_size_mb`

- Description: Likely controls blackbox size mb behavior for network.
- Type: `Int`
- Source default: `32`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_blackbox.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_chatslowmode`

- Description: Server setting: Chat Slowmode
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/ct_chat.cpp:85`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_checksum`

- Description: Likely controls checksum behavior for network.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_checksum.cpp:34`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_checksum_categories`

- Description: Likely controls checksum categories behavior for network.
- Type: `Int`
- Source default: `0x3F`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_checksum.cpp:53`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_checksum_interval`

- Description: Likely controls checksum interval behavior for network.
- Type: `Int`
- Source default: `4`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_checksum.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_cutscenecountdown`

- Description: Server setting: Ready Time
- Type: `Float`
- Source default: `30.0f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:67`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_cutscenereadypercent`

- Description: Server setting: Ready Percent
- Type: `Float`
- Source default: `0.5f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_cutscenereadytype`

- Description: Server setting: Ready Mode
- Type: `Int`
- Source default: `RT_VOTE`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:53`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_desyncdebug`

- Description: Likely controls desyncdebug behavior for network.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:214`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_disablepause`

- Description: Server setting: Pause Policy
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:46`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_echo_debug`

- Description: Likely controls echo debug behavior for network.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:222`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_event_debug`

- Description: Likely controls event debug behavior for network.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_diagnostics.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_extratic`

- Description: Likely controls extratic behavior for network.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_hcde_native_only`

- Description: Requires HCDE-native networking/capability paths for multiplayer sessions.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:306`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_limitconversations`

- Description: Likely controls limitconversations behavior for network.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_limitsaves`

- Description: Likely controls limitsaves behavior for network.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:43`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_movement_debug`

- Description: Likely controls movement debug behavior for network.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_movement_diag.cpp:23`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_password`

- Description: Likely controls password behavior for network.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_IGNORE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_net.cpp:998`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_predict_debug`

- Description: Controls HCDE prediction diagnostics: off, CSV sampling, and/or on-screen/debug trace output depending on level.
- Type: `Int`
- Source default: `3`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:207`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_predict_debug_interval`

- Description: Tic interval used by prediction CSV/debug sampling.
- Type: `Int`
- Source default: `15`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:215`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_predict_softwarn_ack_lag`

- Description: Soft warning threshold for client ack lag during prediction diagnostics.
- Type: `Int`
- Source default: `3`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:262`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_predict_softwarn_mirror_delta`

- Description: Soft warning threshold for invasion mirror drift during prediction diagnostics.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:289`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_predict_softwarn_passive_storm`

- Description: Soft warning threshold for passive update storms during prediction diagnostics.
- Type: `Int`
- Source default: `5`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:297`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_reconcile_debug`

- Description: Likely controls reconcile debug behavior for network.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:231`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_repeatableactioncooldown`

- Description: Likely controls repeatableactioncooldown behavior for network.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:44`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_rewind_depth`

- Description: Likely controls rewind depth behavior for network.
- Type: `Int`
- Source default: `10`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_rewind.cpp:44`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_rewind_enable`

- Description: Likely controls rewind enable behavior for network.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_rewind.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_rewind_interval`

- Description: Likely controls rewind interval behavior for network.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_rewind.cpp:36`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_rewind_max_mb`

- Description: Likely controls rewind max mb behavior for network.
- Type: `Int`
- Source default: `32`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_rewind.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_self_test_run_client`

- Description: Likely controls self test run client behavior for network.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:238`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `net_ticbalance`

- Description: Likely controls ticbalance behavior for network.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:41`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `neverswitchonpickup`

- Description: Likely controls neverswitchonpickup.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `nocheats`

- Description: Likely controls nocheats.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/st_stuff.cpp:297`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `nointerscrollabort`

- Description: Likely controls nointerscrollabort.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/intermission/intermission.cpp:72`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `nomonsterinterpolation`

- Description: Likely controls nomonsterinterpolation.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_enemy.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opl_core`

- Description: Likely controls opl core.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:244`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opl_fullpan`

- Description: Likely controls opl fullpan.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:249`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opl_gain`

- Description: Likely controls opl gain.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:254`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opl_numchips`

- Description: Likely controls opl numchips.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:239`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_auto_arpeggio`

- Description: Likely controls opn auto arpeggio.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:316`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_chan_alloc`

- Description: Likely controls opn chan alloc.
- Type: `Int`
- Source default: `-1 /*OPNMIDI_ChanAlloc_AUTO*/`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:311`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_chips_count`

- Description: Likely controls opn chips count.
- Type: `Int`
- Source default: `8`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:276`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_custom_bank`

- Description: Likely controls opn custom bank.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:301`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_emulator_id`

- Description: Likely controls opn emulator id.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:281`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_fullpan`

- Description: Likely controls opn fullpan.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:291`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_gain`

- Description: Likely controls opn gain.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:321`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_run_at_pcm_rate`

- Description: Likely controls opn run at pcm rate.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:286`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_use_custom_bank`

- Description: Likely controls opn use custom bank.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:296`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `opn_volume_model`

- Description: Likely controls opn volume model.
- Type: `Int`
- Source default: `0 /*OPNMIDI_VolumeModel_AUTO*/`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:306`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `os_isanyof`

- Description: Likely controls os isanyof.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `paletteflash`

- Description: Likely controls paletteflash.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:86`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `pf_hazard`

- Description: Flag alias backed by paletteflash.
- Type: `Flag`
- Source default: `paletteflash`
- Source flags: `PF_HAZARD`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:90`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `pf_hexenweaps`

- Description: Flag alias backed by paletteflash.
- Type: `Flag`
- Source default: `paletteflash`
- Source flags: `PF_HEXENWEAPONS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:87`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `pf_ice`

- Description: Flag alias backed by paletteflash.
- Type: `Flag`
- Source default: `paletteflash`
- Source flags: `PF_ICE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:89`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `pf_poison`

- Description: Flag alias backed by paletteflash.
- Type: `Flag`
- Source default: `paletteflash`
- Source flags: `PF_POISON`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:88`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `pickup_fade_scalar`

- Description: Likely controls pickup fade scalar.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/2d/v_blend.cpp:44`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `playerclass`

- Description: Likely controls playerclass.
- Type: `String`
- Source default: `"Fighter"`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `png_gamma`

- Description: Likely controls png gamma.
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/m_png.cpp:107`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `png_level`

- Description: Likely controls png level.
- Type: `Int`
- Source default: `5`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/m_png.cpp:100`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `powerup_fade_scalar`

- Description: Likely controls powerup fade scalar.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/2d/v_blend.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `queryiwad`

- Description: Likely controls queryiwad.
- Type: `Bool`
- Source default: `QUERYIWADDEFAULT`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:53`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `queryiwad_key`

- Description: Likely controls queryiwad key.
- Type: `String`
- Source default: `"shift"`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/sdl/i_system.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `quicksavenum`

- Description: Likely controls quicksavenum.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_NOSET|CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:420`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `quicksaverotation`

- Description: Likely controls quicksaverotation.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:421`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `quicksaverotationcount`

- Description: Likely controls quicksaverotationcount.
- Type: `Int`
- Source default: `4`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:423`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_3dfloors`

- Description: Likely controls r 3dfloors.
- Type: `Int`
- Source default: `1`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/scene/r_3dfloors.cpp:35`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_actorspriteshadow`

- Description: render actor sprite shadows. 0 = off, 1 = default, 2 = always on
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:103`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_actorspriteshadowalpha`

- Description: maximum sprite shadow opacity, only effective with hardware renderers (0.0 = fully transparent, 1.0 = opaque)
- Type: `Float`
- Source default: `0.7`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:117`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_actorspriteshadowdist`

- Description: how far sprite shadows should be rendered
- Type: `Float`
- Source default: `2200.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:110`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_actorspriteshadowfadeheight`

- Description: distance over which sprite shadows should fade, only effective with hardware renderers (0 = infinite)
- Type: `Float`
- Source default: `0.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:124`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_actorspriteshadowstyle`

- Description: actor sprite shadow style. 0 = classic, 1 = quake-style, 2 = doom3-style
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:131`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_blendmethod`

- Description: Likely controls r blendmethod.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/drawers/r_draw_pal.cpp:40`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_clearbuffer`

- Description: Likely controls r clearbuffer.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:92`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_deathcamera`

- Description: Likely controls r deathcamera.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:91`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_debug_disable_vis_filter`

- Description: Likely controls r debug disable vis filter.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:509`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_debug_draw`

- Description: Likely controls r debug draw.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/r_thread.cpp:34`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_debug_nolimitanamorphoses`

- Description: Likely controls r debug nolimitanamorphoses.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:75`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_dithertransparency`

- Description: Use dithered-transparency shading for actor-occluding level geometry
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_CHEAT`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:96`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_drawfuzz`

- Description: Likely controls r drawfuzz.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/renderstyle.cpp:31`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_drawmirrors`

- Description: Likely controls r drawmirrors.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/line/r_line.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_drawplayersprites`

- Description: Likely controls r drawplayersprites.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:94`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_drawtrans`

- Description: Likely controls r drawtrans.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/renderstyle.cpp:30`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_drawvoxels`

- Description: Likely controls r drawvoxels.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:93`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_dynlights`

- Description: Likely controls r dynlights.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/drawers/r_draw.cpp:49`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_extralight`

- Description: Likely controls r extralight.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:349`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_fakecontrast`

- Description: Likely controls r fakecontrast.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_sectors.cpp:53`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_fogboundary`

- Description: Likely controls r fogboundary.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/line/r_line.cpp:61`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_fullbrightignoresectorcolor`

- Description: Likely controls r fullbrightignoresectorcolor.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/scene/r_translucent_pass.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_fuzzscale`

- Description: Likely controls r fuzzscale.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/drawers/r_draw.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_highlight_portals`

- Description: Likely controls r highlight portals.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/scene/r_portal.cpp:70`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_line_distance_cull`

- Description: Likely controls r line distance cull.
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/scene/r_opaque_pass.cpp:95`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_linearsky`

- Description: Likely controls r linearsky.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/plane/r_skyplane.cpp:55`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_lod_bias`

- Description: Likely controls r lod bias.
- Type: `Float`
- Source default: `-1.5`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/drawers/r_draw_rgba.cpp:70`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_magfilter`

- Description: Likely controls r magfilter.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/drawers/r_draw_rgba.cpp:61`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_maxparticles`

- Description: Likely controls r maxparticles.
- Type: `Int`
- Source default: `4000`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:218`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_minfilter`

- Description: Likely controls r minfilter.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/drawers/r_draw_rgba.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_mipmap`

- Description: Likely controls r mipmap.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/drawers/r_draw_rgba.cpp:67`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_model_distance_cull`

- Description: Likely controls r model distance cull.
- Type: `Float`
- Source default: `1024.f`
- Source flags: `0/*CVAR_ARCHIVE | CVAR_GLOBALCONFIG*/`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/scene/r_opaque_pass.cpp:107`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_models`

- Description: Likely controls r models.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/scene/r_scene.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_multithreaded`

- Description: Likely controls r multithreaded.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/r_thread.cpp:33`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_noaccel`

- Description: Likely controls r noaccel.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/things/r_playersprite.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_particles`

- Description: Likely controls r particles.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_effect.cpp:55`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_portal_recursions`

- Description: Likely controls r portal recursions.
- Type: `Int`
- Source default: `4`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_quakeintensity`

- Description: Likely controls r quakeintensity.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:97`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_radarclipper`

- Description: Use the horizontal clipper from camera->tracer's perspective
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_CHEAT`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:95`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_rail_smartspiral`

- Description: Likely controls r rail smartspiral.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_effect.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_rail_spiralsparsity`

- Description: Likely controls r rail spiralsparsity.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_effect.cpp:53`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_rail_trailsparsity`

- Description: Likely controls r rail trailsparsity.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_effect.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_scene_multithreaded`

- Description: Likely controls r scene multithreaded.
- Type: `Int`
- Source default: `1`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/scene/r_scene.cpp:41`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_skipmats`

- Description: Likely controls r skipmats.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:56`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_skyboxes`

- Description: Likely controls r skyboxes.
- Type: `Bool`
- Source default: `true`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/scene/r_portal.cpp:72`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_skymode`

- Description: Likely controls r skymode.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_sky.cpp:44`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_sprite_distance_cull`

- Description: Likely controls r sprite distance cull.
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/scene/r_opaque_pass.cpp:83`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_spriteadjust`

- Description: Likely controls r spriteadjust.
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/gametexture.cpp:425`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_spriteclipanamorphicminbias`

- Description: Likely controls r spriteclipanamorphicminbias.
- Type: `Float`
- Source default: `0.6`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_sprites.cpp:76`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_ticstability`

- Description: Likely controls r ticstability.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_diag_commands.cpp:1520`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_vanillatrans`

- Description: Likely controls r vanillatrans.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/r_data/r_vanillatrans.cpp:32`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_viewsize`

- Description: Likely controls r viewsize.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_NOSET`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/viewport/r_viewport.cpp:46`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `r_visibility`

- Description: Likely controls r visibility.
- Type: `Float`
- Source default: `8.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:331`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `reverbedit_id1`

- Description: Likely controls reverbedit id1.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_NOSET`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/s_reverbedit.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `reverbedit_id2`

- Description: Likely controls reverbedit id2.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_NOSET`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/s_reverbedit.cpp:53`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `reverbedit_name`

- Description: Likely controls reverbedit name.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_NOSET`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/s_reverbedit.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `reverbsavename`

- Description: Likely controls reverbsavename.
- Type: `String`
- Source default: `""`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/s_reverbedit.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `save_dir`

- Description: Likely controls save dir.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SYSTEM_ONLY`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/savegamemanager.cpp:41`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `save_formatted`

- Description: Likely controls save formatted.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:286`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `save_sort_order`

- Description: Likely controls save sort order.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/savegamemanager.cpp:43`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `saveargs`

- Description: Likely controls saveargs.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `saved_drawplayersprite`

- Description: Likely controls saved drawplayersprite.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:574`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `saved_screenblocks`

- Description: Likely controls saved screenblocks.
- Type: `Int`
- Source default: `10`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:573`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `saved_showmessages`

- Description: Likely controls saved showmessages.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:575`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `saveloadconfirmation`

- Description: Likely controls saveloadconfirmation.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:412`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `savenetargs`

- Description: Likely controls savenetargs.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:56`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `savenetfile`

- Description: Likely controls savenetfile.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:55`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `savestatistics`

- Description: Likely controls savestatistics.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/gamedata/statistics.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_cooperative_enable`

- Description: Likely controls sb cooperative enable.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_cooperative_headingcolor`

- Description: Likely controls sb cooperative headingcolor.
- Type: `Int`
- Source default: `CR_RED`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:69`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_cooperative_otherplayercolor`

- Description: Likely controls sb cooperative otherplayercolor.
- Type: `Int`
- Source default: `CR_GREY`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:71`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_cooperative_yourplayercolor`

- Description: Likely controls sb cooperative yourplayercolor.
- Type: `Int`
- Source default: `CR_GREEN`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:70`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_deathmatch_enable`

- Description: Likely controls sb deathmatch enable.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:73`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_deathmatch_headingcolor`

- Description: Likely controls sb deathmatch headingcolor.
- Type: `Int`
- Source default: `CR_RED`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_deathmatch_otherplayercolor`

- Description: Likely controls sb deathmatch otherplayercolor.
- Type: `Int`
- Source default: `CR_GREY`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:76`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_deathmatch_yourplayercolor`

- Description: Likely controls sb deathmatch yourplayercolor.
- Type: `Int`
- Source default: `CR_GREEN`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:75`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_teamdeathmatch_enable`

- Description: Likely controls sb teamdeathmatch enable.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:78`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sb_teamdeathmatch_headingcolor`

- Description: Likely controls sb teamdeathmatch headingcolor.
- Type: `Int`
- Source default: `CR_RED`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/hu_scores.cpp:79`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `screenblocks`

- Description: Likely controls screenblocks.
- Type: `Int`
- Source default: `12`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:424`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `screenshot_dir`

- Description: Likely controls screenshot dir.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/m_misc.cpp:70`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `screenshot_quiet`

- Description: Likely controls screenshot quiet.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/m_misc.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `screenshot_type`

- Description: Likely controls screenshot type.
- Type: `String`
- Source default: `"png"`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/m_misc.cpp:69`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `script_debug`

- Description: Likely controls script debug.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/fragglescript/t_parse.cpp:29`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sentstats_hwr_done`

- Description: Likely controls sentstats hwr done.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_anonstats.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `setslotstrict`

- Description: Likely controls setslotstrict.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/gamedata/a_weapons.cpp:524`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `show_messages`

- Description: enable/disable showing messages
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_cmds.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `show_obituaries`

- Description: Likely controls show obituaries.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_cmds.cpp:64`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `showendoom`

- Description: Likely controls showendoom.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/startscreen/endoom.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `showsecretsector`

- Description: Likely controls showsecretsector.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_spec.cpp:593`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `silence_menu_hover`

- Description: Silences cursor movement when implicitly selecting with mouse
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/optionmenu.cpp:30`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `silence_menu_scroll`

- Description: Silences cursor movement when using mouse wheel
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/optionmenu.cpp:29`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `skill`

- Description: sets the skill for the next newly started game
- Type: `Int`
- Source default: `2`
- Source flags: `CVAR_SERVERINFO|CVAR_LATCH`
- Macro: `CVARD_NAMED`
- Ref symbol: `gameskill`
- Source: `src/g_game.cpp:285`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `skin`

- Description: Likely controls skin.
- Type: `String`
- Source default: `"base"`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `skyoffset`

- Description: Likely controls skyoffset.
- Type: `Float`
- Source default: `0.f`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_skydome.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_aldevice`

- Description: Likely controls snd aldevice.
- Type: `String`
- Source default: `"Default"`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/oalsound.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_aldriver`

- Description: See alsoftrc.sample for details
- Type: `String`
- Source default: `DEFAULT_DRIVER`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/oalsound.cpp:56`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_alresampler`

- Description: Likely controls snd alresampler.
- Type: `String`
- Source default: `"Default"`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/oalsound.cpp:66`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_backend`

- Description: Likely controls snd backend.
- Type: `String`
- Source default: `DEF_BACKEND`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/i_sound.cpp:63`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_buffersize`

- Description: Likely controls snd buffersize.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/i_sound.cpp:47`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_channels`

- Description: Likely controls snd channels.
- Type: `Int`
- Source default: `128`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/oalsound.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_drawoutput`

- Description: Likely controls snd drawoutput.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:484`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_efx`

- Description: Likely controls snd efx.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/oalsound.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_enabled`

- Description: enables/disables sound effects
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/s_sound.cpp:39`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_environmentprofile`

- Description: Global reverb profile. 0=classic, 1=doomsday room, 2=doomsday cave, 3=doomsday cinematic.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/oalsound.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_footstepvolume`

- Description: Likely controls snd footstepvolume.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:75`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_hrtf`

- Description: Likely controls snd hrtf.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/i_sound.cpp:48`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_mastervolume`

- Description: Likely controls snd mastervolume.
- Type: `Float`
- Source default: `0.5f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/i_sound.cpp:83`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_menuvolume`

- Description: Likely controls snd menuvolume.
- Type: `Float`
- Source default: `0.6f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/menu/menu.cpp:57`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_mididevice`

- Description: Likely controls snd mididevice.
- Type: `Int`
- Source default: `DEF_MIDIDEV`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_midi_base.cpp:91`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_midiprecache`

- Description: Likely controls snd midiprecache.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:491`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_musicmode`

- Description: Likely controls snd musicmode.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/oalsound.cpp:67`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_musicvolume`

- Description: controls music volume
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/i_music.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_pitched`

- Description: Likely controls snd pitched.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/s_sound.cpp:43`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_samplerate`

- Description: Likely controls snd samplerate.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/i_sound.cpp:39`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_sfxvolume`

- Description: Likely controls snd sfxvolume.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/i_sound.cpp:102`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_streambuffersize`

- Description: Likely controls snd streambuffersize.
- Type: `Int`
- Source default: `64`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:513`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_superstereowidth`

- Description: Likely controls snd superstereowidth.
- Type: `Float`
- Source default: `0.45f`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/oalsound.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `snd_waterreverb`

- Description: Likely controls snd waterreverb.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/sound/oalsound.cpp:57`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `splashfactor`

- Description: Likely controls splashfactor.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_map.cpp:6078`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `st_oldouch`

- Description: Likely controls st oldouch.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/sbar_mugshot.cpp:316`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `st_scale`

- Description: Likely controls st scale.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_statusbar/shared_sbar.cpp:94`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `statfile`

- Description: Likely controls statfile.
- Type: `String`
- Source default: `"zdoomstat.txt"`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/gamedata/statistics.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `stillbob`

- Description: Likely controls stillbob.
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:57`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `storesavepic`

- Description: Likely controls storesavepic.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:289`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `strictdecorate`

- Description: Likely controls strictdecorate.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/scripting/backend/vmbuilder.cpp:34`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_aircontrol`

- Description: Server setting: Air Control
- Type: `Float`
- Source default: `0.00390625f`
- Source flags: `CVAR_SERVERINFO|CVAR_NOSAVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:1404`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0.00390625`

### `sv_allowallscripts`

- Description: Likely controls allowallscripts behavior for server.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_acs.cpp:10958`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_allowcrouch`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_YES_CROUCH`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:692`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_allowfreelook`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_YES_FREELOOK`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:688`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_allowjump`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_YES_JUMP`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:686`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_alwaysspawnmulti`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_ALWAYS_SPAWN_MULTI`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:777`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_alwaystally`

- Description: Server setting: Tally Policy
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_level.cpp:173`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_ammofactor`

- Description: Likely controls ammofactor behavior for server.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_SERVERINFO|CVAR_CHEAT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_interaction.cpp:71`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_autocompat`

- Description: Likely controls autocompat behavior for server.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_map.cpp:70`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_barrelrespawn`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_BARRELS_RESPAWN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:758`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_chasecam`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_CHASECAM`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:769`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_cheats`

- Description: Likely controls cheats behavior for server.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_cmds.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_coophalveammo`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_COOP_HALVE_AMMO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:699`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_cooploseammo`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_COOP_LOSE_AMMO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:698`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_cooplosearmor`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_COOP_LOSE_ARMOR`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:696`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_cooploseinventory`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_COOP_LOSE_INVENTORY`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:693`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_cooplosekeys`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_COOP_LOSE_KEYS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:694`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_cooplosepowerups`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_COOP_LOSE_POWERUPS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:697`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_cooploseweapons`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_COOP_LOSE_WEAPONS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:695`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_coopsharekeys`

- Description: Flag alias backed by dmflags3.
- Type: `Flag`
- Source default: `dmflags3`
- Source flags: `DF3_COOP_SHARE_KEYS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:792`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_corpsefilter`

- Description: Selects which corpse queues sv_corpsequeuesize trims: 0 off, 1 monsters, 2 players, 3 both.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE|CVAR_SERVERINFO|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:176`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_corpsequeuesize`

- Description: Maximum queued corpses retained by corpse cleanup; used with sv_corpsefilter.
- Type: `Int`
- Source default: `64`
- Source flags: `CVAR_ARCHIVE|CVAR_SERVERINFO|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:184`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_crouch`

- Description: Likely controls crouch behavior for server.
- Type: `Mask`
- Source default: `dmflags`
- Source flags: `DF_NO_CROUCH|DF_YES_CROUCH`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:703`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_damagefactorfriendly`

- Description: Likely controls damagefactorfriendly behavior for server.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_SERVERINFO|CVAR_CHEAT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_interaction.cpp:69`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_damagefactormobj`

- Description: Likely controls damagefactormobj behavior for server.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_SERVERINFO|CVAR_CHEAT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_interaction.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_damagefactorplayer`

- Description: Likely controls damagefactorplayer behavior for server.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_SERVERINFO|CVAR_CHEAT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_interaction.cpp:70`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_dedicated_autostart`

- Description: Likely controls dedicated autostart behavior for server.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_net.cpp:88`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_degeneration`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_YES_DEGENERATION`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:756`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_disableautohealth`

- Description: Likely controls disableautohealth behavior for server.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_interaction.cpp:825`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_disallowspying`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_DISALLOW_SPYING`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:768`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_disallowsuicide`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NOSUICIDE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:770`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_dontcheckammo`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_DONTCHECKAMMO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:772`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_doubleammo`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_YES_DOUBLEAMMO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:755`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_dropstyle`

- Description: Server setting: Drop Style
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_enemy.cpp:75`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_falldamage`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_FORCE_FALLINGHX`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:673`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_fallingdamage`

- Description: Likely controls fallingdamage behavior for server.
- Type: `Mask`
- Source default: `dmflags`
- Source flags: `DF_FORCE_FALLINGHX|DF_FORCE_FALLINGZD`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:705`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_fastmonsters`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_FAST_MONSTERS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:684`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_fastweapons`

- Description: Server setting: Fast Weapons
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_pspr.cpp:90`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_forcerespawn`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_FORCE_RESPAWN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:677`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_freelook`

- Description: Likely controls freelook behavior for server.
- Type: `Mask`
- Source default: `dmflags`
- Source flags: `DF_NO_FREELOOK|DF_YES_FREELOOK`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:706`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_gametype`

- Description: Server setting: Game Type
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:334`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_gravity`

- Description: Server setting: Gravity
- Type: `Float`
- Source default: `800.f`
- Source flags: `CVAR_SERVERINFO|CVAR_NOSAVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_mobj.cpp:122`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `800`

### `sv_hostname`

- Description: Server setting: Hostname
- Type: `String`
- Source default: `GAMENAME " server"`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_net.cpp:84`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `Untitled Odamex Server`

### `sv_infiniteammo`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_INFINITE_AMMO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:680`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_infiniteinventory`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_INFINITE_INVENTORY`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:764`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_instantreaction`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_INSTANT_REACTION`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:700`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionbasebudget`

- Description: Base monster budget each wave starts with.
- Type: `Int`
- Source default: `24`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:123`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionbossbonus`

- Description: Extra budget added during boss waves.
- Type: `Int`
- Source default: `20`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:158`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionbosswaveevery`

- Description: Boss wave cadence (e.g. 5 = every 5th wave, 0 = never).
- Type: `Int`
- Source default: `5`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:153`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionbudgetstep`

- Description: Budget increase applied per wave number.
- Type: `Int`
- Source default: `8`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:128`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasioncleanuptime`

- Description: Seconds allowed for cleanup phase after spawning ends.
- Type: `Float`
- Source default: `4.0f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:81`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasioncountdowntime`

- Description: Seconds before wave 1 starts ("Prepare for invasion" countdown).
- Type: `Float`
- Source default: `30.0f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasiondebug`

- Description: Server setting: Invasion Debug
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net.cpp:188`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionexitonvictory`

- Description: Server setting: Invasion Exit Victory
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:96`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionintermissiontime`

- Description: Seconds between completed waves before the next wave starts.
- Type: `Float`
- Source default: `6.0f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:86`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionmaxactive`

- Description: Optional cap for active invasion monsters. 0 disables the cap; positive values are clamped by the engine.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:148`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionperplayer`

- Description: Additional budget per extra active player.
- Type: `Int`
- Source default: `6`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:133`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionresulttime`

- Description: Seconds to keep the final victory/failure state visible.
- Type: `Float`
- Source default: `8.0f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:91`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionsimlod`

- Description: Enables server-side simulation LOD for invasion monsters so distant actors think less often under heavy load.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:169`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionsimloddormantinterval`

- Description: Think interval in tics for dormant distant invasion simulation.
- Type: `Int`
- Source default: `TICRATE * 3`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:190`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionsimlodfullrange`

- Description: Distance within which invasion monsters keep full-rate simulation.
- Type: `Float`
- Source default: `2048.0f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:172`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionsimlodreducedinterval`

- Description: Think interval in tics for reduced-rate invasion simulation.
- Type: `Int`
- Source default: `5`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:184`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionsimlodreducedrange`

- Description: Distance within which invasion monsters use reduced-rate simulation before becoming dormant.
- Type: `Float`
- Source default: `4096.0f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:178`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionspawnburst`

- Description: Maximum monsters spawned per spawn tick burst.
- Type: `Int`
- Source default: `3`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:143`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionspawninterval`

- Description: Seconds between spawn ticks while wave spawning is active.
- Type: `Float`
- Source default: `0.35f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:138`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionspawntime`

- Description: Wave spawn window length in seconds before cleanup phase.
- Type: `Float`
- Source default: `8.0f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:76`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionspotfallback`

- Description: Fallback to generic spawning when tagged invasion spots cannot be used.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:166`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionspotusemaptags`

- Description: Restrict native invasion spots by map thing TID/tag. Keep disabled for Skulltag/Zandronum map compatibility; the spot arguments already control wave timing.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:163`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_invasionwaves`

- Description: Maximum number of invasion waves in a run.
- Type: `Int`
- Source default: `8`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:99`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_itemrespawn`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_ITEMS_RESPAWN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:683`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_jump`

- Description: Likely controls jump behavior for server.
- Type: `Mask`
- Source default: `dmflags`
- Source flags: `DF_NO_JUMP|DF_YES_JUMP`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:704`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_keepfrags`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_YES_KEEPFRAGS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:759`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_killallmonsters`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_KILL_MONSTERS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:765`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_killbossmonst`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_KILLBOSSMONST`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:773`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_localitems`

- Description: Flag alias backed by dmflags3.
- Type: `Flag`
- Source default: `dmflags3`
- Source flags: `DF3_LOCAL_ITEMS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:793`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_losefrag`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_YES_LOSEFRAG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:761`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_maxplayers`

- Description: Server setting: Max Players
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_net.cpp:89`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `4`

### `sv_monsterrespawn`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_MONSTERS_RESPAWN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:682`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_motd`

- Description: Server setting: MOTD
- Type: `String`
- Source default: `"Welcome to " GAMENAME`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_net.cpp:85`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `Welcome to Odamex`

### `sv_natport`

- Description: Server setting: NAT Port
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/sv_master.cpp:54`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_noarmor`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_ARMOR`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:678`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_noautoaim`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NOAUTOAIM`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:771`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_noautomap`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NO_AUTOMAP`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:766`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_noautomapallies`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NO_AUTOMAP_ALLIES`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:767`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nobfgaim`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NO_FREEAIMBFG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:757`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nocoopitems`

- Description: Flag alias backed by dmflags3.
- Type: `Flag`
- Source default: `dmflags3`
- Source flags: `DF3_NO_COOP_ONLY_ITEMS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:795`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nocoopthings`

- Description: Flag alias backed by dmflags3.
- Type: `Flag`
- Source default: `dmflags3`
- Source flags: `DF3_NO_COOP_ONLY_THINGS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:796`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nocountendmonst`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NOCOUNTENDMONST`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:774`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nocrouch`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_CROUCH`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:691`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_noexit`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_EXIT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:679`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_noextraammo`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NO_EXTRA_AMMO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:779`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nofov`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_FOV`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:689`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nofreelook`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_FREELOOK`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:687`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nohealth`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_HEALTH`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:670`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_noitems`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_ITEMS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:671`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nojump`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_JUMP`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:685`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nolocaldrops`

- Description: Flag alias backed by dmflags3.
- Type: `Flag`
- Source default: `dmflags3`
- Source flags: `DF3_NO_LOCAL_DROPS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:794`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nomonsters`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_MONSTERS`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:681`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_noplayerclip`

- Description: Flag alias backed by dmflags3.
- Type: `Flag`
- Source default: `dmflags3`
- Source flags: `DF3_NO_PLAYER_CLIP`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:791`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_norespawn`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NO_RESPAWN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:760`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_noteamswitch`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NO_TEAM_SWITCH`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:754`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_nothingspawn`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NO_COOP_THING_SPAWN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:776`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_novertspread`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_NOVERTSPREAD`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:778`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_noweaponspawn`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_NO_COOP_WEAPON_SPAWN`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:690`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_oldfalldamage`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_FORCE_FALLINGZD`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:674`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_pistolstart`

- Description: Flag alias backed by dmflags3.
- Type: `Flag`
- Source default: `dmflags3`
- Source flags: `DF3_PISTOL_START`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:798`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_rememberlastweapon`

- Description: Flag alias backed by dmflags3.
- Type: `Flag`
- Source default: `dmflags3`
- Source flags: `DF3_REMEMBER_LAST_WEAP`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:797`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_respawnprotect`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_YES_RESPAWN_INVUL`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:762`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_respawnsuper`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_RESPAWN_SUPER`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:775`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_samelevel`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_SAME_LEVEL`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:675`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_samespawnspot`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_SAME_SPAWN_SPOT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:763`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_singleplayerrespawn`

- Description: Likely controls singleplayerrespawn behavior for server.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO | CVAR_CHEAT`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_user.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_smartaim`

- Description: Server setting: Smart Aim
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_map.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_spawnfarthest`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_SPAWN_FARTHEST`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:676`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_unlimited_pickup`

- Description: Likely controls unlimited pickup behavior for server.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/console/c_cmds.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_upnp`

- Description: Likely controls upnp behavior for server.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/sv_master.cpp:55`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_usemapsettingswavelimit`

- Description: If enabled, map-defined invasion wavelimit metadata overrides sv_invasionwaves when present.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:120`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `sv_usemasters`

- Description: Likely controls usemasters behavior for server.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/sv_master.cpp:774`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_weapondrop`

- Description: Flag alias backed by dmflags2.
- Type: `Flag`
- Source default: `dmflags2`
- Source flags: `DF2_YES_WEAPONDROP`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:753`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `0`

### `sv_weaponstay`

- Description: Flag alias backed by dmflags.
- Type: `Flag`
- Source default: `dmflags`
- Source flags: `DF_WEAPONS_STAY`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:672`
- Present in runtime snapshot: Yes
- Runtime snapshot value: `1`

### `team`

- Description: Likely controls team.
- Type: `Int`
- Source default: `TEAM_NONE`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `teamdamage`

- Description: Server setting: Team Damage
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_SERVERINFO | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:236`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `teamplay`

- Description: Likely controls teamplay.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:333`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `telezoom`

- Description: Likely controls telezoom.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/playsim/p_teleport.cpp:34`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `tf`

- Description: Likely controls tf.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/r_utility.cpp:480`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `ticker`

- Description: Likely controls ticker.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:192`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `tilt`

- Description: Likely controls tilt.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/plane/r_visibleplane.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timelimit`

- Description: Server setting: Time Limit
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:479`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_channel_pressure`

- Description: Likely controls timidity channel pressure.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:402`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_chorus`

- Description: Likely controls timidity chorus.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:392`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_config`

- Description: Likely controls timidity config.
- Type: `String`
- Source default: `GAMENAMELOWERCASE`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL | CVAR_SYSTEM_ONLY`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:458`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_drum_effect`

- Description: Likely controls timidity drum effect.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:427`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_drum_power`

- Description: Likely controls timidity drum power.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:437`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_key_adjust`

- Description: Likely controls timidity key adjust.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:442`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_lpf_def`

- Description: Likely controls timidity lpf def.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:407`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_min_sustain_time`

- Description: Likely controls timidity min sustain time.
- Type: `Float`
- Source default: `5000.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:452`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_modulation_envelope`

- Description: Likely controls timidity modulation envelope.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:417`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_modulation_wheel`

- Description: Likely controls timidity modulation wheel.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:372`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_overlap_voice_allow`

- Description: Likely controls timidity overlap voice allow.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:422`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_pan_delay`

- Description: Likely controls timidity pan delay.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:432`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_portamento`

- Description: Likely controls timidity portamento.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:377`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_reverb`

- Description: Likely controls timidity reverb.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:382`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_reverb_level`

- Description: Likely controls timidity reverb level.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:387`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_surround_chorus`

- Description: Likely controls timidity surround chorus.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:397`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_temper_control`

- Description: Likely controls timidity temper control.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:412`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `timidity_tempo_adjust`

- Description: Likely controls timidity tempo adjust.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:447`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `topskew`

- Description: Likely controls topskew.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/hwrenderer/scene/hw_walls.cpp:2129`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `transsouls`

- Description: Likely controls transsouls.
- Type: `Float`
- Source default: `0.75f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:506`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `turbo`

- Description: Likely controls turbo.
- Type: `Float`
- Source default: `100.f`
- Source flags: `CVAR_NOINITCALL | CVAR_CHEAT`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_game.cpp:383`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `turnspeedsprintfast`

- Description: Likely controls turnspeedsprintfast.
- Type: `Int`
- Source default: `1280`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:149`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `turnspeedsprintslow`

- Description: Likely controls turnspeedsprintslow.
- Type: `Int`
- Source default: `320`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:157`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `turnspeedwalkfast`

- Description: Likely controls turnspeedwalkfast.
- Type: `Int`
- Source default: `640`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:145`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `turnspeedwalkslow`

- Description: Likely controls turnspeedwalkslow.
- Type: `Int`
- Source default: `320`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:153`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `ui_color_mix`

- Description: Likely controls ui color mix.
- Type: `Float`
- Source default: `.35`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:75`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `ui_colors`

- Description: Likely controls ui colors.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `ui_generic`

- Description: Likely controls ui generic.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/fonts/v_text.cpp:313`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `ui_screenborder_classic_scaling`

- Description: Likely controls ui screenborder classic scaling.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/2d/v_draw.cpp:36`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `ui_theme`

- Description: launcher theme. 0: auto, 1: dark, 2: light
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/widgets/widgetresourcedata.cpp:30`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `uiscale`

- Description: Likely controls uiscale.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:132`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `underwater_fade_scalar`

- Description: Likely controls underwater fade scalar.
- Type: `Float`
- Source default: `1.0f`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/2d/v_blend.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `use_joystick`

- Description: enables input from the joystick if it is present
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/m_joy.cpp:80`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `use_mouse`

- Description: Likely controls use mouse.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/sdl/i_input.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `var_friction`

- Description: Likely controls var friction.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:143`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `var_pushers`

- Description: Likely controls var pushers.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/g_cvars.cpp:137`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vertspread`

- Description: Likely controls vertspread.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:62`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_activeinbackground`

- Description: Likely controls vid activeinbackground.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:951`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_adapter`

- Description: Likely controls vid adapter.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/sdl/sdlglvideo.cpp:224`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_allowtrueultrawide`

- Description: Likely controls vid allowtrueultrawide.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE|CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/2d/v_draw.cpp:43`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_aspect`

- Description: Likely controls vid aspect.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:425`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_blackpoint`

- Description: adjusts what the engine outputs as black
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:152`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_contrast`

- Description: adjusts contrast component of gamma ramp
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:133`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_cropaspect`

- Description: Likely controls vid cropaspect.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/r_videoscale.cpp:187`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_cursor`

- Description: Likely controls vid cursor.
- Type: `String`
- Source default: `"None"`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:485`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_defheight`

- Description: Likely controls vid defheight.
- Type: `Int`
- Source default: `480`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:191`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_defwidth`

- Description: Likely controls vid defwidth.
- Type: `Int`
- Source default: `640`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:190`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_dontdowait`

- Description: Likely controls vid dontdowait.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:38`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_fixgamma`

- Description: adjusts gamma component of gamma ramp
- Type: `Float`
- Source default: `0.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:118`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_fps`

- Description: Likely controls vid fps.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/engine/i_interface.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_fsdwmhack`

- Description: Likely controls vid fsdwmhack.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/base_sysfb.cpp:56`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_fsdwmhackalpha`

- Description: Likely controls vid fsdwmhackalpha.
- Type: `Int`
- Source default: `255`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/base_sysfb.cpp:60`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_fullscreen`

- Description: Likely controls vid fullscreen.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:456`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_gamma`

- Description: (internal) target output gamma
- Type: `Float`
- Source default: `GAMMA_DEFAULT`
- Source flags: `CVAR_NOINITCALL`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:83`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_hdr`

- Description: Likely controls vid hdr.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:461`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_i_blackpoint`

- Description: Likely controls vid i blackpoint.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_VIRTUAL | CVAR_NOINITCALL | CVAR_SYSTEM_ONLY`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:149`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_i_whitepoint`

- Description: Likely controls vid i whitepoint.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_VIRTUAL | CVAR_NOINITCALL | CVAR_SYSTEM_ONLY`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:150`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_lowerinbackground`

- Description: Likely controls vid lowerinbackground.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_net_invasion.cpp:39`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_maxfps`

- Description: Likely controls vid maxfps.
- Type: `Int`
- Source default: `500`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:73`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_nopalsubstitutions`

- Description: Likely controls vid nopalsubstitutions.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/rendering/swrenderer/textures/r_swtexture.cpp:540`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_preferbackend`

- Description: Likely controls vid preferbackend.
- Type: `Int`
- Source default: `BACKEND_DEFAULT`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:87`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_renderer`

- Description: Likely controls vid renderer.
- Type: `Int`
- Source default: `1`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:145`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_rendermode`

- Description: Likely controls vid rendermode.
- Type: `Int`
- Source default: `4`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:439`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_saturation`

- Description: adjusts saturation component of gamma ramp
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:139`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_scale_customheight`

- Description: Likely controls vid scale customheight.
- Type: `Int`
- Source default: `VID_MIN_HEIGHT`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/r_videoscale.cpp:48`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_scale_custompixelaspect`

- Description: Likely controls vid scale custompixelaspect.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/r_videoscale.cpp:55`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_scale_customwidth`

- Description: Likely controls vid scale customwidth.
- Type: `Int`
- Source default: `VID_MIN_WIDTH`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/r_videoscale.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_scale_linear`

- Description: Likely controls vid scale linear.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/r_videoscale.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_scalefactor`

- Description: Likely controls vid scalefactor.
- Type: `Float`
- Source default: `1.0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/r_videoscale.cpp:171`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_scalemode`

- Description: Likely controls vid scalemode.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/r_videoscale.cpp:180`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_sdl_render_driver`

- Description: Likely controls vid sdl render driver.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/posix/sdl/sdlglvideo.cpp:87`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_shadersupport`

- Description: Likely controls vid shadersupport.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_SYSTEM_ONLY`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:85`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_showpalette`

- Description: Likely controls vid showpalette.
- Type: `Int`
- Source default: `0`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:510`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_vsync`

- Description: Likely controls vid vsync.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:194`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vid_whitepoint`

- Description: adjusts what the engine outputs as white
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVARD`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_cvars.cpp:164`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vk_debug`

- Description: Likely controls vk debug.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/vulkan/system/vk_renderdevice.cpp:69`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vk_debug_callstack`

- Description: Likely controls vk debug callstack.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/vulkan/system/vk_renderdevice.cpp:74`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vk_device`

- Description: Likely controls vk device.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/vulkan/system/vk_renderdevice.cpp:76`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vk_exclusivefullscreen`

- Description: Likely controls vk exclusivefullscreen.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/vulkan/textures/vk_framebuffer.cpp:32`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vk_hdr`

- Description: Likely controls vk hdr.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/vulkan/textures/vk_framebuffer.cpp:31`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vk_raytrace`

- Description: Likely controls vk raytrace.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/vulkan/system/vk_renderdevice.cpp:63`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vk_submit_size`

- Description: Likely controls vk submit size.
- Type: `Int`
- Source default: `1000`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/vulkan/renderer/vk_renderstate.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vm_debug`

- Description: Likely controls vm debug.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:3151`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vm_debug_port`

- Description: Likely controls vm debug port.
- Type: `Int`
- Source default: `19021`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:3167`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vm_jit`

- Description: Likely controls vm jit.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/scripting/vm/vmframe.cpp:41`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vm_jit_aot`

- Description: Likely controls vm jit aot.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/scripting/vm/vmframe.cpp:49`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vm_warnthinkercreation`

- Description: Likely controls vm warnthinkercreation.
- Type: `Bool`
- Source default: `false`
- Source flags: `0`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/scripting/backend/codegen_doom.cpp:991`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vr_enable_quadbuffered`

- Description: Likely controls vr enable quadbuffered.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/platform/win32/win32glvideo.cpp:68`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vr_hunits_per_meter`

- Description: Likely controls vr hunits per meter.
- Type: `Float`
- Source default: `41.0f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_vrmodes.cpp:45`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vr_ipd`

- Description: Likely controls vr ipd.
- Type: `Float`
- Source default: `0.062f`
- Source flags: `CVAR_ARCHIVE|CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_vrmodes.cpp:39`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vr_mode`

- Description: Likely controls vr mode.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_GLOBALCONFIG|CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_vrmodes.cpp:33`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vr_screendist`

- Description: Likely controls vr screendist.
- Type: `Float`
- Source default: `0.80f`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_vrmodes.cpp:42`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `vr_swap_eyes`

- Description: Likely controls vr swap eyes.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/hwrenderer/data/hw_vrmodes.cpp:36`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `warningstoerrors`

- Description: Likely controls warningstoerrors.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_GLOBALCONFIG | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/scripting/backend/vmbuilder.cpp:35`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wavelimit`

- Description: Legacy Skulltag compatibility override for invasion waves. 0 disables the override; 1..255 forces that wave count.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO | CVAR_NOSAVE`
- Macro: `CUSTOM_CVAR_NAMED`
- Ref symbol: `wavelimit_compat`
- Source: `src/d_net_invasion.cpp:106`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wbobfire`

- Description: Likely controls wbobfire.
- Type: `Float`
- Source default: `0.f`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:59`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wbobspeed`

- Description: Likely controls wbobspeed.
- Type: `Float`
- Source default: `1.f`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_netinfo.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wi_autoadvance`

- Description: Likely controls wi autoadvance.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_SERVERINFO`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/wi_stuff.cpp:57`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wi_cleantextscale`

- Description: Likely controls wi cleantextscale.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/wi_stuff.cpp:58`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wi_noautostartmap`

- Description: Likely controls wi noautostartmap.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_USERINFO | CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/wi_stuff.cpp:56`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wi_percents`

- Description: Likely controls wi percents.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/wi_stuff.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wi_showtotaltime`

- Description: Likely controls wi showtotaltime.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/wi_stuff.cpp:55`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wildmidi_config`

- Description: Likely controls wildmidi config.
- Type: `String`
- Source default: `""`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL | CVAR_SYSTEM_ONLY`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:469`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wildmidi_enhanced_resampling`

- Description: Likely controls wildmidi enhanced resampling.
- Type: `Bool`
- Source default: `true`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:479`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wildmidi_reverb`

- Description: Likely controls wildmidi reverb.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/audio/music/music_config.cpp:474`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `win_h`

- Description: Likely controls win h.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:53`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `win_maximized`

- Description: Likely controls win maximized.
- Type: `Bool`
- Source default: `false`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:54`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `win_w`

- Description: Likely controls win w.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:52`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `win_x`

- Description: Likely controls win x.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:50`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `win_y`

- Description: Likely controls win y.
- Type: `Int`
- Source default: `-1`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/rendering/v_video.cpp:51`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `wipetype`

- Description: Likely controls wipetype.
- Type: `Int`
- Source default: `1`
- Source flags: `CVAR_ARCHIVE`
- Macro: `CUSTOM_CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/d_main.cpp:480`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

### `xbrz_colorformat`

- Description: Likely controls xbrz colorformat.
- Type: `Int`
- Source default: `0`
- Source flags: `CVAR_ARCHIVE | CVAR_GLOBALCONFIG`
- Macro: `CVAR`
- Ref symbol: `same as cvar name`
- Source: `src/common/textures/hires/hqresize.cpp:93`
- Present in runtime snapshot: No
- Runtime snapshot value: `n/a`

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
- Current value: `odamex`
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
- Current value: `odamex-base`
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
- Current value: `odamex-base,unlagged,client-prediction,dc-stats,script-profiles`
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

- Description: Flag alias backed by dmflags.
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

- Description: Flag alias backed by dmflags2.
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

- Description: Flag alias backed by dmflags.
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

- Description: Flag alias backed by dmflags.
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
- Current value: `Untitled Odamex Server`
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

- Description: Flag alias backed by dmflags.
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
- Current value: `Welcome to Odamex`
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

- Description: Flag alias backed by dmflags.
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

- Description: Flag alias backed by dmflags2.
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

- Description: Flag alias backed by dmflags2.
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

- Description: Flag alias backed by dmflags.
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

