# HCDE Maintenance and Tooling Status

This note is the Step 7 execution checkpoint for the roadmap integration plan.
Maintenance work should stay separate from feature-port experiments and from
gameplay packet lanes.

## Current surfaces

- **Updater review**: `tools/verify-hcde-updater.ps1` verifies release asset
  selection, trusted update URLs, lock handling, backup creation, payload copy,
  rollback, and status logging.
- **Console/startup regressions**: HCDE already has console diagnostics in
  netcode commands plus startup/server-mode boundary diagnostics through
  `HCDE_ServerMode_PrintDiagnostics()`.
- **Dedicated server UI**: `hcde_servermode.*` owns the dedicated-server runtime
  boundary, including startup UI suppression, room UI suppression, authority
  settings control, and fatal guards if client UI is created by a dedicated
  server.
- **Launcher/netstart UI**: `networkpage.cpp` and `netstartwindow.cpp` display
  server query and room state. UI cleanup should stay in these widget layers.
- **RCON utility**: no dedicated RCON command channel was found in this slice.
  It should be added as admin tooling, not as a gameplay packet extension.

## Runtime smoke check

Use the console command:

```text
hcde_maintenance_surfaces
```

The command reports server UI suppression state, authority UI state, the updater
verification script, and the RCON boundary. It is diagnostic-only.

## Step 7 rules

- Keep updater/release verification outside engine runtime changes.
- Keep hcdeserv UI cleanup in server-mode and launcher/widget code; do not
  alter gameplay authority to support UI convenience.
- Remove redundant apply-style UI flows by wiring widgets to existing server
  runtime state transitions, not by duplicating server state.
- Build RCON as a narrow authenticated admin channel with explicit command
  allowlists and audit logging.
- Do not route RCON, updater, or launcher commands through movement, snapshot,
  authority-event, or gameplay packet lanes.
