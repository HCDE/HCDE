# HCDE ID24 Surface Audit

Roadmap board item: **#2** ("ID24"). The board lists this as "Ready (queued)"
but the engine already has substantial ID24 support landed. This audit
captures the present surface, the open gaps, and the integration
boundaries a finishing change must respect.

## What's already in tree

| Subsystem | What's there | Files |
| --- | --- | --- |
| Levelnum extension | ID24 allows levelnums >= 10 in `ExMy` style names; `id24_levelnum` parallel field is propagated through `level_info_t`. | `gamedata/g_mapinfo.{cpp,h}`, `gamedata/hcde_emapinfo.cpp`, `gamedata/umapinfo.cpp` |
| Support WAD | `id24.wad` autoload via `i_loadsupportwad` CVAR (default on, disabled in netgames). | `d_iwad.cpp`, `d_main.h::SupportWAD` |
| Intermission anims | `id24anim` flag enables ID24-style ExitAnim/EnterAnim layers in `wi_stuff.cpp`. | `wi_stuff.cpp` |
| Nightmare respawn | Per-actor `MinRespawnTics` and `RespawnDice` fields used by Nightmare respawn timing. | `playsim/actor.h` |
| UMAPINFO ID24 numbering | Level numbering walks an `id24_levelnum` counter so cross-MAPINFO maps compare correctly. | `gamedata/umapinfo.cpp` |

This is enough that ID24-flagged WADs load, place themselves correctly in
the level table, and use ID24 intermission art. The "Ready" status on the
board reflects "needs polish / verification", not "needs to be built".

## Gaps and open questions

| ID | Area | Open question / gap |
| --- | --- | --- |
| I1 | DEHEXTRA / extended thing slots | Confirm full coverage of ID24's expanded thing/state/sprite slot ranges. ID24 raises several caps over MBF21; spot-check that DEHACKED parsing accepts the new ranges and that the playsim doesn't truncate them. |
| I2 | New flags / properties | ID24 introduces several new actor flags and behaviour fields. Inventory: which are mapped, which are missing? `MinRespawnTics`/`RespawnDice` are wired; sprite + thing-state extensions need verification. |
| I3 | Compatibility ledger | An `id24compat` flag (similar to `mbf21compat`) gates behaviour where ID24 deviates from prior compat surfaces. Audit the COMPATF-style flag space and document each. |
| I4 | UMAPINFO + ID24 interaction | ID24 ships an updated UMAPINFO grammar (new keys for boss actions, intermission tweaks, level-number rules). Confirm the existing parser accepts the full ID24 grammar. |
| I5 | Multiplayer exposure | None of the ID24 features should affect server authority. Verify that `id24_levelnum` and intermission art changes are presentation-side only and that any new gameplay properties (e.g. respawn timing) participate in actor replication where relevant. |
| I6 | Soak coverage | A small smoke-test suite that loads a known ID24 WAD, walks the level table, runs an intermission, and checks no warnings would catch regressions cheaply. |

## Integration boundaries

ID24 is a **compatibility importer**, not a runtime layer. The HCDE
discipline:

- ID24 features land as data in the existing playsim/actor model. New
  fields (e.g. `MinRespawnTics`) are added to existing structs; no
  parallel ID24 "engine mode" branches the simulation.
- Presentation-side artefacts (intermission anims, support-WAD art) live
  alongside the existing ZDoom/UMAPINFO code, gated by per-record flags.
- Any behaviour change ID24 demands must respect server authority:
  the authoritative simulation runs the new rule, clients predict it,
  and snapshots replicate the resulting state. No client-only ID24
  behaviour shortcuts.
- The `i_loadsupportwad` CVAR explicitly disables in netgames; that's
  the right shape for the autoload concern. A cooperative/match server
  can still load `id24.wad` explicitly via `-file`.

## Concrete next steps (sized for follow-up changes)

1. **Inventory pass for I1/I2.** Diff HCDE's actor / dehacked / state
   tables against the ID24 spec; produce a table of "supported / missing /
   ambiguous" rows. Should fit in one PR alongside the verification
   smoke test (I6).
2. **ID24 compat-flag survey (I3).** List which behaviours are gated by
   the existing `compat_*` flags, which need new ID24-specific flags,
   and which can be unconditional. Document in a dedicated
   `docs/HCDE_ID24_COMPAT.md`.
3. **UMAPINFO grammar diff (I4).** Run the ID24 reference WADs through
   the loader with `-debug` and capture any parser warnings. Add the
   missing keys without breaking existing behaviour.

None of these are blockers for shipping; they're the polish pass that
moves #2 from "Ready" to "Done foundations".

## DEHEXTRA / Extended Slot Inventory (2026-05-28)

This table captures the observable ID24-adjacent surface already wired in
HCDE. A full "supported / missing / ambiguous" matrix against the ID24
reference spec requires the official ID24 document; this inventory lists
what is *known to be present* from code inspection.

| Area | Observable support | Status | Notes |
| --- | --- | --- | --- |
| Level numbering | `id24_levelnum` parallel field in `level_info_t`; `GetDefaultLevelNum` extended for `ExMy` names with `id24num >= 10`. | Supported | `g_mapinfo.cpp`, `hcde_emapinfo.cpp`, `umapinfo.cpp` |
| Support WAD | `id24.wad` autoload via `i_loadsupportwad` (serverinfo, disabled in netgames). | Supported | `d_iwad.cpp`, `d_main.h::SupportWAD` |
| Intermission anims | `ExitAnim` / `EnterAnim` with `id24anim` flag; V1/V2 snapshot headers. | Supported | `wi_stuff.cpp` |
| Nightmare respawn | Per-actor `MinRespawnTics`, `RespawnDice`. | Supported | `playsim/actor.h` |
| UMAPINFO numbering | `id24_levelnum` counter walk in `ParseMapEntry`. | Supported | `gamedata/umapinfo.cpp` |
| DEH/thing slot ranges | No explicit ID24 cap raise observed in `d_dehacked.cpp` or actor table init. | Needs spec diff | ID24 raises several thing/state/sprite caps; verify parser accepts new ranges. |
| DEH/state slot ranges | Same as above. | Needs spec diff | Extended state table size not yet confirmed. |
| DEH/sprite slot ranges | Same as above. | Needs spec diff | Sprite name table extension not yet confirmed. |
| New actor flags/properties | `MinRespawnTics`/`RespawnDice` wired; other ID24 flag surfaces (e.g., new `MF2_*` or `MF3_*`) not yet enumerated. | Needs spec diff | Inventory against ID24 flag list required. |
| Compat surface | No dedicated `id24compat` flag observed; existing `compat_*` flags may cover some behaviours. | Needs survey | See item I3 / `HCDE_ID24_COMPAT.md`. |

**Next action:** obtain the ID24 reference spec and produce the complete
supported/missing/ambiguous table. The rows above are the starting point
from a single-pass code scan.

### DEHEXTRA / Extended Flag Notes (2026-05-28 addendum)

The current scan shows solid MBF21-era plumbing but no obvious dedicated
ID24 cap/flag namespace yet:

- `gamedata/d_dehacked.cpp` has explicit `MBF21 Bits` handling for things,
  states, and weapons, plus MBF21 action functions such as
  `MBF21_JumpIfFlagsSet`, `MBF21_AddFlags`, and `MBF21_RemoveFlags`.
- `playsim/actor.h` exposes MBF21-ish flag surfaces in `MF8_*` for boss death
  and AI behaviours, plus ID24-specific `MinRespawnTics` and `RespawnDice`.
- `doomdef.h` / `d_main.cpp` expose `COMPATF2_NOMBF21`, but there is no
  `COMPATF2_NOID24`, `id24compat`, or equivalent ID24-specific compatibility
  toggle observed in this pass.
- `scripting/vmthunks_actors.cpp` exports `MinRespawnTics` and `RespawnDice`
  to VM code, so the known Nightmare-respawn surface is script-visible.
- No explicit `ID24 Bits`, `ID24 Flags`, or DEHEXTRA range-raise parser branch
  was observed. That does not prove absence of support (some ranges may be
  dynamic or inherited from MBF21), but it means the finishing PR needs a spec
  diff rather than a blind "looks supported" closeout.

Recommended verification matrix for the follow-up PR:

| Spec area | Current confidence | Verification action |
| --- | --- | --- |
| Thing/state/sprite index ranges | Unknown | Build a tiny DEH patch at the highest ID24-promised thing, state, and sprite slots; verify parser accepts it and the runtime can spawn/render it. |
| New actor flags | Partial | Diff ID24's flag list against `actor.h`, `thingdef_properties.cpp`, and `d_dehacked.cpp`; mark each as supported, mapped-to-existing, missing, or intentionally rejected. |
| Weapon fields | MBF21 known, ID24 unknown | Diff ID24 weapon fields against `d_dehacked.cpp` weapon parser and `PClassWeapon` fields. |
| Compat behavior | Missing dedicated ID24 toggle | Decide whether ID24 deviations are unconditional, MAPINFO-scoped, or gated by a new compat bit. |
| Multiplayer replication | Partial | For any new gameplay field, check actor serialization and snapshot replication. Presentation-only ID24 fields can stay local. |

Smoke-test harness: `tests/id24_validation/build_id24_validation_pack.py`
now generates a tiny `E1M10`/`E1M11` WAD plus companion PK3/DEH artifacts,
and `tests/id24_validation/validate_id24_smoke.py` runs the generated
manifest and writes `dist/report.json` with per-case output tails. The
map-numbering, intermission-anim parser, and Nightmare respawn VM/default
cases are executable. The DEHEXTRA high-slot and extended-flag rows remain
disabled until the official ID24 range/flag spec diff is filled in.

Exit criteria for #2 "Done foundations": the table above is replaced by a
complete supported/missing/ambiguous matrix and the disabled DEHEXTRA /
extended-flag smoke rows become executable rather than spec placeholders.
