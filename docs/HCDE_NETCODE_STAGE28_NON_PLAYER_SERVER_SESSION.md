# HCDE Netcode Stage 28 Non-Player Server Session

Date: 2026-05-12

Stage 28 moves dedicated-server-owned settings authority state into HCDE server
session state. No Odamex source was copied in this stage.

## What Changed

Dedicated server mode now records authority metadata in `hcde_servermode`
instead of treating the reserved internal arbitrator slot as a playable settings
controller:

- authority internal slot
- whether the authority is backed by a real player
- settings-controller state
- waiting state
- display name used by diagnostics and host logs

The net layer exposes `Net_LocalCanControlSettings()` so gameplay/admin code can
ask one authority question without knowing whether the current process is a
normal player host or an HCDE dedicated server session.

## Safety Rules

- Normal peer-host sessions still use `players[consoleplayer].settings_controller`.
- Dedicated server sessions answer local settings authority from
  `hcde_servermode`.
- The reserved dedicated server slot is forced to `settings_controller == false`
  and `waiting == false` whenever Stage 28 updates server-owned state.
- This stage does not change wire format, packet records, or Doom Connector
  launch arguments.

## Current Boundary

Stage 28 removes direct local settings-permission reads from console, gameplay,
server-CVAR, and bot/admin paths. Stage 29 later adds a separate HCDE service
authority identity boundary, but the dedicated server still keeps the inherited
reserved internal authority slot for startup and sync compatibility.

## Remaining Work

- Stage 29 added a service authority identity boundary for new HCDE service/live
  code.
- Move remaining inherited startup and fallback packet paths to HCDE service
  identities.
- Drop the reserved internal slot once startup, sync, host migration, and live
  replication no longer require a player-backed authority index.
- Apply live reconciliation mutations after inherited lockstep consistency is
  retired.
