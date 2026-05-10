# HCDE Golden Rules

These are project guardrails for HCDE licensing, source imports, Doom Connector
integration, and master-server integration. They are engineering rules for the
project, not legal advice. If a planned change violates one of these rules,
stop the work until there is a documented workaround.

## Hard Stop Rule

If a request would violate these rules, do not implement it.

Instead:

1. Name the rule being violated.
2. Explain the risk in plain language.
3. Propose one or more compliant workarounds.
4. Continue only after the workaround is chosen.

## License Rules

Current project license: HCDE is GPL-3.0-or-later.

1. HCDE engine binaries must only combine license-compatible code.

   GPL-3.0-or-later code can be combined with GPL-2.0-or-later code by using
   GPLv3 terms for the combined engine. GPL-2.0-only code must not be imported
   into the combined HCDE engine unless a separate license grant or replacement
   implementation is found.

2. Preserve upstream notices.

   Keep file headers, copyright notices, license texts, and attribution files
   for every imported source tree or subsystem.

3. Record provenance before integration.

   Every imported subsystem needs an upstream URL, commit hash, import scope,
   local path, license summary, and audit note before it can be wired into live
   engine code.

4. Source availability follows distribution.

   If HCDE distributes a GPL-covered binary, the corresponding source for that
   binary and local modifications must be made available under the applicable
   GPL terms.

5. No incompatible asset imports.

   Do not import packaged graphics, sounds, fonts, videos, soundfonts, models,
   or other assets until their licenses have been audited separately from code.

## Separation Rules

6. Doom Connector must remain a separate program from HCDE.

   Doom Connector may launch HCDE, query HCDE servers, download metadata, show
   compatibility tips, and display stats. It must not link against HCDE engine
   libraries or copy HCDE GPL implementation code.

7. The master server must remain a separate program from HCDE.

   The master server may receive heartbeats, answer server-list queries, store
   stats, and expose APIs. It must not link against HCDE engine libraries or
   copy HCDE GPL implementation code unless it is intentionally made GPL too.

8. Communicate at arm's length.

   HCDE, Doom Connector, and the master server should communicate through
   documented protocols: UDP queries, TCP/HTTP APIs, JSON, logs, database
   records, command-line arguments, or process exit codes.

9. Do not share implementation files across the boundary.

   Shared protocol documentation is allowed. Shared generated constants or
   schemas are allowed only when they are clearly protocol descriptions, not
   copied engine implementation.

10. HCDE must work without Doom Connector.

    Doom Connector can be the preferred launcher and stats client, but HCDE
    must remain runnable and usable without a proprietary or separately licensed
    companion application.

## Multiplayer And Stats Rules

11. Advanced stats are allowed through protocol data.

    HCDE servers may publish match events, player stats, mod manifests, version
    info, compatibility profiles, ping data, and server rules through documented
    network or log protocols. Doom Connector may collect and display that data
    as a separate application.

12. Stats must not require linking into the engine.

    Do not implement Doom Connector stats by embedding Connector code inside
    HCDE or embedding HCDE engine code inside Connector.

13. Server authority stays in HCDE.

    Doom Connector and the master server may coordinate discovery, stats, and
    metadata. They must not become hidden gameplay authority for live HCDE
    matches.

## Import Rules

14. Reference imports are not live engine code.

    Files under `imports/` are staging/reference material until explicitly wired
    into the build. Do not assume imported code is active just because it exists.

15. Local upstream checkouts are scratch.

    Full upstream checkouts under `upstream/` are local audit scratch unless
    tracked intentionally. Curated imports belong under `imports/`.

16. Prefer neutral adapters first.

    New source-port features should first enter HCDE as policy, inventory,
    parser, manifest, or merge layers before mutating live actor, state, sound,
    renderer, netcode, or scripting registries.

17. No silent last-writer-wins.

    When ZScript, ACS, EDF, DDF, COAL, Lua, DECORATE/ThingDef, DeHackEd, or
    native code define overlapping behavior, HCDE must report a merge decision
    or conflict. It must not silently let whichever layer loaded last win.

## Current Boundary Decision

HCDE is the open-source engine and dedicated server.

Doom Connector is the separate launcher, server browser, metadata client, and
advanced stats interface.

The master server is a separate discovery and stats service.

This boundary is intentional. Do not blur it without a new documented licensing
and architecture review.

## Sources For These Rules

- GNU GPL FAQ: https://www.gnu.org/licenses/gpl-faq.en.html
- GPLv2 FAQ: https://www.gnu.org/licenses/old-licenses/gpl-2.0-faq.en.html
- Legacy upstream source and license reference: https://github.com/odamex/odamex
