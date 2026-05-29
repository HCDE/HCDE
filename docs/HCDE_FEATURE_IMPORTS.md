# HCDE Feature-Import Boundaries

This is a working note covering three roadmap items at once because they
share the same set of integration questions: how an imported feature lands
without breaking server authority, fixed-tic playsim, or demo determinism.

- **Roadmap #11** — International Doom -- Two Specific Features.
- **Roadmap #10** — Crispy Doom -- Variable Framerate System.
- **Roadmap #3**  — Eternity's Spatial Audio Engine.

Each section produces a concrete recommendation: pick which feature(s) to
land, where the boundary sits, and what review gates the change has to
pass before merge.

---

## #11 International Doom: pick 2 features

International Doom (a Crispy fork) ships dozens of small QoL features.
Picking two surface-area-bounded ones lets us land a real first import
without committing to a full feature-port pipeline.

### Recommended candidates

#### 1. Smooth weapon bobbing curve (presentation only)

International Doom replaces the linear weapon-bob lookup with a smoothed
curve that removes the jitter most visible at high mouselook speeds.

- **Layer:** presentation. The bob curve is sampled per *render frame*,
  not per gametic, and feeds only the on-screen weapon sprite offset.
  The authoritative weapon angle is unchanged.
- **Multiplayer:** invisible. Other players never see your local weapon
  bob; it's a HUD artefact.
- **Determinism:** unaffected. The bob value is not part of `usercmd_t`,
  not part of any savegame data, and not part of any snapshot.
- **Surface area:** ~30 lines in the renderer, gated behind a CVAR
  (`r_weapon_bob_smooth`, default off for a quiet first import).
- **Risk:** very low. Worst case the toggle is forgotten and someone has
  to flip it back.

#### 2. Brightmap-aware fullbright sprites

International Doom extends the fullbright sprite list with author-driven
overrides so monster eyes / weapon lights stay lit through dark sectors.

- **Layer:** presentation. Affects only the rendered light contribution,
  not collision, not visibility for AI, not anything the authority sees.
- **Multiplayer:** invisible. Lighting is local.
- **Determinism:** unaffected.
- **Surface area:** small data table plus a renderer hook. Lives next to
  the existing brightmap support in the renderer; gated behind
  `r_fullbright_overrides` (default off).
- **Current implementation:** `r_fullbright_overrides=1` lets GLDEFS
  `fullbright` texture metadata force sprite fullbright in hardware and
  software sprite render paths. GLDEFS `disablefullbright` still wins, so mods
  can opt specific rotations/brightmaps out of forced fullbright.
- **Risk:** low. Mod compatibility risk is the main thing — a mod that
  defines its own brightmap should win over the override table; existing
  precedence rules cover this.

### Explicitly NOT in scope for the first import

- **Movement / damage / sound feel changes.** These touch playsim and
  would need full multiplayer review. They are valid candidates later
  but are not the right first import.
- **Anything that changes the contents of a savegame, demo, or
  snapshot.** Determinism review is its own engineering effort.
- **HUD overhauls.** International Doom has nice hudfont work but the
  HUD already has heavy compatibility baggage (`COMPATF_*`, statusbar
  variations, `dsda_hud`). A first import should not touch it.

### Review gates before merge

1. New CVARs default off and are documented in the launcher and wiki.
2. A demo recorded with the feature on and one with it off play back
   identically (proves no playsim drift).
3. Multiplayer smoke test: two clients with the feature in opposite
   states stay in sync (proves no replication drift).

---

## #10 Crispy variable framerate

Crispy's variable-framerate system decouples the render loop from the
35 Hz tic. HCDE inherits the GZDoom interpolation framework which already
does this, so the question is whether the **Crispy** specifics add
anything beyond what's already shipping.

### Current state in HCDE

| Concern | Where it lives | Status |
| --- | --- | --- |
| Render frame interpolation between tics | `rendering/` (HW interpolation buffers) | Working. |
| Authoritative tic rate | `TICRATE` (35 Hz) | Fixed; do not touch. |
| Presentation timing | `r_maxfps` / display refresh rate | Working. |
| `usercmd_t` rate | One per gametic | Fixed; do not change. |

### What's actually open under #10

The roadmap card name suggests Crispy's UI / option menu
("variable framerate" toggle, smoothing options, etc.) more than its
interpolation algorithm. The proposed scope is therefore:

1. **Document the existing presentation/playsim split** so future changes
   don't accidentally mix them. (Doing this here.)
2. **Audit Crispy's option menu** for knobs we don't already expose. If
   there's a real gap we surface it via a CVAR; if not, we close the
   roadmap item with a doc note.
3. **Confirm `r_maxfps`-style ceilings still apply** when running with
   `vid_vsync` and the various uncapped modes the launcher offers.

### Hard rules

- **The fixed-tic playsim must remain authoritative.** Any presentation
  feature that shortens the gametic interval is rejected. Variable
  framerate means variable *render* frame interval, never variable tic.
- **`usercmd_t` is sampled once per gametic.** Mouse/keyboard events
  accumulated between tics may be averaged or peak-sampled, but the
  command stream the authority sees is still 35 Hz exactly.
- **Network traffic is gametic-paced.** Render-frame events do not
  produce snapshots, deltas, or authority events.

If a future change ever wants to deviate from these rules (e.g. for a
"split-tic" experiment), it needs roadmap-level review and a separate
networking design — it is *not* a Crispy import.

### Recommendation

The option-menu audit is filed in `docs/HCDE_CRISPY_VFR_AUDIT.md`.
The interpolation work itself is already shipping under the GZDoom-derived
renderer, so Crispy "variable framerate" maps to a UX polish task, not an
engine import.

---

## #3 Eternity Spatial Audio (facade boundary)

Eternity's spatial audio engine is mature and well-engineered. The
question is how to import it without coupling the authoritative
simulation to client-only audio state.

### Boundary contract

| Concern | HCDE side | Eternity import side |
| --- | --- | --- |
| Sound source identity | `AActor*` / `FPolyObj*` / `SOURCE_None` | Translated to an opaque source handle the audio engine owns. |
| Sound trigger | `S_Sound(...)` call sites unchanged. | A new backend behind the existing `S_Sound` interface. |
| Sound state | Engine-side: nothing. The renderer tracks playback. | Audio-engine internal data; not serialised. |
| Snapshots / saves | Carry "this actor said hello at this tic"; do not carry waveform state. | Audio engine reconstructs from triggers; never replays from save data. |
| Authority | Sounds may be triggered by authoritative events but *never* drive authoritative state. | Replication of "this trigger fired" lives in existing event lanes. No new packets. |
| Listener | One per local viewport (split-screen aware). | Per-listener mixer state is local; never replicated. |

### Concrete next-step

Phase 1 facade has landed as `src/common/audio/sound/s_sound_eternity.cpp`.
`snd_backend=eternity` now constructs a real `SoundRenderer` implementation
instead of returning `nullptr`; the facade is intentionally silent until the
actual Eternity mixer is vendored, but it records 2D/3D sound submits,
listener updates, 3D parameter updates, stream requests, and mix ticks.
`snd_eternity_status` reports those counters and `snd_eternity_probe` submits
a diagnostic trigger through the selected backend path.

The first real backend import should:

1. Replace the silent facade body with the vendored Eternity mixer while
   preserving the existing `SoundRenderer` contract and diagnostics.
2. Add the Eternity audio engine as a sibling backend to the existing
   audio backends (OpenAL etc.) under `common/audio/sound/`. **No new
   abstraction layer; reuse `SoundSystem` interfaces.**
3. Hide all backend selection behind `snd_backend` CVAR
   (or whatever the existing CVAR is). Default unchanged.
4. Run the existing audio test scenes in `tests/` against the new
   backend to verify no behavioural divergence on identical inputs.

### First real backend boundary

The first implementation PR should be deliberately narrow:

- Register an `eternity` backend name with the existing sound backend selector,
  but leave the default backend unchanged.
- Translate existing `S_Sound(...)` calls into Eternity source/listener handles.
  Do not add new sound trigger APIs.
- Treat all spatial state as client-local cache. Source handles may point back
  to actors, polyobjects, or ambient sources, but they are not serialized and
  never appear in snapshots.
- Keep save/demo playback backend-agnostic. A demo may sound different under
  different backends, but the authoritative state and command stream must be
  identical.
- Extend `snd_eternity_status` once registration exists so it can report
  whether the backend is compiled, selected, initialized, and actively mixing.

### Anti-goals

- **No new replicated audio state.** A "sound is playing" packet is not
  authoritative; the existing actor-state replication (which already
  carries enough info for clients to start the same sound locally) is
  the only audio-replication mechanism we use.
- **No demo-determinism dependency on backend.** Two clients running the
  same demo with different audio backends must produce the same
  authoritative output (different *audio* output is fine).
- **No coupling to AI hearing / `noise_alert` / etc.** Those go through
  the existing `P_NoiseAlert` machinery on the authoritative side and
  must not be moved into the audio engine.

### Recommendation

Promote #3 from "Ready (queued)" to "Ready -- design accepted". Real
implementation is a multi-PR effort (backend skeleton, then per-platform
glue, then test sweep). The design boundary above is the only piece
that needs to be locked before a coding PR opens.
