#!/usr/bin/env python3
"""HCDE RCON smoke tests."""

from __future__ import annotations

import argparse
import socket
import struct
import subprocess
import time
from pathlib import Path


def fnv1a(text: str) -> str:
    value = 2166136261
    for b in text.encode("utf-8"):
        value ^= b
        value = (value * 16777619) & 0xFFFFFFFF
    return f"{value:08x}"


def send_frame(sock: socket.socket, text: str) -> None:
    data = text.encode("utf-8")
    sock.sendall(struct.pack(">I", len(data)) + data)


def recv_frame(sock: socket.socket) -> str:
    header = sock.recv(4)
    if len(header) != 4:
        raise RuntimeError("short frame header")
    length = struct.unpack(">I", header)[0]
    if length > 4096:
        raise RuntimeError(f"oversize frame {length}")
    body = b""
    while len(body) < length:
        chunk = sock.recv(length - len(body))
        if not chunk:
            raise RuntimeError("short frame body")
        body += chunk
    return body.decode("utf-8", errors="replace")


def wait_connect(port: int, timeout: float = 8.0) -> socket.socket:
    deadline = time.time() + timeout
    last_error: Exception | None = None
    while time.time() < deadline:
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=0.5)
            sock.settimeout(2.0)
            return sock
        except OSError as exc:
            last_error = exc
            time.sleep(0.1)
    raise RuntimeError(f"could not connect to RCON port {port}: {last_error}")


def expect_refused(port: int) -> None:
    deadline = time.time() + 2.0
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.25):
                raise RuntimeError(f"unexpectedly connected to disabled RCON port {port}")
        except OSError:
            time.sleep(0.1)


def launch_server(args: argparse.Namespace, port: int, enable: bool, password: str | None) -> subprocess.Popen[str]:
    cmd = [
        str(args.server),
        "-dedicated",
        "-host",
        "1",
        "-iwad",
        str(args.iwad),
        "+set",
        "sv_rcon_port",
        str(port),
        "+set",
        "sv_rcon_enable",
        "1" if enable else "0",
    ]
    if password is not None:
        cmd.extend(["+set", "sv_rcon_password", password])
    cmd.extend(["+map", args.map])
    return subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def stop_server(proc: subprocess.Popen[str]) -> str:
    proc.terminate()
    try:
        return proc.communicate(timeout=8)[0] or ""
    except subprocess.TimeoutExpired:
        proc.kill()
        return proc.communicate(timeout=8)[0] or ""


def run_case(args: argparse.Namespace, name: str, port: int, enable: bool, password: str | None, action) -> None:
    print(f"[case] {name}")
    proc = launch_server(args, port, enable, password)
    try:
        time.sleep(2.5)
        action(port)
    except Exception:
        output = stop_server(proc)
        print(output[-2000:])
        raise
    finally:
        if proc.poll() is None:
            stop_server(proc)
    print(f"[pass] {name}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, default=Path("build/RelWithDebInfo/hcdeserv.exe"))
    parser.add_argument("--iwad", type=Path, default=Path("doom2.wad"))
    parser.add_argument("--map", default="map01")
    parser.add_argument("--base-port", type=int, default=29240)
    args = parser.parse_args()

    password = "rcon-test"

    run_case(args, "disabled", args.base_port, False, password, expect_refused)
    run_case(args, "no-password", args.base_port + 1, True, None, expect_refused)

    def auth_fail(port: int) -> None:
        with wait_connect(port) as sock:
            hello = recv_frame(sock)
            assert hello.startswith("nonce "), hello
            send_frame(sock, "auth badverifier")
            assert recv_frame(sock).startswith("ERR auth failed")

    run_case(args, "auth-fail", args.base_port + 2, True, password, auth_fail)

    def auth_success_and_ping(port: int) -> None:
        with wait_connect(port) as sock:
            hello = recv_frame(sock)
            assert hello.startswith("nonce "), hello
            nonce = hello.split(" ", 1)[1]
            send_frame(sock, "auth " + fnv1a(f"{nonce}:{password}"))
            assert recv_frame(sock).startswith("OK authenticated")
            send_frame(sock, "ping")
            assert recv_frame(sock) == "OK pong"
            send_frame(sock, "quit")
            assert recv_frame(sock) == "ERR command not allowed"

    run_case(args, "auth-success-allowed-command", args.base_port + 3, True, password, auth_success_and_ping)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
