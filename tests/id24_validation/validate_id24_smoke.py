#!/usr/bin/env python3
"""Run HCDE's generated ID24 smoke validation pack."""

from __future__ import annotations

import argparse
import json
import os
import queue
import signal
import subprocess
import sys
import threading
import time
import re
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = ROOT / "tests" / "id24_validation"
DIST = SCRIPT_DIR / "dist"
BUILD_SCRIPT = SCRIPT_DIR / "build_id24_validation_pack.py"


@dataclass
class CaseResult:
    case_id: str
    description: str
    status: str
    duration_seconds: float
    command: list[str]
    matched_pass_markers: list[str]
    matched_fail_lines: list[str]
    notes: str
    log_tail: list[str]


def now_utc_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def read_json(path: Path) -> dict[str, Any]:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise RuntimeError(f"manifest not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"manifest is invalid JSON: {path}: {exc}") from exc


def replace_tokens(value: str, hcde: Path, iwad: Path | None) -> str:
    resolved = value.replace("{root}", str(ROOT))
    resolved = resolved.replace("{hcde}", str(hcde))
    if iwad is not None:
        resolved = resolved.replace("{iwad}", str(iwad))
    return resolved


def resolve_repo_path(value: str) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return ROOT / path


def build_validation_pack() -> None:
    proc = subprocess.run(
        [sys.executable, str(BUILD_SCRIPT)],
        cwd=str(ROOT),
        check=False,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            "failed to build ID24 validation pack:\n"
            f"{proc.stdout}\n{proc.stderr}"
        )
    if proc.stdout.strip():
        print(proc.stdout.strip())


def load_manifest_cases(manifest_path: Path, hcde: Path, iwad: Path | None) -> list[dict[str, Any]]:
    raw = read_json(manifest_path)
    cases_raw = raw.get("cases")
    if not isinstance(cases_raw, list):
        raise RuntimeError(f"manifest missing 'cases' list: {manifest_path}")

    normalized: list[dict[str, Any]] = []
    for index, case in enumerate(cases_raw):
        if not isinstance(case, dict):
            raise RuntimeError(f"manifest case at index {index} is not an object")

        files = []
        for entry in case.get("files", []):
            files.append(resolve_repo_path(replace_tokens(str(entry), hcde, iwad)))

        iwad_value = case.get("iwad")
        iwad_path = None
        if iwad_value is not None:
            iwad_path = Path(replace_tokens(str(iwad_value), hcde, iwad))
        elif iwad is not None:
            iwad_path = iwad

        normalized.append(
            {
                "id": str(case.get("id", f"case-{index + 1}")),
                "description": str(case.get("description", "")),
                "enabled": bool(case.get("enabled", True)),
                "timeout_seconds": float(case.get("timeout_seconds", 45)),
                "skip_if_missing_files": bool(case.get("skip_if_missing_files", True)),
                "iwad_path": iwad_path,
                "files": files,
                "mode": str(case.get("mode", "parse")),
                "args": [replace_tokens(str(x), hcde, iwad) for x in case.get("args", [])],
                "pass_markers": [str(x) for x in case.get("pass_markers", [])],
                "fail_markers": [str(x) for x in case.get("fail_markers", [])],
            }
        )
    return normalized


def terminate_process(proc: subprocess.Popen[str]) -> None:
    if proc.poll() is not None:
        return
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


def run_case(hcde: Path, case: dict[str, Any], workdir: Path) -> CaseResult:
    case_id = case["id"]
    if not case["enabled"]:
        return CaseResult(case_id, case["description"], "skipped", 0.0, [], [], [], "case disabled", [])

    missing = []
    iwad_path: Path | None = case["iwad_path"]
    if iwad_path is not None and not iwad_path.exists():
        missing.append(str(iwad_path))
    for path in case["files"]:
        if not path.exists():
            missing.append(str(path))
    if missing and case["skip_if_missing_files"]:
        return CaseResult(case_id, case["description"], "skipped", 0.0, [], [], [], f"missing files: {', '.join(missing)}", [])
    if missing:
        return CaseResult(case_id, case["description"], "error", 0.0, [], [], [], f"missing required files: {', '.join(missing)}", [])

    cmd = [str(hcde)]
    if iwad_path is not None:
        cmd.extend(["-iwad", str(iwad_path)])
    if case["files"]:
        for f in case["files"]:
            cmd.extend(["-file", str(f)])
    # Parse-mode cases use the engine's documented "compile-only" path (see
    # FArg_norun in src/d_main.cpp): it bails just before video init, after all
    # WADs are mounted and all scripts/DEH/MAPINFO are parsed. Runtime cases
    # intentionally omit -norun and are terminated by this harness as soon as
    # their pass marker appears.
    if case["mode"] == "parse":
        cmd.append("-norun")
    elif case["mode"] != "runtime":
        return CaseResult(case_id, case["description"], "error", 0.0, [], [], [], f"unknown mode: {case['mode']}", [])
    cmd.extend(case["args"])

    start = time.monotonic()
    proc = subprocess.Popen(
        cmd,
        cwd=str(workdir),
        stdout=subprocess.PIPE,  # Capture stdout
        stderr=subprocess.STDOUT,  # Redirect stderr to stdout
        text=True,
        encoding="utf-8",
        errors="replace",
    )

    matched_pass: set[str] = set()
    matched_fail_lines: list[str] = []
    # status is decided by the post-loop classifier; "unknown" is just a
    # placeholder. The previous default of "timeout" was sticky: when the
    # process exited cleanly with no markers, every branch in the classifier
    # short-circuited and the case was misreported as a timeout even though
    # nothing actually timed out.
    status = "unknown"
    notes = ""
    output_lines: list[str] = []
    timed_out = False
    line_queue: queue.Queue[str] = queue.Queue()

    def enqueue_output() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            line_queue.put(line)

    def process_line(line: str) -> None:
        stripped = line.strip()
        if stripped:
            output_lines.append(stripped)
        for marker in case["pass_markers"]:
            if marker in line:
                matched_pass.add(marker)
        for marker in case["fail_markers"]:
            if marker in line and line not in matched_fail_lines:
                matched_fail_lines.append(line)
    reader: threading.Thread | None = None

    try:
        # Read output through a background thread so runtime cases cannot hang
        # forever inside readline() if the engine keeps running but goes quiet.
        reader = threading.Thread(target=enqueue_output, daemon=True)
        reader.start()
        while proc.poll() is None:
            try:
                process_line(line_queue.get(timeout=0.05))
            except queue.Empty:
                pass

            if case["pass_markers"] and len(matched_pass) == len(case["pass_markers"]):
                break

            if time.monotonic() - start > case["timeout_seconds"]:
                timed_out = True
                status = "timeout"
                notes = f"timed out after {case['timeout_seconds']}s"
                break

        # Process any queued output before terminating/after process exit.
        while True:
            try:
                process_line(line_queue.get_nowait())
            except queue.Empty:
                break

    finally:
        terminate_process(proc)
        if reader is not None:
            reader.join(timeout=1.0)
        while True:
            try:
                process_line(line_queue.get_nowait())
            except queue.Empty:
                break

    # Classifier precedence:
    #   1. Any fail marker fires => failed (regardless of pass markers, since
    #      an "Unknown command" line means the run was structurally broken).
    #   2. All pass markers matched => passed.
    #   3. Hit the wall clock => timeout (set above before break).
    #   4. Process exited cleanly but never produced a pass marker => failed.
    #   5. Otherwise non-zero exit code => failed with the exit code.
    if matched_fail_lines:
        status = "failed"
        notes = "fail marker found"
    elif case["pass_markers"] and len(matched_pass) == len(case["pass_markers"]):
        status = "passed"
        notes = "all pass markers matched"
    elif timed_out:
        # status / notes already set when the timeout fired.
        pass
    elif proc.returncode != 0:
        status = "failed"
        notes = f"process exited with non-zero code {proc.returncode}"
    else:
        status = "failed"
        notes = "process exited before pass markers"

    return CaseResult(
        case_id=case_id,
        description=case["description"],
        status=status,
        duration_seconds=round(time.monotonic() - start, 3),
        command=cmd,
        matched_pass_markers=sorted(matched_pass),
        matched_fail_lines=matched_fail_lines,
        notes=notes,
        log_tail=output_lines[-80:] if output_lines else [],
    )


def write_report(report_path: Path, results: list[CaseResult], manifest_path: Path, hcde: Path, iwad: Path | None) -> None:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "generated_at": now_utc_iso(),
        "manifest": str(manifest_path),
        "hcde": str(hcde),
        "iwad": str(iwad) if iwad is not None else None,
        "results": [asdict(r) for r in results],
    }
    with report_path.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(payload, f, indent=2)
        f.write("\n")


def default_hcde_path() -> Path:
    exe = "hcde.exe" if os.name == "nt" else "hcde"
    return ROOT / "build" / "RelWithDebInfo" / exe


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hcde", type=Path, default=default_hcde_path(), help="HCDE executable to run.")
    parser.add_argument("--iwad", type=Path, default=ROOT / "doom2.wad", help="IWAD used for smoke runs.")
    parser.add_argument("--manifest", type=Path, default=DIST / "manifest.json", help="Generated ID24 manifest.")
    parser.add_argument("--report", type=Path, default=DIST / "report.json", help="JSON report path.")
    parser.add_argument("--no-build", action="store_true", help="Do not rebuild the generated fixture pack first.")
    args = parser.parse_args()

    if not args.no_build:
        build_validation_pack()

    hcde = args.hcde.resolve()
    if not hcde.exists():
        raise RuntimeError(f"HCDE executable not found: {hcde}")

    iwad = args.iwad.resolve() if args.iwad is not None else None
    cases = load_manifest_cases(args.manifest, hcde, iwad)
    results = [run_case(hcde, case, ROOT) for case in cases]
    write_report(args.report, results, args.manifest, hcde, iwad)

    failed = [r for r in results if r.status in {"failed", "timeout", "error"}]
    for result in results:
        print(f"[{result.status.upper()}] {result.case_id}: {result.notes}")
        if result.log_tail:
            print("--- Log Tail ---")
            for line in result.log_tail:
                print(line)
            print("----------------")
    print(f"Wrote report: {args.report}")
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
