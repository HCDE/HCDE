#!/usr/bin/env python3
"""Validate HCDE Doom Retro compat-flag plumbing by static inspection.

This does not replace runtime demo/save/netgame soaks. It creates a stable
report that proves the default-off DR flags and CVAR aliases are present, then
records the remaining runtime verification rows explicitly.
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = ROOT / "tests" / "doomretro_validation" / "dist" / "dr_compat_report.json"


CHECKS = [
    {
        "id": "compat-bit-crusher",
        "path": ROOT / "src" / "doomdef.h",
        "needle": "COMPATF2_DR_CRUSHER",
        "description": "Doom Retro crusher compat bit is reserved in compatflags2.",
    },
    {
        "id": "compat-bit-liquidfriction",
        "path": ROOT / "src" / "doomdef.h",
        "needle": "COMPATF2_DR_LIQUIDFRICTION",
        "description": "Doom Retro liquid-friction compat bit is reserved in compatflags2.",
    },
    {
        "id": "compat-cvar-crusher",
        "path": ROOT / "src" / "d_main.cpp",
        "needle": "compat_dr_crusher",
        "description": "Crusher compat bit has a server-controlled compat CVAR alias.",
    },
    {
        "id": "compat-cvar-liquidfriction",
        "path": ROOT / "src" / "d_main.cpp",
        "needle": "compat_dr_liquidfriction",
        "description": "Liquid-friction compat bit has a server-controlled compat CVAR alias.",
    },
    {
        "id": "crusher-default-off-gate",
        "path": ROOT / "src" / "playsim" / "p_map.cpp",
        "needle": "COMPATF2_DR_CRUSHER",
        "description": "Crusher behavior change is gated by the DR compat bit.",
    },
    {
        "id": "pain-smooth-cvar",
        "path": ROOT / "src" / "r_view_pain_smooth.cpp",
        "needle": "r_view_pain_smooth",
        "description": "Presentation-only pain smoothing CVAR exists outside compatflags.",
    },
]


RUNTIME_ROWS = [
    {
        "id": "demo-roundtrip",
        "status": "pending-runtime-launch",
        "description": "Record/play a demo with compat_dr_crusher=0 and =1 and confirm recorded compat state wins on playback.",
    },
    {
        "id": "save-roundtrip",
        "status": "pending-runtime-launch",
        "description": "Save/load a crusher fixture with compat_dr_crusher=1 and confirm the bit and thinker state survive.",
    },
    {
        "id": "netgame-server-authority",
        "status": "pending-runtime-launch",
        "description": "Start dedicated server with compat_dr_crusher=1 and confirm clients inherit compatflags2.",
    },
]


def now_utc_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def run_check(check: dict) -> dict:
    path = check["path"]
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return {
            "id": check["id"],
            "description": check["description"],
            "status": "failed",
            "path": str(path.relative_to(ROOT)),
            "needle": check["needle"],
            "notes": "file missing",
        }

    found = check["needle"] in text
    return {
        "id": check["id"],
        "description": check["description"],
        "status": "passed" if found else "failed",
        "path": str(path.relative_to(ROOT)),
        "needle": check["needle"],
        "notes": "needle found" if found else "needle missing",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()

    checks = [run_check(check) for check in CHECKS]
    failed = [check for check in checks if check["status"] != "passed"]
    payload = {
        "generated_at": now_utc_iso(),
        "static_checks": checks,
        "runtime_rows": RUNTIME_ROWS,
    }

    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(payload, f, indent=2)
        f.write("\n")

    for check in checks:
        print(f"[{check['status'].upper()}] {check['id']}: {check['notes']}")
    for row in RUNTIME_ROWS:
        print(f"[{row['status'].upper()}] {row['id']}: {row['description']}")
    print(f"Wrote report: {args.report}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
