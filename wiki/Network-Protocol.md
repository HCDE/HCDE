# Network Protocol

During dedicated join, HCDE negotiates and uses multiple lanes:

- connect negotiation magic: `HCD3` (service connect)
- pregame service channel: reliable service messages (`HPS_*`, seq/ack)
- live lane accounting:
  - control
  - command
  - authority
  - player snapshot
  - actor delta
  - query/registry
- actor-delta scheduling:
  - invasion actor deltas build per-client priority queues before serialization
  - `net_profile` and `net_lanes` report queue depth, priority depth, deferred depth, and top score
  - `net_relevance` reports critical/high/medium/low/dormant actor interest per client
  - far low-interest actors are skipped until their keepalive interval while protected actor state stays hot
- server simulation LOD:
  - `sv_invasionsimlod` conservatively throttles far idle invasion monsters before world ticking
  - `net_simlod` reports full/reduced/dormant actors, suspended thinkers, skipped ticks, and wake reasons
  - actors wake immediately for distance, health changes, combat targets, boss waves, projectiles, deaths, and action-priority states
- compact actor deltas:
  - modern live peers negotiate `actor-registry-v1` and `actor-delta-v2`
  - `HCDA` v2 uses registry class ids, field masks, quantized positions/velocities, and compact angles
  - invasion uses `HCDA` as the live actor stream, and coop/DM snapshots can now carry top-level shared-registry `HCDA` records
  - non-invasion `HCDA` currently establishes budgeted shared baselines without spawning or steering gameplay actors; authority-driven actor birth/apply remains a later migration step
  - the shared `HCDA` path is now the live invasion actor stream; the old invasion `HCIA` v5 fallback is retired
- projectile policy:
  - projectile deltas are ranked by baseline state, distance, viewer targeting, inbound velocity, player ownership, and keepalive age
  - near/inbound/player-relevant projectiles stay hot while distant projectile storms are allowed to thin out until keepalive or renewed relevance
  - `net_profile`, `net_relevance`, and `net_stressreport` expose projectile policy tiers, skips, keepalives, protected records, inbound shots, and player-owned shots
- lane budgets:
  - modern live peers negotiate `lane-budgets-v1`
  - protected lanes stay first: control, commands, authority headers, and player snapshots are preserved before optional actor detail
  - optional authority event replay is capped separately from actor deltas, and actor deltas use their own budget instead of consuming all remaining snapshot space
  - `net_lanes` and `net_stressreport` report lane budgets, budget clamps, deferrals, and byte counters
- authority events:
  - modern live peers negotiate `authority-events-v1`
  - `HCAV` v1 carries protected server-authored gameplay facts on the authority lane
  - invasion spawn, damage/health, and death/despawn facts now move through `HCAV`; the old invasion `HCIS` spawn fallback is retired
  - damage/health facts are interval-throttled and stale per-actor health facts are skipped when a newer damage/despawn fact is retained
  - coop/DM pickup spawn/restore and retire facts now move through `HCAV`, so item existence is protected even when shared actor detail is deferred
  - `net_profile` and `net_stressreport` break down `HCAV` pressure by spawn, damage, despawn, pickup spawn, pickup retire, and superseded retained facts
- baseline repair and catchup:
  - runtime late join and client reset paths clear stale shared actor baselines per client
  - a bounded repair window forces fresh full actor records without increasing the whole snapshot budget
  - late joiners walk retained `HCAV` spawn history forward with a per-client catchup cursor instead of relying only on the last replay tail
  - `net_profile`, `net_lanes`, and `net_stressreport` expose repair windows, reset counts, catchup records, and pending replay ids
- competitive/high-ping audit:
  - player command and player snapshot lanes stay protected before optional actor detail
  - shared `HCDA` actor deltas continue to suppress player pawns so unlagged/player correction stays isolated
  - `net_unlagged` reports latency, command/snapshot flow, player-lane budget pressure, prediction repairs, hard reconciliations, and actor-lane deferrals
- mode migration:
  - invasion monsters/projectiles are refreshed into the shared registry from their authoritative ids
  - coop and DM periodically populate the shared registry with conservative actor categories for the shared `HCDA` lane
  - `net_migration` reports the current mode scan and source counts
- stress and soak diagnostics:
  - `net_stressreport` emits one compact pressure snapshot covering world tic cost, actor queues, delta budget deferrals, relevance tiers, simulation LOD, mode migration, lane counters, and per-peer queue state
  - each stress report also writes a compact `DebugTrace` entry on the `net` channel for advanced-debugger and trace-file workflows
  - `tests/netcode_step12/netcode_step12_stress.py` runs repeatable invasion/coop/DM dedicated-server soak cases while launcher queries stay active
  - the Step 12 harness supports optional client join pressure, master-server advertising, mod-specific pressure commands, pressure presets, debug trace export via `--trace-save-dir`, JSON summaries via `--summary-dir`, and external high-ping/jitter/loss shaping metadata
- runtime debug trace streams (enabled by default via `debugtrace_enable` and `debugtrace_stream`):
  - `%LOCALAPPDATA%/hcde/hcde_trace.hcde.latest.log` for the client and `%LOCALAPPDATA%/hcde/hcde_trace.hcdeserv.latest.log` for the dedicated server
  - session-scoped files also land as `hcde_trace.<process>.<session>.log`; match the `sess=` field across client and server logs when diagnosing net stalls
  - in-game: `debugtraceopen` prints paths, `debugtrace [channel]` dumps the ring buffer, `debugtraceflush` / `debugtracesave` export on demand
- live gameplay channel types:
  - control
  - client inputs
  - server snapshots
- live control capability trailer: `HCAP` v1 bitmask, negotiated peer-to-peer and ignored by older peers
- gameplay envelope magic: `HGPL`
  - includes room id, protocol version, payload kind, game tic

Current payload families documented in code/docs include:

- client input payloads (`HCIN` family with explicit records)
- server snapshot payloads (`HCSN` family with explicit records)
- server-authored world delta stream (`HCDW`) used for validation/telemetry and live authority evolution
- shared replicated actor registry scaffold used to assign actor categories, class table ids, and reverse lookups before actor-delta v2 owns serialization

Detailed stage notes live in:

- `tests/netcode_step12/README.md`
- `docs/HCDE_NETCODE_STAGE*.md`
