"""Step F6 disconnect / reconnect probe.

Spawns a dedicated server and two clients. After 12 seconds, kills one client
abruptly (simulating link drop). After 6 more seconds, launches a third client
that joins to the same slot. Captures net_profile to verify:
  - Server handles abrupt drop without escalating replay storms / desyncing
    surviving peer.
  - New join completes native capability handshake cleanly.
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


def launch_server(bin_dir: Path, iwad: Path, port: int, log_path: Path) -> subprocess.Popen[str]:
    argv = [
        str(bin_dir / "hcdeserv.exe"),
        "-server", "8",
        "-iwad", str(iwad),
        "-port", str(port),
        "-noadvertise",
        "+sv_hostname", "HCDE F6 Disconnect Probe",
        "+sv_gametype", "0",
        "+map", "MAP01",
    ]
    proc = subprocess.Popen(
        argv, cwd=str(bin_dir),
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, encoding="utf-8", errors="replace", bufsize=1,
    )
    _stream_to_file(proc.stdout, log_path)
    return proc


def launch_client(bin_dir: Path, iwad: Path, port: int, label: str, log_path: Path) -> subprocess.Popen[str]:
    argv = [
        str(bin_dir / "hcde.exe"),
        "-join", f"127.0.0.1:{port}",
        "-dedicatedjoin",
        "-iwad", str(iwad),
        "-nosound", "-nomusic",
        "+name", label,
    ]
    proc = subprocess.Popen(
        argv, cwd=str(bin_dir),
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, encoding="utf-8", errors="replace", bufsize=1,
    )
    _stream_to_file(proc.stdout, log_path)
    return proc


def send(proc: subprocess.Popen[str], command: str) -> None:
    if proc.stdin is None:
        return
    try:
        proc.stdin.write(command + "\n")
        proc.stdin.flush()
    except OSError:
        pass


def parse_net_profile(lines: list[str]) -> dict[str, dict[str, int]]:
    out: dict[str, dict[str, int]] = {}
    capture = False
    for line in lines:
        s = line.strip()
        if s.startswith("HCDE net profile:"):
            capture = True
            continue
        if capture:
            for prefix in ("live:", "client-input:", "snapshots:"):
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
    parser.add_argument("--port", type=int, default=18930)
    parser.add_argument("--label", default="f6-disconnect")
    parser.add_argument("--trace-dir", required=True, type=Path)
    args = parser.parse_args(argv)

    run_id = f"{args.label}_{int(time.time())}_{uuid.uuid4().hex[:6]}"
    trace_dir: Path = args.trace_dir / run_id
    trace_dir.mkdir(parents=True, exist_ok=True)

    server_log = trace_dir / "server.log"
    print(f"[f6] {run_id}")
    print(f"[f6] trace -> {trace_dir}")

    server = launch_server(args.bin_dir, args.iwad, args.port, server_log)
    time.sleep(3.0)
    if server.poll() is not None:
        print("[f6] server died early")
        return 2

    send(server, "net_profile_reset")
    time.sleep(0.5)

    print("[f6] launching initial clients A and B")
    client_a = launch_client(args.bin_dir, args.iwad, args.port, "F6_DiscA", trace_dir / "client_a.log")
    time.sleep(1.0)
    client_b = launch_client(args.bin_dir, args.iwad, args.port, "F6_DiscB", trace_dir / "client_b.log")

    print("[f6] phase 1: steady state (12s)")
    time.sleep(12.0)
    if server.poll() is not None:
        print("[f6] server died during phase 1")
        return 3

    send(server, "net_capabilities")
    send(server, "net_profile")
    time.sleep(1.5)

    print("[f6] phase 2: killing client B abruptly")
    client_b.kill()
    time.sleep(6.0)
    if server.poll() is not None:
        print("[f6] server died during phase 2")
        return 4
    send(server, "net_profile")
    time.sleep(1.5)

    print("[f6] phase 3: launching reconnect client C")
    client_c = launch_client(args.bin_dir, args.iwad, args.port, "F6_DiscC", trace_dir / "client_c.log")
    time.sleep(12.0)

    send(server, "net_capabilities")
    send(server, "net_stressreport")
    send(server, "net_profile")
    time.sleep(2.0)

    for c in (client_a, client_c):
        if c.poll() is None:
            c.terminate()
            try:
                c.wait(timeout=5)
            except subprocess.TimeoutExpired:
                c.kill()
    if server.poll() is None:
        send(server, "quit")
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.terminate()
            server.wait(timeout=5)

    lines = server_log.read_text(encoding="utf-8", errors="replace").splitlines()
    profile = parse_net_profile(lines)

    print(json.dumps(profile, indent=2))
    (trace_dir / "summary.json").write_text(json.dumps({
        "run_id": run_id,
        "profile": profile,
        "trace_dir": str(trace_dir),
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
