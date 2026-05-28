# HCDE Tactical AI Status

This note is the Step 13 execution checkpoint for the roadmap integration plan.
The AI roadmap item is treated as server-authoritative gameplay infrastructure:
C++ exposes deterministic profiles and probes, while future ZScript or playsim
logic consumes those facts on the authority.

## Current Surfaces

- **Mode gate**: `sv_hcde_tactical_ai` is the serverinfo switch for tactical AI
  behavior. It defaults off until concrete behavior consumers are wired.
- **Tier profiles**: `hcde_ai_profile` mutates authority-owned tier profiles for
  aggression, cover, flank, suppression, retreat, and portal-retreat weights.
- **Live probes**: `hcde_ai_surfaces` and `hcde_ai_probe` scan live monsters for
  target pressure, player targeting, heard targets, bosses, dormant monsters,
  line of sight, and suggested Predator Economy tiers.
- **Safety boundary**: Step 13 does not change monster movement or targeting by
  itself. It creates the authoritative input surface that future tactical
  behavior can consume.

## Runtime Smoke Check

```text
hcde_ai_surfaces
hcde_ai_probe 64
hcde_ai_profile 3 70 35 35 25 20 15
```

The profile revision should increase after `hcde_ai_profile`.

## Step 13 Rules

- Tactical decisions must run on the service authority in netgames.
- Clients may visualize AI state, but must not choose cover, flank, suppression,
  retreat, target, or tier decisions locally.
- Future ZScript actors should consume authoritative profile/probe state through
  narrow APIs or replicated facts instead of ad hoc client state.
- Predator Economy tier behavior should feed this surface before enabling
  high-risk monster behavior such as portal retreats or coordinated group hunts.
