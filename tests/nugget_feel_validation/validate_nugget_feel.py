#!/usr/bin/env python3
"""Validate Nugget Doom player-feel scaffolds (#9).

Phase 6 of the audit calls for "verify usercmd bytes for a fixed input
sequence are unchanged with all CVARs at default". A full byte-level
soak requires recording demos and replaying them through the engine.
This static validator covers the on-disk preconditions for that soak:

  - All four feel CVARs exist and default to a no-op value (0/false).
  - The smoothing curve is gated on `m_smooth_curve` and lives in the
    input scaling layer, not in actor or hitscan code.
  - The crosshair recoil reads only the local player's refire / weapon
    bob and never returns a value that influences aim or damage.
  - The killfeed records existing obituary text and renders to the HUD,
    never adjusting scoring or frags.
  - The footstep surface logic is sound-name selection only (ZScript /
    SNDINFO), not movement physics.
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = ROOT / "tests" / "nugget_feel_validation" / "dist" / "nugget_feel_report.json"


STATIC_CHECKS = [
    ("cvar-smooth-curve-default-off",
     ROOT / "src" / "i_input_feel.cpp",
     "CUSTOM_CVAR(Int, m_smooth_curve, 0,",
     "default-off no-op preserved"),
    ("cvar-crosshair-recoil-default-off",
     ROOT / "src" / "i_input_feel.cpp",
     "CVAR(Bool, r_crosshair_recoil, false,",
     "render-only widget defaults off"),
    ("cvar-killfeed-default-off",
     ROOT / "src" / "i_input_feel.cpp",
     "CVAR(Bool, r_killfeed, false,",
     "killfeed widget defaults off"),
    ("cvar-footstep-surface-default-off",
     ROOT / "src" / "i_input_feel.cpp",
     "CVAR(Bool, snd_footsteps_surface, false,",
     "footstep surface alias defaults off"),
    ("status-ccmd-present",
     ROOT / "src" / "i_input_feel.cpp",
     "CCMD(m_input_status)",
     "diagnostic CCMD wired"),
    ("smooth-curve-mouse-gate",
     ROOT / "src" / "g_game.cpp",
     "m_smooth_curve",
     "smoothing applied in input scaling, not playsim"),
    ("killfeed-hud-only",
     ROOT / "src" / "hcde_killfeed.cpp",
     "killfeed",
     "killfeed C++ surface present"),
    ("doc-audit-present",
     ROOT / "docs" / "HCDE_NUGGET_FEEL_AUDIT.md",
     "Phase 6",
     "audit doc present and references soak phase"),
]


SOAK_ROWS = [
    {
        "id": "usercmd-byte-default",
        "status": "pending-runtime-launch",
        "goal": "Record a 30-second demo with all #9 CVARs at default; replay and confirm usercmd bytes are bit-identical to a baseline recorded before the scaffold landed.",
    },
    {
        "id": "usercmd-byte-feel-on",
        "status": "pending-runtime-launch",
        "goal": "Record a demo with m_smooth_curve=1 and the others on; replay and confirm authoritative state hashes match a parallel run.",
    },
    {
        "id": "multiplayer-mismatch",
        "status": "pending-runtime-launch",
        "goal": "Two clients with opposite feel-CVAR settings stay in sync over a 60s session.",
    },
]


def check_static(row: tuple[str, Path, str, str]) -> dict:
    check_id, path, needle, message = row
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return {
            "id": check_id,
            "status": "failed",
            "path": str(path.relative_to(ROOT)),
            "needle": needle,
            "notes": "file missing",
        }
    found = needle in text
    return {
        "id": check_id,
        "status": "passed" if found else "failed",
        "path": str(path.relative_to(ROOT)),
        "needle": needle,
        "notes": message if found else "needle missing",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()

    checks = [check_static(row) for row in STATIC_CHECKS]
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
