# HCDE Code Audit — Bugs, Races, Stale Code, and Comments

**Date:** 2026-05-28  
**Scope:** `src/` (engine and HCDE-specific code; not vendored `libraries/`)  
**Related docs:** `docs/NETCODE_REVIEW.md`, `docs/HCDE_WEAKPOINTS.md`, `docs/LEGACY_NETCODE_REMAINDER_AUDIT.md`, `docs/HCDE_NETCODE_OVERHAUL.md`

This document is a working audit checklist. Items are **findings to verify and fix**, not guaranteed bugs in every build configuration. Severity reflects likely impact if the described condition occurs.

---

## How to use this document

| Severity | Meaning |
| --- | --- |
| **Critical** | Memory safety, data loss, or authority/cheat-class correctness risk |
| **High** | Desync, stuck sessions, or maintainability traps that routinely cause regressions |
| **Medium** | Dead/stale paths, misleading APIs, or latent bugs under mods/net stress |
| **Low** | Cleanup, comment hygiene, deprecated CVARs, debug-only hooks |

When fixing an item, update its row (or remove it) and add a one-line note in the commit message.

**Search helpers** (from `LEGACY_NETCODE_REMAINDER_AUDIT.md`):

```text
NCMD_  DEM_  usercmd_t  Net_DoCommand  Net_TrySkipCommand
HCDEAbortLiveGameplaySend  cl_noprediction  UNSAFE_CCMD
```

---

## Executive summary

| Category | Approx. count | Highest-risk area |
| --- | ---: | --- |
| Netcode layering / duplicate parsers | 8 | `d_net.cpp`, `d_net_snapshot_part2.cpp` |
| Threading / global state | 5 | `hw_bsp.cpp` worker queue, `i_net.cpp` single-thread assumption |
| Dead / unreachable / `#if 0` | 7 | `i_net.cpp` legacy setup log, `d_netinfo.cpp`, `p_mobj.cpp` serialize |
| Stale legacy / external coupling | 5 | Doom Connector map override, `-joindedicated`, deprecated CVARs |
| Misleading comments | 6 | Native-send “fallback” docs, `load` error text, `I_NetClientConnected` |
| Open TODO/FIXME in `src/` | ~158 markers / ~98 lines | Playsim, net, render, VM, debugger |

---

## 1. Netcode — bugs, conflicts, and stale layering

### 1.1 Dual command parsers (maintainability = bug factory)

| Sev | Location | Finding |
| --- | --- | --- |
| **High** | `d_net.cpp` — `Net_DoCommand()` (~7181+), `Net_TrySkipCommand()` (~7801+) | Two hand-maintained parsers for the same `DEM_*` stream. Comment at ~7802: *“making setting up net commands a nightmare.”* Any new `DEM_` opcode must update **both** or skip/execute sizes diverge → desync or parse stalls. |
| **High** | `d_net.cpp` — `Net_SkipCommand()` (~8202+) | If `Net_TrySkipCommand` fails, advances stream by **1 byte** “to stay monotonic” — can misalign the stream and hide malformed packets instead of failing closed. |
| **High** | `d_net_snapshot_part2.cpp` — `HCDEAppendNativeCommandEventsToLegacyBuffer()` (~299+) | Native `HCIN` payloads are decoded then **repacked into legacy `NCMD`/`DEM` layout** for `Net_DoCommand`. Three layers (native → legacy buffer → executor) despite HCDE-native policy. |
| **Medium** | `d_net_snapshot_part2.cpp` — `HCDEApplyNativeGameplayHeader()` (~336+) | Comment: mirrors inline logic in legacy `NetUpdate` receive loop — duplicated header handling. |
| **Medium** | `d_net.cpp` — `TryRunTics()` (~6378+) | Dual tic gating: authority wall-clock `availableTics` vs client lockstep `(lowestSequence - gametic/TicDup) + 1` plus dedicated bonuses. Intentional transition; easy to break. |
| **Medium** | `d_net.cpp` — `CheckConsistencies()` (~5206+) | Legacy hash loop may be dead when `Net_UsesServerAuthoritativeConsistency()` is always true for HCDE netgames — verify before deleting. |
| **Medium** | `d_net.cpp` — `Net_AuthorityLevelStartStatusAfterRelease()` (~4530+) | Always returns `LST_READY`; `(void)I_IsLocalHCDEServiceAuthority()` — possible stub hiding missing level-start gating. |
| **Low** | `d_net.cpp` — `SendHeartbeat()` (~5165+) | `TODO`: heartbeat path could detect loss for retransmit — unfinished reliability work. |

### 1.2 Native send path — comments vs behavior

| Sev | Location | Finding |
| --- | --- | --- |
| **High** | `d_net_snapshot_part2.cpp` — `HSendLiveGameplayPacket()` (~1279+) | **Not a gameplay fallback:** aborts HCDE gameplay peers via `HCDEAbortLiveGameplaySend`. Older comments on native wrappers still said “fall back to `HSendLiveGameplayPacket`” — corrected in source (see comment pass). |
| **High** | `d_net_snapshot_part2.cpp` — `HSendNativeClientInputPacket()` / `HSendNativeServerSnapshotPacket()` | Return `false` only when peer is not a native target; return `true` after native attempt (success or abort) so caller must **not** legacy-encode. Misreading return values causes double-send or missing send. |

### 1.3 Transport and session (`i_net.cpp`)

| Sev | Location | Finding |
| --- | --- | --- |
| **Medium** | `i_net.cpp` — `GetPacket()`, globals `Connected[]`, `TransmitBuffer` | No locks; safe only if **all** net I/O runs on the main loop thread. Breaks if networking is pumped elsewhere. See `NETCODE_REVIEW.md` §1.1 (race not reproduced today). |
| **Medium** | `i_net.cpp` — `DriveRuntimeSetupStateForClient()` | Reliable pregame queue saturation can stall `CSTAT_CONNECTING` → `CSTAT_READY`. Enqueue failures are checked; backoff/telemetry still thin. |
| **Medium** | `i_net.cpp` — `AddClientConnection()` else branch (~2263+) | Logs “legacy setup” when `!connectInfo.Present`, but `TryProcessSetupConnectPacket()` **rejects** non-HCDE peers (~2132+). Else branch is defensive/unreachable in current HCDE-only admission policy. |
| **Low** | `i_net.cpp` — `FARG(joindedicated)` | Legacy alias for `-dedicatedjoin`; keep until external launchers updated. |

**Fixed (per `NETCODE_REVIEW.md`):** `GetPacket()` decompression path — bounds checks for `msgSize < 5` and copy length (review doc §1.2).

---

## 2. Threading, races, and shared globals

| Sev | Location | Finding |
| --- | --- | --- |
| **High** | `hw_bsp.cpp` — `RenderJobQueue`, `WorkerThread()` (~82–145) | SPSC queue with atomics but **no explicit memory ordering**; worker busy-spins. Producer writes job then `writeindex`. |
| **High** | `hw_bsp.cpp` (~1053+) | Main thread `P_CheckSight()` while worker may touch BSP; comment cites shared global `validcount` — cross-thread visibility hazard. |
| **High** | `ZScriptDebugger.cpp` (~755+) | `TODO`: REPL eval not safe on debugger thread; dangerous path guarded by `#if 0` only. |
| **Medium** | `d_net.cpp` — `NetBuffer`, `ClientStates[]` | Global net state mutated on send/receive without locks (same single-thread model as `i_net.cpp`). |
| **Low** | `hw_drawinfo.cpp` — `FDrawInfoList` | Mutex used correctly — contrast with render queue pattern. |

---

## 3. Dead, unreachable, and disabled code

| Sev | Location | Finding |
| --- | --- | --- |
| **Medium** | `d_netinfo.cpp` — `D_UserInfoChanged()` / `SetServerVar()` | `#if 0` blocks (autoaim clamp, teamplay migration) in userinfo path. |
| **Medium** | `p_mobj.cpp` — `DActorModelData` serialize (~1872+) | `//TODO, unreachable` on `precalcIQM` read; write stores `"nullptr"` — **silent save/load loss** for that variant. |
| **Low** | `b_think.cpp` (~109+) | `#if 0` pitch adjustment — permanently disabled bot behavior. |
| **Low** | `p_map.cpp` (~422+) | Debug `CCMD(ffcf)` — comment says remove when complete. |
| **Low** | `hw_clipper.cpp` (~187) | `node->end = end` “never triggers … Remove?” |

---

## 4. Stale, conflicting, and external coupling

| Sev | Location | Finding |
| --- | --- | --- |
| **Medium** | `hcde_mod_compat.cpp` — `HCDE_ModCompat_ResolveStartupMapOverride()` (~468+) | Documents **Doom Connector** default maps (`MAP01`/`E1M1`) — external launcher coupling in core. |
| **Medium** | `p_user.cpp` — `cl_noprediction` (~79+) | Marked **deprecated** but still read in `P_PredictClient()` (~1813) — conflicts with dedicated prediction policy. |
| **Low** | `releasepage.cpp` (~116) | Comment: “Doom Connector-style” release viewing. |
| **Medium** | `i_net.cpp` (~145+, ~1539) | “old room/session window”; menu dispatch TODO — pregame UI still tied to legacy start screen. |

Do **not** delete `DEM_*` / `usercmd_t` / `NCMD_*` setup without reading `LEGACY_NETCODE_REMAINDER_AUDIT.md` — many are shared with demos and handshakes.

---

## 5. Playsim and gameplay correctness risks

| Sev | Location | Finding |
| --- | --- | --- |
| **High** | `p_user.cpp` — `FActorBackup::PostBackup` / `PostRollback` (~360+, ~403+) | TODO: rollback clears `MF_PICKUP`/`MF2_PUSHWALL` — **determinism** risk for polyobjects / net rewind. |
| **High** | `p_sectors.cpp` (~113+) | TODO: sector damage heuristic for special 214 **does not verify tag** — false positives. |
| **Medium** | `p_acs.h` (~173+) | TODO: pointer/count validation on ACS buffer ops — VM boundary. |
| **Medium** | `g_game.cpp` — `G_Ticker()` (~1401+) | TODO: player reborn/enter should use **queues** — sync reborn vs net state. |
| **Medium** | `events.h` — `DEventHandler` (~375+) | TODO: inheritance change “horribly break anything?” |
| **Medium** | `p_setup.cpp` (~429+) | Player spawn / deathmatch setup — comment historically said “horribly hacky”; still fragile (see §6). |

---

## 6. Comment audit — corrected in tree vs still needed

### Fixed in this pass (2026-05-28)

| File | Was | Now |
| --- | --- | --- |
| `c_cmds.cpp` `CCMD(load)` | “saving to an absolute path…” | “loading from an absolute path…” |
| `d_net_snapshot_part2.cpp` | Native wrappers implied legacy fallback via `HSendLiveGameplayPacket` | Documents abort / no legacy gameplay fallback |
| `i_net.cpp` `I_NetClientConnected` | “add them to the player list” | Clarifies launcher UI update via `NetStartWindow::NetConnect` |
| `p_setup.cpp` `P_SetupLevel` | Empty “This is motivated as follows:” | Explains `maptype = MAPTYPE_UNKNOWN` reset |
| `i_net.cpp` `AddClientConnection` | Misleading “legacy setup” log | Notes HCDE-only admission makes `!Present` path unreachable |

### Still misleading or too thin (fix when touching file)

| Sev | Location | Issue |
| --- | --- | --- |
| **Medium** | `d_net.cpp` `Net_DoCommand` | 600+ line switch; header does not list invariants or DEM groups |
| **Low** | `i_net.cpp` `I_NetLoop` | “main thread locked up” — omit dedicated vs listen-server `NetStartWindow::NetLoop` split |
| **Low** | `c_cmds.cpp` `Cmd_God` block comment | Classic `argv(0) god` header vs modern cheat path |
| **Low** | `p_setup.cpp` ~476 | `[ZZ]` “god-knows-what stage” — document ordering vs `PlayerEntered` |

---

## 7. Unsafe / debug-only surfaces

| Sev | Location | Finding |
| --- | --- | --- |
| **Medium** | `c_cmds.cpp` | `UNSAFE_CCMD(load/save/playdemo/timedemo/...)` — direct execution; document policy in wiki/contributor doc. |
| **Medium** | `d_main.cpp` | `UNSAFE_CCMD(debug_restart)`; `LightDefaults` leak note if not cleared. |
| **Low** | Broad `assert()` in engine | Release builds may abort on bad mod data instead of recovering (`zstring`, `tarray`, `udmf`, …). |

---

## 8. Recommended fix order

1. **Net command single parser** — generate skip sizes from one table or shared visitor; add fuzz/round-trip tests for `Net_TrySkipCommand` vs `Net_DoCommand`.
2. **Native path documentation and tests** — assert no legacy gameplay encode after `HSendNative*Packet` returns true; integration test for abort/replay paths.
3. **`GetPacket` / session hardening** — keep bounds checks; add metrics for reject reasons (already partially via `DebugTrace`).
4. **Render worker / BSP** — formalize memory barriers or run sight checks on worker; document `validcount` contract.
5. **Playsim rewind** — address `FActorBackup` flag clearing and sector 214 tag check.
6. **Dead code pass** — remove `#if 0` blocks or ticket them; fix `p_mobj` model serialize TODO.
7. **Comment sweep** — when editing a file, require header comment on non-trivial `static` functions in `d_net*` and `i_net.cpp`.

---

## 9. Open TODO/FIXME inventory (representative)

Full grep: `rg 'TODO|FIXME|HACK' src --glob '*.{cpp,h}'`

| Area | Files (sample) | Theme |
| --- | --- | --- |
| Net | `d_net.cpp`, `d_main.cpp` | Heartbeat retransmit, GL/netgame window timing |
| Playsim | `p_user.cpp`, `p_mobj.cpp`, `p_map.cpp`, `p_sectors.cpp` | Rewind, models, portals, damage |
| Render | `hw_*.cpp` | Portals, flats, hacks, draw lists |
| VM / debugger | `vm.h`, `ZScriptDebugger.cpp`, `dap/*` | Coercion, thread-unsafe REPL |
| Data | `g_mapinfo.cpp`, `d_dehacked.cpp` | Format edge cases |

Treat each TODO as **either** a tracked issue **or** delete if obsolete.

---

## 10. What this audit is not

- Not a replacement for playtesting, `tests/netcode_step12`, or MBF21 stage harnesses.
- Not a license or asset audit (`docs/licenses/`, `wadsrc_*`).
- Not covering third-party trees under `libraries/` except where HCDE wraps them.

---

## Changelog

| Date | Change |
| --- | --- |
| 2026-05-28 | Initial consolidated audit; comment fixes in `c_cmds.cpp`, `d_net_snapshot_part2.cpp`, `i_net.cpp`, `p_setup.cpp`. |
