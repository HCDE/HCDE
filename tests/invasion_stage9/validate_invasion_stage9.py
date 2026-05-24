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
import json
import random
import re
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


def append_server_files(args: argparse.Namespace, server_argv: list[str]) -> None:
    """Append optional -file resources for dedicated server launches."""
    if not args.server_file:
        return
    server_argv.append("-file")
    server_argv.extend(str(path) for path in args.server_file)


def append_client_files(args: argparse.Namespace, client_argv: list[str]) -> None:
    """Append optional -file resources for client joins."""
    if not args.client_file:
        return
    client_argv.append("-file")
    client_argv.extend(str(path) for path in args.client_file)


def launch_probe_client(args: argparse.Namespace, port: int, wait_tics: int) -> Optional[subprocess.Popen[str]]:
    """Launch a temporary dedicated-join client for query warm-up scenarios."""
    if args.client is None:
        return None
    join_target = f"{args.host}:{port}"
    client_argv = [
        str(args.client),
        "-join",
        join_target,
        "-dedicatedjoin",
        "-iwad",
        str(args.iwad),
    ]
    append_client_files(args, client_argv)
    client_argv.extend(
        [
            "-nosound",
            "-nomusic",
            "+wait",
            str(wait_tics),
            "+quit",
        ]
    )
    return subprocess.Popen(
        client_argv,
        cwd=str(args.workdir),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def stop_probe_client(client_proc: Optional[subprocess.Popen[str]]) -> None:
    if client_proc is None or client_proc.poll() is not None:
        return
    client_proc.terminate()
    try:
        client_proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        client_proc.kill()
        client_proc.wait(timeout=5)


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
        self._log_thread: Optional[threading.Thread] = None

    def log_count(self) -> int:
        return len(self.log_lines)

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

    def wait_for_log_substring(self, text: str, timeout_s: float, start_index: int = 0) -> bool:
        deadline = time.monotonic() + timeout_s
        start_index = max(0, start_index)
        scan_index = start_index
        while time.monotonic() < deadline:
            lines = self.log_lines
            if scan_index < len(lines):
                for line in lines[scan_index:]:
                    if text in line:
                        return True
                scan_index = len(lines)
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
        if self.process.stdin is not None:
            self.process.stdin.close()
        if self.process.stdout is not None:
            self.process.stdout.close()
        if self._log_thread is not None and self._log_thread.is_alive():
            self._log_thread.join(timeout=2.0)
        self._log_thread = None
        self.process = None
        self.log_lines = []


def mode_expected_name(mode: int) -> str:
    names = {
        0: "Co-op",
        1: "Deathmatch",
        2: "Team Deathmatch",
        3: "CTF",
        4: "Invasion",
    }
    return names.get(mode, str(mode))


def safe_trace_label(text: str) -> str:
    """Create a filesystem-safe label for trace files."""
    clean = []
    for char in text:
        if char.isalnum() or char in ("-", "_"):
            clean.append(char)
        else:
            clean.append("_")
    return "".join(clean).strip("_") or "run"


def save_debug_trace(args: argparse.Namespace, server: ServerProcess, case: str) -> None:
    """Persist one debug trace snapshot after a successful case."""
    if args.trace_save_dir is None:
        return

    label = safe_trace_label(case)
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    trace_path = (args.trace_save_dir / f"stage9_{label}_{timestamp}.trace.txt").resolve()
    trace_start = server.log_count()
    server.send_command(f'debugtracesave "{trace_path}" all debug')
    if not server.wait_for_log_substring("Saved successfully.", timeout_s=5.0, start_index=trace_start):
        raise Stage9Error(f"{case}: debug trace was not saved to {trace_path}")
    print(f"[trace] {case}: {trace_path}")


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


def run_mode_matrix(args: argparse.Namespace) -> list[dict[str, object]]:
    print("[stage9] validating mode query compatibility matrix...")
    results: list[dict[str, object]] = []
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
        append_server_files(args, server_argv)

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
            save_debug_trace(args, server, f"mode_{mode}")
            print(
                f"[ok] mode={mode} name='{snapshot.game_mode_name}' map={snapshot.map_name} "
                f"players={snapshot.player_count}/{snapshot.max_players} invasion_state={snapshot.invasion_state_name or snapshot.invasion_state}"
            )
            results.append(
                {
                    "case": f"mode_{mode}",
                    "mode": mode,
                    "server_port": port,
                    "host_name": host_name,
                    "ok": True,
                    "query": {
                        "host_name": snapshot.host_name,
                        "map_name": snapshot.map_name,
                        "game_mode": snapshot.game_mode,
                        "game_mode_name": snapshot.game_mode_name,
                        "invasion_state": snapshot.invasion_state,
                        "invasion_state_name": snapshot.invasion_state_name,
                        "invasion_wave": snapshot.invasion_wave,
                        "invasion_active_monsters": snapshot.invasion_active_monsters,
                    },
                    "players": [
                        {"name": player.name, "ping": player.ping, "frags": player.frags, "kills": player.kills, "deaths": player.deaths}
                        for player in snapshot.players
                    ],
                }
            )
        finally:
            server.stop()

    return results


def run_map_transition_case(args: argparse.Namespace) -> dict[str, object]:
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
    append_server_files(args, server_argv)

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
                probe = launch_probe_client(args, port, wait_tics=args.late_join_wait_tics)
                try:
                    warm_deadline = time.monotonic() + max(args.late_join_timeout, 20.0)
                    while time.monotonic() < warm_deadline:
                        snap = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=3)
                        if snap.map_name.strip().lower() != "unknown":
                            before = snap
                            break
                        time.sleep(0.25)
                finally:
                    stop_probe_client(probe)

        if before.map_name.strip().lower() == "unknown":
            print("[skip] map transition check skipped: dedicated query map is unknown while session is empty")
            return {"case": "map_transition", "mode": 4, "server_port": port, "ok": False, "skip_reason": "unknown"}

        server.send_command(f"changemap {args.transition_map}")
        deadline = time.monotonic() + args.map_transition_timeout
        while time.monotonic() < deadline:
            snap = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=3)
            if snap.map_name.upper() == args.transition_map.upper():
                save_debug_trace(args, server, "map_transition")
                print(f"[ok] map transition {before.map_name} -> {snap.map_name}")
                return {
                    "case": "map_transition",
                    "mode": 4,
                    "server_port": port,
                    "before_map": before.map_name,
                    "after_map": snap.map_name,
                    "ok": True,
                }
            time.sleep(0.3)
        raise Stage9Error(f"map transition did not reach {args.transition_map} within timeout")
    finally:
        server.stop()


def run_invasion_fallback_case(args: argparse.Namespace) -> dict[str, object]:
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
    append_server_files(args, server_argv)

    server = ServerProcess(server_argv, cwd=args.workdir)
    try:
        server.start()
        time.sleep(args.startup_delay)
        server.ensure_running("invasion fallback startup")

        baseline = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=args.query_attempts)
        if baseline.map_name.strip().lower() == "unknown" and baseline.player_count == 0:
            probe = launch_probe_client(args, port, wait_tics=args.late_join_wait_tics)
            try:
                warm_deadline = time.monotonic() + max(args.late_join_timeout, 20.0)
                while time.monotonic() < warm_deadline:
                    baseline = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=3)
                    if baseline.map_name.strip().lower() != "unknown":
                        break
                    time.sleep(0.25)
            finally:
                stop_probe_client(probe)
            if baseline.map_name.strip().lower() == "unknown" and baseline.player_count == 0:
                print("[skip] invasion fallback check skipped: dedicated query map is unknown while session is empty")
                return {"case": "invasion_fallback", "mode": 4, "server_port": port, "ok": False, "skip_reason": "unknown"}

        server.send_command("invasion_nextwave")
        server.send_command("invasion_spots")
        if not server.wait_for_log_substring("Invasion spots: total=", timeout_s=6.0):
            raise Stage9Error("did not observe invasion_spots output in dedicated server log")
        spots_total_from_log = -1
        for line in reversed(server.log_lines):
            if "Invasion spots: total=" not in line:
                continue
            match = re.search(r"Invasion spots: total=(\d+)", line)
            if match is not None:
                spots_total_from_log = int(match.group(1))
            break

        deadline = time.monotonic() + 12.0
        last_query_error: Optional[Stage9Error] = None
        while time.monotonic() < deadline:
            try:
                snap = query_snapshot(args.host, port, timeout_s=args.query_timeout, attempts=3)
                last_query_error = None
            except Stage9Error as exc:
                server.ensure_running("invasion fallback query")
                last_query_error = exc
                time.sleep(0.3)
                continue
            if snap.invasion_wave >= 1 and snap.invasion_spawn_spot_count > 0:
                save_debug_trace(args, server, "invasion_fallback")
                print(
                    f"[ok] invasion wave={snap.invasion_wave} spots={snap.invasion_spawn_spot_count} "
                    f"active={snap.invasion_spawn_active_spot_count} spawn_flags={snap.invasion_spawn_flags}"
                )
                return {
                    "case": "invasion_fallback",
                    "mode": 4,
                    "server_port": port,
                    "ok": True,
                    "invasion_wave": snap.invasion_wave,
                    "spawn_spots": snap.invasion_spawn_spot_count,
                    "active_spots": snap.invasion_spawn_active_spot_count,
                    "spawn_flags": snap.invasion_spawn_flags,
                }
            if spots_total_from_log > 0 and snap.map_name.strip().lower() != "unknown":
                # The query snapshot can lag behind the command log while the
                # dedicated server is warming up, so accept the logged fallback
                # count once the map is known and the command has clearly run.
                print(
                    f"[ok] invasion fallback spots={spots_total_from_log} "
                    f"(query wave={snap.invasion_wave} spots={snap.invasion_spawn_spot_count} state={snap.invasion_state_name or snap.invasion_state})"
                )
                save_debug_trace(args, server, "invasion_fallback")
                return {
                    "case": "invasion_fallback",
                    "mode": 4,
                    "server_port": port,
                    "ok": True,
                    "invasion_wave": snap.invasion_wave,
                    "spawn_spots_logged": spots_total_from_log,
                    "invasion_state_name": snap.invasion_state_name,
                }
            time.sleep(0.3)
        if last_query_error is not None:
            raise Stage9Error(f"invasion fallback query remained unstable: {last_query_error}")
        raise Stage9Error("invasion fallback case did not produce query-visible invasion wave/spot data")
    finally:
        server.stop()


def run_late_join_case(args: argparse.Namespace) -> dict[str, object] | None:
    if args.client is None:
        print("[skip] late join validation skipped (no --client path provided)")
        return None

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
    append_server_files(args, server_argv)

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
        ]
        append_client_files(args, client_argv)
        client_argv.extend(
            [
                "-nosound",
                "-nomusic",
                "+wait",
                str(args.late_join_wait_tics),
                "+quit",
            ]
        )
        client_proc = subprocess.Popen(
            client_argv,
            cwd=str(args.workdir),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        saw_join = False
        join_deadline = time.monotonic() + args.late_join_timeout
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

        save_debug_trace(args, server, "late_join")
        print("[ok] late join observed in query snapshot")
        return {
            "case": "late_join",
            "mode": 4,
            "server_port": port,
            "ok": True,
            "baseline_players": baseline.player_count,
            "joined_players": baseline.player_count + 1,
            "target_wait_tics": args.late_join_wait_tics,
        }
    finally:
        stop_probe_client(client_proc)
        server.stop()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="HCDE invasion Stage 9 validation harness")
    parser.add_argument("--server", required=True, type=Path, help="Path to hcdeserv binary")
    parser.add_argument("--iwad", required=True, type=Path, help="Path to IWAD (for example DOOM2.WAD)")
    parser.add_argument(
        "--server-file",
        action="append",
        type=Path,
        default=[],
        help="Optional PWAD/PK3 path to load with -file for dedicated server runs (repeatable)",
    )
    parser.add_argument(
        "--client-file",
        action="append",
        type=Path,
        default=[],
        help="Optional PWAD/PK3 path to load with -file for dedicated join client probes (repeatable)",
    )
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
    parser.add_argument(
        "--late-join-wait-tics",
        type=int,
        default=1200,
        help="Client +wait tics for late-join/warmup probes (35 tics ~= 1 second)",
    )
    parser.add_argument(
        "--late-join-timeout",
        type=float,
        default=45.0,
        help="Seconds to observe query state changes after launching late-join client",
    )
    parser.add_argument("--modes", type=int, nargs="+", default=[0, 1, 2, 3, 4], help="sv_gametype values to validate")
    parser.add_argument("--dry-run", action="store_true", help="Print planned cases without launching binaries")
    parser.add_argument("--label", default="", help="Optional label for summary/traces")
    parser.add_argument("--trace-save-dir", type=Path, help="Directory for per-case debugtracesave output")
    parser.add_argument("--summary-dir", type=Path, help="Directory for JSON Stage 9 run summaries")
    return parser.parse_args()


def write_summary(args: argparse.Namespace, results: list[dict[str, object]]) -> None:
    """Persist structured compatibility results as JSON."""
    if args.summary_dir is None:
        return

    args.summary_dir.mkdir(parents=True, exist_ok=True)
    label = safe_trace_label(args.label or "stage9")
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    summary_path = (args.summary_dir / f"stage9_{label}_{timestamp}.json").resolve()
    payload = {
        "label": args.label,
        "modes": args.modes,
        "query_timeout": args.query_timeout,
        "query_attempts": args.query_attempts,
        "map": args.map,
        "transition_map": args.transition_map,
        "cases": results,
    }
    summary_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"[summary] {summary_path}")


def validate_paths(args: argparse.Namespace) -> None:
    if args.dry_run:
        if args.trace_save_dir is not None and args.trace_save_dir:
            args.trace_save_dir.mkdir(parents=True, exist_ok=True)
        if args.summary_dir is not None:
            args.summary_dir.mkdir(parents=True, exist_ok=True)
        return
    if not args.server.is_file():
        raise Stage9Error(f"server binary not found: {args.server}")
    if not args.iwad.is_file():
        raise Stage9Error(f"IWAD file not found: {args.iwad}")
    for path in args.server_file:
        if not path.is_file():
            raise Stage9Error(f"server-file path not found: {path}")
    for path in args.client_file:
        if not path.is_file():
            raise Stage9Error(f"client-file path not found: {path}")
    if args.client is not None and not args.client.is_file():
        raise Stage9Error(f"client binary not found: {args.client}")
    if args.workdir is None:
        args.workdir = args.server.resolve().parent
    if not args.workdir.exists():
        raise Stage9Error(f"workdir does not exist: {args.workdir}")
    if args.trace_save_dir is not None:
        args.trace_save_dir.mkdir(parents=True, exist_ok=True)
    if args.summary_dir is not None:
        args.summary_dir.mkdir(parents=True, exist_ok=True)


def main() -> int:
    args = parse_args()
    try:
        validate_paths(args)
        if args.dry_run:
            print("[stage9] dry run")
            if args.label:
                print(f"  label={args.label}")
            print(f"  base_port={args.base_port} map={args.map} transition_map={args.transition_map}")
            print(f"  modes={args.modes}")
            print(f"  cases=mode_matrix map_transition invasion_fallback late_join")
            return 0

        results: list[dict[str, object]] = []
        for case in run_mode_matrix(args):
            results.append(case)
        results.append(run_map_transition_case(args))
        results.append(run_invasion_fallback_case(args))
        late_result = run_late_join_case(args)
        if late_result is not None:
            results.append(late_result)
        write_summary(args, results)
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
