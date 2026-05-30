#!/usr/bin/env python3
"""Validate Eternity Spatial Audio backend facade scaffolds (#3).

The Phase 1 facade lives in src/common/audio/sound/s_sound_eternity.cpp.
This validator runs the static contract checks called for in the audit:

  - The facade exposes the documented entry points.
  - The backend selector in i_sound.cpp routes `snd_backend=eternity`
    to the facade rather than the OpenAL/Null fallbacks.
  - The boundary contract in HCDE_FEATURE_IMPORTS.md still says
    "no replicated audio state" and the facade doesn't accidentally
    reach into snapshots or savegames.

A real Eternity mixer requires vendoring upstream code; this validator
documents that the engine surface is ready for it.
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = ROOT / "tests" / "eternity_audio_validation" / "dist" / "eternity_audio_report.json"


STATIC_CHECKS = [
    ("facade-source-present",
     ROOT / "src" / "common" / "audio" / "sound" / "s_sound_eternity.cpp",
     "EternitySoundRenderer",
     "facade class registered"),
    ("facade-factory-export",
     ROOT / "src" / "common" / "audio" / "sound" / "s_sound_eternity.cpp",
     "I_CreateEternitySoundRenderer",
     "factory function exported"),
    ("facade-status-ccmd",
     ROOT / "src" / "common" / "audio" / "sound" / "s_sound_eternity.cpp",
     "CCMD(snd_eternity_status)",
     "diagnostic CCMD wired"),
    ("facade-probe-ccmd",
     ROOT / "src" / "common" / "audio" / "sound" / "s_sound_eternity.cpp",
     "CCMD(snd_eternity_probe)",
     "diagnostic probe CCMD wired"),
    ("facade-selector-routed",
     ROOT / "src" / "common" / "audio" / "sound" / "i_sound.cpp",
     "I_CreateEternitySoundRenderer",
     "selector routes snd_backend=eternity"),
    ("facade-spatial-tracking",
     ROOT / "src" / "common" / "audio" / "sound" / "s_sound_eternity.cpp",
     "FEternitySpatialSubmit",
     "facade records spatial submits"),
    ("facade-no-snapshot-include",
     ROOT / "src" / "common" / "audio" / "sound" / "s_sound_eternity.cpp",
     "d_net_snapshot",
     "facade does NOT include net snapshot headers (boundary check)",
     "needle-must-be-absent"),
    ("facade-no-save-include",
     ROOT / "src" / "common" / "audio" / "sound" / "s_sound_eternity.cpp",
     "p_saveg",
     "facade does NOT include savegame headers (boundary check)",
     "needle-must-be-absent"),
    ("facade-openal-reverb-note",
     ROOT / "src" / "common" / "audio" / "sound" / "s_sound_eternity.cpp",
     "use snd_backend=openal for full audio with snd_env_reverb support",
     "facade points reverb users at the OpenAL EFX path"),
    ("doc-boundary-contract",
     ROOT / "docs" / "HCDE_FEATURE_IMPORTS.md",
     "Eternity Spatial Audio (facade boundary)",
     "audit doc references facade boundary"),
    ("plan-ready-status",
     ROOT / "docs" / "HCDE_ROADMAP_INTEGRATION_PLAN.md",
     "snd_backend=eternity",
     "roadmap plan references the facade"),
]


SOAK_ROWS = [
    {
        "id": "registers-on-launch",
        "status": "pending-runtime-launch",
        "goal": "Launch with +snd_backend eternity and confirm snd_eternity_status reports 'registration attempted = yes' and 'renderer constructed = yes'.",
    },
    {
        "id": "diagnostic-probe",
        "status": "pending-runtime-launch",
        "goal": "Run snd_eternity_probe and confirm the diagnostic counter increments without warnings.",
    },
    {
        "id": "fallback-when-unselected",
        "status": "pending-runtime-launch",
        "goal": "Launch with default snd_backend; verify Eternity is NOT selected and OpenAL/Null is active. snd_eternity_status should report 'selected = no'.",
    },
    {
        "id": "vendor-real-mixer",
        "status": "blocked-on-vendoring",
        "goal": "Vendor the Eternity mixer source (or compatible library); replace the silent facade body with real audio output.",
    },
]


def check_static(row: tuple) -> dict:
    if len(row) == 4:
        check_id, path, needle, message = row
        mode = "needle-must-be-present"
    else:
        check_id, path, needle, message, mode = row

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
    if mode == "needle-must-be-absent":
        ok = not found
        notes = message if ok else f"needle '{needle}' present (boundary violation)"
    else:
        ok = found
        notes = message if ok else "needle missing"

    return {
        "id": check_id,
        "status": "passed" if ok else "failed",
        "path": str(path.relative_to(ROOT)),
        "needle": needle,
        "mode": mode,
        "notes": notes,
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
