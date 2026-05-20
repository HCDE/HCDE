#!/usr/bin/env python3
"""HCDE MBF21 Stage 1 baseline and smoke-test harness.

Stage 1 goals:
1. Rebuild the generated MBF21 validation pack deterministically.
2. Run one or more manifest-driven validation cases.
3. Emit a machine-readable report so later stages can compare against a known
   baseline before/after compatibility changes.
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = ROOT / "tests" / "mbf21_validation"
DEFAULT_MANIFEST = SCRIPT_DIR / "mbf21_stage1_suite.json"
DEFAULT_REPORT = SCRIPT_DIR / "dist" / "mbf21_stage1_report.json"
BUILD_SCRIPT = SCRIPT_DIR / "build_mbf21_validation_pack.py"


class Stage1Error(RuntimeError):
    """Raised when Stage 1 harness setup or execution fails."""


@dataclass
class CaseResult:
    """Result object for one MBF21 case execution."""

    case_id: str
    description: str
    status: str
    duration_seconds: float
    command: list[str]
    matched_pass_markers: list[str]
    matched_fail_lines: list[str]
    notes: str
    log_tail: list[str]


def _now_utc_iso() -> str:
    """Return an RFC3339-ish UTC timestamp for report metadata."""
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def _read_json(path: Path) -> dict[str, Any]:
    """Load JSON from disk with a clear Stage 1 error message on failure."""
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise Stage1Error(f"manifest not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise Stage1Error(f"manifest is invalid JSON: {path}: {exc}") from exc


def _replace_tokens(value: str, hcde: Path, iwad: Path | None) -> str:
    """Expand simple manifest tokens into absolute paths."""
    resolved = value.replace("{root}", str(ROOT))
    resolved = resolved.replace("{hcde}", str(hcde))
    if iwad is not None:
        resolved = resolved.replace("{iwad}", str(iwad))
    return resolved


def _load_manifest_cases(manifest_path: Path, hcde: Path, iwad: Path | None) -> list[dict[str, Any]]:
    """Parse and normalize manifest case entries."""
    raw = _read_json(manifest_path)
    cases_raw = raw.get("cases")
    if not isinstance(cases_raw, list):
        raise Stage1Error(f"manifest missing 'cases' list: {manifest_path}")

    normalized: list[dict[str, Any]] = []
    for index, case in enumerate(cases_raw):
        if not isinstance(case, dict):
            raise Stage1Error(f"manifest case at index {index} is not an object")

        case_id = str(case.get("id", f"case-{index+1}"))
        description = str(case.get("description", case_id))
        enabled = bool(case.get("enabled", True))
        timeout_seconds = float(case.get("timeout_seconds", 30))
        pass_markers = [str(x) for x in case.get("pass_markers", ["HCDE_MBF21_VALIDATION: PASS"])]
        fail_markers = [str(x) for x in case.get("fail_markers", ["HCDE_MBF21_VALIDATION: FAIL"])]
        prebuild = bool(case.get("prebuild_validation_pack", False))
        skip_if_missing_files = bool(case.get("skip_if_missing_files", True))

        iwad_value = case.get("iwad")
        iwad_path = None
        if iwad_value is not None:
            iwad_path = Path(_replace_tokens(str(iwad_value), hcde, iwad))
        elif iwad is not None:
            iwad_path = iwad

        files = []
        for entry in case.get("files", []):
            files.append(Path(_replace_tokens(str(entry), hcde, iwad)))

        args = []
        for entry in case.get("args", []):
            args.append(_replace_tokens(str(entry), hcde, iwad))

        normalized.append(
            {
                "id": case_id,
                "description": description,
                "enabled": enabled,
                "timeout_seconds": timeout_seconds,
                "pass_markers": pass_markers,
                "fail_markers": fail_markers,
                "prebuild_validation_pack": prebuild,
                "skip_if_missing_files": skip_if_missing_files,
                "iwad_path": iwad_path,
                "files": files,
                "args": args,
            }
        )

    return normalized


def _build_validation_pack(force: bool) -> None:
    """Run the MBF21 pack builder script when requested by the harness."""
    cmd = [sys.executable, str(BUILD_SCRIPT)]
    if force:
        print("[mbf21-stage1] Rebuilding validation pack...")
    proc = subprocess.run(cmd, cwd=str(ROOT), check=False, text=True, capture_output=True)
    if proc.returncode != 0:
        raise Stage1Error(
            "failed to build MBF21 validation pack:\n"
            f"{proc.stdout}\n{proc.stderr}"
        )
    if proc.stdout.strip():
        print(proc.stdout.strip())


def _terminate_process(proc: subprocess.Popen[str]) -> None:
    """Stop a spawned HCDE process cleanly, then force-kill if needed."""
    if proc.poll() is not None:
        return

    # Try polite termination first.
    if os.name == "nt":
        proc.send_signal(signal.CTRL_BREAK_EVENT if hasattr(signal, "CTRL_BREAK_EVENT") else signal.SIGTERM)
    else:
        proc.terminate()

    try:
        proc.wait(timeout=3)
        return
    except subprocess.TimeoutExpired:
        pass

    proc.kill()
    proc.wait(timeout=5)


def _read_stream_lines(proc: subprocess.Popen[str], sink: list[str]) -> None:
    """Collect process output lines in a shared list."""
    assert proc.stdout is not None
    for line in proc.stdout:
        sink.append(line.rstrip("\r\n"))


def _run_case(hcde: Path, case: dict[str, Any], workdir: Path) -> CaseResult:
    """Run one MBF21 validation case and evaluate PASS/FAIL markers."""
    case_id = case["id"]
    description = case["description"]
    timeout_seconds = case["timeout_seconds"]
    pass_markers = case["pass_markers"]
    fail_markers = case["fail_markers"]
    iwad_path: Path | None = case["iwad_path"]
    files: list[Path] = case["files"]
    args: list[str] = case["args"]
    skip_if_missing = case["skip_if_missing_files"]

    missing = []
    if iwad_path is not None and not iwad_path.exists():
        missing.append(str(iwad_path))
    for path in files:
        if not path.exists():
            missing.append(str(path))

    if missing and skip_if_missing:
        return CaseResult(
            case_id=case_id,
            description=description,
            status="skipped",
            duration_seconds=0.0,
            command=[],
            matched_pass_markers=[],
            matched_fail_lines=[],
            notes=f"required files missing: {', '.join(missing)}",
            log_tail=[],
        )
    if missing:
        return CaseResult(
            case_id=case_id,
            description=description,
            status="error",
            duration_seconds=0.0,
            command=[],
            matched_pass_markers=[],
            matched_fail_lines=[],
            notes=f"required files missing and skip disabled: {', '.join(missing)}",
            log_tail=[],
        )

    cmd: list[str] = [str(hcde)]
    if iwad_path is not None:
        cmd.extend(["-iwad", str(iwad_path)])
    if files:
        cmd.append("-file")
        cmd.extend(str(path) for path in files)
    cmd.extend(args)

    log_lines: list[str] = []
    start = time.monotonic()

    proc = subprocess.Popen(
        cmd,
        cwd=str(workdir),
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=(subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0),
        bufsize=1,
    )

    reader = threading.Thread(target=_read_stream_lines, args=(proc, log_lines), daemon=True)
    reader.start()

    matched_pass: set[str] = set()
    matched_fail_lines: list[str] = []
    status = "timeout"
    notes = ""

    try:
        while True:
            for line in log_lines:
                if line not in matched_fail_lines:
                    for marker in fail_markers:
                        if marker in line:
                            matched_fail_lines.append(line)
                            break
                for marker in pass_markers:
                    if marker in line:
                        matched_pass.add(marker)

            if matched_fail_lines:
                status = "failed"
                notes = "detected fail marker(s) in output"
                break

            if all(marker in matched_pass for marker in pass_markers):
                status = "passed"
                notes = "all pass markers matched"
                break

            if proc.poll() is not None:
                status = "failed"
                notes = f"process exited before matching pass markers (exit={proc.returncode})"
                break

            if time.monotonic() - start > timeout_seconds:
                status = "timeout"
                notes = "case timed out before pass markers appeared"
                break

            time.sleep(0.05)
    finally:
        _terminate_process(proc)
        reader.join(timeout=2)

    duration = time.monotonic() - start
    return CaseResult(
        case_id=case_id,
        description=description,
        status=status,
        duration_seconds=round(duration, 3),
        command=cmd,
        matched_pass_markers=sorted(matched_pass),
        matched_fail_lines=matched_fail_lines,
        notes=notes,
        log_tail=log_lines[-80:],
    )


def _write_report(report_path: Path, payload: dict[str, Any]) -> None:
    """Write JSON report with stable formatting for review/diffing."""
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--hcde",
        type=Path,
        default=ROOT / "build" / "RelWithDebInfo" / "hcde.exe",
        help="Path to hcde.exe used for validation runs.",
    )
    parser.add_argument(
        "--iwad",
        type=Path,
        default=Path(r"C:\Users\user\Downloads\DOOM2.WAD"),
        help="Path to IWAD used for validation cases.",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=DEFAULT_MANIFEST,
        help="Path to MBF21 Stage 1 case manifest JSON.",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=DEFAULT_REPORT,
        help="Path to JSON report output.",
    )
    parser.add_argument(
        "--workdir",
        type=Path,
        default=ROOT,
        help="Working directory for launched HCDE runs.",
    )
    parser.add_argument(
        "--rebuild-pack",
        action="store_true",
        help="Rebuild the generated MBF21 validation pack before running cases.",
    )
    args = parser.parse_args()

    hcde = args.hcde.resolve()
    if not hcde.exists():
        raise Stage1Error(f"hcde binary not found: {hcde}")

    iwad = args.iwad.resolve() if args.iwad is not None else None
    manifest = args.manifest.resolve()
    report_path = args.report.resolve()
    workdir = args.workdir.resolve()

    cases = _load_manifest_cases(manifest, hcde, iwad)
    if args.rebuild_pack or any(case["prebuild_validation_pack"] for case in cases):
        _build_validation_pack(force=True)

    enabled_cases = [case for case in cases if case["enabled"]]
    if not enabled_cases:
        raise Stage1Error("manifest has no enabled cases")

    print(f"[mbf21-stage1] Running {len(enabled_cases)} case(s) from {manifest}")
    results: list[CaseResult] = []
    for case in enabled_cases:
        print(f"[mbf21-stage1] Case {case['id']}: {case['description']}")
        result = _run_case(hcde, case, workdir)
        results.append(result)
        print(f"[mbf21-stage1] -> {result.status} ({result.duration_seconds:.3f}s)")
        if result.notes:
            print(f"[mbf21-stage1]    {result.notes}")

    passed = sum(1 for result in results if result.status == "passed")
    failed = sum(1 for result in results if result.status == "failed")
    timed_out = sum(1 for result in results if result.status == "timeout")
    skipped = sum(1 for result in results if result.status == "skipped")
    errored = sum(1 for result in results if result.status == "error")

    overall_status = "passed"
    if failed or timed_out or errored:
        overall_status = "failed"

    report_payload = {
        "schema_version": 1,
        "stage": "mbf21-stage1-baseline",
        "timestamp_utc": _now_utc_iso(),
        "root": str(ROOT),
        "hcde": str(hcde),
        "iwad": str(iwad) if iwad is not None else "",
        "manifest": str(manifest),
        "workdir": str(workdir),
        "summary": {
            "overall_status": overall_status,
            "total": len(results),
            "passed": passed,
            "failed": failed,
            "timeout": timed_out,
            "skipped": skipped,
            "error": errored,
        },
        "cases": [
            {
                "id": result.case_id,
                "description": result.description,
                "status": result.status,
                "duration_seconds": result.duration_seconds,
                "command": result.command,
                "matched_pass_markers": result.matched_pass_markers,
                "matched_fail_lines": result.matched_fail_lines,
                "notes": result.notes,
                "log_tail": result.log_tail,
            }
            for result in results
        ],
    }
    _write_report(report_path, report_payload)
    print(f"[mbf21-stage1] Report written: {report_path}")

    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Stage1Error as exc:
        print(f"[mbf21-stage1] ERROR: {exc}", file=sys.stderr)
        raise SystemExit(2)
