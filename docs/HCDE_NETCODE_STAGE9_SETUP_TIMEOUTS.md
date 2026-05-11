# HCDE Netcode Stage 9 Setup Timeouts

Date: 2026-05-11

Stage 9 adds a hard timeout to retained HCDE pregame service messages. No
Odamex source was copied in this stage.

## What Changed

Retained `PRE_HCDE_SERVICE` messages now track their first send time. If a
required service message is still unacknowledged after 15 seconds, the host
logs the exact service, key, sequence, elapsed time, and send count.

The host then sends `PRE_SETUP_TIMEOUT` to that client, removes the client from
the setup roster, and continues with the remaining clients instead of waiting
forever.

## Covered Paths

- Pregame setup services, including console assignment, user info, map load,
  game info, peer user info, and setup ACKs.
- The Stage 8 start-game acknowledgement wait.
- Dedicated server mode and hidden launcher joins.

## Client Behavior

Clients receiving `PRE_SETUP_TIMEOUT` stop the join flow with:

`The server timed out during HCDE setup`

## Current Boundary

The timeout applies to the HCDE retained pregame service queue. It does not yet
replace the inherited live-game peer protocol with Odamex-style command and
snapshot traffic.

## Remaining Work

- Stage 10 added a live `HLIV` packet lane and diagnostic control frames.
- Move actual client command messages onto the live packet lane.
- Port or reimplement snapshot/player-state/actor-state replication.
- Move map transitions after startup onto the same server-authored service path.
- Replace the hidden dedicated server slot once gameplay authority no longer
  depends on the inherited peer setup model.
