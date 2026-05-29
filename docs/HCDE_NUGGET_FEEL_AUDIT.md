# HCDE Roadmap #9 — Nugget Doom Player Feel & Input Scoping

**Last updated:** 2026-05-28
**Status:** Phase 1-5 implemented default-off. This document defines the
integration boundary for follow-up tuning and soak.

## What #9 actually means

Board item: "Nugget Doom -- Player Feel & Input." Nugget is a Crispy
fork with player-side QoL: smoother turning, optional autorun nuance,
weapon switching tweaks, optional crosshair behaviours, footstep sounds
gated by surface, etc. The integration goal is to **selectively** import
the feel-side changes that don't touch playsim physics or netcode, while
explicitly **rejecting** anything that would change movement physics,
hitscan determinism, or projectile behaviour.

This document is paired with #7 (gyroscope input — see the existing
`src/i_input_gyro.cpp` scaffold) which is the input *device* axis.
This document covers the input *processing* axis.

## Existing in-tree surface

- **usercmd construction.** `src/g_game.cpp` (and the network
  side in `d_net*.cpp`) builds `usercmd_t` per tic from raw input. Any
  player-feel change ultimately turns into different `usercmd_t` bytes
  that flow through prediction and the server.
- **Input devices.** `src/common/platform/*` (SDL/Win32) feeds raw events
  to `c_dispatch.cpp` and the binding system.
- **Bindings.** `src/common/console/c_bind.cpp`.
- **Crosshair / hud.** `src/sbar*.cpp` and ZScript HUD code.
- **Sound footstep hook.** Already partially exists via SECF_*
  flags / `*footstep` sound mapping.

## Hard rules (presentation vs playsim)

Two categories. Anything in category A is fair game; anything in
category B is **rejected for #9** (it belongs to #8 Doom Retro physics
or a separate explicit roadmap discussion).

### Category A — Allowed under #9

- **Smoothing of LOOK and TURN at the input layer.**
  Acceleration curves, deadzone shaping, mouse-smoothing toggles. These
  feed into `usercmd_t.angleturn` / `usercmd_t.pitch`. The result is the
  authoritative input the server still gets — only its *shape over
  frames* changes.
- **Crosshair behaviour.** Render-side dot/recoil/bloom indicators.
  Pure presentation.
- **HUD additions.** Visible-only widgets driven by replicated state.
- **Console messages.** Killfeed-style print formatting.
- **Optional view bob style.** Already partially landed under #11
  (`r_weapon_bob_smooth`); apply the same pattern for similar
  presentation-only knobs.
- **Footstep sound triggering.** Sound channel selection / surface
  sampling for the sound name. The sound itself is already replicated
  via the existing actor sound path.

### Category B — Rejected under #9 (belongs elsewhere)

- **Movement physics changes.** Friction, momentum caps, ground/air
  control, jump impulses. These mutate playsim state and would change
  netgame outcomes.
- **Hitscan or projectile changes.** Bullet spread tables, attack ranges,
  damage formulas.
- **Weapon switch latency / stowaway tic counts.** These change the
  authoritative weapon state machine.
- **Autoaim behaviour.** Already replicated/server-authoritative; mutating
  this changes hitscan results.

If you want any of category B, file it under #8 (Doom Retro physics) or
its own roadmap entry — not here.

## Phased plan

- **Phase 0 (this doc).** Define the boundary.
- **Phase 1.** **Landed 2026-05-28.** Presentation-only CVARs now live in
  `src/i_input_feel.cpp` (mirrors the gyro scaffold layout):
    - `m_smooth_curve` (0=linear, 1=eased) — applied to mouse input
      *before* it reaches usercmd construction, but only at the point
      where we already smooth/scale, no playsim impact.
    - `r_crosshair_recoil` (Bool) — render-side widget.
    - `r_killfeed` (Bool) — render-side widget.
    - `snd_footsteps_surface` (Bool) — sound-name selection only.
- **Phase 2.** **Landed 2026-05-28.** `m_smooth_curve` now shapes mouse
  deltas in `G_BuildTiccmd` through a stateless smoothstep curve when enabled.
  Default `0` is identity/current behaviour. `m_input_status` reports the
  current scaffold values.
- **Phase 3.** **Landed 2026-05-28.** `r_crosshair_recoil` offsets only the
  rendered crosshair using clamped local `player_t::refire` and weapon bob
  tuning. It does not change aim, hitscan, commands, or actor state.
- **Phase 4.** **Landed 2026-05-28.** `r_killfeed` records existing obituary
  text into a HUD-side ring buffer and renders recent entries on the overlay.
  It does not alter scoring/frags.
- **Phase 5.** **Landed 2026-05-28.** `snd_footsteps_surface` selects
  `*footstep-solid` / `*footstep-liquid` aliases in ZScript only. Defaults
  alias back to `*land` and mods can override them.
- **Phase 6.** Soak in single-player + multiplayer; verify usercmd bytes
  for a fixed input sequence are unchanged with all CVARs at default.

## Anti-patterns we will reject

- **CVARs that gate physics.** A CVAR named `m_smoothing` is fine; a
  CVAR named `p_air_control` is not under #9.
- **Predict-time-only changes that don't reconcile.** If client and
  server compute different `usercmd_t` results from the same raw input,
  prediction breaks. The smoothing must be applied to the same input on
  both sides, or only on the client side *before* the input becomes a
  usercmd.
- **Bypassing the existing binding system.** New input goes through
  `c_bind.cpp`; we do not introduce a parallel input layer.

## Source map (where Phase 1+ code goes)

| Concern | File |
| ------- | ---- |
| Player-feel CVARs and curves | `src/i_input_feel.cpp` (new) |
| Mouse smoothing wire-in | `src/g_game.cpp` (existing scaling block) |
| Crosshair recoil widget | `src/g_statusbar/shared_sbar.cpp` |
| Killfeed widget | `src/hcde_killfeed.{cpp,h}`, `src/g_statusbar/shared_sbar.cpp`, `src/playsim/p_interaction.cpp` |
| Footstep sound selection | `wadsrc/static/zscript/actors/player/player.zs`, `wadsrc/static/sndinfo.txt`, sound-name only |
| Build system | `src/CMakeLists.txt` (add `i_input_feel.cpp`) |

## Open questions (for the implementing PR)

1. Mouse smoothing must be deterministic when the same raw input
   sequence is replayed. The current scaling is already a function of
   raw input only. Confirm that adding an eased curve doesn't introduce
   any time-dependent state.
2. Should the killfeed and crosshair recoil settings be CVAR_ARCHIVE
   per-user, or also expose them to ZScript? Probably both, as we do
   today for similar presentation toggles.
3. Footstep surface sampling — is the existing `*footstep` sound name
   sufficient, or do we need a richer table? (Probably sufficient for
   #9; a richer sound system belongs to #3 Eternity Spatial Audio.)
