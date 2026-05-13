# HCDE Netcode Stage 31 Packet Build Authority

Date: 2026-05-12

Stage 31 moves inherited tic balancing and packet construction onto HCDE
service-authority helpers. No Odamex source was copied in this stage.

## What Changed

The following runtime paths now ask HCDE's service-authority identity helpers
instead of directly comparing against `Net_Arbitrator`:

- timeout retransmit marking in `Net_UpdateStatus`
- host-side tic pacing and `CF_UPDATED` balancing in `Net_UpdateStatus`
- host-side start/end sequence selection in the packet builder
- client packet target filtering for authority-directed sends
- consistency-count emission for authority-authored packets
- stability byte and latency-field emission for authority-authored packets

The packet format is unchanged. This stage only makes packet construction use
the same authority identity as the receive/start boundary from Stage 30.

## Safety Rules

- This stage does not remove the inherited internal server slot.
- The deterministic tic stream still uses inherited packet layouts.
- Normal peer-host sessions still resolve service authority to the inherited
  host slot.
- Dedicated server sessions can answer local authority from HCDE server session
  state.
- Doom Connector launch arguments do not change.

## Current Boundary

Stage 31 removes direct `Net_Arbitrator` checks from the packet-build and
tic-balance slice. Stage 32 moved local quit and host-migration packet emission
onto the same service-authority boundary. Stats/debug text,
settings-controller lists, cutscene ready host selection, and some admin
commands still contain inherited arbitrator checks.

## Remaining Work

- Move stats/debug text and settings-controller lists onto service-authority
  helpers.
- Move cutscene-ready host labels and admin command ownership onto
  service-authority helpers.
- Drop the reserved internal slot after startup, sync, and live replication no
  longer require a player-backed authority index.
