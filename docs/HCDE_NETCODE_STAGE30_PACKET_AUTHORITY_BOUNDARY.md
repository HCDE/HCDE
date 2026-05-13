# HCDE Netcode Stage 30 Packet Authority Boundary

Date: 2026-05-12

Stage 30 moves the inherited runtime packet receive/start boundary onto HCDE
service-authority helpers. No Odamex source was copied in this stage.

## What Changed

The following paths now ask HCDE's service-authority identity helpers instead of
directly comparing against `Net_Arbitrator`:

- inherited packet-size calculation for host-authored packets
- disconnect forwarding and authority disconnect detection
- level-start ready/start coordination
- main packet receive handling for host-authored delay, latency, consistency,
  and sequence fields
- authority heartbeat/update gating in `NetUpdate`

The packet format is unchanged. This stage only names the authority boundary so
later stages can replace the inherited authority slot without changing every
packet parser again.

## Safety Rules

- This stage does not remove the inherited internal server slot.
- The deterministic tic stream still uses inherited packet layouts.
- Normal peer-host sessions still resolve service authority to the inherited
  host slot.
- Dedicated server sessions can answer local authority from HCDE server session
  state.

## Current Boundary

Stage 30 removes direct `Net_Arbitrator` checks from the packet-size,
level-start, disconnect, and receive-routing slice. Stage 31 moved packet
building and tic balancing onto the same service-authority boundary. Stats/debug
text, settings-controller lists, and host migration still contain inherited
arbitrator checks.

## Remaining Work

- Replace host migration and quit records with HCDE service authority records.
- Move stats/debug text and settings-controller lists onto service-authority
  helpers.
- Drop the reserved internal slot after startup, sync, and live replication no
  longer require a player-backed authority index.
