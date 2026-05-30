#!/usr/bin/env python3
"""Validate Predator Economy scaffolding and emit soak placeholders."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = ROOT / "tests" / "predator_validation" / "dist" / "predator_report.json"


STATIC_CHECKS = [
    ("snapshot-v1", ROOT / "src" / "d_net_predator.h", "FHCDEPredatorSnapshotV1"),
    ("capability-v1", ROOT / "src" / "d_net_predator.h", "HCDELiveCapPredatorSnapshotV1"),
    ("currency-field", ROOT / "src" / "d_net_predator.h", "PlayerCurrency"),
    ("buy-request", ROOT / "src" / "d_net_predator.h", "FHCDEPredatorBuyRequest"),
    ("buy-result", ROOT / "src" / "d_net_predator.h", "FHCDEPredatorBuyResult"),
    ("role-selection", ROOT / "src" / "d_net_predator.cpp", "HCDEPredatorServerSelectPredatorRole"),
    ("netevent-buy-name", ROOT / "src" / "d_net_predator.h", "HCDEPredatorBuyNetEventName"),
    ("netevent-handler", ROOT / "src" / "d_net.cpp", "HCDEPredator_HandleNetEvent"),
    ("buy-console-command", ROOT / "src" / "d_net_predator.cpp", "CCMD(predator_buy)"),
    ("runtime-pawn-mirror", ROOT / "src" / "d_net_predator.cpp", "HCDEPredatorMirrorPawnState"),
    ("runtime-pawn-guard", ROOT / "src" / "d_net_predator.cpp", "IsDescendantOf(cls)"),
    ("runtime-pawn-buy-result", ROOT / "src" / "d_net_predator.cpp", "HCDE_SetPredatorBuyResult"),
    ("cheat-whitelist-cvar", ROOT / "src" / "d_net_predator.cpp", "sv_predator_allow_cheats"),
    ("cheat-filter-helper", ROOT / "src" / "d_net_predator.h", "HCDEPredatorShouldRejectCheatOpcode"),
    ("cheat-filter-give", ROOT / "src" / "d_net.cpp", "HCDEPredatorShouldRejectCheatOpcode(DEM_GIVECHEAT)"),
    ("cheat-filter-take", ROOT / "src" / "d_net.cpp", "HCDEPredatorShouldRejectCheatOpcode(DEM_TAKECHEAT)"),
    ("cheat-filter-generic", ROOT / "src" / "d_net.cpp", "HCDEPredatorShouldRejectCheatOpcode(DEM_GENERICCHEAT)"),
    ("cheat-filter-setinv", ROOT / "src" / "d_net.cpp", "HCDEPredatorShouldRejectCheatOpcode(DEM_SETINV)"),
    ("cheat-filter-warp", ROOT / "src" / "d_net.cpp", "HCDEPredatorShouldRejectCheatOpcode(DEM_WARPCHEAT)"),
    ("zscript-pawn", ROOT / "wadsrc" / "static" / "zscript" / "actors" / "predator" / "predator_player.zs", "class HCDEPredatorPawn"),
    ("zscript-round-state", ROOT / "wadsrc" / "static" / "zscript" / "actors" / "predator" / "predator_player.zs", "HCDE_SetPredatorRoundState"),
    ("zscript-buy-result", ROOT / "wadsrc" / "static" / "zscript" / "actors" / "predator" / "predator_player.zs", "HCDE_SetPredatorBuyResult"),
    ("zscript-buy-eligibility", ROOT / "wadsrc" / "static" / "zscript" / "actors" / "predator" / "predator_player.zs", "HCDE_CanPredatorBuy"),
]


SOAK_ROWS = [
    {
        "id": "save-roundtrip",
        "status": "pending-runtime-launch",
        "goal": "Save/load with sv_predator_enable=1 and confirm round state, currency, and role mirror survive.",
    },
    {
        "id": "demo-roundtrip",
        "status": "pending-runtime-launch",
        "goal": "Record/play a short predator round and confirm pr_predator role selection and snapshot-visible state match.",
    },
    {
        "id": "dedicated-late-join",
        "status": "pending-runtime-launch",
        "goal": "Start hcdeserv with predator enabled, join late, and confirm snapshot V1 state is visible to the joining client.",
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
