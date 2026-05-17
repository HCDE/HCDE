# HCDE Shadow Renderer Stage 3 Profiles

Date: 2026-05-17

Stage 3 adds a compact HCDE shadow profile control on top of the existing
dynamic light, shadowmap, and actor sprite shadow CVARs. This is a UI and
configuration layer only; it does not replace renderer internals and does not
import behavior from other engines.

## Menu Path

`Options -> Display Options -> Dynamic Light Options`

The menu now exposes:

- `HCDE shadow profile`
- `Apply enhanced shadow preset`
- Existing dynamic light and shadowmap controls
- Existing actor sprite shadow controls

## Profiles

| Profile | Intent |
| --- | --- |
| Manual | Leaves all current shadow CVARs unchanged. |
| Off | Disables light shadowmaps and actor sprite shadows. |
| Performance | Keeps dynamic lighting enabled, disables light shadowmaps, and uses short, lighter sprite shadows. |
| Balanced | Enables dynamic light shadowmaps at 1024 with low filtering and normal sprite shadows. |
| Enhanced | Applies the Stage 1 enhanced preset: 2048 shadowmaps, medium filtering, and longer balanced sprite shadows. |
| Cinematic | Enables higher-cost 4096 shadowmaps, high filtering, always-on sprite shadows, and longer fade distance. |

## CVARs

The new archived global CVAR is:

```text
hcde_shadowprofile
```

Values:

| Value | Profile |
| ---: | --- |
| 0 | Manual |
| 1 | Off |
| 2 | Performance |
| 3 | Balanced |
| 4 | Enhanced |
| 5 | Cinematic |

Changing the profile applies existing renderer CVARs immediately. Advanced
users can still adjust the individual CVARs afterward.

## Compatibility Boundary

- Profiles are clean-room grouped defaults for HCDE's existing renderer.
- No K8Vavoom, Doom 3, Helion, or Zandronum renderer code is used.
- Quake-style and Doom 3-style shadow behavior remain later-stage clean-room
  experiments, not part of Stage 3.
