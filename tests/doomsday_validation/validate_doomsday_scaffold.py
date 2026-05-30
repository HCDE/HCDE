#!/usr/bin/env python3
"""Validate HCDE Doomsday three-features scaffolding (#6)."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = ROOT / "tests" / "doomsday_validation" / "dist" / "doomsday_report.json"


STATIC_CHECKS = [
    ("fakeradio-cvar", ROOT / "src" / "r_doomsday_features.cpp", "CVAR(Bool, r_fakeradio"),
    ("fakeradio-strength", ROOT / "src" / "r_doomsday_features.cpp", "r_fakeradio_strength"),
    ("geom-ao-cvar", ROOT / "src" / "r_doomsday_features.cpp", "CVAR(Bool, r_geom_ao"),
    ("geom-ao-strength", ROOT / "src" / "r_doomsday_features.cpp", "r_geom_ao_strength"),
    ("reverb-cvar", ROOT / "src" / "r_doomsday_features.cpp", "CVAR(Bool, snd_env_reverb"),
    ("status-ccmd", ROOT / "src" / "r_doomsday_features.cpp", "CCMD(r_doomsday_status)"),
    ("render-blend-hook", ROOT / "src" / "rendering" / "2d" / "v_blend.cpp", "HCDE #6 Doomsday-style presentation hook"),
    ("render-fakeradio-read", ROOT / "src" / "rendering" / "2d" / "v_blend.cpp", "r_fakeradio_strength"),
    ("render-geom-ao-read", ROOT / "src" / "rendering" / "2d" / "v_blend.cpp", "r_geom_ao_strength"),
    ("reverb-openal-gate", ROOT / "src" / "common" / "audio" / "sound" / "oalsound.cpp", "snd_env_reverb gates environmental reverb"),
    ("reverb-openal-efx", ROOT / "src" / "common" / "audio" / "sound" / "oalsound.cpp", "LoadReverb(env"),
    ("audit-doc", ROOT / "docs" / "HCDE_DOOMSDAY_AUDIT.md", "first render-path fallback"),
]


SOAK_ROWS = [
    {
        "id": "render-default-off",
        "status": "pending-runtime-launch",
        "goal": "Launch MAP01 with all #6 CVARs default off and compare a screenshot hash to baseline.",
    },
    {
        "id": "render-fallback-on",
        "status": "pending-runtime-launch",
        "goal": "Launch with r_fakeradio=1 and r_geom_ao=1; verify only local view-blend output changes.",
    },
    {
        "id": "reverb-backend",
        "status": "pending-runtime-launch",
        "goal": "Launch with snd_backend=openal, snd_efx=1, snd_env_reverb toggled on/off; verify EFX reverb changes and dry fallback.",
    },
]


def run_static_check(check: tuple[str, Path, str]) -> dict:
    check_id, path, needle = check
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return {"id": check_id, "status": "failed", "path": str(path.relative_to(ROOT)), "needle": needle, "notes": "file missing"}

    found = needle in text
    return {
        "id": check_id,
        "status": "passed" if found else "failed",
        "path": str(path.relative_to(ROOT)),
        "needle": needle,
        "notes": "needle found" if found else "needle missing",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()

    checks = [run_static_check(check) for check in STATIC_CHECKS]
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "static_checks": checks,
        "soak_rows": SOAK_ROWS,
    }

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    for check in checks:
        print(f"[{check['status'].upper()}] {check['id']}: {check['notes']}")
    for row in SOAK_ROWS:
        print(f"[{row['status'].upper()}] {row['id']}: {row['goal']}")
    print(f"Wrote report: {args.report}")
    return 1 if any(check["status"] != "passed" for check in checks) else 0


if __name__ == "__main__":
    raise SystemExit(main())
