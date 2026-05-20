#!/usr/bin/env python3
"""HCDE Invasion Stage 9 compatibility smoke harness.

This script launches hcdeserv for each multiplayer mode, queries the launcher
snapshot endpoint, and validates that Stage 8 invasion metadata does not break
non-invasion modes.

Optional checks can also run:
  - late join observation (requires hcde client binary)
  - map transition via dedicated server console command
  - invasion fallback spot compatibility on classic maps
"""

from __future__ import annotations

import argparse
import os
import queue
import random
import socket
import struct
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


MSG_CHALLENGE = 5560020
LAUNCHER_CHALLENGE = 777123


class Stage9Error(RuntimeError):
    """Raised when a Stage 9 validation check fails."""


@dataclass
class QueryPlayer:
    name: str
    ping: int
    frags: int
    kills: int
    deaths: int


@dataclass
class QuerySnapshot:
    host_name: str
    player_count: int
    max_players: int
    map_name: str
    session_state: str
    deathmatch: bool
    skill: int
    teamplay: bool
    time_left: int
    frag_limit: int
    version: str
    git_hash: str
    players: list[QueryPlayer]
    game_name: str
    game_mode: int = 0
    game_mode_name: str = ""
    invasion_state: int = 0
    invasion_state_tics: int = 0
    invasion_state_name: str = ""
    invasion_wave: int = 0
    invasion_max_waves: int = 0
    invasion_wave_budget: int = 0
    invasion_wave_spawned: int = 0
    invasion_wave_cleared: int = 0
    invasion_wave_flags: int = 0
    invasion_spawn_spot_count: int = 0
    invasion_spawn_active_spot_count: int = 0
    invasion_spawn_plan_budget: int = 0
    invasion_spawn_active_tag: int = 0
    invasion_spawn_flags: int = 0
    invasion_active_monsters: int = 0


class ByteReader:
    """Small binary reader for launcher query replies."""

    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0

    def remaining(self) -> int:
        return len(self.data) - self.offset

    def read_u8(self) -> int:
        if self.remaining() < 1:
            raise Stage9Error("query packet truncated while reading byte")
        value = self.data[self.offset]
        self.offset += 1
        return value

    def read_u16(self) -> int:
        if self.remaining() < 2:
            raise Stage9Error("query packet truncated while reading short")
        value = (self.data[self.offset] << 8) | self.data[self.offset + 1]
        self.offset += 2
        return value

    def read_u32(self) -> int:
        if self.remaining() < 4:
            raise Stage9Error("query packet truncated while reading long")
        value = struct.unpack_from(">I", self.data, self.offset)[0]
        self.offset += 4
        return value

    def read_cstring(self) -> str:
        end = self.data.find(b"\x00", self.offset)
        if end < 0:
            raise Stage9Error("query packet truncated while reading string")
        value = self.data[self.offset:end].decode("utf-8", errors="replace")
        self.offset = end + 1
        return value


def parse_launcher_snapshot(packet: bytes, expected_token: int) -> QuerySnapshot:
    rd = ByteReader(packet)
    challenge = rd.read_u32()
    if challenge != MSG_CHALLENGE:
        raise Stage9Error(f"unexpected query reply challenge: {challenge}")

    _server_time = rd.read_u32()
    token = rd.read_u32()
    if token != expected_token:
        raise Stage9Error(f"query token mismatch: expected {expected_token}, got {token}")

    host_name = rd.read_cstring()
    player_count = rd.read_u8()
    max_players = rd.read_u8()
    map_name = rd.read_cstring()
    session_state = rd.read_cstring()
    deathmatch = rd.read_u8() != 0
    skill = rd.read_u8()
    teamplay = rd.read_u8() != 0
    time_left = rd.read_u16()
    frag_limit = rd.read_u16()
    version = rd.read_cstring()
    git_hash = rd.read_cstring()

    listed_players = rd.read_u8()
    players: list[QueryPlayer] = []
    for _ in range(listed_players):
        name = rd.read_cstring()
        ping = rd.read_u16()
        frags = struct.unpack(">h", struct.pack(">H", rd.read_u16()))[0]
        kills = struct.unpack(">h", struct.pack(">H", rd.read_u16()))[0]
        deaths = struct.unpack(">h", struct.pack(">H", rd.read_u16()))[0]
        players.append(QueryPlayer(name=name, ping=ping, frags=frags, kills=kills, deaths=deaths))

    game_name = rd.read_cstring()
    snapshot = QuerySnapshot(
        host_name=host_name,
        player_count=player_count,
        max_players=max_players,
        map_name=map_name,
        session_state=session_state,
        deathmatch=deathmatch,
        skill=skill,
        teamplay=teamplay,
        time_left=time_left,
        frag_limit=frag_limit,
        version=version,
        git_hash=git_hash,
        players=players,
        game_name=game_name,
    )

    # Stage 8 appended fields are optional for compatibility with older binaries.
    if rd.remaining() > 0:
        snapshot.game_mode = rd.read_u8()
        snapshot.game_mode_name = rd.read_cstring()
    if rd.remaining() > 0:
        snapshot.invasion_state = rd.read_u8()
        snapshot.invasion_state_tics = rd.read_u16()
        snapshot.invasion_state_name = rd.read_cstring()
    if rd.remaining() > 0:
        snapshot.invasion_wave = rd.read_u16()
        snapshot.invasion_max_waves = rd.read_u16()
        snapshot.invasion_wave_budget = rd.read_u16()
        snapshot.invasion_wave_spawned = rd.read_u16()
        snapshot.invasion_wave_cleared = rd.read_u16()
        snapshot.invasion_wave_flags = rd.read_u8()
    if rd.remaining() > 0:
        snapshot.invasion_spawn_spot_count = rd.read_u16()
        snapshot.invasion_spawn_active_spot_count = rd.read_u16()
        snapshot.invasion_spawn_plan_budget = rd.read_u16()
        snapshot.invasion_spawn_active_tag = rd.read_u16()
        snapshot.invasion_spawn_flags = rd.read_u8()
    if rd.remaining() > 0:
        snapshot.invasion_active_monsters = rd.read_u16()

    return snapshot


def query_snapshot(host: str, port: int, timeout_s: float = 0.5, attempts: int = 6) -> QuerySnapshot:
    last_error: Optional[Exception] = None
    for _ in range(attempts):
        token = random.getrandbits(32)
        packet = struct.pack(">II", LAUNCHER_CHALLENGE, token)
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.settimeout(timeout_s)
            sock.sendto(packet, (host, port))
            data, _from = sock.recvfrom(65535)
            return parse_launcher_snapshot(data, token)
        except Exception as exc:  # noqa: BLE001
            last_error = exc
            time.sleep(0.2)
        finally:
            sock.close()
    raise Stage9Error(f"query failed after {attempts} attempts: {last_error}")


class ServerProcess:
    """Dedicated server process wrapper with async log capture."""

    def __init__(self, argv: list[str], cwd: Path):
        self.argv = argv
        self.cwd = cwd
        self.process: Optional[subprocess.Popen[str]] = None
        self.log_lines: list[str] = []
        self._log_queue: queue.Queue[str] = queue.Queue()
        self._log_thread: Optional[threading.Thread] = None

    def start(self) -> None:
        self.process = subprocess.Popen(
            self.argv,
            cwd=str(self.cwd),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        self._log_thread = threading.Thread(target=self._read_logs, daemon=True)
        self._log_thread.start()

    def _read_logs(self) -> None:
        assert self.process is not None
        assert self.process.stdout is not None
        for line in self.process.stdout:
            clean = line.rstrip("\r\n")
            self.log_lines.append(clean)
            self._log_queue.put(clean)

    def send_command(self, command: str) -> None:
        if self.process is None or self.process.stdin is None:
            raise Stage9Error("server process not running")
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def ensure_running(self, context: str) -> None:
        if self.process is None:
            raise Stage9Error(f"{context}: server process was not started")
        code = self.process.poll()
        if code is not None:
            tail = "\n".join(self.log_lines[-12:])
            detail = f"\n--- server log tail ---\n{tail}\n-----------------------" if tail else " (no log output captured)"
            raise Stage9Error(f"{context}: server exited early with code {code}{detail}")

    def wait_for_log_substring(self, text: str, timeout_s: float) -> bool:
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            while True:
                try:
                    line = self._log_queue.get_nowait()
                except queue.Empty:
                    break
                if text in line:
                    return True
            time.sleep(0.05)
        return False

    def stop(self) -> None:
        if self.process is None:
            return
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)


def mode_expected_name(mode: int) -> str:
    names = {
        0: "Co-op",
        1: "Deathmatch",
        2: "Team Deathmatch",
        3: "CTF",
        4: "Invasion",
    }
    return names.get(mode, str(mode))


def validate_mode_snapshot(mode: int, snapshot: QuerySnapshot) -> None:
    if snapshot.game_mode != mode:
        raise Stage9Error(f"mode {mode} query mismatch: game_mode={snapshot.game_mode}")
    expected_name = mode_expected_name(mode).lower()
    if expected_name not in snapshot.game_mode_name.lower():
        raise Stage9Error(
            f"mode {mode} query mismatch: expected game mode name containing '{expected_name}', got '{snapshot.game_mode_name}'"
        )

    if mode != 4 and snapshot.invasion_state != 0:
        raise Stage9Error(
            f"mode {mode} should keep invasion disabled in query, got invasion_state={snapshot.invasion_state}"
        )
    if mode == 4 and not snapshot.invasion_state_name:
        raise Stage9Error("invasion mode query did not include invasion state name")


def run_mode_matrix(args: argparse.Namespace) -> None:
    print("[stage9] validating mode query compatibility matrix...")
    for mode in args.modes:
        port = args.base_port + mode
        host_name = f"HCDE Stage9 mode {mode}"
        server_argv = [
            str(args.server),
            "-server",
            str(args.server_slots),
            "-iwad",
            str(args.iwad),
            "-port",
            str(port),
            "-noadvertise",
            "+sv_hostname",
            host_name,
            "+sv_gametype",
            str(mode),
            "+map",
            args.map,
        ]

        server = ServerProcess(server_argv, cwd=args.workdir)
        try:
            server.start()
            time.sleep(args.startup_delay)
            server.ensure_running(f"mode {mode} startup")
            try:
                snapshot = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=args.query_attempts)
            except Stage9Error:
                server.ensure_running(f"mode {mode} query")
                raise
            validate_mode_snapshot(mode, snapshot)
            print(
                f"[ok] mode={mode} name='{snapshot.game_mode_name}' map={snapshot.map_name} "
                f"players={snapshot.player_count}/{snapshot.max_players} invasion_state={snapshot.invasion_state_name or snapshot.invasion_state}"
            )
        finally:
            server.stop()


def run_map_transition_case(args: argparse.Namespace) -> None:
    print("[stage9] validating dedicated map transition compatibility...")
    port = args.base_port + 40
    server_argv = [
        str(args.server),
        "-server",
        str(args.server_slots),
        "-iwad",
        str(args.iwad),
        "-port",
        str(port),
        "-noadvertise",
        "+sv_hostname",
        "HCDE Stage9 map transition",
        "+sv_gametype",
        "4",
        "+map",
        args.map,
    ]

    server = ServerProcess(server_argv, cwd=args.workdir)
    try:
        server.start()
        time.sleep(args.startup_delay)
        server.ensure_running("map transition startup")
        try:
            before = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=args.query_attempts)
        except Stage9Error:
            server.ensure_running("map transition query")
            raise
        if before.game_mode != 4:
            raise Stage9Error("map transition case expected invasion mode on startup")

        # Some dedicated builds keep map identity as "unknown" until a real level
        # is running. Try a one-time warm load before asserting map transitions.
        if before.map_name.strip().lower() == "unknown":
            server.send_command(f"map {args.map}")
            warm_deadline = time.monotonic() + min(args.map_transition_timeout, 6.0)
            while time.monotonic() < warm_deadline:
                snap = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=3)
                if snap.map_name.strip().lower() != "unknown":
                    before = snap
                    break
                time.sleep(0.3)

            if before.map_name.strip().lower() == "unknown":
                print("[skip] map transition check skipped: dedicated query map is unknown while session is empty")
                return

        server.send_command(f"changemap {args.transition_map}")
        deadline = time.monotonic() + args.map_transition_timeout
        while time.monotonic() < deadline:
            snap = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=3)
            if snap.map_name.upper() == args.transition_map.upper():
                print(f"[ok] map transition {before.map_name} -> {snap.map_name}")
                return
            time.sleep(0.3)
        raise Stage9Error(f"map transition did not reach {args.transition_map} within timeout")
    finally:
        server.stop()


def run_invasion_fallback_case(args: argparse.Namespace) -> None:
    print("[stage9] validating invasion fallback compatibility path...")
    port = args.base_port + 50
    server_argv = [
        str(args.server),
        "-server",
        str(args.server_slots),
        "-iwad",
        str(args.iwad),
        "-port",
        str(port),
        "-noadvertise",
        "+sv_hostname",
        "HCDE Stage9 invasion fallback",
        "+sv_gametype",
        "4",
        "+map",
        args.map,
    ]

    server = ServerProcess(server_argv, cwd=args.workdir)
    try:
        server.start()
        time.sleep(args.startup_delay)
        server.ensure_running("invasion fallback startup")

        baseline = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=args.query_attempts)
        if baseline.map_name.strip().lower() == "unknown" and baseline.player_count == 0:
            print("[skip] invasion fallback check skipped: dedicated query map is unknown while session is empty")
            return

        server.send_command("invasion_nextwave")
        server.send_command("invasion_spots")
        if not server.wait_for_log_substring("Invasion spots: total=", timeout_s=6.0):
            raise Stage9Error("did not observe invasion_spots output in dedicated server log")

        deadline = time.monotonic() + 12.0
        while time.monotonic() < deadline:
            try:
                snap = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=3)
            except Stage9Error:
                server.ensure_running("invasion fallback query")
                raise
            if snap.invasion_wave >= 1 and snap.invasion_spawn_spot_count > 0:
                print(
                    f"[ok] invasion wave={snap.invasion_wave} spots={snap.invasion_spawn_spot_count} "
                    f"active={snap.invasion_spawn_active_spot_count} spawn_flags={snap.invasion_spawn_flags}"
                )
                return
            time.sleep(0.3)
        raise Stage9Error("invasion fallback case did not produce query-visible invasion wave/spot data")
    finally:
        server.stop()


def run_late_join_case(args: argparse.Namespace) -> None:
    if args.client is None:
        print("[skip] late join validation skipped (no --client path provided)")
        return

    print("[stage9] validating late join query compatibility...")
    port = args.base_port + 60
    server_argv = [
        str(args.server),
        "-server",
        str(args.server_slots),
        "-iwad",
        str(args.iwad),
        "-port",
        str(port),
        "-noadvertise",
        "+sv_hostname",
        "HCDE Stage9 late join",
        "+sv_gametype",
        "4",
        "+map",
        args.map,
    ]

    server = ServerProcess(server_argv, cwd=args.workdir)
    client_proc: Optional[subprocess.Popen[str]] = None
    try:
        server.start()
        time.sleep(args.startup_delay)
        server.ensure_running("late join startup")
        try:
            baseline = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=args.query_attempts)
        except Stage9Error:
            server.ensure_running("late join baseline query")
            raise

        join_target = f"{args.host}:{port}"
        client_argv = [
            str(args.client),
            "-join",
            join_target,
            "-dedicatedjoin",
            "-iwad",
            str(args.iwad),
            "-nosound",
            "-nomusic",
            "+wait",
            "175",
            "+quit",
        ]
        client_proc = subprocess.Popen(
            client_argv,
            cwd=str(args.workdir),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        saw_join = False
        join_deadline = time.monotonic() + 20.0
        while time.monotonic() < join_deadline:
            try:
                snap = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=3)
            except Stage9Error:
                server.ensure_running("late join polling query")
                raise
            if snap.player_count > baseline.player_count:
                saw_join = True
                break
            time.sleep(0.25)
        if not saw_join:
            raise Stage9Error("late join validation did not observe player count increase")

        print("[ok] late join observed in query snapshot")
    finally:
        if client_proc is not None and client_proc.poll() is None:
            client_proc.terminate()
            try:
                client_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                client_proc.kill()
                client_proc.wait(timeout=5)
        server.stop()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="HCDE invasion Stage 9 validation harness")
    parser.add_argument("--server", required=True, type=Path, help="Path to hcdeserv binary")
    parser.add_argument("--iwad", required=True, type=Path, help="Path to IWAD (for example DOOM2.WAD)")
    parser.add_argument("--client", type=Path, help="Optional path to hcde client binary for late-join check")
    parser.add_argument(
        "--workdir",
        type=Path,
        help="Working directory used to launch binaries (default: server binary directory)",
    )
    parser.add_argument("--host", default="127.0.0.1", help="Host used for launcher query")
    parser.add_argument("--base-port", type=int, default=17600, help="Base UDP port for scenario runs")
    parser.add_argument("--server-slots", type=int, default=4, help="Visible player slot count")
    parser.add_argument("--map", default="MAP01", help="Primary test map")
    parser.add_argument("--transition-map", default="MAP02", help="Map used by map transition scenario")
    parser.add_argument("--startup-delay", type=float, default=1.5, help="Server startup delay before first query")
    parser.add_argument("--query-timeout", type=float, default=0.5, help="Per-query socket timeout in seconds")
    parser.add_argument("--query-attempts", type=int, default=8, help="Retry count per query")
    parser.add_argument("--map-transition-timeout", type=float, default=45.0, help="Map transition wait timeout")
    parser.add_argument("--modes", type=int, nargs="+", default=[0, 1, 2, 3, 4], help="sv_gametype values to validate")
    return parser.parse_args()


def validate_paths(args: argparse.Namespace) -> None:
    if not args.server.is_file():
        raise Stage9Error(f"server binary not found: {args.server}")
    if not args.iwad.is_file():
        raise Stage9Error(f"IWAD file not found: {args.iwad}")
    if args.client is not None and not args.client.is_file():
        raise Stage9Error(f"client binary not found: {args.client}")
    if args.workdir is None:
        args.workdir = args.server.resolve().parent
    if not args.workdir.exists():
        raise Stage9Error(f"workdir does not exist: {args.workdir}")


def main() -> int:
    args = parse_args()
    try:
        validate_paths(args)
        run_mode_matrix(args)
        run_map_transition_case(args)
        run_invasion_fallback_case(args)
        run_late_join_case(args)
    except Stage9Error as exc:
        print(f"[fail] {exc}")
        return 1
    except KeyboardInterrupt:
        print("[abort] interrupted by user")
        return 130

    print("[pass] Stage 9 compatibility harness completed successfully")
    return 0


if __name__ == "__main__":
    sys.exit(main())
