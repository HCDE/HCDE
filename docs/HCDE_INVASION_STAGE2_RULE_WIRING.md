# HCDE Invasion Stage 2: Gameplay Rule Wiring

Stage 2 hardens core gameplay-rule behavior for native Invasion mode
(`sv_gametype 4`) so active wave flow no longer depends on generic co-op timing
assumptions.

## What Stage 2 Adds

1. **Explicit active-round state gate**
   - Added a helper that defines active invasion round states:
     - `spawning`
     - `cleanup`
     - `intermission`
2. **Participant-aware round ownership**
   - In active round states, if there are no real participants (dedicated slot
     excluded), invasion now resets to `waiting` (`no-participants`) instead of
     running orphaned wave timers.
3. **Wipe failure rule**
   - Added living-participant counting (`PST_LIVE`, valid pawn, health > 0).
   - If all participants are dead during active round states, invasion now
     triggers failure (`all-players-dead`) after a short one-second grace window
     to avoid false failures on same-tic state transitions.
4. **Budget-count cleanup**
   - Participant counting now returns exact connected participant count.
   - Wave budget calculation preserves minimum one-player baseline internally.
5. **State reset hygiene**
   - Wipe grace timer is cleared whenever invasion leaves active round states or
     when invasion state is reset.

## Files Updated

- `src/d_net.cpp`

## Stage 2 Boundary

This stage is rule-wiring only. It does not alter monster pool composition,
spawn-point scan order, UI layout, or launcher query protocol shape.
