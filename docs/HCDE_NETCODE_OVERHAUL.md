# HCDE Netcode Overhaul

## Purpose

HCDE's multiplayer goal is to provide the best multiplayer experience this engine can reasonably achieve: Odamex-style responsiveness, stronger world replication, rugged dedicated-server behavior, and broad mod compatibility without giving up competitive correctness.

Invasion is the first torture test because it stresses monsters, projectiles, late joins, server CPU, and packet budgets at the same time. The overhaul must not become an invasion-only subsystem. The target is a shared multiplayer backbone that benefits coop, deathmatch, team modes, survival-style modes, and heavily modded servers.

## Design Goals

- Keep player input responsive under high ping through prediction and compact command flow.
- Preserve server authority for damage, deaths, pickups, spawns, map events, and mode state.
- Keep unlagged player combat as a first-class competitive feature.
- Scale monster-heavy and projectile-heavy gameplay without starving player traffic.
- Degrade gracefully on bad networks by reducing low-priority world detail before gameplay correctness suffers.
- Keep the master server and launcher query protocol stable and separate from live gameplay replication.
- Make unknown mods work conservatively, then allow explicit opt-in metadata for leaner replication.
- Keep hot paths lean: low allocation, compact packets, indexed lookups, fixed budgets, and simple counters.

## Non-Goals

- Perfectly synchronizing every actor every tic.
- Letting monster or projectile traffic delay player commands.
- Requiring the master server registry to understand live actor replication.
- Replacing unlagged with generic actor rewind logic.
- Making every mod actor predictable on clients by default.

## Current Pressure Points

The current live protocol already separates connect negotiation, pregame service messages, client input payloads, server snapshot payloads, and server-authored world deltas. Invasion currently extends this with compact invasion state, spawn events, and actor deltas.

The near-term problems are:

- Invasion actor deltas ride inside the normal server snapshot budget.
- Spawn events and actor pose deltas compete for a small payload window.
- Moving monsters dirty position, velocity, and angle fields frequently.
- Class names and full actor baselines can appear in hot replication paths.
- Actor lookup is array-scanned in paths that damage, block, spawn missiles, and serialize deltas.
- Server simulation can overload before packet serialization is the only bottleneck.

These are useful lessons, but the fix should be a general replication layer rather than more invasion-specific branching.

## Network Lanes

HCDE should treat live multiplayer as separate lanes with independent priority and budget rules.

The current implementation records these lanes explicitly through `net_lanes` while keeping the wire protocol compatible. Actor-delta traffic now builds per-client priority queues for the invasion path, giving each peer its own pressure view before hard byte budgets move into the shared actor layer.

### Command Lane

Carries client user commands, weapon intent, movement intent, and command acknowledgements. This lane must stay small, frequent, and protected from world spam.

### Authority Lane

Carries reliable gameplay decisions: damage, death, respawn, pickup, inventory-affecting events, actor spawns/despawns, map events, ACS/network events, score, team state, and mode state.

### Player Snapshot Lane

Carries player pose corrections, health/state repair, reconciliation data, and baseline data needed by prediction. This lane serves DM and high-ping play before any world actor detail.

### Actor Delta Lane

Carries monsters, projectiles, dropped items, important map actors, and modded gameplay actors. This lane is byte-budgeted, interest-filtered, sequenced, and allowed to degrade quality before it affects command or authority lanes.

### Query And Registry Lane

Carries launcher and master-server facts only. It should remain simple and independent from live gameplay traffic.

## Master Server Compatibility

The master server is a registry, not a gameplay relay. The overhaul should not require it to parse actor deltas, player prediction data, or world snapshots.

The registry and query path should continue to expose stable server facts:

- address and port
- protocol and version
- player count and max players
- game mode
- map
- session state
- optional mod/package identity
- optional capability flags

New live netcode features should be surfaced as capability bits rather than new registry behavior. Examples:

- actor replication v2
- reliable world events
- unlagged enabled
- server simulation LOD
- invasion state/query metadata
- compact projectile events

Older launchers should still be able to list servers and read basic status.

Live gameplay peers negotiate feature support through the `HCAP` trailer on HCDE live control packets. The master server remains a registry; it may display coarse capability flags later, but it does not need to understand live replication formats.

## Unlagged Policy

Unlagged must remain available for competitive modes and high-ping play. It should be centered on player-vs-player combat and server-side validation of player historical poses.

Rules:

- Preserve player pose history for hitscan and latency-compensated weapon validation.
- Keep unlagged player rewind separate from generic monster replication.
- Do not let actor delta backlog affect weapon fire validation.
- Keep server authority final for damage and deaths.
- Allow mode configuration to disable or restrict unlagged if a server wants classic behavior.

Monsters and projectiles may use historical state for fairness or debugging later, but that should not be required for the first shared replication layer.

The first competitive audit pass is active. Shared `HCDA` continues to suppress player pawns, so player correction and any unlagged validation stay isolated on the command/player snapshot path. Player snapshot pressure is now measured explicitly: `HCDW` records max bytes, max records, missing records, budget pressure, local health/state repairs, hard death/respawn repairs, and remote baseline repairs. `net_unlagged` gives a compact per-client view of latency, command/snapshot flow, reconciliation pressure, and actor-lane deferrals so DM/high-ping tuning can see when optional actor traffic is crowding the experience.

## Replicated Actor Model

The shared actor system should replace invasion-only tracking over time.

Each replicated actor needs:

- stable network id
- actor pointer and indexed reverse lookup
- class table id
- replication category
- authority state
- per-client baseline
- per-client priority score
- last sent tic per lane/category
- relevance state

Important categories:

- player pawn
- monster
- projectile
- pickup/item
- dropped inventory
- map actor
- script-owned actor
- visual-only actor
- unknown mod actor

Unknown gameplay actors should default to conservative server authority. Leaner behavior should be enabled through flags, class metadata, or tested heuristics.

The first registry scaffold mirrors current invasion monsters and projectiles into shared actor records while coop and DM scan conservative actor categories into the same registry. This proves identity, class-table, category, and reverse-lookup behavior before all gameplay actor authority moves onto shared deltas.

Actor-delta v2 is now the live invasion actor stream through `actor-registry-v1` plus `actor-delta-v2`. Invasion actor pose deltas use the shared registry id/class table, compact field masks, quantized positions and velocities, and 16-bit angles through the `HCDA` chunk. Coop and DM snapshots can also carry top-level shared-registry `HCDA` records after world deltas. That non-invasion path is baseline/accounting first: it serializes conservative non-player actors and parses them on receive without spawning duplicate gameplay actors or touching unlagged/player prediction. The invasion-specific `HCIA` v5 stream is retired and no longer advertised as a local live capability.

Projectile replication now has its own policy instead of inheriting generic actor priority. The policy ranks live projectiles per client by whether a baseline already exists, whether the projectile is targeting or closing on that client, distance tier, player ownership, and keepalive age. Critical projectiles stay protected, high/medium projectiles remain hot, and far low/dormant projectile storms are skipped until they become relevant again or hit their keepalive. The same policy is used by invasion actor queues and the shared coop/DM `HCDA` sender.

Lane budgets are now negotiated through `lane-budgets-v1`. Protected lanes remain first: control, command, authority headers, and player snapshot records are not allowed to lose space to optional actor detail. Optional authority event replay and actor deltas receive independent byte caps, so a spawn-history burst cannot consume the actor lane and a monster/projectile storm cannot consume the rest of a snapshot packet. When a lane is capped, `net_lanes`, `net_profile`, and `net_stressreport` expose clamps, deferred records, and byte counters for tuning.

Baseline repair now has an explicit per-client recovery path. Runtime late join, reconnect, and client reset cases clear shared `HCDA` baselines, then open a short repair window where actor deltas are forced back to full records without widening the whole packet. Late joiners also get an authority-event catchup cursor so retained `HCAV` spawn, damage/health, despawn, and pickup spawn/retire history walks forward from the oldest retained event instead of only replaying the most recent tail. Damage/health facts are pressure-controlled: small rapid changes are interval-throttled, large changes bypass the throttle, and stale per-actor damage facts are skipped when a newer damage or despawn fact exists. `net_profile`, `net_lanes`, and `net_stressreport` expose active repair windows, reset counts, authority catchup records, next replay id, and per-fact `HCAV` pressure for spawn, damage, despawn, pickup spawn, pickup retire, and superseded retained facts.

## Interest Management

The actor delta lane should send the most important affordable state for each client, not every changed actor.

Priority inputs:

- distance to client player
- visibility or recent visibility
- actor is targeting the player
- actor recently damaged or was damaged by the player
- actor is attacking, firing, dying, spawning, or teleporting
- actor is a projectile near the player or owned by a relevant actor
- actor is objective-critical or boss-like
- actor affects blocking, pickup, scripting, or map progression
- actor was not sent recently and needs a low-rate keepalive

Far actors should receive coarse or low-rate updates. Irrelevant cosmetic actors should be skipped first.

The priority-queue pass is active in the invasion actor-delta sender. It ranks each client's tracked actors by baseline need, death/full-delta pressure, attack state, projectile status, distance, target relevance, and cursor fairness. Interest management now classifies actors as critical, high, medium, low, or dormant per client. Low and dormant actors are skipped until keepalive, while missing baselines, deaths, attack states, targeted actors, and nearby projectiles stay protected. `net_profile`, `net_lanes`, and `net_relevance` expose queue depth, skipped actors, keepalives, and tier counts.

## Projectile Policy

Projectile replication must be classified instead of treating every projectile as equal.

- Hitscan: validate with unlagged player history and send compact impact/damage events.
- Critical projectile: server-authoritative actor with reliable spawn/death and budgeted pose updates.
- Predictable projectile: client may spawn visual immediately; server confirms impact and damage.
- Cosmetic projectile: visual-only or low-priority.
- Spam projectile: pooled, coalesced, or represented by compact burst/event records.

The goal is correct gameplay and convincing visuals when every supported projectile type fires at once, without requiring full per-tic state for every sprite.

## Server Simulation LOD

Network serialization is only half the problem. Monster-heavy mods can overload server thinking before packets are sent.

The server should support safe simulation levels:

- full tic: actors near players, fighting, blocking, attacking, or objective-critical
- reduced tic: far monsters that are awake but not interacting with players
- dormant: far idle actors that can be reactivated by distance, sound, script, damage, or mode pressure
- visual-only client mirror: no authoritative gameplay work on the client

LOD must preserve correctness near players and must not break scripts that expect specific actors to remain active. Unknown or script-heavy actors should remain conservative until measured.

The first server simulation LOD pass is active for invasion monsters on the authority. It runs before world ticking, leaves projectiles, boss waves, combat-targeted actors, recent spawns, deaths, and action-priority actors at full tic, and only throttles far idle monsters. Reduced actors receive scheduled think ticks; dormant actors receive lower-rate scheduled ticks and wake immediately when players approach or health changes. `net_simlod` reports current full/reduced/dormant counts, suspended thinkers, skipped ticks, and wake reasons.

## Mod Compatibility

Mod support should be explicit but forgiving.

Default heuristics:

- shootable, solid, or damaging actors are server-authoritative gameplay actors
- monsters are server-authoritative with client visual mirrors
- missiles are classified by flags, owner, lifetime, speed, and damage behavior
- inventory and pickup actors use reliable authority events
- decorative actors are low-priority or visual-only when safe
- script-owned actors use reliable events and conservative state replication

Future metadata can let mods declare:

- always replicate
- never replicate
- visual-only
- predictable projectile
- reliable event only
- objective-critical
- no simulation LOD

The engine should remain playable with unknown mods even when it cannot optimize them perfectly.

## Packet And Serialization Rules

- Use per-client byte budgets.
- Prioritize reliable authority before pose detail.
- Use baselines and field masks.
- Quantize transforms and angles.
- Send class names through class tables, not hot delta records.
- Avoid allocation in hot packet paths.
- Avoid O(n) actor lookup in damage, blocking, projectile, and serializer paths.
- Keep packet formats versioned and capability-gated.
- Track skipped updates so overload is visible.

## Metrics

Instrumentation is step 2 because the overhaul needs hard numbers.

Minimum counters:

- packet bytes by lane
- packet count by lane
- reliable queue depth
- actor delta queue depth
- actors considered, sent, skipped, and deferred
- full baselines vs partial deltas
- bytes spent by actor category
- per-client actor budget exhaustion
- server tic time
- invasion active monster count
- projectile count
- actor lookup count and lookup cost
- reconciliation count
- unlagged history usage
- late-join catchup cost

Useful commands:

- `net_profile`
- `net_actorstats`
- `net_lanes`
- `net_relevance`
- `net_stressreport`
- `net_unlagged`
- `invasion_status` extension for net pressure

## Rollout

### Phase 1: Measure And Remove Waste

Add profiling counters and replace invasion replicated actor linear scans with indexed lookup. This should improve current invasion testing without protocol risk.

### Phase 2: Shared Actor Identity

Introduce an engine-wide replicated actor registry with stable ids, class tables, per-client baselines, and reverse lookups. Keep existing invasion replication running while the shared registry proves itself.

### Phase 3: Lane Budgets And Priority Queues

Separate command, authority, player snapshot, actor delta, and query/registry behavior. Per-client actor queues are active for invasion actor deltas. Hard byte budgets are the next part of this phase.

### Phase 4: Compact Actor Delta Protocol

Add capability-negotiated actor delta v2 with quantized transforms, class table ids, field masks, baseline repair, and reliable spawn/death history. The `HCDA` v2 stream is active for invasion actor pose/state deltas; reliable spawn/name history now comes through the shared `HCAV` authority lane.

The first baseline repair pass is active. Late joiners and reset clients now rebuild shared actor baselines through a bounded repair window, while `HCAV` uses a per-client catchup cursor to drain retained authority spawn history over multiple packets when needed.

### Phase 5: Invasion Migration

Move invasion monsters and replicated projectiles from invasion-specific payloads onto the shared actor delta lane. Invasion remains the stress test.

The first migration pass keeps invasion spawn history authoritative while refreshing every tracked invasion monster/projectile into the shared registry from its invasion id. `HCDA` owns live actor pose/state deltas; `HCIA` is retired and no longer advertised.

The first shared authority event pass is active through `authority-events-v1`
and the `HCAV` v1 section on protected server snapshot traffic. Invasion spawn,
damage/health, and death/despawn facts now use `HCAV`, while the old invasion-specific `HCIS`
spawn-event section is retired. The damage/health stream now has adaptive pressure control so monster swarms cannot fill the authority lane with stale health facts. Coop/DM pickup spawn/restore and retire facts also use `HCAV`, protecting item existence even when shared `HCDA` detail is deferred. Authority diagnostics now break down transmitted and received fact types, making it clear whether load is coming from combat, death/despawn, or pickup churn. This is the foundation for moving inventory and script/map facts out of mode-specific payloads.

### Phase 6: Coop Migration

Bring coop monsters, items, map actors, and common projectile behavior onto the shared system with conservative mod defaults.

Coop now periodically scans authoritative map actors into the shared registry with conservative categories. Capable peers can receive those records through the top-level `HCDA` lane, establishing ids, class ids, source tags, reverse lookups, and metrics before actor authority is allowed to spawn or steer coop gameplay actors.

### Phase 7: DM And Competitive Tuning

Validate unlagged, player correction, item pickup authority, projectile behavior, scoreboard state, and high-ping command feel. DM is the latency standard.

DM now scans player pawns, projectiles, pickups, and important map actors into the same shared registry while leaving player snapshots and unlagged validation separate. The shared `HCDA` sender skips player actors, so competitive player correction stays on the player lane while item/projectile authority is prepared for shared deltas. The `net_unlagged` audit command and stress report competitive-lane fields are the first pass at proving actor replication degrades before high-ping command feel does.

### Phase 8: Stress Harness

Build repeatable tests for:

- high ping
- jitter
- packet loss
- late join
- reconnect
- map transition
- monster swarms
- projectile storms
- modded slaughter maps
- master-server listing
- unlagged validation
- dedicated server soak

The first Step 12 harness is active under `tests/netcode_step12`. It launches
dedicated invasion, coop, and DM cases, keeps launcher queries warm during soak,
optionally joins client processes, supports advertised master-server runs, and
asks the server for `net_stressreport` so actor pressure, delta pressure, LOD,
lane, relevance, migration, and peer queue state are captured in one comparable
server log snapshot. `net_stressreport` also emits a compact `DebugTrace` event
on the `net` channel, and the harness can persist per-case traces with
`--trace-save-dir`. The harness also exposes explicit migration-health checks,
so coop/DM coverage regressions can fail fast instead of being buried in the
general pressure output. High-ping, jitter, and packet-loss runs use the same
harness under an external OS/network shaper and record the condition through
`--label`.

## Success Targets

The overhaul should be considered successful when:

- DM remains responsive under high ping and unlagged validation is stable.
- Coop can run monster-heavy maps without player commands being starved.
- Invasion can scale beyond the current small-group bottleneck.
- Projectile-heavy scenes degrade visually before gameplay authority breaks.
- Late joiners converge without forcing huge snapshot spikes.
- Master-server listing and launcher queries remain compatible.
- Unknown mods remain playable with conservative behavior.
- Profiling clearly explains overload instead of hiding it.

## Implementation Order

The major implementation steps are:

1. Write this design document.
2. Add instrumentation.
3. Replace hot-path actor scans with indexed lookups.
4. Add protocol capability negotiation.
5. Build the shared replicated actor registry.
6. Separate network lanes.
7. Add per-client priority queues. Done for the current invasion actor-delta path while preserving packet compatibility.
8. Add compact actor delta serialization. Done for capability-negotiated invasion actor deltas through `HCDA` v2.
9. Add interest management. Done for invasion actor-delta queues with per-client relevance tiers and low-rate keepalives.
10. Add server simulation LOD. Done for conservative far-idle invasion monster throttling with immediate wake rules.
11. Migrate invasion, coop, then DM-specific behavior. In progress: invasion uses `HCDA` as the live actor path and coop/DM populate the shared registry foundation.
12. Build stress tests and tune. The initial repeatable harness and
    `net_stressreport` command are in place; the next tuning passes should use
    those logs to set byte budgets, projectile policy thresholds, migration
    health gates, and cutover criteria.
13. Harden malformed pregame traffic handling. Replayed, truncated, or token-mismatched service traffic now accumulates strikes and can be quarantined briefly so broken peers stop burning setup CPU.

More steps will likely appear after measurement, but these are the major landmarks.

## Current Overhaul Status

1. Authority event lane. Expanded: `HCAV` v1 carries invasion spawn, damage/health, death/despawn, and coop/DM pickup spawn/retire facts on the protected authority lane; damage/health and pickup existence facts skip stale retained entries when a newer per-actor fact exists, and diagnostics break pressure down by fact type.
2. Shared `HCDA` actor deltas for coop/DM. Done: non-invasion snapshots now carry top-level shared actor deltas for conservative non-player registry actors, with receive-side baseline parsing only.
3. Projectile replication policy. Done: projectiles now use a per-client policy for critical/high/medium/low/dormant tiers, inbound/player-owned detection, skip/keepalive behavior, and stress/profile diagnostics.
4. Real byte budgets per lane. Done: `lane-budgets-v1` is advertised, optional authority replay and actor-delta chunks are capped independently, and diagnostics show clamp/defer pressure.
5. Baseline repair and late-join catchup. Done: late join/reset clients force fresh shared actor baselines for a bounded repair window, and late joiners drain retained `HCAV` authority spawn history through a per-client catchup cursor.
6. Unlagged/high-ping competitive audit. Done: player command/snapshot lanes remain protected, shared `HCDA` suppresses player pawns, and `net_unlagged` reports latency, prediction repairs, hard reconciliations, player-lane pressure, and actor-lane deferrals.
7. Stress tuning with real mods and shaped networks. Harness ready: pressure presets, trace export, network-shape metadata, migration-health thresholds, and JSON summaries are in place; real mod soak data still needs to drive the next tuning pass.
8. Retire legacy invasion `HCIS`/`HCIA` fallback paths. Done: live invasion snapshots always emit `HCAV` authority events plus shared-registry `HCDA` actor deltas, the old serializers/parsers are removed, and the old `invasion-actor-delta-v5` capability is no longer advertised locally.
9. Protocol/code cleanup and final documentation. Done: retired protocol bits are explicitly named, stale legacy invasion actor baseline state is removed, diagnostics report the modern `HCAV`/`HCDA` paths, and documentation reflects the current wire direction.
10. Malformed pregame traffic quarantine. In progress: repeated setup/service corruption now records strikes, activates short quarantine windows, and exposes the protection state in `net_profile` and `net_stressreport`.
11. Retire the NCMD transcode layer. In progress: HCDE peers speak native `HLIV`/`HCIN`/`HCSN`/`HCDW`/`HCAV`/`HCDA` on the wire, and gameplay send/receive no longer transcodes through classic NCMD. The remaining gate is runtime soak across live join, reconnect, desync recovery, invasion pressure, and shaped latency.
    - Step A (done): never send raw NCMD fallback on the wire when native encode fails; request replay instead.
    - Step B (done): apply native payloads directly to `FClientNetState`, tic queues, and invasion state without repacking into NCMD.
    - Step C (done): build native payloads directly from gameplay state in `NetUpdate()` without an NCMD intermediate. Dedicated-client `HCIN` sends now build from `LocalCmds` and `NetEvents` directly; authority `HCSN` snapshots now build from `ClientStates`, quitter lists, world deltas, actor deltas, and invasion state directly, including retransmit/passive resend and late-join replay windows.
    - Step D (done): delete `BuildHCDE*Payload`, `HCDEDecodeLegacyUserCmdRecord`, unwrap repack helpers, and legacy-byte profile counters. The send-side builders and receive-side repack helpers are gone; native gameplay encode/apply failures now reject and request replay instead of translating through NCMD.
    - Step E (in progress): soak and cleanup. Local Debug builds pass and the audit pass tightened the native path: `eventScratch` was hoisted out of inner receive loops (no more per-tic 14 KB stack zero-init), the client-input event allow-list is now honored on the build side too (disallowed events are silently dropped instead of stalling the peer on a replay loop), quitter validation runs up-front, the `Unwrap*Payload` helpers shed their redundant `nativeApplied` out-param, and every new native send/receive function carries a contract comment.
    - Step F1 (done): replay failure escalation. Repeated native replay requests are counted per peer inside a short window; locally-owned remote clients are marked for disconnect after the strike limit, while authority/not-locally-kickable peers suppress further replay pressure temporarily so decode/encode failures cannot flood forever. `net_profile` reports replay requests, suppressions, and escalations.
    - Step F2 (done): precise native failure reasons. Native builders and apply/unwrap helpers now return stable reason strings for envelope, header, record, event, usercmd, world-delta, actor-delta, invasion, length, and capability failures; replay requests and native send aborts propagate those reasons directly instead of logging generic `*-decode` or `*-native-build`.
    - Step F3 (done): reduce large `NetUpdate()` stack scratch. Native send capture arrays for `HCIN`/`HCSN` now live in a module-level `FHCDELiveNativeSendScratch` reused by the main-thread send loop instead of allocating and zeroing tens of kilobytes inside every packet-window iteration.
    - Step F4 (done): HCDE-only gameplay policy. `net_hcde_native_only` defaults on and leaves setup/latency/level-start/control traffic alone, but rejects classic NCMD live gameplay at both outbound fallback and inbound parser boundaries. Locally-owned remote clients are marked for disconnect when they try to keep using legacy gameplay, and `net_profile` reports `legacy-gameplay-reject`.
    - Step F5 (done): automated native smoke/soak checks. The Step 12 harness now has `--require-native-live`, which launches client pressure, runs final `net_profile`, parses native live counters, and fails the run if `HCIN`/`HCSN` traffic is missing or if encode failures, capability rejects, replay storms, or legacy gameplay rejects appear.
    - Step F6 (done): runtime soak. `tests/netcode_step12/f6_runtime_soak.py` and `f6_disconnect_probe.py` keep dedicated server + synthetic clients alive long enough for the live capability handshake to complete (the original `+wait`/`+quit` harness was quitting at clienttic=17 on RelWithDebInfo, hiding native traffic). Five scenarios were exercised against `RelWithDebInfo` against DOOM2 MAP01:
        - Baseline 1-client coop, 35s -- `caps-tx=33/caps-rx=32`, `wrapped=1112`, `snapshots.native-built=1112`, `client-input.native-apply=1117`, `encode-fail=0`, `legacy-gameplay-reject=0`, `replay-req=1`, `replay-escalate=0`.
        - Single-client coop with `sv_invasion 1` + periodic stress reports, 40s -- `wrapped=1290`, `snapshots.native-built=1290`, `client-input.native-apply=1304`, `encode-fail=0`, `legacy-gameplay-reject=0`, `replay-escalate=0`.
        - 3-client coop, 45s -- `caps-tx=45/caps-rx=45`, `wrapped=1526`, `snapshots.native-built=1526`, `client-input.native-apply=1548`, `encode-fail=0`, `legacy-gameplay-reject=0`, `replay-escalate=0`.
        - 2-client coop with `net_extratic 1` + `net_ticbalance 1`, 40s -- snapshot rate doubled to `snapshots.native-built=2636` with `encode-fail=0`, `legacy-gameplay-reject=0`, `replay-req=2`, `replay-escalate=0`.
        - Disconnect/reconnect probe (`f6_disconnect_probe.py`): clients A+B join, B is killed abruptly after 12s, C joins 6s later -- server survives the SIGKILL, no `replay-escalate`, no `legacy-gameplay-reject`, native traffic remains steady through and after the drop (`snapshots.native-built=1001`, `client-input.native-apply=1016`).
        - Across all runs, the only non-zero hardening counter was `cap-reject` (22-42 per join). These are the brief startup tics where the server attempts a native snapshot before the peer's first `caps` echo lands; the gameplay packet is silently deferred (no legacy fallback, no escalation) and the counter freezes at handshake completion. This is the expected gate behavior introduced by Step F4 and confirms native gameplay never leaks NCMD.
    - Cutover gate: all HCDE peers negotiate the full local capability mask, `net_profile` shows `encode-fail=0` and `legacy-rx=0`, and soak tests pass with no replay storms.

## Native-Only Target (No GZDoom Wire Or Internal ABI)

HCDE multiplayer should converge on one stack:

| Layer | Today | Target |
|-------|-------|--------|
| Wire between HCDE peers | Native `HLIV` envelopes with `HCIN`/`HCSN` bodies | Same |
| Receive path | Unwrap native → apply directly | Same |
| Send path | Build native directly from `NetUpdate()` state | Same |
| Non-HCDE peers | Raw NCMD loopback / unsupported | Not supported in release sessions |
| Skulltag map metadata (`wavelimit`, CMPGNINF) | Gameplay/map layer | Keep; not packet legacy |

The old gameplay transcode path is retired. Any remaining legacy counters in diagnostics refer to non-gameplay compatibility/control traffic, not `HCIN`/`HCSN` gameplay payloads.
