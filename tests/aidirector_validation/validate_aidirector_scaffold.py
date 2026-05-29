#!/usr/bin/env python3
"""Validate AI Director scaffolding and emit demo-replay placeholders."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = ROOT / "tests" / "aidirector_validation" / "dist" / "aidirector_report.json"


STATIC_CHECKS = [
    ("master-cvar", ROOT / "src" / "d_net_aidirector.cpp", "sv_aidirector_enable"),
    ("monster-sweep", ROOT / "src" / "d_net_aidirector.cpp", "HCDEAIDirectorObserveMonsters"),
    ("grouping", ROOT / "src" / "d_net_aidirector.cpp", "ObservedClusters"),
    ("regroup-hint-cvar", ROOT / "src" / "d_net_aidirector.cpp", "sv_aidirector_regroup_hint"),
    ("rng-report", ROOT / "src" / "d_net_aidirector.cpp", "DeterministicRngDraws"),
    ("zscript-hint", ROOT / "wadsrc" / "static" / "zscript" / "actors" / "ai" / "hcde_ai_hint.zs", "class HCDEAIRegroupHint"),
]


DEMO_ROWS = [
    {
        "id": "demo-disabled",
        "status": "pending-runtime-launch",
        "goal": "Record/play a single-player demo with sv_aidirector_enable=0 and confirm checksums match baseline.",
    },
    {
        "id": "demo-enabled-observe-only",
        "status": "pending-runtime-launch",
        "goal": "Record/play with sv_aidirector_enable=1 and sv_aidirector_regroup_hint=0; observation counters may change but playsim checksums must match.",
    },
    {
        "id": "demo-enabled-regroup-hint",
        "status": "pending-runtime-launch",
        "goal": "Record/play with regroup hints enabled once hints are applied through actor APIs; pr_aidirector draw count must match.",
    },
]


def check_static(row: tuple[str, Path, str]) -> dict:
    check_id, path, needle = row
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return {"id": check_id, "status": "failed", "path": str(path.relative_to(ROOT)), "needle": needle, "notes": "file missing"}
    found = needle in text
    return {"id": check_id, "status": "passed" if found else "failed", "path": str(path.relative_to(ROOT)), "needle": needle, "notes": "needle found" if found else "needle missing"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()

    checks = [check_static(row) for row in STATIC_CHECKS]
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "static_checks": checks,
        "demo_rows": DEMO_ROWS,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    for check in checks:
        print(f"[{check['status'].upper()}] {check['id']}: {check['notes']}")
    for row in DEMO_ROWS:
        print(f"[{row['status'].upper()}] {row['id']}: {row['goal']}")
    print(f"Wrote report: {args.report}")
    return 1 if any(check["status"] != "passed" for check in checks) else 0


if __name__ == "__main__":
    raise SystemExit(main())
