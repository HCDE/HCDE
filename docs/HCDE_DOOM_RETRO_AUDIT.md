# HCDE Roadmap #8 — Doom Retro Physics Scoping

**Last updated:** 2026-05-28
**Status:** Scoping. No code lands as part of this audit; this document
defines the integration boundary so future PRs are bounded.

## What #8 actually means

Board item: "Doom Retro physics." Doom Retro is a Doom-faithful port
that ships with several physics tweaks (more accurate liquid friction,
fixed crusher / lift edge cases, smoother screen kick on damage, etc.).
The integration goal is to bring **specific** Doom-Retro physics
correctness fixes into HCDE — not to import all of Doom Retro's
behaviour wholesale.

## Why this is the highest-risk item on the roadmap

#8 touches **playsim**. Playsim changes are the most expensive thing we
can do because:

1. **Multiplayer determinism.** All clients run the same playsim. A
   physics tweak that changes a single floor friction value will change
   what each client computes and trigger desync (or, more subtly, change
   the snapshot stream so we can't replay old demos).
2. **Demo compatibility.** All shipped demos and saves were recorded
   under the current physics. Any change either:
     - silently breaks them (unacceptable), or
     - is gated behind a compat flag and the gating is wired into demo
       playback so old demos load with the old physics.
3. **Mod compatibility.** Custom maps and mods exploit physics
   quirks (the "boom slime trail", BSP edge cases, etc.). A "fix" that
   changes a quirk used by a popular megawad is a regression for that
   megawad.

For these reasons, **every** Doom-Retro tweak we accept under #8 must
ship with a compat flag and demo-version pinning.

## Existing in-tree surface

- **Compat flag system.** `compatflags` and `compatflags2` already exist
  with bit flags like `COMPATF_BOOMSCROLL`, `COMPATF_INVISIBILITY`, etc.
  Each Doom-Retro tweak gets a new bit (or piggybacks on an existing
  bit if semantically equivalent).
- **Demo version pinning.** `DEMOGAMEVERSION` controls how demos are
  read. Any new physics gate that demos depend on must bump or extend
  this.
- **MAPINFO.** Maps can opt into compat behaviours per-map. Doom-Retro
  tweaks that are map-pack specific should be opt-in via MAPINFO not via
  user CVARs.
- **Playsim hot files.** `src/playsim/p_map.cpp`, `p_user.cpp`,
  `p_mobj.cpp`, `p_local.h`, `p_floor.cpp`, etc.

## Hard rules (playsim discipline)

These are non-negotiable for any #8 PR:

1. **Default off.** Every imported tweak ships behind a compat flag
   defaulted to "vanilla behaviour". Operators opt in.
2. **Demo-aware gating.** Demos record the compat flag state. On
   playback, the recorded flags win — not the player's current settings.
   (This is already how `compatflags` works; #8 must not introduce a new
   path that bypasses it.)
3. **Server-controlled in netgames.** In multiplayer the server CVAR is
   authoritative; clients adopt it on join. A client whose flags
   disagree must either flip to match or refuse to join. Same model as
   existing compat handling.
4. **Sectioned PRs.** One physics tweak per PR. Do not bundle. A bundled
   PR is unmergeable because if any one tweak regresses a megawad we
   have to revert the whole thing.
5. **Documented motivation per tweak.** Each compat flag's commit body
   must include:
     - The Doom-Retro source link or commit
     - The vanilla behaviour
     - The new behaviour
     - At least one specific case where the new behaviour is observably
       better
     - Any megawad we know to be sensitive to the change
6. **No silent floating-point swaps.** Many "physics fixes" are
   double-vs-fixed-point conversions. Those changes ripple unpredictably
   through every collision check. Any precision change must be benched
   on at least one large megawad demo and its result diff'd; if there's
   any divergence, the change either ships behind a flag with the
   matching behaviour preserved by default, or is rejected.

## Suggested first imports (smallest first)

Three candidates, ordered by risk:

### Candidate 1 — view-kick smoothing on damage (lowest risk)

Doom Retro smooths the camera kick the player sees when taking damage.
This is **rendering-only** if we extract it carefully — the underlying
`damagecount` / pain timer is unchanged. Implement as a new CVAR
`r_view_pain_smooth` (default off), applied only on the rendering side
in `r_main.cpp`/the view setup. This belongs more to #11 / #9 than #8,
but mention it here because it's the gentlest first PR.

### Candidate 2 — crusher edge-case fix (medium risk)

Doom Retro fixes a long-standing case where a crusher leaves a corpse
glued to the wall. Vanilla behaviour is a known quirk; the fix is small
and demo/save-safe (no field changes, only the corpse-gluing branch).
Gated behind a new `COMPATF2_DR_CRUSHER` (or equivalent) flag, default
off.

### Candidate 3 — liquid friction tweak (high risk; ship last)

Doom Retro adjusts friction in deep water. This **does** change movement
physics, so it requires a compat flag, demo-version awareness, and
multiplayer gating. Skip for the first PR; ship after the compat flag
infrastructure for #8 has been validated by Candidates 1 and 2.

## Phased plan

- **Phase 0 (this doc).** Define the boundary.
- **Phase 1.** Add compat flag namespace `COMPATF2_DR_*` + serializer
  hooks in `g_level.cpp`/`p_setup.cpp` so demos and saves can store the
  new flags. No tweaks land yet — just plumbing. **Bit plumbing landed
  2026-05-28:** `COMPATF2_DR_CRUSHER`,
  `COMPATF2_DR_LIQUIDFRICTION`, `compat_dr_crusher`, and
  `compat_dr_liquidfriction` exist, default off.
- **Phase 2.** Candidate 2 (crusher) imported under `COMPATF2_DR_CRUSHER`.
  Soak: vanilla demos still play with the bit clear; megawad demos with
  known crusher use cases are tested.
- **Phase 3.** Multiplayer soak with the flag set on a server; verify
  late-join works.
- **Phase 4.** Candidate 1 (pain view kick) — but as `r_view_pain_smooth`
  on the rendering side, not as a compat flag (since it's
  presentation-only). **CVAR scaffold landed 2026-05-28** in
  `src/r_view_pain_smooth.cpp`; the presentation path is not wired yet.
- **Phase 5+.** Candidate 3 and beyond, one PR each.

## What this is NOT

- Not "make HCDE play like Doom Retro by default". Defaults stay at
  current HCDE/GZDoom behaviour. Operators opt in.
- Not a license to drop in floating-point math everywhere. Fixed-point
  vs. double semantics are tracked per change.
- Not a path to fast-tracked physics changes that bypass the compat
  flag system.

## Source map (where Phase 1+ code goes)

| Concern | File |
| ------- | ---- |
| Compat flag definitions | `src/g_level.h` (extend `compatflags2` enum) |
| Compat flag CVAR + serializer | `src/g_level.cpp` |
| Demo version pin | `src/d_protocol.h` (`DEMOGAMEVERSION`) |
| Playsim tweak sites | `src/playsim/p_floor.cpp`, `p_map.cpp`, `p_mobj.cpp` (case by case) |
| View-kick smoothing | `src/r_main.cpp` (presentation-only path) |

## Open questions (for the implementing PR)

1. Should Doom Retro tweaks share a single "DR profile" CVAR (set all
   bits at once) or stay strictly per-bit? Per-bit is safer; a profile
   CVAR can compose them later if useful.
2. Demo backwards-compat: if we extend `compatflags2`, do we need to
   bump `DEMOGAMEVERSION`? (Probably yes for any tweak that changes
   playsim outputs; possibly no for presentation-only tweaks.)
3. Is there a Doom-Retro tweak with no actor-state cost that we can use
   as the absolute first canary PR? (The pain-view-kick falls into
   this; that's why it's listed first.)
