# HCDE RCON Status

This note is the Step 17 execution checkpoint for the roadmap integration plan.
RCON is maintenance/admin tooling and must stay outside gameplay authority,
prediction, and snapshot lanes.

## Current Surfaces

- **Server knobs**:
  - `sv_rcon_password` enables RCON when non-empty.
  - `sv_rcon_allow_remote` gates non-loopback admin packets.
  - `sv_rcon_allowlist` controls which console command tokens may be queued.
- **Utility**: `hcdercon <host[:port]> <password> <command...>`
- **Diagnostics**: `hcde_maintenance_surfaces` reports enabled/remote/stats state,
  including a `replayed=` counter for packets rejected by the nonce replay ring.
- **Replay protection**: every accepted (sender,nonce) pair is recorded in a
  small in-process ring; a duplicate request returns `StatusRejected` rather
  than re-queueing the command. The ring is process-lifetime; restarting the
  server clears it.

## Runtime Smoke Check

Start a dedicated server with a password, then run:

```text
hcdercon 127.0.0.1 <password> hcde_maintenance_surfaces
```

Expected response:

```text
status=queued message=RCON command queued
```

Then confirm the server console/log shows the maintenance surface output and
`hcde_maintenance_surfaces` reports `rcon: enabled=1`.

## Step 17 Rules

- Keep RCON on the UDP game port as a separate authenticated packet family.
- Queue only allowlisted console commands; reject `quit`/`exit` and unknown tokens.
- Do not route RCON through movement, snapshot, authority-event, or client-input lanes.
- Default to localhost-only admin access unless `sv_rcon_allow_remote` is enabled.
- Reject any (sender,nonce) pair that has already been accepted; the client
  must pick a fresh nonce per command.
