# HCDE Predator Economy Status

This note is the Step 12 execution checkpoint for the roadmap integration plan.
Predator Economy is a native gameplay mode, so its scoring and ecosystem facts
must originate from the service authority.

## Current surfaces

- **Mode identity**: `sv_gametype 5` selects Predator Economy. It currently uses
  team deathmatch spawn/team plumbing while dedicated faction rules are built.
- **Authority state**: `hcde_predator_economy_surfaces` reports the server-owned
  fact ledger: faction score, Essence on the floor, banked/spent Essence, marked
  monsters, evolution nodes, sealed portals, crisis tier, debt players, monster
  tiers, and revision.
- **Mutation surface**: `hcde_predator_economy_reset` and
  `hcde_predator_economy_fact` require the local service authority slot. They
  are diagnostics/foundation commands for wiring future actor pickups, monster
  tiering, debt-clock, and portal events into one canonical mode state.
- **Session knobs**: `sv_predator_economy_autostart`,
  `sv_predator_economy_session_seconds`, `sv_predator_economy_debt_seconds`, and
  `sv_predator_economy_debt_points_per_second` are serverinfo cvars.

## Runtime smoke check

```text
sv_gametype 5
hcde_predator_economy_surfaces
hcde_predator_economy_reset 1
hcde_predator_economy_fact essence_drop 25
hcde_predator_economy_fact hunter_bank 10
hcde_predator_economy_fact crisis 1 3
```

The fact revision should increase on each authority mutation.

## Step 12 Rules

- Keep faction score, Essence, debt, crisis, portal seals, evolution nodes, and
  monster tier facts authority-owned.
- Future client HUDs should read replicated mode facts. They must not invent
  score or economy state locally.
- Future pickups and monster events should feed this ledger through narrow
  authority events or replicated mode fields.
- Until faction-specific spawn/loadout rules exist, `sv_gametype 5` deliberately
  reuses team-deathmatch plumbing without claiming full Predator Economy rules.
