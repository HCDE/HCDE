# HCDE Predator Validation

Roadmap board item: #12 ("Predator Economy Game Mode").

`validate_predator_scaffold.py` verifies the static Predator Economy surface:

- snapshot V1 contract
- live capability bit
- replicated currency fields
- buy request/result structs
- deterministic role-selection helper
- ZScript pawn extension surface

The generated report also carries pending runtime rows for save round-trip,
demo round-trip, and dedicated late-join soak. Reports live in `dist/` and
are ignored by git.
