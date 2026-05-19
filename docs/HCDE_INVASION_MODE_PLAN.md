# HCDE Native Invasion Mode Plan

NetVibe and Doom Connector can expose Invasion as a real HCDE game mode once the
engine has a native mode behind `sv_gametype 4`. Until the wave system is wired,
`sv_gametype 4` keeps cooperative gameplay rules so existing maps and servers do
not break.

## Stage 1 - Mode Identity And Reporting

`sv_gametype 4` is named Invasion everywhere user-visible. Server GUI choices,
launcher query snapshots, and host/session summaries report Invasion instead of
the old Horde placeholder. Query packets append the game-mode id and display name
after the legacy fields so older clients can keep reading the packet shape they
already know.

## Stage 2 - Invasion State Machine

Add a native state machine for waiting, countdown, spawning, cleanup, intermission,
victory, and failure. This should be server-authoritative and should not depend on
ACS scripts to decide the active wave state.

## Stage 3 - Wave Director

Define wave records, monster budgets, spawn pacing, boss waves, escalation rules,
and end conditions. The director should be deterministic on the server and expose
enough status for clients, HUDs, and scripts.

## Stage 4 - Invasion Spawn Points

Promote invasion spawn-point support from compatibility shims into native map
logic. Support classic invasion spot classes, map-defined tags, delayed activation,
and fallback behavior when a map has no invasion-specific spots.

## Stage 5 - Native Monster Spawning

Create server-owned monster spawning, replacement selection, spawn validation,
telefrag/safety handling, and cleanup. Spawning must work in dedicated-server mode
and must replicate correctly to clients.

## Stage 6 - ACS And ZScript API

Expose read-only and controlled write APIs such as current wave, total waves,
active monster count, invasion state, remaining countdown, and wave control hooks.
Legacy mod compatibility should be kept where practical.

## Stage 7 - Networking

Replicate invasion state, wave changes, countdowns, boss flags, and victory/failure
events through the HCDE netcode. Late joiners need an immediate authoritative
snapshot so the UI and gameplay state match the server.

## Stage 8 - UI And Server Tools

Add launcher/server controls for Invasion settings, show live wave state in server
queries, and expose the mode cleanly to NetVibe. Server admins should be able to
configure countdowns, limits, and wave definitions without touching source code.

## Stage 9 - Testing And Compatibility

Test co-op, deathmatch, team deathmatch, CTF fallback, and Invasion so the new mode
does not regress existing multiplayer. Include dedicated-server, launcher query,
late join, map transition, and old invasion-map compatibility cases.

## Stage 10 - Full Documentation

Document how Invasion works for players, server admins, mappers, and modders. This
should cover cvars, map requirements, spawn-point behavior, ACS/ZScript APIs, wave
definition format, dedicated-server hosting, NetVibe integration, and migration
notes for existing invasion-style mods.

Stage 10 reference:

- `docs/HCDE_INVASION_STAGE10_FULL_DOCUMENTATION.md`
