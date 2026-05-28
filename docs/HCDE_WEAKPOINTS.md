# HCDE Weak Points and Cleanup Notes

**Date:** 2026-05-24

This document collects the main code and playability weak points identified during the review of the HCDE repository. It is intended as a focused cleanup checklist.

## 1. Network / Multiplayer Weak Points

1. `docs/NETCODE_REVIEW.md`
   - explicitly calls out a potential buffer overflow and UI freezes under high-pressure scenarios.
   - notes inconsistent logging between `DebugTrace::Markf()` and `Printf()`.

2. `docs/HCDE_NETCODE_OVERHAUL.md`
   - the multiplayer netcode is still under active overhaul.
   - many live protocols and actor/authority lanes are being reworked, making multiplayer reliability a natural weak point.
   - complex features include lane budgets, late-join repair windows, actor delta streams, unlagged validation, and server-side simulation LOD.

3. `src/d_net.cpp`
   - contains numerous desync warnings and packet validation paths.
   - has explicit warnings for malformed packets, oversized payloads, and consistency mismatches.
   - the netcode is clearly a high-risk area because it is large, stateful, and sensitive to malformed or lossy input.

4. `src/i_net.cpp` (referenced by `docs/NETCODE_REVIEW.md`)
   - `HandleIncomingConnection()` was flagged for shared-state access and possible race conditions.
   - `GetPacket()` was flagged for unsafe buffer handling and malformed packet risk.
   - `DriveRuntimeSetupStateForClient()` and connection setup queues were flagged for saturation issues.
   - `Host_CheckForConnections()` was flagged for callback/state-modification complexity.

5. Session token / packet parsing logic
   - session token mismatch handling is currently simple reject logic.
     * No retry/backoff strategy for transient token failures.
     * No token refresh or rotation mechanism for degraded connections.
     * Immediate disconnect on mismatch without graceful degradation.
   - packet parsing sometimes returns `false` with inconsistent recovery behavior across callers.
     * Callers may not check return value consistently.
     * No centralized error recovery or fallback parsing mode.
     * Silent failures can lead to state desync without user awareness.
   - buffer overflow risks in packet header parsing.
     * Fixed-size buffers may overflow on oversized headers.
     * No bounds checking on parsed field lengths.
   - malformed packet handling lacks comprehensive validation.
     * Missing validation for corrupted checksums or version mismatches.
     * No packet integrity verification before parsing.
   - these are weak points for robustness during hostile or broken network traffic.
     * Need explicit retry/backoff for session token failures.
     * Need consistent error recovery paths across all callers.
     * Need telemetry for parsing failure rates and recovery success.
     * Need graceful degradation modes for hostile network conditions.

## 2. Legacy / Unsafe / Hacky Code

1. `src/p_setup.cpp`
   - contains the comment: `// horribly hacky - yes, this needs rewritten.`
   - indicates a likely fragile and hard-to-maintain path in player spawn and deathmatch setup.

2. `src/d_main.cpp`
   - `LightDefaults.DeleteAndClear();` is marked as leaking heap memory if not cleared.
   - `UNSAFE_CCMD(debug_restart)` and other unsafe commands appear in game restart/restart paths.

3. `src/console/c_cmds.cpp`
   - multiple `UNSAFE_CCMD` declarations for commands such as `load`, `save`, `playdemo`, `timedemo`.
   - suggests direct command execution is allowed in unsafe or debug-only contexts.

4. Broad `assert()` usage
   - many engine files rely on assertions for invariants rather than runtime recovery.
   - `src/common/utility/zstring.*`, `src/common/utility/tarray.h`, `src/maploader/udmf.cpp`, and others contain assert-heavy logic.
   - this is a weak point for release stability if invalid data reaches those checks.

5. Bundled third-party code and compatibility surfaces
   - the repo includes bundled libraries such as ZWidget, VPX, and others.
   - older engine subsystems and platform-specific compatibility code expand the failure surface.

## 3. Playability / User Experience Weak Points

1. Multiplayer join/start flow complexity
   - separate `hcde`, `hcdeserv`, and `hcdemaster` process model is powerful but more failure-prone.
   - launcher/master-server integration adds more moving parts to the user flow.

2. Late join / reconnect behavior
   - the docs identify late join as a torture test.
   - current repair and catchup logic is complex and likely to be a source of playability bugs.

3. High-ping / desync handling
   - `docs/HCDE_NETCODE_OVERHAUL.md` emphasizes unlagged player history and high-ping audits.
   - these are usability-critical features, but they also mean the gameplay experience can break subtly if not perfectly tuned.

4. Mod compatibility risk
   - many `COMPATF_*` flags in `src/doomdef.h` and the long [CVAR Reference wiki](https://github.com/bokoxthexchocobo/HCDE/wiki/CVAR-Reference) show a lot of compatibility overrides.
   - support for legacy Doom/Heretic/Hexen behavior means a wider range of edge cases and inconsistent gameplay across mods.

## 4. Recommended Cleanup Actions

1. Prioritize stabilizing netcode handling and packet safety.
   - harden `src/i_net.cpp` and `src/d_net.cpp` around malformed input.
   - standardize error handling and recovery paths.

2. Audit and reduce unsafe/hacky paths.
   - rewrite or refactor `src/p_setup.cpp` spawn logic.
   - centralize and document unsafe command behavior in `src/console/c_cmds.cpp`.

3. Add telemetry and diagnostic consistency.
   - normalize logging style across network code.
   - add explicit retry/backoff for session token failures.

4. Document critical netcode invariants.
   - expand comments in `src/i_net.cpp` and packet-handling functions.
   - add a small architecture note in `docs/NETCODE_REVIEW.md` or a companion doc for future reviewers.

5. Flag high-risk gameplay scenarios for testing.
   - late join / reconnect
   - invasion/projectile-heavy scenes
   - high latency / unlagged mode
   - custom mod compatibility on dedicated servers

## 5. File References for Cleanup

- `docs/NETCODE_REVIEW.md`
- `docs/HCDE_NETCODE_OVERHAUL.md`
- [CVAR Reference (wiki)](https://github.com/bokoxthexchocobo/HCDE/wiki/CVAR-Reference)
- `src/d_net.cpp`
- `src/i_net.cpp`
- `src/p_setup.cpp`
- `src/d_main.cpp`
- `src/console/c_cmds.cpp`
- `src/doomdef.h`

---

This note is intentionally concise and targeted. Use it as the starting checklist for cleanup and stabilization work.