#!/usr/bin/env python3
"""HCDE NanoBSP comparison harness scaffold.

This harness verifies that the NanoBSP reference source and HCDE-native adapter
are present, records source hashes, loads a manifest of deterministic
comparison cases, and writes a JSON report that queues the cases for an engine
run. The actual ZDBSP-vs-NanoBSP comparison requires launching HCDE so the
maploader can build both node sets from real WAD data.

The eventual engine-backed runner compares:

  - existing ZDBSP-style builder subsector/seg counts
  - NanoBSP subsector/seg counts
  - line-to-seg mapping hashes
  - fixed line-of-sight sample results
  - build time per map
"""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = ROOT / "tests" / "nanobsp_validation"
DEFAULT_MANIFEST = SCRIPT_DIR / "nanobsp_compare_manifest.json"
DEFAULT_REPORT = SCRIPT_DIR / "dist" / "nanobsp_compare_report.json"
VENDOR_DIR = ROOT / "src" / "nodebuilders" / "nanobsp"
VENDOR_FILES = [
    VENDOR_DIR / "nano_bsp.c",
    VENDOR_DIR / "nano_bsp.h",
]
STATIC_CHECKS = [
    (
        "loader-preflight",
        ROOT / "src" / "d_nanobsp_loader.cpp",
        "PreflightAdapterInput",
        "runtime dispatch classifies adapter-ready vs blocked input",
    ),
    (
        "loader-blocker-mask",
        ROOT / "src" / "d_nanobsp_loader.cpp",
        "HNBLOCK_POLYOBJECTS",
        "polyobject cases are explicitly tracked before adapter enablement",
    ),
    (
        "loader-history-blockers",
        ROOT / "src" / "d_nanobsp_loader.cpp",
        "blockers=0x%02x",
        "r_nanobsp_status exposes the adapter blocker mask",
    ),
    (
        "loader-emits-native-arrays",
        ROOT / "src" / "d_nanobsp_loader.cpp",
        "NanoBSPExtractToLevel",
        "adapter emits vertices/segs/subsectors/nodes into FLevelLocals",
    ),
    (
        "loader-success-record",
        ROOT / "src" / "d_nanobsp_loader.cpp",
        "adapter-emitted:hcde-nanobsp",
        "r_nanobsp_status can report successful adapter emission",
    ),
]


def now_utc_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def sha256_file(path: Path) -> str | None:
    if not path.exists():
        return None
    hasher = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def load_manifest(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def inspect_vendor() -> dict[str, Any]:
    files = []
    missing = []
    for path in VENDOR_FILES:
        rel = str(path.relative_to(ROOT))
        digest = sha256_file(path)
        files.append(
            {
                "path": rel,
                "exists": digest is not None,
                "sha256": digest,
            }
        )
        if digest is None:
            missing.append(rel)
    return {
        "status": "missing" if missing else "present",
        "missing": missing,
        "files": files,
    }


def run_static_checks() -> list[dict[str, Any]]:
    checks = []
    for check_id, path, needle, notes in STATIC_CHECKS:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except FileNotFoundError:
            checks.append(
                {
                    "id": check_id,
                    "status": "failed",
                    "path": str(path.relative_to(ROOT)),
                    "needle": needle,
                    "notes": "file missing",
                }
            )
            continue

        found = needle in text
        checks.append(
            {
                "id": check_id,
                "status": "passed" if found else "failed",
                "path": str(path.relative_to(ROOT)),
                "needle": needle,
                "notes": notes if found else "needle missing",
            }
        )
    return checks


def build_case_result(case: dict[str, Any], adapter_available: bool) -> dict[str, Any]:
    if not adapter_available:
        return {
            "id": case["id"],
            "description": case.get("description", ""),
            "status": "skipped",
            "reason": "adapter-missing",
            "map": case.get("map", ""),
            "los_samples": case.get("los_samples", []),
            "zdbsp": None,
            "nanobsp": None,
            "comparison": None,
        }

    return {
        "id": case["id"],
        "description": case.get("description", ""),
        "status": "queued",
        "reason": "engine-run-required",
        "map": case.get("map", ""),
        "los_samples": case.get("los_samples", []),
        "zdbsp": None,
        "nanobsp": None,
        "comparison": {
            "status": "pending",
            "required_command_flow": [
                "run HCDE with hcde_nanobsp_loader=0 and capture baseline report",
                "run HCDE with hcde_nanobsp_loader=1 in software-renderer mode",
                "compare subsector_count, seg_count, line_mapping_hash, los_samples, and load_time_ms",
            ],
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    vendor = inspect_vendor()
    static_checks = run_static_checks()
    adapter_available = all(check["status"] == "passed" for check in static_checks)
    results = [
        build_case_result(case, adapter_available)
        for case in manifest.get("cases", [])
    ]

    payload = {
        "generated_at": now_utc_iso(),
        "manifest": str(args.manifest),
        "vendor": vendor,
        "static_checks": static_checks,
        "adapter_available": adapter_available,
        "adapter_contract": {
            "status": "hcde-native-adapter-present" if adapter_available else "adapter-static-check-failed",
            "blocker_mask_bits": [
                "empty-geometry",
                "missing-array",
                "polyobjects",
                "emit-failed",
            ],
            "runtime_status_command": "r_nanobsp_status",
        },
        "single_player_soak": {
            "status": "pending-engine-run",
            "report_fields": [
                "map",
                "builder",
                "subsector_count",
                "seg_count",
                "line_mapping_hash",
                "los_sample_mismatches",
                "build_time_ms",
                "visible_rendering_notes",
            ],
        },
        "multiplayer_late_join_soak": {
            "status": "pending-engine-run",
            "report_fields": [
                "server_builder",
                "client_builder",
                "map",
                "snapshot_index_match",
                "late_join_success",
                "state_hash_match",
                "disconnect_reason",
            ],
        },
        "results": results,
    }

    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(payload, f, indent=2)
        f.write("\n")

    print(f"NanoBSP vendor status: {vendor['status']}")
    for check in static_checks:
        print(f"[{check['status'].upper()}] {check['id']}: {check['notes']}")
    for result in results:
        print(f"[{result['status'].upper()}] {result['id']}: {result['reason']}")
    print(f"Wrote report: {args.report}")
    failed_static = any(check["status"] != "passed" for check in static_checks)
    return 0 if vendor["status"] == "present" and not failed_static else 1


if __name__ == "__main__":
    raise SystemExit(main())
