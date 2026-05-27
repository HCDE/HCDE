#!/usr/bin/env python3
"""Decode paired HCDE black box / diagnostic bundles and report divergence hints."""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator, Optional

# Layout must match FBlackboxEntryHeader in src/d_net_blackbox.cpp (40 bytes,
# pragma packed). Field order: magic, type, direction, lane, room, seq, ack,
# peerslot, gametic, clienttic, timestamp_ms, payload_size.
HBBX_MAGIC = 0x58484248  # 'HBBX' little-endian
ENTRY_HEADER = struct.Struct("<IBBBBIIiiiQI")
assert ENTRY_HEADER.size == 40, f"unexpected header size {ENTRY_HEADER.size}"
BBOX_PACKET = 1
BBOX_TIC_INDEX = 2


@dataclass
class BlackboxEntry:
    entry_type: int
    direction: int
    lane: int
    room: int
    seq: int
    ack: int
    peer_slot: int
    gametic: int
    clienttic: int
    timestamp_ms: int
    payload_size: int
    payload: bytes


def iter_blackbox_entries(path: Path) -> Iterator[BlackboxEntry]:
    data = path.read_bytes()
    offset = 0
    while offset + ENTRY_HEADER.size <= len(data):
        magic, etype, direction, lane, room, seq, ack, peer, gametic, clienttic, ts, payload_size = ENTRY_HEADER.unpack_from(data, offset)
        if magic != HBBX_MAGIC:
            break
        offset += ENTRY_HEADER.size
        payload = data[offset:offset + payload_size]
        offset += payload_size
        yield BlackboxEntry(
            int(etype), int(direction), int(lane), int(room), int(seq), int(ack),
            int(peer), int(gametic), int(clienttic), int(ts), int(payload_size), payload,
        )


def load_sidecar(path: Path) -> dict:
    sidecar = Path(f"{path}.json")
    if not sidecar.is_file():
        return {}
    try:
        return json.loads(sidecar.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def scan_trace_desyncs(trace_path: Path) -> list[tuple[int, str]]:
    if not trace_path.is_file():
        return []
    out: list[tuple[int, str]] = []
    pattern = re.compile(r"CHECKSUM_MISMATCH category=(\S+) tic=(\d+)")
    gametic = 0
    for line in trace_path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.search(r"gametic=(\d+)", line)
        if m:
            gametic = int(m.group(1))
        m2 = pattern.search(line)
        if m2:
            out.append((int(m2.group(2)), m2.group(1)))
        if "MARK label=" in line:
            out.append((gametic, f"mark:{line.strip()}"))
        if "HCDE prediction fault" in line:
            out.append((gametic, "prediction_fault"))
    return out


def summarize_blackbox(path: Path) -> dict:
    entries = list(iter_blackbox_entries(path))
    packets = [e for e in entries if e.entry_type == BBOX_PACKET]
    tics = [e for e in entries if e.entry_type == BBOX_TIC_INDEX]
    tx = sum(1 for e in packets if e.direction == 0)
    rx = sum(1 for e in packets if e.direction == 1)
    return {
        "path": str(path),
        "entries": len(entries),
        "packets": len(packets),
        "tic_indexes": len(tics),
        "tx_packets": tx,
        "rx_packets": rx,
        "gametic_range": (
            min((e.gametic for e in entries), default=0),
            max((e.gametic for e in entries), default=0),
        ),
        "sidecar": load_sidecar(path),
    }


def find_bundle_traces(bundle_dir: Path) -> list[Path]:
    """Collect every text-style log file in a diagnostic bundle.

    Bundles assembled by Net_DiagWriteBundle place a `net_trace_snapshot.txt`
    file plus copies of the streaming `hcde_trace_*.log` / `latest_*.txt`
    artifacts and the prediction-fault `*.log` next to manifest.txt. Match all
    of those rather than only the live stream files so the replay tool can
    surface every anchor that the engine recorded.
    """
    traces: list[Path] = []
    if not bundle_dir.is_dir():
        return traces
    for name in bundle_dir.iterdir():
        if not name.is_file():
            continue
        if name.suffix in (".log", ".txt") and (
            "hcde_trace" in name.name
            or name.name.startswith("net_trace")
            or name.name.startswith("latest_")
            or "prediction" in name.name
        ):
            traces.append(name)
    return traces


def compare_pair(client_bundle: Path, server_bundle: Path) -> str:
    lines = ["HCDE black box / bundle compare", ""]
    client_bb = sorted(client_bundle.glob("hcde_blackbox_*.bin"))
    server_bb = sorted(server_bundle.glob("hcde_blackbox_*.bin"))
    if client_bb:
        lines.append(f"Client blackbox: {summarize_blackbox(client_bb[-1])}")
    if server_bb:
        lines.append(f"Server blackbox: {summarize_blackbox(server_bb[-1])}")

    client_events: list[tuple[int, str]] = []
    server_events: list[tuple[int, str]] = []
    for trace in find_bundle_traces(client_bundle):
        client_events.extend(scan_trace_desyncs(trace))
    for trace in find_bundle_traces(server_bundle):
        server_events.extend(scan_trace_desyncs(trace))

    lines.append("")
    lines.append("Client trace anchors:")
    for tic, label in client_events[:20]:
        lines.append(f"  tic={tic} {label}")
    lines.append("Server trace anchors:")
    for tic, label in server_events[:20]:
        lines.append(f"  tic={tic} {label}")

    client_marks = {tic for tic, ev in client_events if ev.startswith("mark:")}
    server_marks = {tic for tic, ev in server_events if ev.startswith("mark:")}
    client_desync = [(tic, ev) for tic, ev in client_events if ev != "prediction_fault" and not ev.startswith("mark:")]
    server_desync = [(tic, ev) for tic, ev in server_events if ev != "prediction_fault" and not ev.startswith("mark:")]
    if client_desync or server_desync:
        first = min(client_desync + server_desync, key=lambda x: x[0], default=None)
        if first:
            lines.append("")
            lines.append(f"First divergent anchor: tic={first[0]} category/event={first[1]}")
    if client_marks | server_marks:
        lines.append("")
        lines.append(f"Correlated mark tics client={sorted(client_marks)} server={sorted(server_marks)}")
    return "\n".join(lines)


def main(argv: Optional[Iterable[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Replay/compare HCDE diagnostic bundles")
    parser.add_argument("paths", nargs="+", help="blackbox .bin file or diagnostic bundle directory")
    parser.add_argument("-o", "--output", help="write HTML/text report")
    args = parser.parse_args(list(argv) if argv is not None else None)

    paths = [Path(p) for p in args.paths]
    if len(paths) == 1 and paths[0].suffix == ".bin":
        report = json.dumps(summarize_blackbox(paths[0]), indent=2)
    elif len(paths) == 2 and paths[0].is_dir() and paths[1].is_dir():
        report = compare_pair(paths[0], paths[1])
    else:
        chunks = []
        for path in paths:
            if path.suffix == ".bin":
                chunks.append(json.dumps(summarize_blackbox(path), indent=2))
            elif path.is_dir():
                chunks.append(f"Bundle {path}:")
                for bb in sorted(path.glob("hcde_blackbox_*.bin")):
                    chunks.append(json.dumps(summarize_blackbox(bb), indent=2))
                for trace in find_bundle_traces(path):
                    chunks.append(f"Trace {trace.name}: {len(scan_trace_desyncs(trace))} anchors")
            else:
                chunks.append(f"Unknown path: {path}")
        report = "\n\n".join(chunks)

    if args.output:
        Path(args.output).write_text(report, encoding="utf-8")
        print(f"Wrote {args.output}")
    else:
        print(report)
    return 0


if __name__ == "__main__":
    sys.exit(main())
