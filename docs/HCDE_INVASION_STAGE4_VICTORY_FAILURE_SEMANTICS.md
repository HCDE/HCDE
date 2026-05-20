# HCDE Invasion Stage 4: Victory/Failure Semantics

Stage 4 makes invasion wave resolution deterministic and immediate when clear
conditions are already satisfied.

## What Stage 4 Adds

1. **Centralized wave-clear advancement**
   - Added `Net_AdvanceInvasionAfterWaveClear(...)` to route:
     - final wave clear -> `victory`
     - non-final wave clear -> `intermission`
2. **Immediate clear advancement in spawning**
   - If a wave has already spawned and cleared all budgeted monsters, invasion
     now advances immediately without waiting for cleanup timers.
3. **Immediate clear advancement in cleanup**
   - Cleanup no longer waits for timeout if clear conditions are already met.
4. **Timeout failure remains explicit**
   - Cleanup timeout still fails the wave only when clear conditions are not met
     by the time cleanup expires.

## Files Updated

- `src/d_net.cpp`

## Stage 4 Boundary

This stage adjusts state-machine semantics only. It does not change spawn pool
selection, query packet shape, UI layout, or external launcher protocol.
