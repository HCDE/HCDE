# HCDE EDGE Classic Compatibility Stage 3

Stage 3 adds a neutral DDF manifest bridge to the HCDE mod compatibility tool.
It still does not turn EDGE Classic definitions into live HCDE gameplay.

## What The Bridge Does

The scanner now parses DDF section properties into HCDE-owned manifest entries.
Each entry records:

- source file and line
- DDF category and section label
- bridge status: `bridgeable`, `partial`, or `manual`
- supported properties and their normalized values
- manual properties that still need adapter decisions
- duplicate scalar property keys that require an explicit adapter decision

This gives later stages a stable, reviewable input without copying DDF files or
letting definitions silently become engine behavior. Repeated properties are
preserved as visible ambiguity instead of being treated as clean bridgeable
data. Repeatable state groups such as `STATES(...)` remain manual adapter data
without being treated as duplicate scalar properties.

## Generated File

When EDGE Classic DDF resources are detected, candidates now include:

```text
edge_manifest.json
```

The manifest is structured data for tooling and review. `edge_validation.md`
also includes a readable summary of the same bridge entries.

## Boundary

- Bridgeable means the property is safe to preserve as neutral manifest data.
- Partial means some properties are recorded but the section still has manual
  behavior or duplicate keys that must be designed later.
- Manual means the section is visible but not ready for translation.
- COAL, Lua, RTS, and RadTrig remain report-only.

Stage 3 keeps the golden-rule boundary: no hidden runtime adapter and no
third-party DDF content copied into HCDE source.

## Next Stage

Stage 4 is documented in `docs/HCDE_EDGE_CLASSIC_COMPAT_STAGE4.md`. It writes
candidate `ANIMDEFS` and `SNDINFO` files for narrow data-only animation and
sound mappings while keeping complex attacks, specials, states, and script hooks
manual.
