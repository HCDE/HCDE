# HCDE ID24 Compatibility Flag Survey

Roadmap board item: **#2** (ID24). This document is the deliverable for
audit item I3: "List which behaviours are gated by the existing
`compat_*` flags, which need new ID24-specific flags, and which can be
unconditional."

## Existing compat surface (HCDE / GZDoom lineage)

HCDE inherits the ZDoom/GZDoom `compat_*` CVAR family and the
`COMPATF_*` bitmask. Key examples (non-exhaustive):

| CVAR | Bit | Typical behaviours gated |
| --- | --- | --- |
| `compat_mlook` | `COMPATF_MLOOK` | Mouse look policy |
| `compat_sound` | `COMPATF_SOUND` | Sound origin / attenuation |
| `compat_floors` | `COMPATF_FLOORS` | Floor clipping / sector special handling |
| `compat_nopassover` | `COMPATF_NO_PASSOVER` | Line pass-over rules |
| `compat_limitpain` | `COMPATF_LIMITPAIN` | Pain elemental limit |
| `compat_shorttex` | `COMPATF_SHORTTEX` | Texture name truncation |
| `compat_stairs` | `COMPATF_STAIRS` | Stair building rules |
| `compat_doorstuck` | `COMPATF_DOORS` | Door stuck behaviour |
| `compat_falling` | `COMPATF_FALLING` | Falling damage / sector damage |
| `compat_explode1/2` | `COMPATF_EXPLODE*` | Explosion damage / radius |
| `compat_invisibility` | `COMPATF_INVISIBILITY` | Partial invisibility rules |
| `compat_silent` | `COMPATF_SILENT` | Silent BFG / tracer rules |
| `compat_instant` | `COMPATF_INSTANT` | Instant door / lift rules |
| `compat_silentpickup` | `COMPATF_SILENTPICKUP` | Pickup sound policy |
| `compat_wallrunning` | `COMPATF_WALLRUN` | Wall-running / infinite height |
| `compat_pushers` | `COMPATF_PUSHERS` | Wind / current pusher behaviour |
| `compat_anybossdeath` | `COMPATF_ANYBOSSDEATH` | Boss death trigger policy |
| `compat_minotaur` | `COMPATF_MINOTAUR` | Minotaur (Heretic) behaviour |
| `compat_mushroom` | `COMPATF_MUSHROOM` | Mushroom cloud (Strife) |
| `compat_mbf21` | `COMPATF_MBF21` | MBF21-specific rules (already a dedicated surface) |

See `src/doomdef.h` and `src/gamedata/d_dehacked.cpp` for the full
`COMPATF_*` enumeration and the CVAR-to-bit mapping.

## ID24 behaviours that may need new flags

ID24 introduces several new actor flags, state behaviours, and
gameplay rules. The ones most likely to require a compat gate are:

| ID24 behaviour | Likely needs new flag? | Rationale |
| --- | --- | --- |
| New `MF2_*` / `MF3_*` actor flags | Yes | Existing `compat_*` flags do not cover new flag semantics; a dedicated `compat_id24_newflags` or per-flag `compat_id24_<name>` may be required. |
| Extended respawn timing (`MinRespawnTics`, `RespawnDice`) | Maybe | If the new timing deviates from Nightmare behaviour in edge cases, a compat gate lets servers opt back to classic timing. |
| New boss death / tag behaviours | Yes | `compat_anybossdeath` already exists; ID24 may add additional boss-death triggers that need a separate gate. |
| New intermission / exit rules | Unlikely | Intermission is presentation-side; no authority impact. |
| New UMAPINFO keys (boss actions, etc.) | Unlikely | Data-driven; parser can accept new keys without a compat flag. |
| DEH/thing/state/sprite cap raises | No | Cap raises are parser limits, not behavioural changes. |

## Recommended flag strategy

1. **Do not proliferate per-feature `compat_id24_*` flags** unless a
   concrete behavioural divergence is demonstrated. Most ID24 features
   are additive (new fields, new caps) and can be unconditional once
   the parser accepts them.

2. **Introduce a single `compat_id24` umbrella flag** (or reuse/extend
   `compat_mbf21`) that gates *all* ID24-specific behavioural changes
   that differ from MBF21/ZDoom. This keeps the surface small and
   matches how `compat_mbf21` already works.

3. **Document each gated behaviour** in the flag's help text and in
   this file. Unconditional behaviours (new caps, new intermission art,
   support WAD) should be explicitly called out as "always on for ID24
   maps."

## Open questions for the full survey

- Which ID24 actor flags have semantics that differ from existing
  `MF2_*` / `MF3_*` flags in a way that would break vanilla maps?
- Are there any ID24 state or codepointer behaviours that change the
  outcome of existing maps when the new cap is used?
- Does the ID24 spec mandate any change to the "boss death" or
  "tag 666/667" rules that would require a compat gate?

These questions require the official ID24 reference document. Once
available, each row should be classified as:

- **Unconditional** — always active for ID24 maps; no compat flag.
- **Gated by existing flag** — covered by `compat_mbf21` or another
  existing `compat_*`.
- **Requires new `compat_id24_*` flag** — behavioural divergence that
  needs an opt-out.

## Status

This document is a placeholder for the full survey. The initial
inventory (2026-05-28) lists the recommended strategy and the open
questions. A follow-up pass with the ID24 spec will fill in the
classification table.

## 2026-05-29 — `compat_noid24` umbrella flag landed

The umbrella opt-out called for in §"Recommended flag strategy" item 2 is
now wired up:

| Surface | Token / name | Files |
| --- | --- | --- |
| `COMPATF2_*` enum bit | `COMPATF2_NOID24` (1 << 24) | `src/doomdef.h` |
| User CVAR | `compat_noid24` (Flag, off by default) | `src/d_main.cpp` |
| MAPINFO key | `compat_noid24` | `src/gamedata/g_mapinfo.cpp` |
| `compatibility.txt` token | `noid24` | `src/maploader/compatibility.cpp` |

Semantics:

- **Default off.** ID24 features remain active by default; this flag is an
  opt-out, not an opt-in. Server admins or MAPINFO authors can turn it on
  for maps that need pre-ID24 behaviour.
- **Mirrors `compat_nombf21`.** The shape, default, persistence, and
  MAPINFO/`compatibility.txt` exposure all match the existing MBF21
  umbrella so operators learn one pattern.
- **Behavioural gating is per-feature.** Code that implements an ID24
  deviation should test `(i_compatflags2 & COMPATF2_NOID24) == 0` in the
  same place it would test `COMPATF2_NOMBF21`. Until those tests are
  written, the flag is observable but inert — no current ID24 behaviour is
  gated yet.

When the official ID24 spec diff lands and a concrete deviation is
identified (e.g. a new boss-death or respawn-tic rule), the implementing
PR should add the gate at the call site and reference this section.
