# HCDE Invasion Stage 7: Networking

Stage 7 wires invasion runtime state into HCDE live server-snapshot transport so
clients and late joiners get authoritative wave/counter state immediately.

## What This Stage Adds

1. A dedicated invasion snapshot block (`HCIV`) is appended to each wrapped
   HCDE server snapshot payload.
2. Snapshot decode applies invasion state client-side before command playback.
3. Late joiners receive invasion state on their first authoritative snapshot.

## `HCIV` Snapshot Payload

Fields:

- protocol version
- flags (boss-wave bit)
- invasion state enum
- state tics (countdown/phase remaining)
- wave index
- max waves
- wave budget
- wave spawned
- wave cleared
- active monsters

## Client Application Rules

- On receipt, values are clamped to safe ranges and copied into the local
  invasion runtime state.
- Active monster actor pointers are not reconstructed from the network payload;
  the replicated active-monster count is tracked as a numeric mirror.
- `Net_GetInvasionActiveMonsterCount()` returns replicated count on non-authority
  peers and authoritative tracked actors on the authority.

## Compatibility Notes

- The invasion payload is optional on decode; snapshots without `HCIV` still
  parse through the world-delta section.
- New snapshots always include `HCIV` so invasion UI/API calls can reflect
  server state without ACS polling hacks.
