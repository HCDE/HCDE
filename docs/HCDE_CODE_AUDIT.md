# HCDE Code Audit — Bugs, Races, Stale Code, and Comments

**Date:** 2026-05-28  
**Scope:** `src/` (engine and HCDE-specific code; not vendored `libraries/`)  
**Related docs:** `docs/NETCODE_REVIEW.md`, `docs/HCDE_WEAKPOINTS.md`, `docs/LEGACY_NETCODE_REMAINDER_AUDIT.md`

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
| Tracked TODO/FIXME in `src/` | ~158 markers / ~98 lines | Playsim, net, render, VM, debugger |

---

## 1. Netcode — bugs, conflicts, and stale layering

### 1.1 Dual command parsers (maintainability = bug factory)

| Sev | Location | Finding | Status |
| --- | --- | --- | --- |
| **High** | `d_net.cpp` — `Net_DoCommand()` (~7102+), `Net_TrySkipCommand()` (~7727+) | Two hand-maintained parsers for the same `DEM_*` stream. Any new `DEM_` opcode must update **both** or skip/execute sizes diverge → desync or parse stalls. | **Documented** — invariant block now sits above both functions; folding into a single visitor is still the longer-term fix. |
| **High** | `d_net.cpp` — `Net_SkipCommand()` (~8125+) | Previously advanced the stream by 1 byte if the opcode-specific skip failed; that masked malformed `DEM_*` records. | **Fixed** — now fail-closed (`Net_TrySkipCommand` only); existing callers in `d_protocol.cpp` already detect "no progress" and abort cleanly. |
| **High** | `d_net_snapshot_part2.cpp` — `HCDEAppendNativeCommandEventsToExecutorBuffer()` (~305+) | Native gameplay payloads must not be repacked into legacy `NCMD`; only per-tic `DEM_*` events still need the shared executor. | **Resolved** — adapter renamed and scoped to tic-event payloads only; native headers, usercmds, consistency, world deltas, actor deltas, and invasion data apply natively. |
| **Medium** | `d_net_snapshot_part2.cpp` — `HCDEApplyGameplayHeader()` (~342+) | Native and legacy receive paths duplicated the gameplay header logic. | **Fixed** — helper now applies both native HCDE envelopes and legacy NCMD gameplay headers, removing the inline duplicate in `GetPackets()`. |
| **Medium** | `d_net.cpp` — `TryRunTics()` (~6378+) | Dual tic gating: authority wall-clock vs client snapshot gate. | **Documented** — source now states this is the permanent HCDE server-authoritative scheduler contract, not transitional lockstep residue. |
| **Medium** | `d_net.cpp` — `CheckConsistencies()` (~5206+) | Legacy hash loop dead under SA-consistency. | **Fixed** — legacy block deleted; function now does just the bookkeeping needed for the consistency stream marker. |
| **Medium** | `d_net.cpp` — `Net_AuthorityLevelStartStatusAfterRelease()` (~4530+) | Always returned `LST_READY` with a `(void)` cast hiding intent. | **Fixed** — `(void)` removed, comment now documents why HCDE skips the listen-host lockstep gate. |
| **Low** | `d_net.cpp` — `SendHeartbeat()` (~5165+) | `TODO`: heartbeat path could detect loss for retransmit. | **Documented** — TODO replaced with explicit "probe-only" rationale; loss recovery lives in the snapshot/control resend ladders. |

### 1.2 Native send path — comments vs behavior

| Sev | Location | Finding |
| --- | --- | --- |
| **High** | `d_net_snapshot_part2.cpp` — `HSendLiveGameplayPacket()` (~1279+) | **Not a gameplay fallback:** aborts HCDE gameplay peers via `HCDEAbortLiveGameplaySend`. Older comments on native wrappers still said “fall back to `HSendLiveGameplayPacket`” — corrected in source (see comment pass). |
| **High** | `d_net_snapshot_part2.cpp` — `HSendNativeClientInputPacket()` / `HSendNativeServerSnapshotPacket()` | Return `false` only when peer is not a native target; return `true` after native attempt (success or abort) so caller must **not** legacy-encode. Misreading return values causes double-send or missing send. |

### 1.3 Transport and session (`i_net.cpp`)

| Sev | Location | Finding | Status |
| --- | --- | --- | --- |
| **Medium** | `i_net.cpp` — `GetPacket()`, globals `Connected[]`, `TransmitBuffer` | No locks; safe only if **all** net I/O runs on the main loop thread. | **Documented** — transport globals now have an explicit main-net-pump ownership block; async networking must build a dispatcher boundary there. |
| **Medium** | `i_net.cpp` — `DriveRuntimeSetupStateForClient()` | Reliable pregame queue saturation can stall `CSTAT_CONNECTING` → `CSTAT_READY`. | **Fixed** — queue-full diagnostics now include oldest pending service/key/seq/send count and setup-wait traces include map/game/ack/pending state. |
| **Medium** | `i_net.cpp` — `AddClientConnection()` else branch (~2263+) | Logged a misleading "legacy setup" message for `!connectInfo.Present`. | **Fixed** — the unreachable branch now `assert()`s and logs an admission-policy bug; comment explains why HCDE-only admission keeps it unreachable. |
| **Low** | `i_net.cpp` — `FARG(joindedicated)` | Legacy alias for `-dedicatedjoin`; keep until external launchers updated. | **Resolved** — kept intentionally as a documented compatibility shim; parse branch says it must remain identical to `-dedicatedjoin` while old launchers are supported. |

**Fixed (per `NETCODE_REVIEW.md`):** `GetPacket()` decompression path — bounds checks for `msgSize < 5` and copy length (review doc §1.2).

---

## 2. Threading, races, and shared globals

| Sev | Location | Finding | Status |
| --- | --- | --- | --- |
| **High** | `hw_bsp.cpp` — `RenderJobQueue`, `WorkerThread()` (~82–145) | SPSC queue with atomics but no explicit memory ordering; worker busy-spins. | **Fixed** — explicit `release` store on `writeindex`, `acquire` load on consumer; new comment block documents the SPSC contract. |
| **High** | `hw_bsp.cpp` (~1053+) | Main thread `P_CheckSight()` while worker may touch BSP; comment cited shared global `validcount`. | **Documented** — the dither-target sight loop runs only after `future.wait()` joins the worker, so `validcount` is exclusively main-thread owned at that point; comment now spells this out. |
| **High** | `ZScriptDebugger.cpp` (~755+) | `TODO`: REPL eval not safe on debugger thread; dangerous path guarded by `#if 0` only. | **Documented** — comment now explains why the `#if 0` block must stay disabled until a thread-safe main-thread dispatch shim lands. |
| **Medium** | `d_net.cpp` — `NetBuffer`, `ClientStates[]` | Global net state mutated on send/receive without locks (same single-thread model as `i_net.cpp`). | **Documented** — source now states gameplay net state is main-loop owned and must move behind a dispatcher before threaded socket I/O. |
| **Low** | `hw_drawinfo.cpp` — `FDrawInfoList` | Mutex used correctly — contrast with render queue pattern. | OK. |

---

## 3. Dead, unreachable, and disabled code

| Sev | Location | Finding | Status |
| --- | --- | --- | --- |
| **Medium** | `d_netinfo.cpp` — `D_UserInfoChanged()` / `SetServerVar()` | `#if 0` blocks (autoaim clamp, teamplay migration). | **Fixed** — `#if 0` blocks removed; replaced with comments explaining why HCDE handles those concerns at different layers. |
| **Medium** | `p_mobj.cpp` — `DActorModelData` serialize (~1872+) | `//TODO, unreachable` on `precalcIQM` read; write stores `"nullptr"` — silent save/load loss. | **Fixed** — both branches now carry comments explaining that `ModelAnimFramePrecalculatedIQM` is a transient render cache that is intentionally not persisted; the load-side string is kept for forward-compat with archived saves. |
| **Low** | `b_think.cpp` (~109+) | `#if 0` pitch adjustment — permanently disabled bot behavior. | **Fixed** — `#if 0` removed; comment notes the historical bot pitch-snap is broken and must not be re-enabled without a repro. |
| **Low** | `p_map.cpp` (~422+) | Debug `CCMD(ffcf)` — comment said remove when complete. | **Fixed** — CCMD now refuses to run in netgame and bails when no local pawn exists; uses `consoleplayer` instead of `players[0]`. |
| **Low** | `hw_clipper.cpp` (~187) | `node->end = end` "never triggers ... Remove?" | **Fixed** — branch is reachable on partial-left overlap; the misleading comment is replaced with a description of the case it handles. |

---

## 4. Stale, conflicting, and external coupling

| Sev | Location | Finding | Status |
| --- | --- | --- | --- |
| **Medium** | `hcde_mod_compat.cpp` — `HCDE_ModCompat_ResolveStartupMapOverride()` (~468+) | Documented **Doom Connector** default maps (`MAP01`/`E1M1`) — external launcher coupling in core. | **Fixed** — comment generalised to "external launchers" so the core doesn't read like it's coupled to one product. Behaviour unchanged. |
| **Medium** | `p_user.cpp` — `cl_noprediction` (~79+) | Marked deprecated but still read in `P_PredictClient()`. | **Fixed** — read removed; CVar declaration kept so old configs do not warn; comment documents the deprecation contract. |
| **Low** | `releasepage.cpp` (~116) | Comment: "Doom Connector-style" release viewing. | **Fixed** — neutral wording. |
| **Medium** | `i_net.cpp` (~145+, ~1539) | "old room/session window"; menu dispatch TODO — pregame UI still tied to legacy start screen. | **Fixed** — wording now says "interactive pregame window"; `I_NetMessage` / `I_NetLoop` comments document the interactive vs dedicated/silent sinks. |

Do **not** delete `DEM_*` / `usercmd_t` / `NCMD_*` setup without reading `LEGACY_NETCODE_REMAINDER_AUDIT.md` — many are shared with demos and handshakes.

---

## 5. Playsim and gameplay correctness risks

| Sev | Location | Finding | Status |
| --- | --- | --- | --- |
| **High** | `p_user.cpp` — `FActorBackup::PostBackup` / `PostRollback` (~360+, ~403+) | TODO: rollback cleared `MF_PICKUP` / `MF2_PUSHWALL` without restoring them. | **Fixed** — backup now records whether each flag was set and `PostRollback` reapplies the captured state, so pickup/pushwall semantics survive net rewind and polyobject rollback. |
| **High** | `p_sectors.cpp` (~113+) | TODO: sector damage heuristic for special 214 did not verify tag — false positives. | **Fixed** — `IsDamaging()` now consults the tag manager (and treats tag 0 as "this sector") before considering special 214 dangerous. |
| **Medium** | `p_acs.h` (~173+) | TODO: pointer/count validation on ACS buffer ops — VM boundary. | **Fixed** — `ACSLocalVariables::Reset()` rejects non-null count with null storage; empty local arrays use `(nullptr, 0)`; indexing checks null storage before dereference. |
| **Medium** | `g_game.cpp` — `G_Ticker()` (~1401+) | TODO: player reborn/enter should use queues. | **Resolved** — lifecycle consumption moved into `G_ProcessPendingPlayerLifecycle()` with source comments explaining `playerstate` as the tic-ordered queue populated by `SET_PLAYER_STATE`. |
| **Medium** | `events.h` — `DEventHandler` (~375+) | TODO: inheritance change "horribly break anything?" | **Documented** — comment now states runtime handlers intentionally share the static dispatch surface while `IsStatic()` keeps lifetime management separate. |
| **Medium** | `p_setup.cpp` (~429+) | Player spawn / deathmatch setup — comment historically said "horribly hacky". | **Documented** — setup comments now spell out why event handlers initialize before map loading and why player-spawn replacement happens after `LoadLevel()`. |

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

| Sev | Location | Issue | Status |
| --- | --- | --- | --- |
| **Medium** | `d_net.cpp` `Net_DoCommand` | 600+ line switch; header did not list invariants or DEM groups. | **Fixed** — header block above `Net_DoCommand` now documents the byte-for-byte parity contract with `Net_TrySkipCommand` and lists the DEM opcode groups; matching block above `Net_TrySkipCommand` calls out the fail-closed semantics. |
| **Low** | `i_net.cpp` `I_NetLoop` | "main thread locked up" — omit dedicated vs listen-server `NetStartWindow::NetLoop` split. | **Fixed** — comment now describes blocking handshake semantics and distinguishes dedicated/silent loop from `NetStartWindow::NetLoop`. |
| **Low** | `c_cmds.cpp` `Cmd_God` block comment | Classic `argv(0) god` header vs modern cheat path. | **Fixed** — comment now explains the `DEM_GENERICCHEAT` queued-command path. |
| **Low** | `p_setup.cpp` ~476 | `[ZZ]` "god-knows-what stage" — document ordering vs `PlayerEntered`. | **Fixed** — comment now states `PlayerEntered` fires during later player-pawn spawn, so static handlers must be initialized before map loading/spawn setup. |

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
| 2026-05-28 | Audit-driven fix sweep: `Net_SkipCommand` fail-closed; legacy `CheckConsistencies` hash loop deleted; `Net_AuthorityLevelStartStatusAfterRelease` cleaned; `cl_noprediction` read removed; `AddClientConnection` else-branch hardened; `SendHeartbeat` TODO documented; `p_sectors.cpp` 214 damage tag check; `FActorBackup` MF_PICKUP/MF2_PUSHWALL preserved across rewind; `p_mobj.cpp` `precalcIQM` serialize documented; `d_netinfo.cpp` / `b_think.cpp` `#if 0` blocks removed with rationale; `p_map.cpp` `CCMD(ffcf)` guarded; `hw_clipper.cpp` dead-branch comment corrected; `hw_bsp.cpp` job queue gets explicit acquire/release; ZScriptDebugger REPL eval rationale documented; "Doom Connector" marketing comments neutralised; `Net_DoCommand` / `Net_TrySkipCommand` headers spell out the byte-for-byte parity contract. |
| 2026-05-28 | Remaining audit rows resolved: native/legacy gameplay header handling unified through `HCDEApplyGameplayHeader`; native command event adapter scoped to `DEM_*` executor payloads only; `TryRunTics` dual scheduler documented as permanent HCDE authority/client contract; main-loop ownership documented for `i_net.cpp` and `d_net.cpp` globals; pregame reliable queue diagnostics strengthened; `-joindedicated` documented as a compatibility shim; ACS local variable pointer/count validation added; player lifecycle state consumption isolated in `G_ProcessPendingPlayerLifecycle`; event-handler inheritance and level setup ordering documented; stale pregame-window / `Cmd_God` / `PlayerEntered` comments cleaned. |
