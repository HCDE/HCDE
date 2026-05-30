# HCDE Roadmap #13 — Monster AI System Scoping

**Last updated:** 2026-05-28
**Status:** Phase 3 initial regroup hint wired behind `sv_aidirector_enable`
and `sv_aidirector_regroup_hint`. This document defines the integration
boundary for future hints.

## What #13 actually means

Board item: "Make a AI system for hcde for monsters / enemies." Read in
context with #12 (Predator Economy) and HCDE's general direction, this is
**not** "rewrite vanilla monster AI". The vanilla state-machine AI in
`p_enemy.cpp` and friends ships hundreds of finely-tuned states that are
the soul of Doom; we will not replace them.

The intent is a layered system that:

1. Keeps every existing monster and weapon **playable identically** to
   today.
2. Adds an **optional** higher-level "director" that can issue tactical
   suggestions to monsters (group up, flee, ambush, retarget) which
   compose with existing A_Look / A_Chase / Pain / etc., rather than
   bypassing them.
3. Is **server-authoritative end-to-end** — clients never run AI logic
   that touches actor state. The director runs on the server only;
   clients see results through the existing actor replication path.

## Existing in-tree surface

- `src/playsim/p_enemy.cpp`, `p_doomenemy.cpp`, `p_heretic_enemy.cpp`,
  `p_hexen_enemy.cpp`, `p_strife_enemy.cpp` — the canonical action
  functions (A_Look, A_Chase, A_FaceTarget, A_*Attack…). These are the
  authoritative state machines.
- `src/playsim/bots/` — the bot system (`b_think.cpp`, `b_bot.cpp`,
  `b_move.cpp`, `b_func.cpp`, `b_game.cpp`). This is closer in shape to
  what #13 asks for, but it targets player-bots, not monsters. It is a
  good template for "where AI logic that touches the actor lives".
- `wadsrc/static/zscript/actors/` — ZScript-side action overrides. Many
  monsters have customized states defined here, not in C++.
- `src/d_net*.cpp` — actor replication. Anything an AI director changes
  on the server must travel through these snapshots; an AI director
  cannot just "tell the client" something out of band.

## Hard rules (server authority)

These are non-negotiable for any #13 PR:

1. **AI lives on the server.** The director's data structures, decision
   timers, target selection scoring, and route planning all run only on
   the authoritative side. Listen-server is server-side; pure-client
   does nothing.
2. **AI mutates actors only through existing playsim APIs.** No bypassing
   `P_DamageMobj`, `P_TryMove`, `P_LineAttack`, etc. If the director
   wants a monster to retreat it sets `target = nullptr` or calls a
   ZScript function that the monster's own state machine reads — it does
   not directly write `mo->x`/`mo->y`.
3. **Determinism in single-player and demos.** Director RNG goes through
   a named `FRandom` that is serialized in saves and demos (see how
   `pr_invasion` is handled today in `d_net.cpp`). Non-deterministic
   inputs (wallclock, untracked counters) are forbidden.
4. **No new client-side AI replication path.** Whatever the director
   produces must already replicate via the existing actor snapshot
   stream. No new opcode for "AI says monster X is now in tactical mode
   Y" — instead, store the tactical mode on the actor (or in a struct
   keyed by actor id) and replicate it through the existing path.
5. **Backward compatibility.** With the director disabled, monster
   behaviour must be **bit-identical** to today on the same input.

## Layering plan

Three layers, top to bottom:

- **Director (server-only).** Periodically (sub-tic or every-N-tic), looks
  at the level: monster groups, threat levels, player positions. Issues
  hints by setting fields on monsters or calling ZScript hooks.
- **Per-monster controller.** A small ZScript-side virtual that reads
  director hints and influences `A_Look`/`A_Chase`/etc. choices when no
  hint is set, falls through to vanilla behaviour. This is where
  per-monster personality lives.
- **State machine (vanilla).** Untouched. The action functions still drive
  movement and attacks.

## Phased plan

- **Phase 0 (this doc).** Define the boundary.
- **Phase 1.** Add a director scaffold gated by a server-controlled CVAR
  `sv_aidirector_enable` (default 0). When 0, no director code paths run
  and behaviour is bit-identical to today. When 1, the director runs but
  emits no hints — purely an observability harness.
- **Phase 2.** Add named FRandom `pr_aidirector` and a status CCMD
  `ai_status` that prints active monster counts, current "groups", and
  the director's frame budget (similar to `net_stressreport`).
- **Phase 3.** Wire the first concrete hint: "regroup". **Initial version
  landed 2026-05-30:** the server-authoritative director clusters nearby
  living monsters, stores regroup targets for clusters of 3+, and
  `P_NewChaseDir` applies a small deterministic bias toward the group center
  when both `sv_aidirector_enable=1` and `sv_aidirector_regroup_hint=1`.
  Movement still goes through `P_DoNewChaseDir`/`P_TryWalk`; no replication
  path or direct actor teleport/state override was added. Soak remains.
- **Phase 4.** Multiplayer soak with `sv_aidirector_enable=1` on a
  dedicated server. Verify saves/demos replay identically.
- **Phase 5+.** Additional hint types (ambush, flee, retarget). Each as
  its own PR, each behind the same CVAR with documented soak metrics.

## Anti-patterns we will reject

- **Per-monster network packets.** No new opcodes for AI state. Replicate
  via the existing actor stream or not at all.
- **Client-side AI prediction.** The client does not predict monster
  behaviour. It interpolates between server snapshots. The director must
  not introduce a client-side decision that influences what the player
  sees outside of normal interpolation.
- **Wallclock-based AI decisions.** The director ticks on the same fixed
  game clock as everything else. No `I_msTime()` in decision logic;
  cycles are budgeted in tics, not milliseconds.

## Source map (where Phase 1+ code goes)

| Concern | File |
| ------- | ---- |
| Director core | `src/d_net_aidirector.cpp` (new), header `.h` |
| Director CVARs/CCMDs | `src/d_net_aidirector.cpp` (CVAR section + `ai_status` CCMD) |
| First playsim hook | `src/playsim/p_enemy.cpp` (`P_NewChaseDir` regroup bias) |
| ZScript per-monster hooks | `wadsrc/static/zscript/actors/<game>/...` (extend existing classes) |
| Replication | reuse existing actor snapshot stream in `d_net_snapshot_part1.cpp`/`part2.cpp` |
| Build system | `src/CMakeLists.txt` (add `d_net_aidirector.cpp`) |

## Open questions (for the implementing PR)

1. How does the director coexist with mods that override `A_Chase` in
   ZScript? Probably "director hints are advisory; ZScript overrides
   win" but this needs a documented contract.
2. What is the right granularity for groups — by sector? Activation
   trigger? Dynamic clusters by proximity?
3. Is per-actor RNG stream needed for fairness, or is a single
   `pr_aidirector` good enough?
