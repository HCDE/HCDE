"""Step F6 runtime soak driver.

Spawns a single dedicated HCDE server and one to N synthetic clients, lets the
HCDE live capability handshake run, captures `net_profile` and `net_stressreport`
output, and writes per-run summary JSON. Unlike the step12 stress harness it
keeps clients alive for the full duration via an explicit per-tic loop instead
of relying on the `+wait` CCMD (which we observed quitting at clienttic=17 in
release builds).
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path
from typing import Optional


def _stream_to_file(stream, path: Path) -> threading.Thread:
    def pump():
        with path.open("w", encoding="utf-8", errors="replace", buffering=1) as fh:
            for line in iter(stream.readline, ""):
                fh.write(line)
                fh.flush()
    t = threading.Thread(target=pump, daemon=True)
    t.start()
    return t


def launch_server(bin_dir: Path, iwad: Path, port: int, gametype: int, map_name: str, log_path: Path) -> subprocess.Popen[str]:
    argv = [
        str(bin_dir / "hcdeserv.exe"),
        "-server", "8",
        "-iwad", str(iwad),
        "-port", str(port),
        "-noadvertise",
        "+sv_hostname", "HCDE F6 Soak",
        "+sv_gametype", str(gametype),
        "+map", map_name,
    ]
    proc = subprocess.Popen(
        argv,
        cwd=str(bin_dir),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )
    _stream_to_file(proc.stdout, log_path)
    return proc


def launch_client(bin_dir: Path, iwad: Path, port: int, index: int, log_path: Path) -> subprocess.Popen[str]:
    argv = [
        str(bin_dir / "hcde.exe"),
        "-join", f"127.0.0.1:{port}",
        "-dedicatedjoin",
        "-iwad", str(iwad),
        "-nosound", "-nomusic",
        "+name", f"F6Probe_{index + 1}",
    ]
    proc = subprocess.Popen(
        argv,
        cwd=str(bin_dir),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )
    _stream_to_file(proc.stdout, log_path)
    return proc


def send(proc: subprocess.Popen[str], command: str) -> None:
    if proc.stdin is None:
        return
    proc.stdin.write(command + "\n")
    proc.stdin.flush()


def tail(path: Path, lines: int = 80) -> list[str]:
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8", errors="replace").splitlines()
    return text[-lines:]


def parse_net_profile(lines: list[str]) -> dict[str, dict[str, int]]:
    out: dict[str, dict[str, int]] = {}
    capture = False
    for line in lines:
        s = line.strip()
        if s.startswith("HCDE net profile:"):
            capture = True
            continue
        if capture:
            if not s or s[0] != "":
                if not s.startswith(" ") and not line.startswith(" "):
                    if s and ":" not in s:
                        capture = False
                        continue
            for prefix in ("live:", "client-input:", "snapshots:", "competitive-player-lane:",
                           "authority-events:", "invasion:", "actor-index:", "shared-actors:",
                           "mode-migration:", "actor-queues:", "actor-interest:"):
                if s.startswith(prefix):
                    key = prefix[:-1]
                    bucket: dict[str, int] = {}
                    rest = s[len(prefix):].strip()
                    for token in rest.split():
                        if "=" in token:
                            k, _, v = token.partition("=")
                            try:
                                bucket[k] = int(v)
                            except ValueError:
                                pass
                    out[key] = bucket
                    break
            else:
                if not line.startswith(" "):
                    capture = False
    return out


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin-dir", required=True, type=Path)
    parser.add_argument("--iwad", required=True, type=Path)
    parser.add_argument("--port", type=int, default=18900)
    parser.add_argument("--client-count", type=int, default=1)
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument("--label", required=True)
    parser.add_argument("--trace-dir", required=True, type=Path)
    parser.add_argument("--gametype", type=int, default=0, help="0=coop, 1=dm, 2=invasion")
    parser.add_argument("--map", default="MAP01")
    parser.add_argument("--server-cvar", action="append", default=[],
                        help="CVAR assignment to send after server warmup, e.g. 'net_extratic 1'")
    parser.add_argument("--periodic-server-command", action="append", default=[],
                        help="Server console commands to fire every ~report_interval")
    parser.add_argument("--report-interval", type=float, default=10.0)
    args = parser.parse_args(argv)

    run_id = f"{args.label}_{int(time.time())}_{uuid.uuid4().hex[:6]}"
    trace_dir: Path = args.trace_dir / run_id
    trace_dir.mkdir(parents=True, exist_ok=True)

    server_log = trace_dir / "server.log"
    client_logs = [trace_dir / f"client_{i + 1}.log" for i in range(args.client_count)]

    print(f"[f6] {run_id}")
    print(f"[f6] trace -> {trace_dir}")

    server = launch_server(args.bin_dir, args.iwad, args.port, args.gametype, args.map, server_log)
    time.sleep(3.0)
    if server.poll() is not None:
        print("[f6] server died early")
        print("\n".join(tail(server_log, 40)))
        return 2

    send(server, "net_profile_reset")
    for cvar in args.server_cvar:
        send(server, cvar)
        time.sleep(0.1)
    time.sleep(0.5)

    clients: list[subprocess.Popen[str]] = []
    for i in range(args.client_count):
        clients.append(launch_client(args.bin_dir, args.iwad, args.port, i, client_logs[i]))
        time.sleep(1.0)

    started = time.monotonic()
    next_report = started + args.report_interval
    while time.monotonic() - started < args.duration:
        if server.poll() is not None:
            print("[f6] server exited during soak")
            break
        if time.monotonic() >= next_report:
            for cmd in args.periodic_server_command:
                send(server, cmd)
                time.sleep(0.05)
            next_report += args.report_interval
        time.sleep(0.5)

    print("[f6] requesting profile")
    send(server, "net_capabilities")
    time.sleep(0.5)
    send(server, "net_stressreport")
    time.sleep(0.5)
    send(server, "net_profile")
    time.sleep(2.0)

    for c in clients:
        if c.poll() is None:
            c.terminate()
            try:
                c.wait(timeout=5)
            except subprocess.TimeoutExpired:
                c.kill()
    if server.poll() is None:
        try:
            send(server, "quit")
        except Exception:
            pass
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.terminate()
            server.wait(timeout=5)

    lines = server_log.read_text(encoding="utf-8", errors="replace").splitlines()
    profile = parse_net_profile(lines)

    summary = {
        "run_id": run_id,
        "duration_s": args.duration,
        "client_count": args.client_count,
        "port": args.port,
        "trace_dir": str(trace_dir),
        "profile": profile,
        "gametype": args.gametype,
        "map": args.map,
        "server_cvars": args.server_cvar,
        "periodic_server_commands": args.periodic_server_command,
    }
    (trace_dir / "summary.json").write_text(json.dumps(summary, indent=2))

    print("[f6] summary:")
    print(json.dumps(profile, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
