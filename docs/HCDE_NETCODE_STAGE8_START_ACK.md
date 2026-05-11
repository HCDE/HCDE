# HCDE Netcode Stage 8 Start Acknowledgement

Date: 2026-05-11

Stage 8 adds a final pregame start acknowledgement for negotiated HCDE service
joins. No Odamex source was copied in this stage.

## What Changed

`PRE_HCDE_SERVICE` now includes `HPS_START_GAME_ACK`.

The server still sends `HPS_START_GAME` as a retained reliable service message,
but it no longer closes the pregame service window immediately after sending it.
Instead, it enters a short start-ack wait loop:

1. Flush the retained `HPS_START_GAME` message oldest-first.
2. Accept `HPS_START_GAME_ACK` from each service client.
3. Mark each service client as start-confirmed.
4. Leave pregame only after every connected service client has acknowledged.

Legacy non-service clients keep the old `PRE_GO` behavior.

## Client Behavior

When a service client receives `HPS_START_GAME`, it sends
`HPS_START_GAME_ACK` before entering the game. The ACK is queued through the
same retained service path and force-flushed once more immediately, giving the
server a duplicate packet with the same sequence if the first send is lost.

## Current Boundary

This stage confirms that each service client received the pregame start signal
before the host leaves pregame. Stage 9 adds timeout handling for retained setup
messages. It does not yet add a general live-game reliable channel. After the
client enters the game, regular `NCMD_SETUP` service packets are still not a
gameplay transport.

## Remaining Work

- Split live gameplay into server service messages and client command messages.
- Port or reimplement snapshot/player-state/actor-state replication.
- Move map transitions after startup onto the same server-authored service path.
- Replace the hidden dedicated server slot once gameplay authority no longer
  depends on the inherited peer setup model.
