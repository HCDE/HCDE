# HCDE Netcode Stage 27 Live Route Authority

Date: 2026-05-12

Stage 27 moves HCDE live packet routing behind explicit authority-route helper
APIs. No Odamex source was copied in this stage.

## What Changed

The live `HLIV` control, client-input, and server-snapshot lanes now ask the
low-level network layer whether a packet should be sent or accepted:

- `I_GetHCDELiveAuthoritySlot`
- `I_IsLocalHCDELiveAuthority`
- `I_IsHCDELiveAuthoritySlot`
- `I_ShouldSendHCDELiveControlTo`
- `I_ShouldSendHCDELiveClientInputTo`
- `I_ShouldSendHCDELiveServerSnapshotTo`
- `I_ShouldAcceptHCDELiveClientInputFrom`
- `I_ShouldAcceptHCDELiveServerSnapshotFrom`

Before this stage, the live layer decided direction by directly checking
`consoleplayer == Net_Arbitrator` or `client == Net_Arbitrator`. Those checks
made every later migration rediscover the inherited arbitrator slot. Stage 27
keeps the current internal authority slot but moves that detail into one route
boundary.

## Safety Rules

- This stage does not change the wire format or protocol versions.
- The current authority slot remains the inherited internal arbitrator until
  Stage 28 replaces server-owned player state.
- Non-authority clients may only send live client input toward the authority.
- The authority may only accept live client input from non-authority peers.
- The authority may only send live server snapshots to non-authority peers.
- Non-authority clients may only accept live server snapshots from the
  authority route.

## Current Boundary

Stage 27 removes direct arbitrator checks from HCDE live packet routing. Stage
28 moved dedicated-server-owned settings authority state into HCDE server
session state, and Stage 29 makes the live route resolve through the HCDE
service-authority identity boundary. Startup sync and fallback packet parsing
still depend on the old internal authority slot.

## Remaining Work

- Stage 28 replaced server-owned settings authority state with non-player
  server session state.
- Stage 29 added a service-authority identity boundary for HCDE service/live
  routing.
- Drop the reserved internal slot once startup, sync, host migration, and live
  replication no longer require a player-backed authority.
- Apply live reconciliation mutations after inherited lockstep consistency is
  retired.
