# HCDE AI Director Validation

Roadmap board item: #13 ("Monster AI System").

`validate_aidirector_scaffold.py` verifies the default-off AI Director
surface:

- master CVAR
- server-only monster sweep
- non-replicated grouping sidecar
- default-off regroup hint CVAR
- deterministic RNG draw reporting
- ZScript hint data carrier

The generated report also records pending single-player demo replay rows for
disabled, observe-only, and regroup-hint configurations. Reports live in
`dist/` and are ignored by git.
