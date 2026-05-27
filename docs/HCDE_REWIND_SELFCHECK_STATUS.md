# HCDE Rewind Self-Check Status

This note records the next hardening slice after the roadmap Step 2 rewind
checkpoint. The goal is to prove that HCDE can enter a historical rewind state
and return to live authority state without changing the serialized world
fingerprint.

## Runtime command

Use the console command:

```text
net_rewind_selfcheck [index]
```

If `index` is omitted or negative, the newest stored keyframe is used. The
command:

- captures the current live authority state
- restores the selected historical keyframe
- restores the captured live state
- recaptures live state
- compares level, globals, and combined CRC fingerprints

The command is diagnostic-only. It does not enable rewind by default, does not
run weapon logic, and does not replay damage.

## Why this matters

Lag compensation and future rewind tooling depend on a safe restore bracket. If
the before/after live CRCs differ, HCDE has evidence that the serializer restore
path is not identity-preserving for the current map/session and rewind should
not be promoted further.

## Boundary

- `net_rewind_selfcheck` requires the local authority and a loaded level.
- It uses existing savegame-backed rewind buffers.
- It restores the capture timer after the probe so normal keyframe cadence is
  not disturbed.
- If restoring the live state fails after a historical restore, the command logs
  a loud `net.rewind` warning because the world may still be in the past.
