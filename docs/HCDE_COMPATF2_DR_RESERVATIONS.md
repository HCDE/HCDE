# HCDE COMPATF2_DR_* reservations

**Last updated:** 2026-05-28
**Roadmap board item:** #8 ("Doom Retro physics")
**Status:** Reserved bits now exist in `compatflags2`; no Doom Retro gameplay
tweaks are enabled or implemented yet. This remains the coordination ledger so
future PRs do not collide or repurpose the bits.

## Why this file exists

`docs/HCDE_DOOM_RETRO_AUDIT.md` requires that every Doom-Retro tweak ships
behind a dedicated `COMPATF2_DR_*` bit, defaulted off, demo-version aware,
and server-controlled in netgames. Adding bits to a shared enum across
multiple PRs is a classic source of merge conflicts and renumbering bugs,
so we keep the planned bit assignments here and cross-check before merging.

The Phase 1 plumbing added:

- `COMPATF2_DR_CRUSHER`
- `COMPATF2_DR_LIQUIDFRICTION`
- `compat_dr_crusher`
- `compat_dr_liquidfriction`

This still does **not**:

- Change demo version.
- Add any playsim behaviour.
- Enable either flag by default.

## Bit reservations

| Reserved name        | Intent (one-liner)                                      | Phase    |
| -------------------- | ------------------------------------------------------- | -------- |
| `COMPATF2_DR_CRUSHER`  | Doom Retro crusher edge-case fix (corpse-glue case).   | Landed as bit only; tweak pending |
| `COMPATF2_DR_LIQUIDFRICTION` | Doom Retro deep-water friction tweak.            | Landed as bit only; high-risk tweak deferred |

Future candidates that pass the audit's "playsim discipline" rules will be
added here before they consume an actual bit.

## Discipline rules carried forward

- One bit per tweak; no sharing.
- Default off; vanilla behaviour wins until an operator opts in.
- Demo recording must persist the flag set used; demo playback must
  honor the recorded value, not the current player CVAR.
- Server-authoritative in netgames; clients adopt server flags or refuse
  to join. This mirrors the existing compat-flag handling.
- One Doom-Retro tweak per PR. No bundles.
- Each PR's commit body must include the Doom Retro source/commit link,
  vanilla behaviour, new behaviour, an observable improvement, and at
  least one known-sensitive megawad we tested against.

## Bits NOT reserved here

- View-kick smoothing on damage. That belongs to a presentation-only
  CVAR (`r_view_pain_smooth`), not a compat flag, per the audit's
  Candidate 1 plan.
- Any non-physics presentation polish (HUD, sounds, weapon bob). Those
  live under #9 / #11 / #25 and never touch `compatflags2`.

## Cross-references

- `docs/HCDE_DOOM_RETRO_AUDIT.md` — the boundary doc and Phase plan.
- `docs/HCDE_ROADMAP_INTEGRATION_PLAN.md` — board status.
- `src/g_level.h` — the eventual home of the `COMPATF2_DR_*` enum members
  (Phase 1 PR adds them here).
- `src/d_protocol.h` — `DEMOGAMEVERSION`, which the Phase 1 PR may need
  to bump if any reserved bit is recorded into demos.
