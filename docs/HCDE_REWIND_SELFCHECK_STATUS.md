# HCDE Rewind Self-Check Status

This note records the next hardening slice after the roadmap Step 2 rewind
checkpoint. The goal is to prove that HCDE can enter a historical rewind state
and return to live authority state without changing the serialized world
fingerprint.

## Runtime command

Use the console command:

```text
net_rewind_selfcheck [index]
net_rewind_selfcheck_sweep [count] [max_cost_ms] [max_memory_mb] [min_age_tics]
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

`net_rewind_selfcheck_sweep` repeats the same identity-preserving restore check
against the newest stored keyframes and prints a pass/fail line suitable for
Step 6 smoke runs. The optional thresholds default to:

- `count`: newest 3 stored keyframes, or fewer if the ring has not filled yet.
- `max_cost_ms`: 250 ms maximum observed restore/selfcheck cost.
- `max_memory_mb`: current `net_rewind_max_mb`.
- `min_age_tics`: 35 tics. Very fresh frames are skipped by default so the
  sweep validates the stable rewind history window instead of in-flight
  dedicated/client start or teardown transients.

The sweep result only passes when every checked keyframe restores cleanly, the
observed max selfcheck cost stays below the threshold, and the ring buffer stays
within the memory threshold. It also fails if there are not enough keyframes at
or beyond `min_age_tics` to satisfy `count`.

## Step 6 baseline

Known-good local smoke target:

```text
map: MAP01 (DOOM2.WAD)
settings: net_rewind_enable=1, net_rewind_interval=1.0,
          net_rewind_depth=10, net_rewind_max_mb=32
command: net_rewind_selfcheck_sweep 3 250 32
expected: result=pass, fail=0, memory-threshold=ok
observed: pass=3, fail=0, max-cost=24ms, memory=0.11/32.00 MB
```

Run the representative smoke with the synthetic client still connected. The
short `+wait ... +quit` join probe exits almost immediately on this release
build and can leave first-bracket disconnect cleanup state that is useful to
debug separately, but it is not the Step 6 known-good rewind baseline.

Representative mod/map baselines should use the same command and store their
own expected count/cost/memory thresholds here once those assets are selected.

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
