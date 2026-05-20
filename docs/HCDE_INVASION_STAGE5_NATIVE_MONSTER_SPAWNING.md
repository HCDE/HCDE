# HCDE Invasion Stage 5: Native Monster Spawning

Stage 5 finalizes server-owned invasion spawning so wave actors are created,
validated, and cleaned up by HCDE authority without ACS-owned spawn flow.

## What Stage 5 Adds

1. **Server-owned spawn path per invasion spot**
   - Wave spawning resolves a monster class from map/drop-list/classic pool
     rules and spawns it from HCDE authority code.
2. **Replacement-aware class resolution**
   - Spawn uses `ALLOW_REPLACE` so modded replacement chains still work.
3. **Spawn safety checks**
   - Player proximity telefrag guard blocks spawn attempts when players overlap
     the spawn radius envelope.
   - Spawn position validation rejects invalid placements and destroys failed
     spawn attempts immediately.
4. **Tracked active invasion actors**
   - Spawned invasion monsters are tracked in the wave director so clear
     progress is authoritative (`spawned - alive`).
5. **Deterministic cleanup ownership**
   - Added server-owned cleanup of tracked invasion actors on wave-reset and
     forced wave transitions so no stale actors bleed into new waves.

## Files Updated

- `src/d_net.cpp`

## Stage 5 Boundary

Stage 5 covers runtime spawning and cleanup ownership only. ACS/ZScript invasion
control surface is documented in Stage 6, and network snapshot replication in
Stage 7.
