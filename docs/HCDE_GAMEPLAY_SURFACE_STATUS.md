# HCDE Gameplay Surface Status

This note is the Step 12 execution checkpoint for the roadmap integration plan.
Predator Economy, monster/enemy AI, and skin taunt sounds are all gameplay-adjacent,
but they do not have the same authority requirements.

## Current surfaces

- **Mode state**: HCDE already has a server-facing game type path through
  `sv_gametype` plus Invasion state. Predator Economy now has native mode
  identity `sv_gametype 5` plus a server-owned economy fact ledger.
- **Authority events**: spawn, damage, despawn, pickup spawn, and pickup retire
  records already exist for replicated gameplay facts. Economy and AI systems
  should add canonical events or replicated mode fields when clients need to
  observe them.
- **Monster AI**: Invasion is the current stress path for monster spawning,
  active monster tracking, and simulation LOD. Tactical AI now has
  serverinfo-gated profiles plus live monster probes through
  `hcde_ai_surfaces`; future behavior should consume those facts on the
  authority.
- **Skin taunt sounds**: `hcde_taunt` resolves the `*taunt` logical player sound
  through the existing skin/player-sound tables. Default game SNDINFO files
  alias `*taunt` to a safe existing player sound so skins can override it
  without adding a new runtime path.

## Runtime smoke check

Use the console command:

```text
hcde_gameplay_surfaces
```

The command reports the active gameplay mode, Invasion/AI pressure, Predator
Economy state, authority event counters, replicated actor state, and the
cosmetic boundary for taunts. It does not mutate actors, AI state, or sounds.

Predator Economy has a dedicated smoke command:

```text
sv_gametype 5
hcde_predator_economy_surfaces
```

Authority-only mutation smoke:

```text
hcde_predator_economy_reset 1
hcde_predator_economy_fact essence_drop 25
hcde_predator_economy_fact hunter_bank 10
```

Tactical AI has a dedicated smoke command:

```text
hcde_ai_surfaces
hcde_ai_probe 64
```

Authority-only profile mutation smoke:

```text
hcde_ai_profile 3 70 35 35 25 20 15
```

Skin taunts have a dedicated smoke command:

```text
hcde_taunt_surfaces
hcde_taunt
```

## Step 6 rules

- Implement Predator Economy as server-owned mode state with replicated facts,
  not as client-local counters. The Step 12 ledger is the first authority-owned
  landing point for Essence, crisis, debt, node, portal, and monster-tier facts.
- Keep monster/enemy AI decisions authoritative in netgames. The Step 13
  tactical profiles and probes are server-owned inputs for future ZScript or
  playsim behavior, not client-side steering.
- Use existing authority-event and replicated-state lanes for client-visible
  gameplay facts.
- Keep skin taunts cosmetic. If they need network playback, send a narrow
  cosmetic event instead of piggybacking on movement or gameplay packets.
- Do not let cosmetic audio trigger damage, targeting, inventory, scoring, or
  economy changes.
