# HCDE Replication Guard Status

This note is the Step 15 execution checkpoint for the roadmap integration plan.
The defensive pass hardens shared and invasion actor compaction against stale or
already-retiring actor references.

## Current Surfaces

- **Shared actor compaction**: `Net_CompactHCDEReplicatedActors()` already
  resolves `TObjPtr::Get()` before actor-field access and preserves recent
  baseline-only records long enough for client reconciliation.
- **Invasion actor compaction**: invasion mirror compaction now uses live-actor
  accessors before reading `ObjectFlags`, health, class, or velocity. Stale
  indexes are purged when their stored pointer no longer resolves to a live
  actor.
- **Diagnostics**: `hcde_replication_guard_surfaces` forces a compact pass and
  reports tracked invasion/shared counts plus guard counters for null refs,
  euthanized refs, and stale index purges.

## Runtime Smoke Check

```text
hcde_replication_guard_surfaces
```

The command should run safely with or without an active invasion round.

## Step 15 Rules

- Never read `ObjectFlags` or other actor fields from a replicated actor record
  until the `TObjPtr::Get()` result has been checked.
- Removing stale invasion records must also rebuild or purge pointer/id indexes.
- Shared baseline-only records may briefly have no local actor; preserve those
  only through the established reconciliation window.
