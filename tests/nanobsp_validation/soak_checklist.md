# NanoBSP Soak Checklist

Roadmap board item: #4.

This checklist is inactive until the HCDE NanoBSP adapter can run the vendored
source against HCDE map data. It defines the data the comparison harness must
record before `hcde_nanobsp_loader=1` is enabled.

## Single-Player Soak

For each curated map:

- Record builder (`zdbsp-current` / `nanobsp`).
- Record subsector count.
- Record seg count.
- Record line-to-seg mapping hash.
- Run fixed line-of-sight sample grid and count mismatches.
- Record build time in milliseconds.
- Capture visible rendering notes, especially z-fighting around node splits.

Initial map set:

- Doom 2 `MAP01`
- Doom 2 `MAP07`
- Plutonia `MAP01`
- TNT `MAP01`
- One modern large-map fixture once selected

## Multiplayer Late-Join Soak

For each map and builder mode:

- Start dedicated server with the selected builder.
- Connect one client at map start.
- Connect one late-join client after actors/projectiles have spawned.
- Verify snapshot index agreement.
- Verify state hash agreement after join.
- Record late-join success/failure and disconnect reason.

Hard stop:

If NanoBSP produces different subsector ordering, seg mapping, or
line-of-sight results from the current builder on a map where saves/demos or
netgames depend on those indices, that map must stay on the existing builder.
