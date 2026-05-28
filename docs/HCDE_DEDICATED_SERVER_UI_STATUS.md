# HCDE Dedicated Server UI Status

This note is the Step 16 execution checkpoint for the roadmap integration plan.
The `hcdeserv` UI cleanup is maintenance work: it changes operator controls, not
server runtime authority or gameplay packet lanes.

## Current Surfaces

- **Visible settings**: server setting controls auto-apply when a combo box
  changes or when an edit field loses focus. The per-row Apply buttons and
  Apply All button are no longer created.
- **Presets**: selecting a preset applies it immediately.
- **Advanced CVars**: advanced choice values apply on selection change; advanced
  text values apply when the edit field loses focus.
- **Explicit actions**: Start Game, Stop Server, Change Map, Restart, Broadcast,
  Kick, Refresh, and flag On/Off remain explicit buttons because they are actions
  rather than passive setting edits.

## Runtime Smoke Check

```text
hcde_maintenance_surfaces
```

The UI itself is Windows-only and is exercised by the `hcdeserv` target build.

## Step 16 Rules

- Keep dedicated-server UI changes in the Windows console UI/server-mode layer.
- Do not duplicate server state in the UI. Controls should emit the same console
  commands the old Apply buttons emitted.
- Do not alter gameplay authority, prediction, movement, snapshots, or
  authority-event lanes for UI convenience.
