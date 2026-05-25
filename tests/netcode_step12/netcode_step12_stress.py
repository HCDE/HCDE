#!/usr/bin/env python3
"""HCDE Step 12 network stress harness.

The harness launches hcdeserv, keeps the launcher query endpoint warm while the
server is under mode-specific pressure, and asks the server to emit the compact
`net_stressreport` summary at the end of each case.
"""

from __future__ import annotations

import argparse
import json
import random
import socket
import re
import struct
import subprocess
import sys
import threading
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


MSG_CHALLENGE = 5560020
LAUNCHER_CHALLENGE = 777123


class Step12Error(RuntimeError):
    """Raised when a Step 12 stress case fails."""


class ByteReader:
    """Bounds-checked big-endian reader for launcher query packets."""

    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0

    def remaining(self) -> int:
        return len(self.data) - self.offset

    def read_u8(self) -> int:
        if self.remaining() < 1:
            raise Step12Error("query packet truncated while reading byte")
        value = self.data[self.offset]
        self.offset += 1
        return value

    def read_u16(self) -> int:
        if self.remaining() < 2:
            raise Step12Error("query packet truncated while reading short")
        value = (self.data[self.offset] << 8) | self.data[self.offset + 1]
        self.offset += 2
        return value

    def read_u32(self) -> int:
        if self.remaining() < 4:
            raise Step12Error("query packet truncated while reading long")
        value = struct.unpack_from(">I", self.data, self.offset)[0]
        self.offset += 4
        return value

    def read_cstring(self) -> str:
        end = self.data.find(b"\x00", self.offset)
        if end < 0:
            raise Step12Error("query packet truncated while reading string")
        value = self.data[self.offset:end].decode("utf-8", errors="replace")
        self.offset = end + 1
        return value


@dataclass
class QuerySnapshot:
    map_name: str
    player_count: int
    max_players: int
    game_mode: int = 0
    game_mode_name: str = ""
    invasion_state_name: str = ""
    invasion_wave: int = 0
    invasion_active_monsters: int = 0


@dataclass
class StressCase:
    name: str
    mode: int
    map_name: str
    warmup_commands: list[str]
    periodic_commands: list[str]


PRESSURE_PRESET_COMMANDS = {
    "custom": [],
    "smoke": [],
    "broad": ["net_lanes", "net_unlagged"],
    "heavy-invasion": ["net_relevance", "net_simlod", "net_lanes", "net_unlagged"],
    "projectile-storm": ["net_relevance", "net_lanes", "net_unlagged"],
    "competitive-highping": ["net_unlagged", "net_lanes"],
}


def _coerce_token(value: str):
    """Parse a report token as int or float when possible."""
    try:
        if "." in value:
            return float(value)
        return int(value)
    except ValueError:
        return value


def _parse_sim_lod_triplet(raw: object) -> Optional[tuple[int, int, int]]:
    """Parse ``full/reduced/dormant`` sim-lod triplets into integers."""
    if not isinstance(raw, str):
        return None

    parts = raw.split("/")
    if len(parts) != 3:
        return None

    try:
        full, reduced, dormant = (int(part) for part in parts)
    except ValueError:
        return None
    return full, reduced, dormant


def _parse_divided_counts(raw: object, names: tuple[str, ...]) -> Optional[dict[str, int]]:
    """Parse ``a/b/c`` style token values into named integer fields."""
    if not isinstance(raw, str):
        return None

    parts = raw.split("/")
    if len(parts) != len(names):
        return None

    parsed: dict[str, int] = {}
    try:
        for name, part in zip(names, parts):
            parsed[name] = int(part)
    except ValueError:
        return None
    return parsed


def _parse_key_value_pairs(payload: str) -> dict[str, object]:
    """Parse ``key=value`` pairs from a stress report fragment."""
    values: dict[str, object] = {}
    token_re = re.compile(r"(\S+?)=([^\s]+)")
    for key, raw_value in token_re.findall(payload):
        values[key] = _coerce_token(raw_value)
    return values


def _normalize_migration_metrics(raw: dict[str, object]) -> None:
    """Normalize historical grouped migration fields into explicit migration keys."""
    source_bundle = raw.get("sources")
    if isinstance(source_bundle, str):
        source_values = _parse_divided_counts(source_bundle, ("source-shared", "source-invasion", "source-coop", "source-dm"))
        if source_values is not None:
            raw.update(source_values)

    category_bundle = raw.get("categories")
    if isinstance(category_bundle, str):
        category_values = _parse_divided_counts(
            category_bundle,
            (
                "category-monster",
                "category-projectile",
                "category-pickup",
                "category-map",
                "category-script",
                "category-visual",
                "category-unknown",
            ),
        )
        if category_values is not None:
            raw.update(category_values)

    # Legacy output from net_migration used unprefixed source names.
    if "source-shared" not in raw and {"invasion", "coop", "dm"}.issubset(raw.keys()):
        raw["source-shared"] = raw.get("shared", 0)
        raw["source-invasion"] = raw.get("invasion")
        raw["source-coop"] = raw.get("coop")
        raw["source-dm"] = raw.get("dm")


def _parse_migration_line(payload: str) -> dict[str, object]:
    """Parse migration line payload including legacy grouped-format variants."""
    values = _parse_key_value_pairs(payload)

    legacy_source_match = re.search(r"\bsources\s+(\S+)", payload)
    if legacy_source_match is not None:
        source_values = _parse_divided_counts(
            legacy_source_match.group(1),
            ("source-shared", "source-invasion", "source-coop", "source-dm"),
        )
        if source_values is not None:
            values.update(source_values)
            values["source-shared"] = source_values.get("source-shared", 0)

    legacy_category_match = re.search(r"\bcategories\s+(\S+)", payload)
    if legacy_category_match is not None:
        category_values = _parse_divided_counts(
            legacy_category_match.group(1),
            (
                "category-monster",
                "category-projectile",
                "category-pickup",
                "category-map",
                "category-script",
                "category-visual",
                "category-unknown",
            ),
        )
        if category_values is not None:
            values.update(category_values)

    _normalize_migration_metrics(values)
    return values


def parse_stress_metrics(log_lines: list[str]) -> dict[str, object]:
    """Extract the high-level numeric metrics from the final stress report lines."""
    metrics: dict[str, object] = {}
    for line in log_lines:
        stripped = line.strip()
        if stripped.startswith("world pressure:"):
            metrics["world_pressure"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("actor pressure:"):
            metrics["actor_pressure"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("delta pressure:"):
            metrics["delta_pressure"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("authority pressure:"):
            metrics["authority_pressure"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("competitive lane:"):
            metrics["competitive_lane"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("relevance:"):
            metrics["relevance"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("projectile policy:"):
            metrics["projectile_policy"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("migration:"):
            migration = _parse_migration_line(stripped.split(":", 1)[1])
            metrics["migration"] = migration
        elif stripped.startswith("sim-lod:"):
            sim_lod = _parse_key_value_pairs(stripped.split(":", 1)[1])
            current_triplet = _parse_sim_lod_triplet(sim_lod.get("current"))
            if current_triplet is not None:
                sim_lod["current_full"], sim_lod["current_reduced"], sim_lod["current_dormant"] = current_triplet
            metrics["sim_lod"] = sim_lod
        elif stripped.startswith("baseline repair:"):
            metrics["baseline_repair"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("actor-delta-v2:"):
            metrics["actor_delta_v2"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("pregame quarantine:"):
            metrics["pregame_quarantine"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
    return metrics


def parse_native_profile(log_lines: list[str]) -> dict[str, dict[str, object]]:
    """Extract native live counters from the latest ``net_profile`` block."""
    profile: dict[str, dict[str, object]] = {}
    for line in log_lines:
        stripped = line.strip()
        if stripped.startswith("live:"):
            profile["live"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("client-input:"):
            profile["client_input"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
        elif stripped.startswith("snapshots:"):
            profile["snapshots"] = _parse_key_value_pairs(stripped.split(":", 1)[1])
    return profile


def _metric(metrics: dict[str, object], key: str, default: int = 0) -> int:
    """Return an integer metric value from parsed profile/stress dictionaries."""
    value = metrics.get(key, default)
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)
    try:
        return int(str(value))
    except (TypeError, ValueError):
        return default


def validate_native_profile(args: argparse.Namespace, case_name: str, profile: dict[str, dict[str, object]]) -> None:
    """Fail the case if native HCDE live gameplay counters do not prove healthy traffic."""
    live = profile.get("live")
    client_input = profile.get("client_input")
    snapshots = profile.get("snapshots")
    if live is None or client_input is None or snapshots is None:
        raise Step12Error(f"{case_name}: net_profile did not include live/client-input/snapshots blocks")

    failures: list[str] = []
    if _metric(live, "encode-fail") != 0:
        failures.append(f"encode-fail={_metric(live, 'encode-fail')}")
    if _metric(live, "cap-reject") != 0:
        failures.append(f"cap-reject={_metric(live, 'cap-reject')}")
    if _metric(live, "legacy-gameplay-reject") > args.max_native_legacy_rejects:
        failures.append(
            f"legacy-gameplay-reject={_metric(live, 'legacy-gameplay-reject')} "
            f"(max {args.max_native_legacy_rejects})"
        )
    if _metric(live, "replay-req") > args.max_native_replay_requests:
        failures.append(f"replay-req={_metric(live, 'replay-req')} (max {args.max_native_replay_requests})")
    if _metric(live, "replay-suppress") != 0:
        failures.append(f"replay-suppress={_metric(live, 'replay-suppress')}")
    if _metric(live, "replay-escalate") != 0:
        failures.append(f"replay-escalate={_metric(live, 'replay-escalate')}")
    if _metric(client_input, "native-apply") < args.min_native_client_input_applied:
        failures.append(
            f"client-input.native-apply={_metric(client_input, 'native-apply')} "
            f"(min {args.min_native_client_input_applied})"
        )
    if _metric(snapshots, "native-built") < args.min_native_server_snapshot_built:
        failures.append(
            f"snapshots.native-built={_metric(snapshots, 'native-built')} "
            f"(min {args.min_native_server_snapshot_built})"
        )

    if failures:
        raise Step12Error(f"{case_name}: native live profile failed: {', '.join(failures)}")


class ServerProcess:
    """Dedicated server process wrapper with async log capture."""

    def __init__(self, argv: list[str], cwd: Path):
        self.argv = argv
        self.cwd = cwd
        self.process: Optional[subprocess.Popen[str]] = None
        self.log_lines: list[str] = []
        self._log_lock = threading.Lock()
        self._log_thread: Optional[threading.Thread] = None
        self._log_file_path: Optional[Path] = None
        self._stdout_file = None

    def start(self) -> None:
        log_dir = self.cwd / "traces" / "step12_process_logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        self._log_file_path = log_dir / f"hcde_step12_{int(time.time() * 1000)}_{uuid.uuid4().hex[:8]}.log"
        self._stdout_file = self._log_file_path.open("w", encoding="utf-8", errors="replace", buffering=1)
        self.process = subprocess.Popen(
            self.argv,
            cwd=str(self.cwd),
            stdin=subprocess.PIPE,
            stdout=self._stdout_file,
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
        assert self._log_file_path is not None
        position = 0
        while True:
            try:
                with self._log_file_path.open("r", encoding="utf-8", errors="replace") as log_file:
                    log_file.seek(position)
                    lines = log_file.readlines()
                    position = log_file.tell()
            except FileNotFoundError:
                lines = []

            if lines:
                with self._log_lock:
                    self.log_lines.extend(line.rstrip("\r\n") for line in lines)

            if self.process.poll() is not None:
                if not lines:
                    break
            time.sleep(0.05)

    def send_command(self, command: str) -> None:
        if self.process is None or self.process.stdin is None:
            raise Step12Error("server process not running")
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def ensure_running(self, context: str) -> None:
        if self.process is None:
            raise Step12Error(f"{context}: server process was not started")
        code = self.process.poll()
        if code is not None:
            tail = "\n".join(self.snapshot_log_lines()[-20:])
            detail = f"\n--- server log tail ---\n{tail}\n-----------------------" if tail else " (no log output captured)"
            raise Step12Error(f"{context}: server exited early with code {code}{detail}")

    def log_count(self) -> int:
        with self._log_lock:
            return len(self.log_lines)

    def snapshot_log_lines(self) -> list[str]:
        with self._log_lock:
            return list(self.log_lines)

    def wait_for_log_substring(self, text: str, timeout_s: float, start_index: int = 0) -> bool:
        """Wait for new matching server output without consuming older log lines."""
        deadline = time.monotonic() + timeout_s
        read_index = max(start_index, 0)
        while time.monotonic() < deadline:
            with self._log_lock:
                pending = self.log_lines[read_index:]
                read_index = len(self.log_lines)
            for line in pending:
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
        if self._log_thread is not None:
            self._log_thread.join(timeout=2)
        if self._stdout_file is not None:
            self._stdout_file.close()
            self._stdout_file = None


def append_files(paths: list[Path], argv: list[str]) -> None:
    if not paths:
        return
    argv.append("-file")
    argv.extend(str(path) for path in paths)


def append_unique(target: list[str], values: list[str]) -> None:
    """Append commands without duplicating user-provided entries."""
    for value in values:
        if value not in target:
            target.append(value)


def apply_pressure_preset(args: argparse.Namespace) -> None:
    """Expand named soak presets into repeatable command pressure."""
    preset = args.pressure_preset
    append_unique(args.periodic_command, PRESSURE_PRESET_COMMANDS[preset])
    if preset == "heavy-invasion":
        args.wave_pulses = max(args.wave_pulses, 3)
        args.periodic_wave_pulse = True
        args.min_sim_lod_suspended = max(args.min_sim_lod_suspended, 1)
    elif preset == "projectile-storm":
        args.wave_pulses = max(args.wave_pulses, 2)


def network_shape_metadata(args: argparse.Namespace) -> dict[str, object]:
    """Return comparable metadata for externally shaped network runs."""
    metadata: dict[str, object] = {}
    if args.shaper:
        metadata["shaper"] = args.shaper
    if args.rtt_ms is not None:
        metadata["rtt_ms"] = args.rtt_ms
    if args.jitter_ms is not None:
        metadata["jitter_ms"] = args.jitter_ms
    if args.loss_pct is not None:
        metadata["loss_pct"] = args.loss_pct
    if args.bandwidth_kbps is not None:
        metadata["bandwidth_kbps"] = args.bandwidth_kbps
    return metadata


def parse_launcher_snapshot(packet: bytes, expected_token: int) -> QuerySnapshot:
    """Decode the stable launcher query prefix plus optional HCDE mode fields."""
    rd = ByteReader(packet)
    challenge = rd.read_u32()
    if challenge != MSG_CHALLENGE:
        raise Step12Error(f"unexpected query reply challenge: {challenge}")

    _server_time = rd.read_u32()
    token = rd.read_u32()
    if token != expected_token:
        raise Step12Error(f"query token mismatch: expected {expected_token}, got {token}")

    _host_name = rd.read_cstring()
    player_count = rd.read_u8()
    max_players = rd.read_u8()
    map_name = rd.read_cstring()
    _session_state = rd.read_cstring()
    _deathmatch = rd.read_u8()
    _skill = rd.read_u8()
    _teamplay = rd.read_u8()
    _time_left = rd.read_u16()
    _frag_limit = rd.read_u16()
    _version = rd.read_cstring()
    _git_hash = rd.read_cstring()

    listed_players = rd.read_u8()
    for _ in range(listed_players):
        _name = rd.read_cstring()
        _ping = rd.read_u16()
        _frags = rd.read_u16()
        _kills = rd.read_u16()
        _deaths = rd.read_u16()

    _game_name = rd.read_cstring()
    snapshot = QuerySnapshot(map_name=map_name, player_count=player_count, max_players=max_players)

    if rd.remaining() > 0:
        snapshot.game_mode = rd.read_u8()
        snapshot.game_mode_name = rd.read_cstring()
    if rd.remaining() > 0:
        _invasion_state = rd.read_u8()
        _invasion_state_tics = rd.read_u16()
        snapshot.invasion_state_name = rd.read_cstring()
    if rd.remaining() > 0:
        snapshot.invasion_wave = rd.read_u16()
        _max_waves = rd.read_u16()
        _wave_budget = rd.read_u16()
        _wave_spawned = rd.read_u16()
        _wave_cleared = rd.read_u16()
        _wave_flags = rd.read_u8()
    if rd.remaining() > 0:
        _spot_count = rd.read_u16()
        _active_spots = rd.read_u16()
        _plan_budget = rd.read_u16()
        _active_tag = rd.read_u16()
        _spawn_flags = rd.read_u8()
    if rd.remaining() > 0:
        snapshot.invasion_active_monsters = rd.read_u16()

    return snapshot


def query_snapshot(host: str, port: int, timeout_s: float, attempts: int) -> QuerySnapshot:
    """Query the server status endpoint; failures are retried per sample."""
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
            time.sleep(0.15)
        finally:
            sock.close()
    raise Step12Error(f"query failed after {attempts} attempts: {last_error}")


def mode_name(mode: int) -> str:
    if mode == 4:
        return "invasion"
    if mode != 0:
        return "dm"
    return "coop"


def build_cases(args: argparse.Namespace) -> list[StressCase]:
    """Translate requested broad engine modes into concrete dedicated cases."""
    cases: list[StressCase] = []
    for name in args.cases:
        if name == "invasion":
            warmup = [
                "net_profile_reset",
                "net_migration",
                "sv_invasionsimlod 1",
            ]
            warmup.extend("invasion_nextwave" for _ in range(args.wave_pulses))
            cases.append(
                StressCase(
                    name="invasion",
                    mode=4,
                    map_name=args.map,
                    warmup_commands=warmup,
                    periodic_commands=["invasion_nextwave"] if args.periodic_wave_pulse else [],
                )
            )
        elif name == "coop":
            cases.append(
                StressCase(
                    name="coop",
                    mode=0,
                    map_name=args.map,
                    warmup_commands=["net_profile_reset", "net_migration"],
                    periodic_commands=[],
                )
            )
        elif name == "dm":
            cases.append(
                StressCase(
                    name="dm",
                    mode=1,
                    map_name=args.dm_map or args.map,
                    warmup_commands=["net_profile_reset", "net_migration"],
                    periodic_commands=[],
                )
            )
        else:
            raise Step12Error(f"unknown stress case: {name}")
    return cases


def build_server_argv(args: argparse.Namespace, case: StressCase, port: int) -> list[str]:
    """Build a dedicated server command line without shell interpolation."""
    argv = [
        str(args.server),
        "-server",
        str(args.server_slots),
        "-iwad",
        str(args.iwad),
        "-port",
        str(port),
    ]
    if args.advertise:
        argv.append("-advertise")
        for endpoint in args.master:
            argv.extend(["-master", endpoint])
    else:
        argv.append("-noadvertise")
    append_files(args.server_file, argv)
    argv.extend(
        [
            "+sv_hostname",
            f"HCDE Step12 {case.name}",
            "+sv_gametype",
            str(case.mode),
            "+map",
            case.map_name,
        ]
    )
    return argv


def launch_clients(args: argparse.Namespace, port: int, wait_tics: int) -> list[subprocess.Popen[bytes]]:
    """Launch optional short-lived clients to create join and replication pressure."""
    if args.client_count <= 0:
        return []
    if args.client is None:
        raise Step12Error("--client-count requires --client")

    clients: list[subprocess.Popen[bytes]] = []
    for index in range(args.client_count):
        argv = [
            str(args.client),
            "-join",
            f"{args.host}:{port}",
            "-dedicatedjoin",
            "-iwad",
            str(args.iwad),
        ]
        append_files(args.client_file, argv)
        argv.extend(["-nosound", "-nomusic", "+name", f"Step12_{index + 1}", "+wait", str(wait_tics), "+quit"])
        clients.append(subprocess.Popen(argv, cwd=str(args.workdir), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
        time.sleep(args.client_stagger)
    return clients


def stop_clients(clients: list[subprocess.Popen[bytes]]) -> None:
    """Terminate client probes that did not naturally exit."""
    for proc in clients:
        if proc.poll() is not None:
            continue
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)


def run_case(args: argparse.Namespace, case: StressCase, ordinal: int) -> dict[str, object]:
    port = args.base_port + ordinal
    server_argv = build_server_argv(args, case, port)
    print(f"[step12] case={case.name} mode={case.mode} port={port} duration={args.duration:.1f}s")

    server = ServerProcess(server_argv, cwd=args.workdir)
    clients: list[subprocess.Popen[bytes]] = []
    query_count = 0
    query_failures = 0
    last_snapshot: Optional[QuerySnapshot] = None
    summary: dict[str, object] = {
        "case": case.name,
        "mode": case.mode,
        "mode_name": mode_name(case.mode),
        "port": port,
        "map": case.map_name,
        "duration_s": args.duration,
        "pressure_preset": args.pressure_preset,
        "label": args.label,
        "network_shape": network_shape_metadata(args),
        "warmup_commands": case.warmup_commands + args.server_command,
        "periodic_commands": case.periodic_commands + args.periodic_command,
        "query_count": 0,
        "query_failures": 0,
        "ok": False,
    }
    try:
        server.start()
        time.sleep(args.startup_delay)
        server.ensure_running(f"{case.name} startup")

        for command in case.warmup_commands + args.server_command:
            server.send_command(command)
            time.sleep(args.command_delay)

        wait_tics = max(int((args.duration + args.startup_delay + 5.0) * 35), 70)
        clients = launch_clients(args, port, wait_tics)

        next_query = time.monotonic()
        next_report = time.monotonic() + args.report_interval
        next_periodic = time.monotonic() + args.periodic_interval
        deadline = time.monotonic() + args.duration
        while time.monotonic() < deadline:
            now = time.monotonic()
            server.ensure_running(f"{case.name} soak")

            if now >= next_query:
                try:
                    last_snapshot = query_snapshot(args.host, port, args.query_timeout, args.query_attempts)
                    query_count += 1
                except Step12Error as exc:
                    query_failures += 1
                    if query_failures > args.max_query_failures:
                        raise Step12Error(f"{case.name}: launcher query became unstable: {exc}") from exc
                next_query = now + args.query_interval

            if now >= next_report:
                server.send_command("net_stressreport")
                next_report = now + args.report_interval

            periodic_commands = case.periodic_commands + args.periodic_command
            if periodic_commands and now >= next_periodic:
                for command in periodic_commands:
                    server.send_command(command)
                next_periodic = now + args.periodic_interval

            time.sleep(0.05)

        report_start = server.log_count()
        server.send_command("net_migration")
        server.send_command("net_stressreport")
        if not server.wait_for_log_substring("HCDE net stress report:", timeout_s=args.report_wait, start_index=report_start):
            raise Step12Error(f"{case.name}: did not observe net_stressreport output")
        stress_lines = [
            line for line in server.snapshot_log_lines()
            if line.startswith("HCDE net stress report:")
            or line.startswith("  world pressure:")
            or line.startswith("  actor pressure:")
            or line.startswith("  delta pressure:")
            or line.startswith("  authority pressure:")
            or line.startswith("  competitive lane:")
            or line.startswith("  relevance:")
            or line.startswith("  projectile policy:")
            or line.startswith("  sim-lod:")
            or line.startswith("  migration:")
            or line.startswith("  baseline repair:")
            or line.startswith("  actor-delta-v2:")
            or line.startswith("  pregame quarantine:")
        ][-128:]
        stress_metrics = parse_stress_metrics(stress_lines)
        if args.require_stress_metrics and (
            "world_pressure" not in stress_metrics
            or "actor_pressure" not in stress_metrics
            or "delta_pressure" not in stress_metrics
        ):
            raise Step12Error(f"{case.name}: failed to capture required stress metrics")
        if args.require_sim_lod and "sim_lod" not in stress_metrics:
            raise Step12Error(f"{case.name}: failed to capture required sim-lod metrics")
        if args.min_sim_lod_suspended > 0 and case.mode == 4:
            sim_lod = stress_metrics.get("sim_lod")
            if sim_lod is None:
                raise Step12Error(f"{case.name}: no sim-lod metrics available for sim-lod threshold check")
            suspended = sim_lod.get("suspended", 0)
            if not isinstance(suspended, int):
                raise Step12Error(f"{case.name}: sim-lod suspended metric is invalid: {suspended!r}")
            if suspended < args.min_sim_lod_suspended:
                raise Step12Error(
                    f"{case.name}: sim-lod suspended actors too low: {suspended} < {args.min_sim_lod_suspended}"
                )
        if args.require_migration and "migration" not in stress_metrics:
            raise Step12Error(f"{case.name}: failed to capture migration metrics from net_stressreport")
        if args.min_migration_considered > 0 or args.min_migration_touched > 0 or args.min_migration_source_invasion > 0 or args.min_migration_source_coop > 0 or args.min_migration_source_dm > 0:
            migration = stress_metrics.get("migration")
            if migration is None or not isinstance(migration, dict):
                raise Step12Error(f"{case.name}: missing migration metrics for migration thresholds")
            considered = migration.get("considered")
            if isinstance(considered, int) and args.min_migration_considered > considered:
                raise Step12Error(
                    f"{case.name}: migration considered too low: {considered} < {args.min_migration_considered}"
                )
            touched = migration.get("last-touched")
            if isinstance(touched, int) and args.min_migration_touched > touched:
                raise Step12Error(
                    f"{case.name}: migration touched too low: {touched} < {args.min_migration_touched}"
                )

            source_key = {
                4: "source-invasion",
                0: "source-coop",
                1: "source-dm",
            }.get(case.mode)
            source_minimum = {
                "source-invasion": args.min_migration_source_invasion,
                "source-coop": args.min_migration_source_coop,
                "source-dm": args.min_migration_source_dm,
            }.get(source_key)
            if source_key is not None and source_minimum is not None and source_minimum > 0:
                source_value = migration.get(source_key)
                if not isinstance(source_value, int):
                    raise Step12Error(f"{case.name}: migration metric {source_key} is missing or invalid")
                if source_value < source_minimum:
                    raise Step12Error(f"{case.name}: migration {source_key} too low: {source_value} < {source_minimum}")
        native_profile: dict[str, dict[str, object]] = {}
        if args.require_native_live:
            if args.client_count <= 0:
                raise Step12Error(f"{case.name}: --require-native-live requires --client-count >= 1")
            profile_start = server.log_count()
            server.send_command("net_profile")
            if not server.wait_for_log_substring("HCDE net profile:", timeout_s=args.report_wait, start_index=profile_start):
                raise Step12Error(f"{case.name}: did not observe net_profile output")
            profile_lines = server.snapshot_log_lines()[profile_start:]
            native_profile = parse_native_profile(profile_lines)
            validate_native_profile(args, case.name, native_profile)
        if args.min_queries and query_count < args.min_queries:
            raise Step12Error(f"{case.name}: insufficient launcher samples {query_count} < {args.min_queries}")
        save_debug_trace(args, server, case)

        summary["query_count"] = query_count
        summary["query_failures"] = query_failures
        if last_snapshot is not None:
            summary["last_snapshot"] = {
                "map_name": last_snapshot.map_name,
                "player_count": last_snapshot.player_count,
                "max_players": last_snapshot.max_players,
                "game_mode": last_snapshot.game_mode,
                "game_mode_name": last_snapshot.game_mode_name or mode_name(case.mode),
                "invasion_state_name": last_snapshot.invasion_state_name,
                "invasion_wave": last_snapshot.invasion_wave,
                "invasion_active_monsters": last_snapshot.invasion_active_monsters,
            }
        summary["stress_lines"] = stress_lines[-64:]
        summary["stress_metrics"] = stress_metrics
        summary["native_profile"] = native_profile
        summary["ok"] = True

        if last_snapshot is not None:
            print(
                f"[ok] {case.name}: queries={query_count} failures={query_failures} "
                f"map={last_snapshot.map_name} players={last_snapshot.player_count}/{last_snapshot.max_players} "
                f"mode={last_snapshot.game_mode_name or mode_name(case.mode)} "
                f"wave={last_snapshot.invasion_wave} active={last_snapshot.invasion_active_monsters}"
            )
        else:
            print(f"[ok] {case.name}: completed with no successful launcher query samples")
        return summary
    finally:
        stop_clients(clients)
        server.stop()


def validate_paths(args: argparse.Namespace) -> None:
    if args.dry_run:
        return
    if args.server is None or not args.server.is_file():
        raise Step12Error(f"server binary not found: {args.server}")
    if args.iwad is None or not args.iwad.is_file():
        raise Step12Error(f"IWAD file not found: {args.iwad}")
    for path in args.server_file:
        if not path.is_file():
            raise Step12Error(f"server-file path not found: {path}")
    for path in args.client_file:
        if not path.is_file():
            raise Step12Error(f"client-file path not found: {path}")
    if args.client_count > 0 and (args.client is None or not args.client.is_file()):
        raise Step12Error(f"client binary not found: {args.client}")
    if args.require_native_live and args.client_count <= 0:
        raise Step12Error("--require-native-live requires --client-count >= 1")
    if args.require_native_live and (args.client is None or not args.client.is_file()):
        raise Step12Error("--require-native-live requires a valid --client binary")
    if args.workdir is None:
        args.workdir = args.server.resolve().parent
    if not args.workdir.exists():
        raise Step12Error(f"workdir does not exist: {args.workdir}")
    if args.trace_save_dir is not None:
        args.trace_save_dir.mkdir(parents=True, exist_ok=True)
    if args.summary_dir is not None:
        args.summary_dir.mkdir(parents=True, exist_ok=True)
    if args.min_sim_lod_suspended < 0:
        raise Step12Error("--min-sim-lod-suspended must be zero or positive")
    if args.min_migration_considered < 0:
        raise Step12Error("--min-migration-considered must be zero or positive")
    if args.min_migration_touched < 0:
        raise Step12Error("--min-migration-touched must be zero or positive")
    if args.min_migration_source_invasion < 0:
        raise Step12Error("--min-migration-source-invasion must be zero or positive")
    if args.min_migration_source_coop < 0:
        raise Step12Error("--min-migration-source-coop must be zero or positive")
    if args.min_migration_source_dm < 0:
        raise Step12Error("--min-migration-source-dm must be zero or positive")
    if args.min_native_client_input_applied < 0:
        raise Step12Error("--min-native-client-input-applied must be zero or positive")
    if args.min_native_server_snapshot_built < 0:
        raise Step12Error("--min-native-server-snapshot-built must be zero or positive")
    if args.max_native_replay_requests < 0:
        raise Step12Error("--max-native-replay-requests must be zero or positive")
    if args.max_native_legacy_rejects < 0:
        raise Step12Error("--max-native-legacy-rejects must be zero or positive")


def safe_trace_label(text: str) -> str:
    """Make a filesystem-safe trace label that also works as a console token."""
    clean = []
    for char in text:
        if char.isalnum() or char in ("-", "_"):
            clean.append(char)
        else:
            clean.append("_")
    return "".join(clean).strip("_") or "run"


def save_debug_trace(args: argparse.Namespace, server: ServerProcess, case: StressCase) -> None:
    """Ask the engine debug trace system to persist the current soak trace."""
    if args.trace_save_dir is None:
        return

    label = safe_trace_label(args.label)
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    trace_path = (args.trace_save_dir / f"step12_{case.name}_{label}_{timestamp}.trace.txt").resolve()
    trace_start = server.log_count()
    server.send_command(f'debugtracesave "{trace_path}" all debug')
    if not server.wait_for_log_substring("Saved successfully.", timeout_s=args.report_wait, start_index=trace_start):
        raise Step12Error(f"{case.name}: debug trace was not saved to {trace_path}")
    print(f"[trace] {case.name}: {trace_path}")


def write_summary(args: argparse.Namespace, summaries: list[dict[str, object]]) -> None:
    """Persist comparable JSON results for shaped-network and mod soak runs."""
    if args.summary_dir is None:
        return

    label = safe_trace_label(args.label or args.pressure_preset)
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    summary_path = (args.summary_dir / f"step12_{label}_{timestamp}.json").resolve()
    payload = {
        "label": args.label,
        "pressure_preset": args.pressure_preset,
        "network_shape": network_shape_metadata(args),
        "cases": summaries,
    }
    summary_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"[summary] {summary_path}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="HCDE Step 12 netcode stress harness")
    parser.add_argument("--server", type=Path, help="Path to hcdeserv binary")
    parser.add_argument("--client", type=Path, help="Optional path to hcde client binary for join pressure")
    parser.add_argument("--iwad", type=Path, help="Path to IWAD")
    parser.add_argument("--server-file", action="append", type=Path, default=[], help="PWAD/PK3 to load on server")
    parser.add_argument("--client-file", action="append", type=Path, default=[], help="PWAD/PK3 to load on clients")
    parser.add_argument("--workdir", type=Path, help="Launch working directory")
    parser.add_argument("--host", default="127.0.0.1", help="Host used for launcher query and client joins")
    parser.add_argument("--base-port", type=int, default=17900, help="Base UDP port for scenario runs")
    parser.add_argument("--server-slots", type=int, default=8, help="Visible server slots")
    parser.add_argument("--map", default="MAP01", help="Primary stress map")
    parser.add_argument("--dm-map", help="Optional map override for DM case")
    parser.add_argument("--cases", nargs="+", default=["invasion", "coop", "dm"], choices=["invasion", "coop", "dm"])
    parser.add_argument("--duration", type=float, default=30.0, help="Seconds per case")
    parser.add_argument("--startup-delay", type=float, default=1.5, help="Startup delay before commands")
    parser.add_argument("--command-delay", type=float, default=0.1, help="Delay after warmup commands")
    parser.add_argument("--query-interval", type=float, default=2.0, help="Seconds between launcher queries")
    parser.add_argument("--query-timeout", type=float, default=0.5, help="Per-query socket timeout")
    parser.add_argument("--query-attempts", type=int, default=4, help="Attempts per query sample")
    parser.add_argument("--max-query-failures", type=int, default=2, help="Allowed query failures per case")
    parser.add_argument("--min-queries", type=int, default=3, help="Fail case if fewer successful launcher snapshots are observed")
    parser.add_argument("--require-stress-metrics", action="store_true", help="Require final net_stressreport with parsed core metrics")
    parser.add_argument("--report-interval", type=float, default=10.0, help="Seconds between stress reports during soak")
    parser.add_argument("--report-wait", type=float, default=5.0, help="Seconds to wait for final stress report")
    parser.add_argument("--wave-pulses", type=int, default=1, help="Initial invasion_nextwave commands for invasion case")
    parser.add_argument("--periodic-wave-pulse", action="store_true", help="Keep pulsing invasion_nextwave during soak")
    parser.add_argument("--periodic-interval", type=float, default=10.0, help="Seconds between periodic commands")
    parser.add_argument("--server-command", action="append", default=[], help="Extra warmup command sent to every server")
    parser.add_argument("--periodic-command", action="append", default=[], help="Extra periodic command sent during soak")
    parser.add_argument("--pressure-preset", choices=sorted(PRESSURE_PRESET_COMMANDS), default="custom", help="Named command-pressure preset")
    parser.add_argument("--client-count", type=int, default=0, help="Optional clients to join during each case")
    parser.add_argument("--client-stagger", type=float, default=0.5, help="Seconds between client launches")
    parser.add_argument(
        "--require-native-live",
        action="store_true",
        help="Require final net_profile to prove native HCIN/HCSN traffic with no failure/replay storm counters",
    )
    parser.add_argument(
        "--min-native-client-input-applied",
        type=int,
        default=1,
        help="Minimum server-side client-input native-apply counter when --require-native-live is enabled",
    )
    parser.add_argument(
        "--min-native-server-snapshot-built",
        type=int,
        default=1,
        help="Minimum server-side snapshot native-built counter when --require-native-live is enabled",
    )
    parser.add_argument(
        "--max-native-replay-requests",
        type=int,
        default=0,
        help="Maximum allowed replay-req counter when --require-native-live is enabled",
    )
    parser.add_argument(
        "--max-native-legacy-rejects",
        type=int,
        default=0,
        help="Maximum allowed legacy-gameplay-reject counter when --require-native-live is enabled",
    )
    parser.add_argument("--advertise", action="store_true", help="Advertise to configured masters instead of -noadvertise")
    parser.add_argument("--master", action="append", default=[], help="Master endpoint for advertised runs")
    parser.add_argument("--label", default="", help="Free-form label for shaped-network runs")
    parser.add_argument("--shaper", default="", help="External network shaper name used for this run")
    parser.add_argument("--rtt-ms", type=float, help="External round-trip latency used for this run")
    parser.add_argument("--jitter-ms", type=float, help="External jitter used for this run")
    parser.add_argument("--loss-pct", type=float, help="External packet-loss percentage used for this run")
    parser.add_argument("--bandwidth-kbps", type=int, help="External bandwidth cap used for this run")
    parser.add_argument("--trace-save-dir", type=Path, help="Directory for per-case debugtracesave output")
    parser.add_argument("--summary-dir", type=Path, help="Directory for JSON run summaries")
    parser.add_argument("--require-sim-lod", action="store_true", help="Require sim-lod data in final net_stressreport")
    parser.add_argument(
        "--min-sim-lod-suspended",
        type=int,
        default=0,
        help="Fail if final sim-lod suspended actor count is below this value",
    )
    parser.add_argument("--require-migration", action="store_true", help="Require migration metrics in final net_stressreport")
    parser.add_argument("--min-migration-considered", type=int, default=0, help="Fail if migration considered drops below this value")
    parser.add_argument("--min-migration-touched", type=int, default=0, help="Fail if migration touched drops below this value")
    parser.add_argument(
        "--min-migration-source-invasion",
        type=int,
        default=0,
        help="Fail if final invasion source migration count is below this value",
    )
    parser.add_argument(
        "--min-migration-source-coop",
        type=int,
        default=0,
        help="Fail if final coop source migration count is below this value",
    )
    parser.add_argument(
        "--min-migration-source-dm",
        type=int,
        default=0,
        help="Fail if final dm source migration count is below this value",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print planned cases without launching binaries")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        apply_pressure_preset(args)
        cases = build_cases(args)
        validate_paths(args)
        if args.dry_run:
            print("[step12] dry run")
            print(f"  pressure_preset={args.pressure_preset}")
            if args.label:
                print(f"  label={args.label}")
            network_shape = network_shape_metadata(args)
            if network_shape:
                print(f"  network_shape={network_shape}")
            if args.trace_save_dir is not None:
                print(f"  trace_save_dir={args.trace_save_dir}")
            if args.summary_dir is not None:
                print(f"  summary_dir={args.summary_dir}")
            if args.require_native_live:
                print(
                    "  native_live="
                    f"min-client-input={args.min_native_client_input_applied} "
                    f"min-snapshots={args.min_native_server_snapshot_built} "
                    f"max-replay={args.max_native_replay_requests} "
                    f"max-legacy-rejects={args.max_native_legacy_rejects}"
                )
            for index, case in enumerate(cases):
                print(
                    f"  case={case.name} port={args.base_port + index} mode={case.mode} "
                    f"map={case.map_name} warmup={case.warmup_commands + args.server_command} "
                    f"periodic={case.periodic_commands + args.periodic_command}"
                )
            return 0

        if args.label:
            print(f"[step12] label={args.label}")
        summaries: list[dict[str, object]] = []
        for index, case in enumerate(cases):
            summaries.append(run_case(args, case, index))
        write_summary(args, summaries)
    except Step12Error as exc:
        print(f"[fail] {exc}")
        return 1
    except KeyboardInterrupt:
        print("[abort] interrupted by user")
        return 130

    print("[pass] Step 12 netcode stress harness completed successfully")
    return 0


if __name__ == "__main__":
    sys.exit(main())
