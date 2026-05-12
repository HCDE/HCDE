# HCDE Netcode Stage 22 Dedicated Input Authority

Date: 2026-05-11

Stage 22 is the first dedicated-server authority slice after the Stage 21
server snapshot command records. No Odamex source was copied in this stage.

## What Changed

Dedicated HCDE hosts now enforce ownership on live `HCIR` client input records.
When the host uses the hidden dedicated server slot, each negotiated client may
only submit input records for its own assigned player slot.

Allowed dedicated-host input shapes are now:

- zero player records for an input heartbeat
- one player record whose player number equals the sending client slot

Any client input packet that tries to carry multiple player records or a record
for another player is rejected before the temporary inherited tic packet is
rebuilt for the current gameplay parser.

## Safety Rules

- This stage does not change the wire format or protocol versions.
- Non-dedicated legacy host games keep the existing peer-compatible behavior.
- Dedicated hosts still accept only negotiated HCDE live client-input packets.
- Rejected authority violations increment the live peer authority rejection
  counter and are logged through the net debug trace.

## Current Boundary

Stage 22 moves client input admission from peer-trusted records toward dedicated
server authority. The dedicated server still executes accepted commands through
the inherited tic parser, and clients still receive server snapshots that rebuild
temporary inherited packets.

## Remaining Work

- Stage 23 adds the first server-authored world-state delta lane.
- Add baseline repair and client reconciliation.
- Remove the hidden dedicated server player slot once live replication owns the
  game loop.
