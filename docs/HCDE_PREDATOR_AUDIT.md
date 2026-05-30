# HCDE Roadmap #12 — Predator Economy Game Mode Scoping

**Last updated:** 2026-05-28
**Status:** Phase 1 scaffold and Phase 2 snapshot contract landed
default-off; buy/currency/role gameplay remains pending.

## What #12 actually means

Board item: "Predator Economy Game Mode." Read in context with HCDE's
multiplayer focus (Invasion, dedicated servers, RCON), this is a new
*game mode* with an asymmetric "predator" role and a per-round economy
(currency earned by kills/objectives, spent on weapons/upgrades). It is
adjacent in shape to Counter-Strike's buy phase grafted onto a Doom
deathmatch base.

This document does NOT specify game balance. It specifies the **engine
surface area** the mode needs and the rules that surface must follow.

## Reference shape

A typical round of Predator Economy looks like this:

1. **Spawn / setup.** All players spawn. One is selected as the predator.
2. **Buy phase (short).** Survivors spend currency on weapons/upgrades.
   Predator may buy nothing or buy with a different currency pool.
3. **Hunt phase.** Survivors try to complete an objective or survive the
   timer. Predator tries to eliminate survivors.
4. **Round end.** Currency awards posted. Next round starts; roles may
   rotate.

This is structured very much like Invasion — a wave/state machine with
explicit states and explicit transitions. We will steal the Invasion
implementation pattern wholesale (see `docs/HCDE_INVASION_AUDIT.md` and
`src/d_net_invasion.cpp`).

## Existing in-tree surface to reuse

- **State machine pattern.** `EInvasionState`, `FInvasionWaveDirector`,
  `FInvasionSpawnDirectory` in `src/d_net_invasion.cpp`. Predator
  Economy gets `EPredatorState` and `FPredatorRoundDirector` in a new
  file `src/d_net_predator.cpp`.
- **Replicated mode state.** Invasion already replicates wave / state /
  budget through the snapshot stream (`HCDEInvasionSnapshotV2`). Predator
  Economy should do the same — define `HCDEPredatorSnapshotV1` with
  capability gating identical to how Invasion handles
  `HCDELiveCapInvasionSnapshotV2`.
- **Authority events.** Currency gain / spend / round transitions all
  flow as authority events from server to clients, not as ad-hoc
  client-side state writes. Use the existing `DEM_*` lane only for
  **cosmetic** things (e.g., a buy-menu open sound). Authoritative state
  goes through the snapshot stream.
- **Named RNG.** Add `pr_predator` (an `FRandom` named "predator")
  serialized via the same `StaticWriteRNGState` /
  `StaticReadRNGState` path Invasion uses. Predator role assignment must
  be deterministic across saves/demos.

## Hard rules (server authority)

These are non-negotiable:

1. **Currency lives on the server.** Clients see their own balance via a
   per-player replicated field. They cannot mutate it; only authority
   events do.
2. **Buy actions are commands.** A buy is a `DEM_*`-style command from
   client → server. The server validates funds + game phase + item legality
   and either applies the purchase (subtract, grant) and broadcasts a
   one-shot event, or rejects and sends a single negative reply event.
   Clients NEVER pre-spend or pre-grant.
3. **Predator role is server-assigned.** Selection runs on server using
   `pr_predator`. Clients receive the assignment as a replicated field;
   they cannot self-promote.
4. **Round timer is server-side.** Clients see a replicated countdown but
   the server is the only owner. No client-side timer drift triggers a
   phase transition.
5. **No mode bypass through cheats.** Existing cheat opcodes
   (`DEM_GIVECHEAT`, `DEM_TAKECHEAT`) must be filtered or scoped when
   Predator Economy is active. Either disable cheats entirely in the
   mode, or require the server to whitelist explicitly.

## Phased plan

- **Phase 0 (this doc).** Define the boundary.
- **Phase 1.** Skeleton mode in `src/d_net_predator.cpp`:
    - `EPredatorState` enum
    - `FPredatorRoundDirector` with stub `Tick()` that just transitions
      WAITING → BUY → HUNT → END → WAITING on a timer
    - CVARs: `sv_predator_enable`, `sv_predator_round_seconds`,
      `sv_predator_buy_seconds`, `sv_predator_starting_currency`
    - CCMD: `predator_status` (mirror Invasion's status CCMD)
    - Build hooked in `src/CMakeLists.txt`
- **Phase 2.** Snapshot replication:
    - `HCDEPredatorSnapshotV1` with capability gate. **Contract landed
      2026-05-28:** `FHCDEPredatorSnapshotV1`,
      `HCDELiveCapPredatorSnapshotV1`, and build/apply helpers exist for
      round state/countdown mirroring. Wire encoding is a follow-up.
    - Per-player currency field
    - Predator-role marker
    - Round timer
- **Phase 3.** Buy command + validation:
    - New `DEM_PREDATOR_BUY` opcode (or, if we want to avoid bloating the
      `DEM_*` table, route through a generic `DEM_NETEVENT` with a
      well-known event name — this is the lower-risk path because
      `DEM_NETEVENT` already exists).
    - Server-side validator
    - Authority event reply (granted / rejected)
    - **Cheat scoping landed 2026-05-29:** while `sv_predator_enable` is on,
      `DEM_GENERICCHEAT`, `DEM_GIVECHEAT`, `DEM_TAKECHEAT`, `DEM_SETINV`,
      and `DEM_WARPCHEAT` are rejected by default during command playback.
      Servers must explicitly set `sv_predator_allow_cheats=1` to whitelist
      those opcodes for testing.
- **Phase 4.** Predator selection + role-based behaviour. This is where
  ZScript class extensions come in (a predator pawn class derived from
  the existing player pawn).
- **Phase 5.** Soak. Multiplayer test on dedicated server with bots /
  humans, savegame round-trip, demo round-trip, RCON observation via
  `predator_status`.

## What this is NOT

- Not a rewrite of deathmatch scoring. Predator Economy sits next to
  deathmatch the same way Invasion sits next to coop.
- Not a new actor replication system. It uses the snapshot stream we
  already have.
- Not a client-side mode. The client only renders state the server sends.
- Not a cheat-elevation path. Existing cheat protections still apply
  (and may need to tighten further when this mode is active).

## Source map (where Phase 1+ code goes)

| Concern | File |
| ------- | ---- |
| Mode core | `src/d_net_predator.cpp` (new), header `.h` |
| Mode CVARs/CCMDs | `src/d_net_predator.cpp` (CVAR + CCMD section) |
| Snapshot wire format | `src/d_net_snapshot_part1.cpp` / `part2.cpp` (extend `Net_Build*Snapshot`/`Net_Apply*Snapshot`) |
| ZScript predator pawn | `wadsrc/static/zscript/actors/predator/...` (new) |
| Build system | `src/CMakeLists.txt` (add `d_net_predator.cpp`) |
| RNG | new `FRandom pr_predator("predator")` next to `pr_invasion` in `d_net.cpp` |

## Open questions (for the implementing PR)

1. Buy menu — engine UI or ZScript-driven? ZScript is the safer answer
   because it avoids new C++ menu code and lets modders override.
2. Should Predator Economy and Invasion be mutually exclusive at the
   server level, or composable? (Current assumption: mutually exclusive.
   Mixing two state machines that both gate spawning is a soak nightmare.)
3. Does the predator role rotate within a single match, or only between
   matches? (Affects how aggressive the snapshot needs to be about
   replicating the role marker.)
