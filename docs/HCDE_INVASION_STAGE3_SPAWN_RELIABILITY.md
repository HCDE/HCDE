# HCDE Invasion Stage 3: Spawn Reliability Pass

Stage 3 hardens live invasion spawning so one blocked spot does not stall an
entire spawn burst.

## What Stage 3 Adds

1. **Ready-spot filtering helper**
   - Spawn picks now pass through a shared readiness check:
     - honors per-spot next-ready tic
     - honors classic planned-count limits
2. **Burst-level multi-spot retry**
   - `Net_TryConsumeInvasionSpawnSlot()` now tries each currently-ready spot at
     most once per call.
   - If one random pick is blocked (telefrag safety / occupancy), the function
     keeps trying other ready spots before giving up.
3. **Blocked-spot backoff for all spot sources**
   - Classic spots keep spawn-delay pacing behavior.
   - Non-classic fallback spots now also get a short retry delay on failed spawn
     attempts, preventing per-tic hammering on blocked positions.
4. **No behavior change to wave semantics**
   - Wave budget, victory/failure rules, and query payload shape are unchanged.
   - This stage is reliability-focused and local to spawn consumption logic.

## Files Updated

- `src/d_net.cpp`

## Validation Target

- `hcdeserv` builds cleanly in the existing Release test build profile.
