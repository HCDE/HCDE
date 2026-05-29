# HCDE Doom Retro Compat Validation

Roadmap board item: #8 ("Doom Retro physics").

`validate_dr_compat_flags.py` is a static plumbing verifier for the Doom Retro
compat work. It checks that:

- `COMPATF2_DR_CRUSHER` exists.
- `COMPATF2_DR_LIQUIDFRICTION` exists.
- `compat_dr_crusher` exists.
- `compat_dr_liquidfriction` exists.
- the crusher behavior change is gated by `COMPATF2_DR_CRUSHER`.
- `r_view_pain_smooth` exists as a presentation-only CVAR, not a compat bit.

The report also carries the runtime rows that still need a launchable game
fixture:

- demo round-trip
- save round-trip
- dedicated-server/client authority check

Generated reports live in `dist/` and are ignored by git.
