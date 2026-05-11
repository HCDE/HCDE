# HCDE Master Boundary Stage 1 Audit

Date: 2026-05-11

Scope: Stage 1 audit for the HCDE master-server boundary before adding or
expanding Doom Connector master-browser support.

This document is an engineering boundary check. It does not replace the golden
rules, and it is not legal advice.

## Golden Rule Baseline

The relevant rules are:

- Doom Connector remains a separate program from HCDE.
- The master server remains a separate program from HCDE.
- HCDE, Doom Connector, and the master server communicate through documented
  protocols.
- Shared protocol documentation, neutral schemas, and generated constants are
  allowed only when they describe the wire protocol, not implementation.

## Current Components

| Component | Location | Boundary status |
|-----------|----------|-----------------|
| HCDE server advertiser | `src/common/engine/sv_master.cpp` | Engine-side UDP client that sends documented master heartbeats. |
| HCDE master server | `tools/hcdemaster/hcdemaster.cpp` | Standalone process; no HCDE engine headers or libraries. |
| Master protocol | `docs/HCDE_MASTER_PROTOCOL.md` | Approved shared protocol reference. |
| Neutral protocol data | `protocol/hcde_master_protocol.json` and `protocol/hcde_master_protocol.h` | Protocol facts and generated constants only. |
| Doom Connector | External repository/application | May implement the documented protocol independently. |

## Allowed Sharing

These are protocol facts and may be shared with Doom Connector:

- Master hostnames and ports.
- Packet marker values.
- Packet field layouts, sizes, byte order, and validation rules.
- Entry time-to-live meaning.
- Text documentation and neutral schema files.
- Generated constants from a neutral protocol schema, if the generator output
  is limited to protocol data.

## Forbidden Coupling

These would violate the project boundary:

- Linking Doom Connector against HCDE engine libraries.
- Copying `src/common/engine/sv_master.cpp` into Doom Connector.
- Copying `tools/hcdemaster/hcdemaster.cpp` socket loops, storage, threading,
  or process logic into Doom Connector.
- Linking `hcdemaster` against HCDE engine libraries.
- Embedding Doom Connector code inside HCDE or `hcdemaster`.
- Treating the master server as live gameplay authority.

## Audit Findings

- `tools/hcdemaster/CMakeLists.txt` builds `hcdemaster` as its own executable.
- On Windows, `hcdemaster` links only `ws2_32`; it does not link HCDE engine
  targets.
- `tools/hcdemaster/hcdemaster.cpp` uses standard library and platform socket
  headers only. It does not include HCDE engine headers.
- `src/common/engine/sv_master.cpp` remains inside the HCDE engine and only
  sends heartbeat packets to configured masters when `sv_usemasters` is enabled.
- `docs/HCDE_MASTER_PROTOCOL.md` contains the current allowed wire contract.
- `tools/package-hcde-prerelease.ps1` ships the golden rules and master protocol
  docs with release packages.

Local Doom Connector check:

- `C:\Users\User\DoomConnector6` is currently an empty directory.
- `C:\Users\User\DoomConnector6-from-gitea-latest` has an HCDE engine profile
  with `masterserver` disabled and no local implementation found for the HCDE
  master packet markers `5560020` or `777123`.
- `C:\Users\User\DoomConnector6-gitea-clone` does not currently include the
  HCDE engine profile in the same way and no HCDE master implementation was
  found in the searched source files.
- `C:\Users\User\Desktop\client` has Zandronum master-query code only; no HCDE
  master implementation was found in the searched source files.

## Stage 2 Gate

Before Doom Connector gains HCDE public-list browsing, use
`docs/HCDE_MASTER_PROTOCOL.md` and `protocol/hcde_master_protocol.json` as the
source of truth and implement the query side independently in Doom Connector. If
constants are shared, generate them from the neutral protocol schema or copy
only the literal protocol facts, not engine or master-server implementation
code.
