# HCDE Gameplay Surface Status

This note is the Step 6 execution checkpoint for the roadmap integration plan.
Predator Economy, monster/enemy AI, and skin taunt sounds are all gameplay-adjacent,
but they do not have the same authority requirements.

## Current surfaces

- **Mode state**: HCDE already has a server-facing game type path through
  `sv_gametype` plus Invasion state. New gameplay modes should follow that
  model instead of storing important mode state only on clients.
- **Authority events**: spawn, damage, despawn, pickup spawn, and pickup retire
  records already exist for replicated gameplay facts. Economy and AI systems
  should add canonical events or replicated mode fields when clients need to
  observe them.
- **Monster AI**: Invasion is the current stress path for monster spawning,
  active monster tracking, and simulation LOD. Future AI work should remain
  server-owned in netgames.
- **Skin taunt sounds**: skin/player sounds are already data-driven through
  skin and player-sound tables. Taunts should be treated as cosmetic/audio
  events unless a future design deliberately gives them gameplay effects.

## Runtime smoke check

Use the console command:

```text
hcde_gameplay_surfaces
```

The command reports the active gameplay mode, Invasion/AI pressure, authority
event counters, replicated actor state, and the cosmetic boundary for taunts. It
does not mutate mode state, actors, economy state, AI state, or sounds.

## Step 6 rules

- Implement Predator Economy as server-owned mode state with replicated facts,
  not as client-local counters.
- Keep monster/enemy AI decisions authoritative in netgames.
- Use existing authority-event and replicated-state lanes for client-visible
  gameplay facts.
- Keep skin taunts cosmetic. If they need network playback, send a narrow
  cosmetic event instead of piggybacking on movement or gameplay packets.
- Do not let cosmetic audio trigger damage, targeting, inventory, scoring, or
  economy changes.
