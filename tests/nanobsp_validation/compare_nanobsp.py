#!/usr/bin/env python3
"""HCDE NanoBSP comparison harness scaffold.

This harness is intentionally runnable before the NanoBSP adapter is wired.
It verifies that the upstream vendor files are present, records their hashes,
loads a manifest of deterministic comparison cases, and writes a JSON report
that marks each case as `skipped` with reason `adapter-missing`.

Once `src/utility/nodebuilder/nano/` has an HCDE data-structure adapter, this
script becomes the runner that compares:

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
VENDOR_DIR = ROOT / "src" / "utility" / "nodebuilder" / "nano"
VENDOR_FILES = [
    VENDOR_DIR / "nano_bsp.c",
    VENDOR_DIR / "nano_bsp.h",
    VENDOR_DIR / "README.md",
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

    raise NotImplementedError("NanoBSP adapter execution is not wired yet")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    vendor = inspect_vendor()
    adapter_available = False
    results = [
        build_case_result(case, adapter_available)
        for case in manifest.get("cases", [])
    ]

    payload = {
        "generated_at": now_utc_iso(),
        "manifest": str(args.manifest),
        "vendor": vendor,
        "adapter_available": adapter_available,
        "single_player_soak": {
            "status": "pending-adapter",
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
            "status": "pending-adapter",
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
    for result in results:
        print(f"[{result['status'].upper()}] {result['id']}: {result['reason']}")
    print(f"Wrote report: {args.report}")
    return 0 if vendor["status"] == "present" else 1


if __name__ == "__main__":
    raise SystemExit(main())
