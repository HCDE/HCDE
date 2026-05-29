#!/usr/bin/env python3
"""Run HCDE's generated ID24 smoke validation pack."""

from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import threading
import time
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


def read_stream_lines(proc: subprocess.Popen[str], sink: list[str]) -> None:
    assert proc.stdout is not None
    for line in proc.stdout:
        sink.append(line.rstrip("\r\n"))


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
        cmd.append("-file")
        cmd.extend(str(path) for path in case["files"])
    cmd.extend(case["args"])

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
    reader = threading.Thread(target=read_stream_lines, args=(proc, log_lines), daemon=True)
    reader.start()

    matched_pass: set[str] = set()
    matched_fail_lines: list[str] = []
    status = "timeout"
    notes = ""

    try:
        while True:
            for line in log_lines:
                for marker in case["fail_markers"]:
                    if marker in line and line not in matched_fail_lines:
                        matched_fail_lines.append(line)
                for marker in case["pass_markers"]:
                    if marker in line:
                        matched_pass.add(marker)

            if matched_fail_lines:
                status = "failed"
                notes = "fail marker found"
                break
            if case["pass_markers"] and len(matched_pass) == len(case["pass_markers"]):
                status = "passed"
                notes = "all pass markers matched"
                break
            if proc.poll() is not None:
                if case["pass_markers"] and len(matched_pass) == len(case["pass_markers"]):
                    status = "passed"
                    notes = "process exited after matching pass markers"
                else:
                    status = "failed"
                    notes = f"process exited before pass markers (exit={proc.returncode})"
                break
            if time.monotonic() - start > case["timeout_seconds"]:
                status = "timeout"
                notes = f"timed out after {case['timeout_seconds']}s"
                break
            time.sleep(0.05)
    finally:
        terminate_process(proc)
        reader.join(timeout=1)

    return CaseResult(
        case_id=case_id,
        description=case["description"],
        status=status,
        duration_seconds=round(time.monotonic() - start, 3),
        command=cmd,
        matched_pass_markers=sorted(matched_pass),
        matched_fail_lines=matched_fail_lines,
        notes=notes,
        log_tail=log_lines[-80:],
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
    print(f"Wrote report: {args.report}")
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
