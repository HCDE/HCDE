#!/usr/bin/env python3
"""Diagnose HCDE runtime compatibility failures from logs and local archives.

The regular engine debug trace is useful once HCDE knows which subsystem failed,
but mod-compat work often starts one layer earlier: the startup log says a map
needs textures, actors, or ACS behavior that the current load set does not
provide. This tool turns that log noise into a reviewable report and scans local
WAD/PK3 folders for likely missing resource packs.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
import zipfile
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Iterable


ARCHIVE_SUFFIXES = {".wad", ".pk3", ".pkz", ".zip"}
TEXT_SUFFIXES = {
    ".acs",
    ".bex",
    ".cfg",
    ".dec",
    ".deh",
    ".edf",
    ".ini",
    ".mapinfo",
    ".txt",
    ".zs",
}
TEXT_LUMP_NAMES = {
    "ANIMDEFS",
    "CVARINFO",
    "DECORATE",
    "DEHACKED",
    "EDF",
    "EDFROOT",
    "GAMEINFO",
    "GLDEFS",
    "KEYCONF",
    "LANGUAGE",
    "LOADACS",
    "MAPINFO",
    "MENUDEF",
    "SNDINFO",
    "TEXTURES",
    "UMAPINFO",
    "ZMAPINFO",
    "ZSCRIPT",
}
SCRIPT_CONTEXT_WORDS = (
    "actor",
    "class",
    "custommonsterinvasionspot",
    "doomednum",
    "doomednums",
    "dropitem",
    "mapinfo",
    "spawnnum",
    "thing",
)
MAX_TEXT_BYTES = 2 * 1024 * 1024


@dataclass
class LogEvent:
    kind: str
    name: str
    log: str
    line: int
    text: str


@dataclass
class ResourceHit:
    resource: str
    match_kind: str
    detail: str
    count: int = 1


@dataclass
class ResourceCandidate:
    resource: str
    score: int = 0
    hits: list[ResourceHit] = field(default_factory=list)


@dataclass
class RuntimeDiagnosis:
    logs: list[str]
    search_roots: list[str]
    missing_textures: list[LogEvent]
    unknown_thing_ids: list[LogEvent]
    missing_actor_names: list[LogEvent]
    acs_errors: list[LogEvent]
    script_errors: list[LogEvent]
    candidates: list[ResourceCandidate]


def add_unique_event(events: list[LogEvent], event: LogEvent) -> None:
    key = (event.kind, event.name.lower(), event.log, event.line)
    for existing in events:
        existing_key = (existing.kind, existing.name.lower(), existing.log, existing.line)
        if existing_key == key:
            return
    events.append(event)


def parse_logs(paths: Iterable[Path]) -> tuple[list[LogEvent], list[LogEvent], list[LogEvent], list[LogEvent], list[LogEvent]]:
    missing_textures: list[LogEvent] = []
    unknown_thing_ids: list[LogEvent] = []
    missing_actor_names: list[LogEvent] = []
    acs_errors: list[LogEvent] = []
    script_errors: list[LogEvent] = []

    texture_re = re.compile(
        r"\bUnknown\s+(?:floor|ceiling|top|bottom|middle|wall|texture|flat)?\s*texture\s+['\"]?([A-Za-z0-9_.$-]+)",
        re.IGNORECASE,
    )
    flat_re = re.compile(r"\bUnknown\s+flat\s+['\"]?([A-Za-z0-9_.$-]+)", re.IGNORECASE)
    patch_re = re.compile(r"\bUnknown\s+patch\s+['\"]([^'\"]+)['\"]\s+in\s+texture\s+['\"]([^'\"]+)['\"]", re.IGNORECASE)
    thing_re = re.compile(r"\bUnknown\s+type\s+(\d+)\b", re.IGNORECASE)
    actor_re = re.compile(
        r"\b(?:Unknown|missing|undefined)\s+(?:actor|class|type|drop item class)\s+['\"]?([A-Za-z_][A-Za-z0-9_.:-]*)",
        re.IGNORECASE,
    )
    malformed_re = re.compile(r"\bMalformed\s+(PCD_[A-Za-z0-9_]+)\s+in\s+script\s+(\d+)", re.IGNORECASE)
    acs_oob_re = re.compile(
        r"Out of bounds memory access in ACS VM(?:\s+\(([^)]*)\))?",
        re.IGNORECASE,
    )
    script_re = re.compile(r"\b(?:Script error|VM execution aborted|CVMAbort|ACS VM)\b", re.IGNORECASE)

    for path in paths:
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError as exc:
            add_unique_event(
                script_errors,
                LogEvent("log-read-error", str(path), str(path), 0, f"Could not read log: {exc}"),
            )
            continue

        for line_no, line in enumerate(lines, start=1):
            for match in texture_re.finditer(line):
                add_unique_event(
                    missing_textures,
                    LogEvent("texture", match.group(1), str(path), line_no, line.strip()[:260]),
                )
            for match in flat_re.finditer(line):
                add_unique_event(
                    missing_textures,
                    LogEvent("flat", match.group(1), str(path), line_no, line.strip()[:260]),
                )
            for match in patch_re.finditer(line):
                add_unique_event(
                    missing_textures,
                    LogEvent("patch", match.group(1), str(path), line_no, line.strip()[:260]),
                )
            for match in thing_re.finditer(line):
                add_unique_event(
                    unknown_thing_ids,
                    LogEvent("thing-id", match.group(1), str(path), line_no, line.strip()[:260]),
                )
            for match in actor_re.finditer(line):
                add_unique_event(
                    missing_actor_names,
                    LogEvent("actor-name", match.group(1), str(path), line_no, line.strip()[:260]),
                )
            malformed = malformed_re.search(line)
            if malformed:
                detail = f"{malformed.group(1)} in script {malformed.group(2)}"
                add_unique_event(
                    acs_errors,
                    LogEvent("acs-malformed-pcode", detail, str(path), line_no, line.strip()[:260]),
                )
            oob = acs_oob_re.search(line)
            if oob:
                add_unique_event(
                    acs_errors,
                    LogEvent("acs-oob", oob.group(1) or "out-of-bounds", str(path), line_no, line.strip()[:260]),
                )
            if script_re.search(line):
                add_unique_event(
                    script_errors,
                    LogEvent("script-runtime", "script", str(path), line_no, line.strip()[:260]),
                )

    return missing_textures, unknown_thing_ids, missing_actor_names, acs_errors, script_errors


def archive_paths(roots: Iterable[Path], max_size_mb: int) -> list[Path]:
    limit = max_size_mb * 1024 * 1024
    paths: list[Path] = []
    for root in roots:
        if not root.exists():
            continue
        candidates = [root] if root.is_file() else root.rglob("*")
        for candidate in candidates:
            if not candidate.is_file():
                continue
            if candidate.suffix.lower() not in ARCHIVE_SUFFIXES:
                continue
            try:
                if candidate.stat().st_size > limit:
                    continue
            except OSError:
                continue
            paths.append(candidate)
    return sorted(paths, key=lambda item: str(item).lower())


def safe_decode(data: bytes) -> str:
    for encoding in ("utf-8-sig", "utf-8", "cp1252", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("latin-1", errors="replace")


def is_text_entry(name: str) -> bool:
    normalized = name.replace("\\", "/")
    base = Path(normalized).name.upper()
    suffix = Path(normalized).suffix.lower()
    return base in TEXT_LUMP_NAMES or suffix in TEXT_SUFFIXES or normalized.lower().startswith(("actors/", "decorate/", "zscript/"))


def parse_wad_lumps(data: bytes) -> list[tuple[str, bytes]]:
    if len(data) < 12:
        return []
    magic, count, directory_offset = struct.unpack_from("<4sii", data, 0)
    if magic not in {b"IWAD", b"PWAD"} or count < 0 or directory_offset < 12:
        return []
    if directory_offset + count * 16 > len(data):
        return []

    lumps: list[tuple[str, bytes]] = []
    for index in range(count):
        entry_offset = directory_offset + index * 16
        file_pos, size, raw_name = struct.unpack_from("<ii8s", data, entry_offset)
        if file_pos < 0 or size < 0 or file_pos + size > len(data):
            continue
        name = raw_name.split(b"\0", 1)[0].decode("latin-1", errors="replace")
        lumps.append((name, data[file_pos:file_pos + size]))
    return lumps


def parse_texture_lump_names(data: bytes) -> list[str]:
    if len(data) < 4:
        return []
    try:
        count = struct.unpack_from("<i", data, 0)[0]
    except struct.error:
        return []
    if count < 0 or count > 65535 or 4 + count * 4 > len(data):
        return []

    names: list[str] = []
    for index in range(count):
        try:
            offset = struct.unpack_from("<i", data, 4 + index * 4)[0]
        except struct.error:
            continue
        if offset < 0 or offset + 22 > len(data):
            continue
        raw_name = data[offset:offset + 8]
        name = raw_name.split(b"\0", 1)[0].decode("latin-1", errors="replace").strip()
        if name:
            names.append(name)
    return names


def add_hit(candidates: dict[str, ResourceCandidate], resource: Path, match_kind: str, detail: str, score: int) -> None:
    key = str(resource.resolve())
    candidate = candidates.setdefault(key, ResourceCandidate(resource=key))
    candidate.score += score
    for hit in candidate.hits:
        if hit.match_kind == match_kind and hit.detail == detail:
            hit.count += 1
            return
    candidate.hits.append(ResourceHit(resource=key, match_kind=match_kind, detail=detail))


def search_text_for_actor_ids(text: str, ids: set[str]) -> list[tuple[str, str]]:
    hits: list[tuple[str, str]] = []
    if not ids:
        return hits
    for line_no, line in enumerate(text.splitlines(), start=1):
        lower = line.lower()
        if not any(word in lower for word in SCRIPT_CONTEXT_WORDS):
            continue
        for actor_id in ids:
            if re.search(rf"\b{re.escape(actor_id)}\b", line):
                hits.append((actor_id, f"line {line_no}: {line.strip()[:160]}"))
    return hits


def scan_zip_resource(path: Path, texture_names: set[str], actor_ids: set[str], actor_names: set[str], candidates: dict[str, ResourceCandidate]) -> None:
    texture_names_upper = {name.upper() for name in texture_names}
    with zipfile.ZipFile(path) as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            entry_name = info.filename.replace("\\", "/")
            entry_upper = entry_name.upper()
            entry_base = Path(entry_name).stem.upper()
            for texture in texture_names:
                texture_upper = texture.upper()
                if entry_base == texture_upper or Path(entry_name).name.upper() == texture_upper:
                    add_hit(candidates, path, "entry-name-exact", f"{texture} as {entry_name}", 8)
                elif texture_upper in entry_upper:
                    add_hit(candidates, path, "entry-name-contains", f"{texture} in {entry_name}", 4)

            entry_data: bytes | None = None
            if entry_base in {"TEXTURE1", "TEXTURE2"} and info.file_size <= MAX_TEXT_BYTES:
                try:
                    entry_data = zf.read(info)
                except (OSError, RuntimeError, zipfile.BadZipFile):
                    entry_data = None
                if entry_data is not None:
                    for texture_name in parse_texture_lump_names(entry_data):
                        if texture_name.upper() in texture_names_upper:
                            add_hit(candidates, path, "texture-table", f"{texture_name} in {entry_name}", 10)

            if info.file_size > MAX_TEXT_BYTES or not is_text_entry(entry_name):
                continue
            try:
                if entry_data is None:
                    entry_data = zf.read(info)
                text = safe_decode(entry_data)
            except (OSError, RuntimeError, zipfile.BadZipFile):
                continue
            upper_text = text.upper()
            for texture in texture_names:
                if re.search(rf"\b{re.escape(texture.upper())}\b", upper_text):
                    add_hit(candidates, path, "text-reference", f"{texture} in {entry_name}", 2)
            for actor_name in actor_names:
                if re.search(rf"\b{re.escape(actor_name)}\b", text, flags=re.IGNORECASE):
                    add_hit(candidates, path, "actor-name-reference", f"{actor_name} in {entry_name}", 3)
            for actor_id, detail in search_text_for_actor_ids(text, actor_ids):
                add_hit(candidates, path, "thing-id-reference", f"{actor_id} in {entry_name}: {detail}", 7)


def scan_wad_resource(path: Path, texture_names: set[str], actor_ids: set[str], actor_names: set[str], candidates: dict[str, ResourceCandidate]) -> None:
    data = path.read_bytes()
    lumps = parse_wad_lumps(data)
    if not lumps:
        return

    for lump_name, lump_data in lumps:
        lump_upper = lump_name.upper()
        for texture in texture_names:
            texture_upper = texture.upper()
            if lump_upper == texture_upper:
                add_hit(candidates, path, "wad-lump-exact", f"{texture} lump {lump_name}", 8)
            elif texture_upper in lump_upper:
                add_hit(candidates, path, "wad-lump-contains", f"{texture} lump {lump_name}", 4)

        if lump_upper in {"TEXTURE1", "TEXTURE2"}:
            for texture_name in parse_texture_lump_names(lump_data):
                if texture_name.upper() in {name.upper() for name in texture_names}:
                    add_hit(candidates, path, "wad-texture-table", f"{texture_name} in {lump_name}", 10)

        if len(lump_data) <= MAX_TEXT_BYTES and is_text_entry(lump_name):
            text = safe_decode(lump_data)
            for actor_id, detail in search_text_for_actor_ids(text, actor_ids):
                add_hit(candidates, path, "thing-id-reference", f"{actor_id} in {lump_name}: {detail}", 7)
            for actor_name in actor_names:
                if re.search(rf"\b{re.escape(actor_name)}\b", text, flags=re.IGNORECASE):
                    add_hit(candidates, path, "actor-name-reference", f"{actor_name} in {lump_name}", 3)


def scan_resources(
    roots: Iterable[Path],
    texture_names: set[str],
    actor_ids: set[str],
    actor_names: set[str],
    max_size_mb: int,
) -> list[ResourceCandidate]:
    candidates: dict[str, ResourceCandidate] = {}
    for path in archive_paths(roots, max_size_mb):
        try:
            if path.suffix.lower() in {".pk3", ".pkz", ".zip"}:
                scan_zip_resource(path, texture_names, actor_ids, actor_names, candidates)
            elif path.suffix.lower() == ".wad":
                scan_wad_resource(path, texture_names, actor_ids, actor_names, candidates)
        except (OSError, zipfile.BadZipFile, struct.error):
            continue

    return sorted(candidates.values(), key=lambda item: (-item.score, item.resource.lower()))


def unique_names(events: Iterable[LogEvent]) -> list[str]:
    names = sorted({event.name for event in events}, key=lambda value: value.lower())
    return names


def event_counts(events: Iterable[LogEvent]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for event in events:
        counts[event.name] = counts.get(event.name, 0) + 1
    return dict(sorted(counts.items(), key=lambda item: (-item[1], item[0].lower())))


def first_event(events: Iterable[LogEvent], name: str) -> LogEvent | None:
    for event in events:
        if event.name == name:
            return event
    return None


def candidate_details_for_name(candidates: list[ResourceCandidate], name: str) -> list[str]:
    needle = name.lower()
    resources: list[str] = []
    for candidate in candidates:
        for hit in candidate.hits:
            if needle in hit.detail.lower():
                resources.append(Path(candidate.resource).name)
                break
    return resources[:6]


def markdown_table_row(values: Iterable[str]) -> str:
    return "| " + " | ".join(value.replace("\n", " ").replace("|", "\\|") for value in values) + " |"


def render_report(diagnosis: RuntimeDiagnosis) -> str:
    candidates = diagnosis.candidates
    lines: list[str] = []
    lines.append("# HCDE Runtime Resource Diagnosis")
    lines.append("")
    lines.append("This report is generated from HCDE logs plus local WAD/PK3 archive scans. It is diagnostic output only; it does not copy third-party mod content into HCDE.")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append(f"- Logs scanned: `{len(diagnosis.logs)}`")
    lines.append(f"- Search roots: `{len(diagnosis.search_roots)}`")
    lines.append(f"- Missing texture/flat/patch names: `{len(unique_names(diagnosis.missing_textures))}`")
    lines.append(f"- Unknown map thing ids: `{len(unique_names(diagnosis.unknown_thing_ids))}`")
    lines.append(f"- Missing actor/class names: `{len(unique_names(diagnosis.missing_actor_names))}`")
    lines.append(f"- ACS VM warnings/errors: `{len(diagnosis.acs_errors)}`")
    lines.append(f"- Likely resource archives: `{len(candidates)}`")
    lines.append("")

    lines.append("## Likely Resource Archives")
    lines.append("")
    if candidates:
        lines.append(markdown_table_row(["Score", "Archive", "Evidence"]))
        lines.append(markdown_table_row(["---:", "---", "---"]))
        for candidate in candidates[:20]:
            preview = "; ".join(f"{hit.match_kind}: {hit.detail} ({hit.count})" for hit in candidate.hits[:5])
            lines.append(markdown_table_row([str(candidate.score), candidate.resource, preview]))
    else:
        lines.append("- No matching archives found in the configured search roots.")
    lines.append("")

    lines.append("## Missing Textures, Flats, And Patches")
    lines.append("")
    texture_counts = event_counts(diagnosis.missing_textures)
    if texture_counts:
        lines.append(markdown_table_row(["Name", "Count", "First evidence", "Candidate archives"]))
        lines.append(markdown_table_row(["---", "---:", "---", "---"]))
        for name, count in list(texture_counts.items())[:80]:
            event = first_event(diagnosis.missing_textures, name)
            evidence = f"{Path(event.log).name}:{event.line}" if event else ""
            archives = ", ".join(candidate_details_for_name(candidates, name)) or "not found"
            lines.append(markdown_table_row([name, str(count), evidence, archives]))
    else:
        lines.append("- No missing texture, flat, or patch diagnostics were found.")
    lines.append("")

    lines.append("## Unknown Map Thing Ids")
    lines.append("")
    thing_counts = event_counts(diagnosis.unknown_thing_ids)
    if thing_counts:
        lines.append(markdown_table_row(["Id", "Count", "First evidence", "Candidate archives"]))
        lines.append(markdown_table_row(["---:", "---:", "---", "---"]))
        for name, count in thing_counts.items():
            event = first_event(diagnosis.unknown_thing_ids, name)
            evidence = f"{Path(event.log).name}:{event.line}" if event else ""
            archives = ", ".join(candidate_details_for_name(candidates, name)) or "not found"
            lines.append(markdown_table_row([name, str(count), evidence, archives]))
    else:
        lines.append("- No unknown map thing ids were found.")
    lines.append("")

    lines.append("## Missing Actor Or Class Names")
    lines.append("")
    actor_counts = event_counts(diagnosis.missing_actor_names)
    if actor_counts:
        lines.append(markdown_table_row(["Name", "Count", "First evidence", "Candidate archives"]))
        lines.append(markdown_table_row(["---", "---:", "---", "---"]))
        for name, count in actor_counts.items():
            event = first_event(diagnosis.missing_actor_names, name)
            evidence = f"{Path(event.log).name}:{event.line}" if event else ""
            archives = ", ".join(candidate_details_for_name(candidates, name)) or "not found"
            lines.append(markdown_table_row([name, str(count), evidence, archives]))
    else:
        lines.append("- No missing actor/class name diagnostics were found.")
    lines.append("")

    lines.append("## ACS And Script Runtime Issues")
    lines.append("")
    runtime_events = diagnosis.acs_errors + diagnosis.script_errors
    if runtime_events:
        lines.append(markdown_table_row(["Kind", "Name", "Location", "Log text"]))
        lines.append(markdown_table_row(["---", "---", "---", "---"]))
        for event in runtime_events[:80]:
            lines.append(markdown_table_row([event.kind, event.name, f"{Path(event.log).name}:{event.line}", event.text]))
    else:
        lines.append("- No ACS/script runtime diagnostics were found.")
    lines.append("")

    lines.append("## Next Use")
    lines.append("")
    lines.append("- Add or auto-load the highest scoring external resource archives first, then rerun the same command.")
    lines.append("- If texture and actor misses disappear but ACS errors remain, the next bug is VM/API compatibility rather than missing assets.")
    lines.append("- Keep third-party mod assets external unless their license explicitly allows redistribution with HCDE.")
    lines.append("")
    return "\n".join(lines)


def build_diagnosis(args: argparse.Namespace) -> RuntimeDiagnosis:
    log_paths = [path.resolve() for path in args.log]
    search_roots = [path.resolve() for path in args.search_root]
    missing_textures, unknown_thing_ids, missing_actor_names, acs_errors, script_errors = parse_logs(log_paths)
    texture_names = set(unique_names(missing_textures))
    actor_ids = set(unique_names(unknown_thing_ids))
    actor_names = set(unique_names(missing_actor_names))
    candidates = scan_resources(search_roots, texture_names, actor_ids, actor_names, args.max_archive_mb)
    return RuntimeDiagnosis(
        logs=[str(path) for path in log_paths],
        search_roots=[str(path) for path in search_roots],
        missing_textures=missing_textures,
        unknown_thing_ids=unknown_thing_ids,
        missing_actor_names=missing_actor_names,
        acs_errors=acs_errors,
        script_errors=script_errors,
        candidates=candidates,
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a runtime compatibility diagnosis from HCDE logs and local WAD/PK3 archives."
    )
    parser.add_argument("--log", action="append", type=Path, required=True, help="HCDE log file to inspect. May be repeated.")
    parser.add_argument(
        "--search-root",
        action="append",
        type=Path,
        required=True,
        help="Folder or archive to scan for likely missing resources. May be repeated.",
    )
    parser.add_argument("--out", type=Path, help="Markdown report path.")
    parser.add_argument("--json", type=Path, help="Optional JSON report path.")
    parser.add_argument("--max-archive-mb", type=int, default=512, help="Skip archives larger than this many megabytes.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    diagnosis = build_diagnosis(args)
    markdown = render_report(diagnosis)

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(markdown, encoding="utf-8", newline="\n")
        print(f"Wrote HCDE runtime diagnosis: {args.out}")
    else:
        print(markdown)

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(asdict(diagnosis), indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
        print(f"Wrote HCDE runtime diagnosis JSON: {args.json}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
