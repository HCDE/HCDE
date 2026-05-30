# Engine Subsystems

This page is the operator-facing index for the gameplay and netcode
subsystems that have been wired up in HCDE on top of the GZDoom/UZDoom
playsim. Each entry links to the in-tree design/audit doc with deeper
detail (CVARs, state machine, on-the-wire layout, diagnostics).

## Netcode and infrastructure

- **Rewind & lag compensation** — server-side rewind for hitscan and
  short-projectile checks, replay-able input ring, and operator CVARs
  for budget/clamping. See `docs/HCDE_REWIND.md`.
- **RCON utility** — authenticated console-over-TCP for dedicated
  servers, with rate-limited auth, per-command audit log, and a Python
  smoke harness under `tests/rcon_validation/`. See `docs/HCDE_RCON.md`.
- **Net diagnostics & black box** — packet ring + replayable trace files
  emitted by `+debugtrace_enable 1`. See `Network-Protocol` and
  `docs/HCDE_NETCODE_STAGE*.md`.

## Game modes

- **Invasion mode** — wave-based co-op (roadmap #15). Operator guide
  with CVARs, state machine, and spawn logic in `docs/HCDE_INVASION.md`.
  Audit/test notes: `docs/HCDE_INVASION_AUDIT.md`,
  `tests/netcode_step12/f6_invasion_latejoin_replay.py`.
  Single-player is supported: launchers can pass `+sv_gametype 4`
  (or `+set sv_gametype 4`), and the local console player is counted
  as a participant so waves start without a remote authority.
- **Predator economy** — phase 1/2 scaffold and snapshot contract are
  landed default-off; buy/currency/role gameplay remains pending.
  See `docs/HCDE_PREDATOR_AUDIT.md` and
  `wadsrc/static/zscript/actors/predator/`.
- **AI Director (monster AI)** — scoping doc only at the moment;
  defines the integration boundary for the future #13 work.
  See `docs/HCDE_AIDIRECTOR_AUDIT.md` and
  `wadsrc/static/zscript/actors/ai/`.

## Compatibility imports

- **ID24 compatibility** — survey of which behaviours are gated by
  existing `compat_*` flags vs. needing new ID24-specific flags.
  See `docs/HCDE_ID24_AUDIT.md` and `docs/HCDE_ID24_COMPAT.md`.
- **NanoBSP node builder (from Woof)** — scoping doc and
  integration boundary for the loader at `src/d_nanobsp_loader.{cpp,h}`
  and the nodebuilder under `src/utility/nodebuilder/nano/`.
  See `docs/HCDE_NANOBSP_AUDIT.md`.
- **Crispy / Nugget / Doom Retro / Predator / k8vavoom feel imports**
  — feel/render/QoL imports tracked in their respective audits:
  `docs/HCDE_CRISPY_VFR_AUDIT.md`, `docs/HCDE_NUGGET_FEEL_AUDIT.md`,
  `docs/HCDE_DOOM_RETRO_AUDIT.md`,
  `docs/HCDE_RENDERING_K8VAVOOM_AUDIT.md`. The shared boundary doc is
  `docs/HCDE_FEATURE_IMPORTS.md`.

## Client feel and QoL

- **Killfeed** — client-side kill ticker. See
  `src/hcde_killfeed.{cpp,h}` and the related ZScript in
  `wadsrc/static/zscript/`.
- **Gyro / input feel** — controller gyro support and input-feel
  smoothing. See `src/i_input_gyro.{cpp,h}` and
  `src/i_input_feel.cpp`.
- **Smooth view pain** — pain-flash smoothing on the SW renderer.
  See `src/r_view_pain_smooth.cpp`.
- **Taunts** — voice-line taunts wired through the gameplay layer.
  See `src/g_taunt.cpp`.
- **Lag / perf overlay** — the top-of-screen "HCDE lag / perf mirror"
  strip is opt-in via `hcde_lag_hud 1`; the diagnostic-logging gate
  `hcde_hud_debug` no longer paints it on by default.

## Audio runtime dependencies

- **OpenAL runtime** — sound effects require an OpenAL implementation
  on the executable search path. HCDE ships `soft_oal.dll` (OpenAL
  Soft) in releases; when building from source on Windows you can drop
  `soft_oal.dll` (or rename `OpenAL32.dll`) into `build\` /
  `build\RelWithDebInfo\`.
- **libsndfile (`sndfile.dll`)** — required by ZMusic for OGG/FLAC/WAV
  music decoding (used by mods such as `D2Re.pk3`). Windows developer
  builds run `cmake/StageSndFileRuntime.cmake` as a `POST_BUILD` step
  for the `zdoom` target, copying `sndfile.dll` and its license into
  the build output. Set `-DHCDE_SNDFILE_RUNTIME_DLL=<path>` (or place
  the DLL in `build\deps\`) to point CMake at your copy. Vanilla MUS
  and MIDI work without it.

## Validation

Each subsystem has a Python validation harness under `tests/`:

| Subsystem    | Test directory                       |
| ------------ | ------------------------------------ |
| RCON         | `tests/rcon_validation/`             |
| Rewind       | `tests/netcode_step12/replay_blackbox.py` |
| Invasion     | `tests/invasion_stage9/` and `tests/netcode_step12/f6_invasion_latejoin_replay.py` |
| ID24 compat  | `tests/id24_validation/`             |
| NanoBSP      | `tests/nanobsp_validation/`          |
| Doom Retro   | `tests/doomretro_validation/`        |
| Predator     | `tests/predator_validation/`         |
| AI Director  | `tests/aidirector_validation/`       |

The smoke harnesses launch `hcdeserv.exe` and `hcde.exe` from the
`build/RelWithDebInfo` tree; see `tests/netcode_step12/README.md` for
the conventions they share.
