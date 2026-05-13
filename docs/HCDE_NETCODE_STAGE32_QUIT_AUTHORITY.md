# HCDE Netcode Stage 32 Quit Authority

Date: 2026-05-12

Stage 32 moves inherited quit and host-migration packet emission onto HCDE
service-authority helpers. No Odamex source was copied in this stage.

## What Changed

The following paths now ask HCDE's service-authority identity helpers instead
of directly comparing against `Net_Arbitrator`:

- local authority detection in `D_QuitNetGame`
- selecting the next authority slot when the current authority exits
- broadcasting authority exit packets to non-authority peers
- sending client exit packets to the current service authority

The quit packet format is unchanged. Authority exit packets still carry one
replacement slot byte, and client exit packets remain one byte.

## Safety Rules

- This stage does not remove the inherited internal server slot.
- The deterministic tic stream still uses inherited packet layouts.
- Dedicated server sessions skip the reserved server slot when selecting a
  replacement authority.
- Normal peer-host sessions still resolve service authority to the inherited
  host slot.
- Doom Connector launch arguments do not change.

## Current Boundary

Stage 32 removes direct `Net_Arbitrator` checks from the local quit-packet
emission path. Stage 33 moved read-only status text, cutscene ready ownership,
and ping visibility onto the same service-authority boundary. Settings-controller
mutation lists and some admin commands still contain inherited arbitrator
checks.

## Remaining Work

- Move kick/admin command ownership onto service-authority helpers.
- Move settings-controller mutation lists onto service-authority helpers.
- Drop the reserved internal slot after startup, sync, and live replication no
  longer require a player-backed authority index.
