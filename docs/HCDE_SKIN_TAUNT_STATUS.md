# HCDE Skin Taunt Status

This note is the Step 14 execution checkpoint for the roadmap integration plan.
Skin taunts are a cosmetic player-sound feature and must not affect gameplay
state.

## Current Surfaces

- **Command**: `hcde_taunt [variant]` plays the local player's skin-resolved
  `*taunt` voice sound on `CHAN_VOICE`.
- **Diagnostics**: `hcde_taunt_surfaces` reports server policy, local cooldown,
  sound-class resolution, whether a default taunt sound exists, and request
  counters.
- **Policy**: `sv_hcde_taunts_allowed` is a serverinfo gate. Clients control
  local cooldown and volume with `cl_hcde_taunt_cooldown_tics` and
  `cl_hcde_taunt_volume`.
- **Skin hook**: skins and mods can define
  `$playersound <class-or-skin> <gender> *taunt <lump>` or variant sounds such
  as `*taunt-rally`. Default game SNDINFO files alias `*taunt` to existing safe
  player sounds so the command has a baseline.

## Runtime Smoke Check

```text
hcde_taunt_surfaces
hcde_taunt
```

For variants:

```text
hcde_taunt rally
```

The command first tries `*taunt-rally`, then falls back to `*taunt`.

## Step 14 Rules

- Taunts are cosmetic voice events only.
- Taunts must not trigger damage, targeting, AI behavior, inventory changes,
  scoring, economy changes, or movement changes.
- Future multiplayer-wide playback should use a narrow cosmetic event channel,
  not movement commands, gameplay authority records, or prediction repair lanes.
