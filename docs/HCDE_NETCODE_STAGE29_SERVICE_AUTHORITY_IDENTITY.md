# HCDE Netcode Stage 29 Service Authority Identity

Date: 2026-05-12

Stage 29 separates HCDE service authority identity from direct
`Net_Arbitrator` checks in the low-level service/live boundary. No Odamex
source was copied in this stage.

## What Changed

The low-level network layer now exposes service-authority helpers:

- `I_GetHCDEServiceAuthoritySlot`
- `I_IsLocalHCDEServiceAuthority`
- `I_IsHCDEServiceAuthoritySlot`
- `I_IsRemoteHCDEServiceAuthority`

The existing live authority helpers now delegate to the service-authority
helpers. Public reserved-slot mapping also asks the service-authority boundary
which internal slot is currently the authority instead of open-coding
`Net_Arbitrator`.

Dedicated join assignment validation now checks against
`I_GetFirstPlayableClientSlot()` so the service protocol no longer encodes
"playable clients must be greater than the arbitrator" at the call site.

## Safety Rules

- This stage does not remove the inherited internal server slot.
- Existing startup, sync, fallback packet parsing, and host migration branches
  still use `Net_Arbitrator` until they are migrated in later stages.
- Dedicated server authority can be local even when its authority state is not
  player-backed.
- Normal peer-host sessions still resolve service authority to the inherited
  host slot.

## Current Boundary

Stage 29 makes new HCDE service/live code ask an authority identity API instead
of directly learning the inherited arbitrator slot. Stage 30 then moves the
runtime packet-size, receive, disconnect, and level-start boundary onto that
identity API. The inherited sync packet format still uses the old slot index
internally.

## Remaining Work

- Stage 30 moved the packet-size, receive, disconnect, and level-start boundary
  onto HCDE service authority helpers.
- Move inherited packet building and tic balancing onto HCDE service
  identities.
- Replace host migration and quit handling with HCDE service authority records.
- Drop the reserved internal slot after the inherited sync loop no longer needs
  a player-backed authority index.
