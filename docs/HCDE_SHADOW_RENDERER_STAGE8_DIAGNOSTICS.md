# HCDE Shadow Renderer Stage 8 - Diagnostics and Profile Sweep

Stage 8 closes the shadow-renderer upgrade with runtime diagnostics and a full
profile validation pass.

## Shadowmap Stats

`stat shadowmap` now reports:

- `upload`: shadowmap update time.
- `lights`: dynamic lights scanned.
- `eligible`: active lights that requested shadowmaps.
- `shadowmapped`: lights assigned shadowmap rows this frame.
- `dropped`: eligible lights skipped because the current budget was full.
- `rows`: shadowmap rows rendered this frame.
- `priority`: whether Stage 7 light prioritization was active.

This makes budget tuning easier when modders report missing shadows in maps with
many dynamic lights.

## Profile Sweep

Stage 8 verifies every profile value from `0` through `7`:

| Value | Profile |
| ---: | --- |
| 0 | Manual |
| 1 | Off |
| 2 | Performance |
| 3 | Balanced |
| 4 | Enhanced |
| 5 | Cinematic |
| 6 | Quake-style |
| 7 | Doom 3-style |

The sweep should confirm that each profile applies without a fatal error, VM
abort, or unknown command, and that the profile-specific CVARs land on the
expected values.

## Local Validation

Date: 2026-05-17

The local Windows build was rebuilt and all profiles were launched against
`DOOM2.WAD` on `MAP01`.

Logs:

- `C:\Users\user\Downloads\hcde-shadow-stage8-profile-0.log`
- `C:\Users\user\Downloads\hcde-shadow-stage8-profile-1.log`
- `C:\Users\user\Downloads\hcde-shadow-stage8-profile-2.log`
- `C:\Users\user\Downloads\hcde-shadow-stage8-profile-3.log`
- `C:\Users\user\Downloads\hcde-shadow-stage8-profile-4.log`
- `C:\Users\user\Downloads\hcde-shadow-stage8-profile-5.log`
- `C:\Users\user\Downloads\hcde-shadow-stage8-profile-6.log`
- `C:\Users\user\Downloads\hcde-shadow-stage8-profile-7.log`

Results:

| Profile | Result |
| ---: | --- |
| 0 | Passed. Manual left existing CVARs unchanged, as intended. |
| 1 | Passed. Off disabled light shadowmaps and actor shadow style reset to classic. |
| 2 | Passed. Performance used 512 shadowmaps, budget 128, priority enabled. |
| 3 | Passed. Balanced used 1024 shadowmaps, budget 256, priority enabled. |
| 4 | Passed. Enhanced used 2048 shadowmaps, budget 512, priority enabled. |
| 5 | Passed. Cinematic used 4096 shadowmaps, high filter, budget 1024. |
| 6 | Passed. Quake-style disabled light shadowmaps and used sprite shadow style 1. |
| 7 | Passed. Doom 3-style used 4096 shadowmaps, nearest filter, budget 1024, style 2. |

No profile log contained a fatal error, VM abort, unknown command, or unknown
CVAR.
