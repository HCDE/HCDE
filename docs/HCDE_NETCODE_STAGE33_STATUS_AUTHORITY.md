# HCDE Netcode Stage 33 Status Authority

Date: 2026-05-12

Stage 33 moves read-only status, notification, and cutscene ownership paths onto
HCDE service-authority helpers. No Odamex source was copied in this stage.

## What Changed

The following display-facing paths now ask HCDE's service-authority identity
helpers instead of directly comparing against `Net_Arbitrator`:

- cutscene host-only ready selection
- cutscene advance demo command ownership
- `stat network` local authority labels and wait-state text
- `stat network` peer authority labels and latency display
- FOV and settings-controller notification text
- `pings` visibility filtering

The network packet format and gameplay authority behavior are unchanged.

## Safety Rules

- This stage does not remove the inherited internal server slot.
- This stage does not grant, remove, or transfer settings-controller privileges.
- Dedicated server sessions can display authority state without exposing the
  reserved server slot as a playable peer.
- Normal peer-host sessions still resolve service authority to the inherited
  host slot.
- Doom Connector launch arguments do not change.

## Current Boundary

Stage 33 removes direct `Net_Arbitrator` checks from read-only status and
notification paths. Admin commands that mutate player ownership, kick players,
or change settings-controller lists still contain inherited arbitrator checks.

## Remaining Work

- Move kick/admin command ownership onto service-authority helpers.
- Move settings-controller mutation lists onto service-authority helpers.
- Drop the reserved internal slot after startup, sync, and live replication no
  longer require a player-backed authority index.
