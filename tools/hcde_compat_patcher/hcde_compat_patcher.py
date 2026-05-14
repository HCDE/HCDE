#!/usr/bin/env python3
"""Generate reviewable HCDE mod-compat patch candidates.

This tool scans a local Doom mod archive and writes a candidate folder for
HCDE's `wadsrc_mod_compat` work. It intentionally does not copy third-party
mod code or assets into the candidate; the output is a report, provenance data,
and empty/TODO patch stubs for human review.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
import textwrap
import zipfile
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Iterable


TEXT_LUMP_NAMES = {
    "ANIMDEFS",
    "CVARINFO",
    "DECALDEF",
    "DECORATE",
    "EDF",
    "EDFROOT",
    "EMAPINFO",
    "EXTRADATA",
    "GAMEINFO",
    "GLDEFS",
    "KEYCONF",
    "LANGUAGE",
    "LOADACS",
    "LOCKDEFS",
    "MAPINFO",
    "MENUDEF",
    "MODELDEF",
    "SNDINFO",
    "SNDSEQ",
    "TERRAIN",
    "UMAPINFO",
    "VOXELDEF",
    "ZMAPINFO",
    "ZSCRIPT",
}

EDGE_DDF_LUMP_NAMES = {
    "DDFANIM",
    "DDFATK",
    "DDFCOLM",
    "DDFCOLOR",
    "DDFFONT",
    "DDFGAME",
    "DDFIMAGE",
    "DDFLANG",
    "DDFLEVL",
    "DDFLINE",
    "DDFPLAY",
    "DDFSFX",
    "DDFSECT",
    "DDFSTYLE",
    "DDFTHING",
    "DDFWEAP",
}

EDGE_SCRIPT_LUMP_NAMES = {
    "COAL",
    "LUA",
    "RADTRIG",
    "RADTRIGS",
    "RTS",
}

TEXT_EXTENSIONS = {
    ".acs",
    ".bex",
    ".coal",
    ".cfg",
    ".dec",
    ".deh",
    ".ddf",
    ".edf",
    ".ini",
    ".lua",
    ".mapinfo",
    ".rts",
    ".txt",
    ".zs",
}

MAX_TEXT_BYTES = 1_048_576

ETERNITY_EDF_BEHAVIOR_PROPERTIES = {
    "action",
    "activesound",
    "addflags",
    "attacksound",
    "basictype",
    "bloodcolor",
    "cflags",
    "crashstate",
    "damage",
    "deathsound",
    "deathstate",
    "dropitem",
    "explosiondamage",
    "explosionradius",
    "firstdecoratestate",
    "flags",
    "healstate",
    "health",
    "height",
    "mass",
    "meleestate",
    "missiletype",
    "missilestate",
    "obituary",
    "painchance",
    "painsound",
    "painstate",
    "particlefx",
    "radius",
    "reactiontime",
    "remflags",
    "raisestate",
    "seesound",
    "seestate",
    "spawnstate",
    "speed",
    "states",
    "translation",
    "xdeathstate",
}


@dataclass
class ArchiveEntry:
    name: str
    size: int


@dataclass
class TextHit:
    entry: str
    line: int
    text: str


@dataclass
class CompatibilityCheck:
    id: str
    severity: str
    title: str
    detail: str
    hits: list[TextHit] = field(default_factory=list)


@dataclass
class EternityIncludeHint:
    entry: str
    line: int
    kind: str
    target: str


@dataclass
class EternityMapHint:
    entry: str
    line: int
    map_name: str


@dataclass
class EternityThingHint:
    entry: str
    line: int
    kind: str
    name: str
    doomednum: int | None = None
    dehackednum: int | None = None
    spawnid: int | None = None
    compatname: str | None = None
    behavior_properties: list[str] = field(default_factory=list)


@dataclass
class EternityValidationProfile:
    emapinfo_entries: list[str] = field(default_factory=list)
    native_mapinfo_entries: list[str] = field(default_factory=list)
    extradata_entries: list[str] = field(default_factory=list)
    edf_entries: list[str] = field(default_factory=list)
    edf_roots: list[str] = field(default_factory=list)
    edf_includes: list[EternityIncludeHint] = field(default_factory=list)
    emapinfo_maps: list[EternityMapHint] = field(default_factory=list)
    emapinfo_extradata_refs: list[TextHit] = field(default_factory=list)
    edf_things: list[EternityThingHint] = field(default_factory=list)
    stage_flags: dict[str, bool] = field(default_factory=dict)
    notes: list[str] = field(default_factory=list)


@dataclass
class ScanResult:
    source_path: str
    archive_type: str
    sha256: str
    slug: str
    entries: list[ArchiveEntry]
    surfaces: dict[str, list[str]]
    checks: list[CompatibilityCheck]
    warnings: list[str]
    eternity: EternityValidationProfile | None = None


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def make_slug(path: Path) -> str:
    stem = path.stem.lower()
    stem = re.sub(r"[^a-z0-9]+", "_", stem).strip("_")
    return stem or "mod"


def decode_text(data: bytes) -> str:
    for encoding in ("utf-8-sig", "utf-8", "cp1252", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("latin-1", errors="replace")


def is_text_candidate(name: str) -> bool:
    normalized = name.replace("\\", "/")
    base = Path(normalized).name.upper()
    suffix = Path(normalized).suffix.lower()
    if base in TEXT_LUMP_NAMES:
        return True
    if base in EDGE_DDF_LUMP_NAMES or base in EDGE_SCRIPT_LUMP_NAMES:
        return True
    if suffix in TEXT_EXTENSIONS:
        return True
    if normalized.lower().startswith(("actors/", "coal/", "ddf/", "decorate/", "edf/", "scripts/", "zscript/")):
        return True
    return False


def read_zip_archive(path: Path) -> tuple[list[ArchiveEntry], dict[str, bytes], list[str]]:
    entries: list[ArchiveEntry] = []
    texts: dict[str, bytes] = {}
    warnings: list[str] = []

    with zipfile.ZipFile(path) as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            name = info.filename.replace("\\", "/")
            entries.append(ArchiveEntry(name=name, size=info.file_size))
            if is_text_candidate(name):
                if info.file_size > MAX_TEXT_BYTES:
                    warnings.append(f"Skipped large text candidate {name} ({info.file_size} bytes).")
                    continue
                texts[name] = zf.read(info)

    return entries, texts, warnings


def read_wad_archive(path: Path) -> tuple[list[ArchiveEntry], dict[str, bytes], list[str], str]:
    warnings: list[str] = []
    entries: list[ArchiveEntry] = []
    texts: dict[str, bytes] = {}

    with path.open("rb") as fh:
        header = fh.read(12)
        if len(header) != 12:
            raise ValueError("File is too small to be a WAD.")
        magic, lump_count, directory_offset = struct.unpack("<4sii", header)
        magic_text = magic.decode("ascii", errors="replace")
        if magic_text not in ("IWAD", "PWAD"):
            raise ValueError(f"Unsupported WAD magic {magic_text!r}.")
        if lump_count < 0 or directory_offset < 0:
            raise ValueError("WAD directory header contains negative values.")

        fh.seek(directory_offset)
        directory = fh.read(lump_count * 16)
        if len(directory) != lump_count * 16:
            raise ValueError("WAD directory is truncated.")

        for i in range(lump_count):
            chunk = directory[i * 16 : (i + 1) * 16]
            file_pos, size, raw_name = struct.unpack("<ii8s", chunk)
            name = raw_name.split(b"\x00", 1)[0].decode("latin-1", errors="replace")
            entries.append(ArchiveEntry(name=name, size=max(size, 0)))
            if size <= 0 or not is_text_candidate(name):
                continue
            if size > MAX_TEXT_BYTES:
                warnings.append(f"Skipped large text candidate {name} ({size} bytes).")
                continue
            if file_pos < 0:
                warnings.append(f"Skipped text candidate {name}: negative file offset.")
                continue
            current = fh.tell()
            fh.seek(file_pos)
            texts[name] = fh.read(size)
            fh.seek(current)

    return entries, texts, warnings, magic_text


def read_archive(path: Path) -> tuple[str, list[ArchiveEntry], dict[str, bytes], list[str]]:
    suffix = path.suffix.lower()
    if suffix in (".pk3", ".zip"):
        entries, texts, warnings = read_zip_archive(path)
        return "zip", entries, texts, warnings
    if suffix == ".wad":
        entries, texts, warnings, magic = read_wad_archive(path)
        return magic, entries, texts, warnings
    if suffix == ".pk7":
        raise ValueError("PK7/7z archives are not supported by the standard-library scanner yet; unpack or repack as PK3 for analysis.")
    raise ValueError(f"Unsupported mod archive extension: {suffix}")


def add_surface(surfaces: dict[str, list[str]], key: str, name: str) -> None:
    bucket = surfaces.setdefault(key, [])
    if name not in bucket:
        bucket.append(name)


def classify_surfaces(entries: Iterable[ArchiveEntry]) -> dict[str, list[str]]:
    surfaces: dict[str, list[str]] = {}
    for entry in entries:
        name = entry.name.replace("\\", "/")
        lower = name.lower()
        base = Path(name).name.upper()
        suffix = Path(name).suffix.lower()

        if base == "DECORATE" or lower.startswith(("actors/", "decorate/")) or suffix == ".dec":
            add_surface(surfaces, "decorate", name)
        if base == "ZSCRIPT" or lower.startswith("zscript/") or suffix == ".zs":
            add_surface(surfaces, "zscript", name)
        if base in ("MAPINFO", "ZMAPINFO", "UMAPINFO", "EMAPINFO") or suffix == ".mapinfo":
            add_surface(surfaces, "mapinfo", name)
        if base == "EMAPINFO":
            add_surface(surfaces, "eternity_mapinfo", name)
        if base in ("EDF", "EDFROOT") or suffix == ".edf" or lower.startswith("edf/"):
            add_surface(surfaces, "eternity_edf", name)
        if base == "EXTRADATA" or lower.endswith("/extradata") or lower.endswith("/extradata.txt"):
            add_surface(surfaces, "eternity_extradata", name)
        if base in EDGE_DDF_LUMP_NAMES or suffix == ".ddf" or lower.startswith("ddf/"):
            add_surface(surfaces, "edge_ddf", name)
        if base in EDGE_SCRIPT_LUMP_NAMES or suffix in (".coal", ".lua", ".rts") or lower.startswith(("coal/", "scripts/")):
            add_surface(surfaces, "edge_scripts", name)
        if base == "DEHACKED" or suffix in (".deh", ".bex"):
            add_surface(surfaces, "dehacked", name)
        if base == "LOADACS" or suffix == ".acs" or lower.startswith("acs/"):
            add_surface(surfaces, "acs", name)
        if base in ("SNDINFO", "SNDSEQ"):
            add_surface(surfaces, "sound", name)
        if base in ("GLDEFS", "ANIMDEFS", "MODELDEF", "VOXELDEF", "DECALDEF"):
            add_surface(surfaces, "visual_defs", name)
        if re.fullmatch(r"MAP\d\d", base) or re.fullmatch(r"E\dM\d", base):
            add_surface(surfaces, "maps", name)

    return {key: sorted(value, key=str.lower) for key, value in sorted(surfaces.items())}


def find_line_hits(texts: dict[str, bytes], pattern: str, *, flags: int = re.IGNORECASE) -> list[TextHit]:
    rx = re.compile(pattern, flags)
    hits: list[TextHit] = []
    for name, data in sorted(texts.items(), key=lambda kv: kv[0].lower()):
        text = decode_text(data)
        for line_no, line in enumerate(text.splitlines(), start=1):
            if rx.search(line):
                hits.append(TextHit(entry=name, line=line_no, text=line.strip()[:240]))
    return hits


def archive_base_name(name: str) -> str:
    return Path(name.replace("\\", "/")).name.upper()


def strip_script_comment(line: str) -> str:
    in_quote = False
    quote_char = ""
    escape = False
    for index, char in enumerate(line):
        if escape:
            escape = False
            continue
        if in_quote:
            if char == "\\":
                escape = True
            elif char == quote_char:
                in_quote = False
            continue
        if char in ("'", '"'):
            in_quote = True
            quote_char = char
            continue
        if char == "#" or (char == "/" and index + 1 < len(line) and line[index + 1] == "/"):
            return line[:index]
    return line


def tokenize_scriptish(line: str) -> list[str]:
    tokens: list[str] = []
    current: list[str] = []
    in_quote = False
    quote_char = ""
    escape = False
    for char in line:
        if escape:
            current.append(char)
            escape = False
            continue
        if in_quote:
            if char == "\\":
                escape = True
            elif char == quote_char:
                in_quote = False
            else:
                current.append(char)
            continue
        if char in ("'", '"'):
            in_quote = True
            quote_char = char
            continue
        if char == ":":
            if current:
                tokens.append("".join(current))
                current = []
            tokens.append(":")
            continue
        if char.isspace() or char in "{}()[]=;,":
            if current:
                tokens.append("".join(current))
                current = []
            continue
        current.append(char)
    if current:
        tokens.append("".join(current))
    return tokens


def parse_int_token(value: str) -> int | None:
    try:
        return int(value, 0)
    except ValueError:
        return None


def first_int(tokens: list[str], start: int = 0) -> int | None:
    for token in tokens[start:]:
        parsed = parse_int_token(token)
        if parsed is not None:
            return parsed
    return None


def parse_include_hint(name: str, line_no: int, line: str) -> EternityIncludeHint | None:
    tokens = tokenize_scriptish(line)
    if len(tokens) < 2:
        return None
    kind = tokens[0].lower()
    if kind not in {"include", "stdinclude", "userinclude"}:
        return None
    return EternityIncludeHint(entry=name, line=line_no, kind=kind, target=tokens[1])


def parse_emapinfo_hints(name: str, text: str) -> tuple[list[EternityMapHint], list[TextHit]]:
    maps: list[EternityMapHint] = []
    extradata_refs: list[TextHit] = []
    current_map = ""

    for line_no, raw_line in enumerate(text.splitlines(), start=1):
        line = strip_script_comment(raw_line).strip()
        if not line:
            continue

        section = re.match(r"^\[([A-Za-z0-9_]+)\]", line)
        if section:
            current_map = section.group(1).upper()
            maps.append(EternityMapHint(entry=name, line=line_no, map_name=current_map))
            continue

        tokens = tokenize_scriptish(line)
        if len(tokens) >= 2 and tokens[0].lower() == "map":
            current_map = tokens[1].upper()
            maps.append(EternityMapHint(entry=name, line=line_no, map_name=current_map))
            continue

        if tokens and tokens[0].lower() == "extradata":
            value = tokens[1] if len(tokens) > 1 else ""
            detail = f"{current_map or '<global>'}: {value}" if value else current_map or "<global>"
            extradata_refs.append(TextHit(entry=name, line=line_no, text=detail))

    return maps, extradata_refs


def parse_edf_thing_header(name: str, line_no: int, line: str) -> EternityThingHint | None:
    tokens = tokenize_scriptish(line)
    if len(tokens) < 2:
        return None
    kind = tokens[0].lower()
    if kind not in {"thingtype", "thingdelta"}:
        return None

    hint = EternityThingHint(entry=name, line=line_no, kind=kind, name=tokens[1])
    for index, token in enumerate(tokens[2:], start=2):
        if token != ":":
            continue
        hint.doomednum = first_int(tokens, index + 2)
        if hint.doomednum is not None:
            for later in tokens[index + 2 :]:
                parsed = parse_int_token(later)
                if parsed is not None and parsed != hint.doomednum:
                    hint.dehackednum = parsed
                    break
        break
    return hint


def apply_edf_property(hint: EternityThingHint, line: str) -> None:
    tokens = tokenize_scriptish(line)
    if not tokens:
        return
    key = tokens[0].lower()
    if key == "doomednum":
        hint.doomednum = first_int(tokens, 1)
    elif key == "dehackednum":
        hint.dehackednum = first_int(tokens, 1)
    elif key == "spawnid":
        hint.spawnid = first_int(tokens, 1)
    elif key == "compatname" and len(tokens) > 1:
        hint.compatname = tokens[1]
    elif key in ETERNITY_EDF_BEHAVIOR_PROPERTIES and key not in hint.behavior_properties:
        hint.behavior_properties.append(key)


def update_brace_depth(line: str, depth: int) -> int:
    in_quote = False
    quote_char = ""
    escape = False
    for char in line:
        if escape:
            escape = False
            continue
        if in_quote:
            if char == "\\":
                escape = True
            elif char == quote_char:
                in_quote = False
            continue
        if char in ("'", '"'):
            in_quote = True
            quote_char = char
            continue
        if char == "{":
            depth += 1
        elif char == "}" and depth > 0:
            depth -= 1
    return depth


def parse_edf_hints(name: str, text: str) -> tuple[list[EternityIncludeHint], list[EternityThingHint]]:
    includes: list[EternityIncludeHint] = []
    things: list[EternityThingHint] = []
    current: EternityThingHint | None = None
    depth = 0

    for line_no, raw_line in enumerate(text.splitlines(), start=1):
        line = strip_script_comment(raw_line).strip()
        if not line:
            continue

        include = parse_include_hint(name, line_no, line)
        if include is not None:
            includes.append(include)
            continue

        if current is None:
            current = parse_edf_thing_header(name, line_no, line)
            if current is not None:
                after_header = line[line.find("{") + 1 :] if "{" in line else ""
                if after_header.strip():
                    apply_edf_property(current, after_header)
                depth = update_brace_depth(line, 0)
                if depth == 0 and "{" in line:
                    things.append(current)
                    current = None
                continue
        else:
            if depth <= 1:
                property_line = line.split("{", 1)[0].split("}", 1)[0].strip()
                if property_line:
                    apply_edf_property(current, property_line)
            depth = update_brace_depth(line, depth)
            if depth == 0:
                things.append(current)
                current = None

    if current is not None:
        things.append(current)
    return includes, things


def build_eternity_profile(surfaces: dict[str, list[str]], texts: dict[str, bytes]) -> EternityValidationProfile | None:
    eternity_keys = {"eternity_edf", "eternity_extradata", "eternity_mapinfo"}
    if not any(key in surfaces for key in eternity_keys):
        return None

    profile = EternityValidationProfile()
    profile.edf_entries = list(surfaces.get("eternity_edf", []))
    profile.extradata_entries = list(surfaces.get("eternity_extradata", []))
    profile.emapinfo_entries = list(surfaces.get("eternity_mapinfo", []))
    profile.native_mapinfo_entries = [
        name
        for name in surfaces.get("mapinfo", [])
        if archive_base_name(name) in {"MAPINFO", "ZMAPINFO", "UMAPINFO"} or Path(name).suffix.lower() == ".mapinfo"
    ]
    profile.edf_roots = [
        name
        for name in profile.edf_entries
        if archive_base_name(name) in {"EDF", "EDFROOT", "EDFROOT.EDF"}
    ]

    for name, data in sorted(texts.items(), key=lambda kv: kv[0].lower()):
        base = archive_base_name(name)
        suffix = Path(name).suffix.lower()
        lower = name.replace("\\", "/").lower()
        text = decode_text(data)

        if base == "EMAPINFO":
            maps, refs = parse_emapinfo_hints(name, text)
            profile.emapinfo_maps.extend(maps)
            profile.emapinfo_extradata_refs.extend(refs)

        if base in {"EDF", "EDFROOT"} or suffix == ".edf" or lower.startswith("edf/"):
            includes, things = parse_edf_hints(name, text)
            profile.edf_includes.extend(includes)
            profile.edf_things.extend(things)

    behavior_count = sum(1 for thing in profile.edf_things if thing.behavior_properties)
    profile.stage_flags = {
        "stage2_emapinfo": bool(profile.emapinfo_entries),
        "stage3_extradata_xlat": bool(profile.emapinfo_entries or profile.extradata_entries),
        "stage4_edf_manifest": bool(profile.edf_entries),
        "stage5_validation": True,
    }

    if profile.native_mapinfo_entries and profile.emapinfo_entries:
        profile.notes.append("Native MAPINFO/ZMAPINFO/UMAPINFO is present with EMAPINFO; HCDE will prefer native metadata from the same archive.")
    if profile.emapinfo_entries and not profile.emapinfo_maps:
        profile.notes.append("EMAPINFO was found but no map sections were recognized by the scanner.")
    if profile.emapinfo_extradata_refs and not profile.extradata_entries:
        profile.notes.append("EMAPINFO references ExtraData, but no obvious ExtraData lump/path was detected.")
    if profile.edf_entries and not profile.edf_roots:
        profile.notes.append("EDF resources were found without an EDF/EDFROOT entry point; HCDE will parse loose EDF files as diagnostics.")
    if behavior_count:
        profile.notes.append(f"{behavior_count} EDF thingtype entry/entries include behavior fields that need explicit compat patches.")
    if not profile.edf_things and profile.edf_entries:
        profile.notes.append("EDF resources were found, but no thingtype/thingdelta entries were recognized by the scanner.")

    return profile


def build_checks(
    surfaces: dict[str, list[str]],
    texts: dict[str, bytes],
    eternity: EternityValidationProfile | None = None,
) -> list[CompatibilityCheck]:
    checks: list[CompatibilityCheck] = []

    eternity_surfaces = [key for key in ("eternity_edf", "eternity_extradata", "eternity_mapinfo") if key in surfaces]
    if eternity_surfaces:
        checks.append(
            CompatibilityCheck(
                id="eternity-compat-surface",
                severity="manual",
                title="Eternity Engine compatibility surface detected",
                detail="HCDE now has staged EMAPINFO, ExtraData/XLAT, and EDF manifest support. Use the generated Eternity validation profile to separate supported startup behavior from manual actor-compat work.",
            )
        )
        if eternity and eternity.native_mapinfo_entries and eternity.emapinfo_entries:
            checks.append(
                CompatibilityCheck(
                    id="eternity-native-mapinfo-precedence",
                    severity="info",
                    title="Native MAPINFO can override EMAPINFO",
                    detail="HCDE skips EMAPINFO from an archive that also ships native MAPINFO/ZMAPINFO/UMAPINFO. Validate that this is intended before debugging missing EMAPINFO behavior.",
                    hits=[TextHit(entry=name, line=1, text="native MAPINFO-style metadata") for name in eternity.native_mapinfo_entries[:20]],
                )
            )
        if eternity and eternity.edf_things:
            thing_hits = [
                TextHit(
                    entry=thing.entry,
                    line=thing.line,
                    text=f"{thing.kind} {thing.name} doomednum={thing.doomednum if thing.doomednum is not None else '<none>'}",
                )
                for thing in eternity.edf_things[:40]
            ]
            checks.append(
                CompatibilityCheck(
                    id="eternity-edf-thingtypes",
                    severity="candidate",
                    title="EDF thing definitions detected",
                    detail="HCDE can bind DoomEdNums only to matching HCDE/ZScript actors without conflicts. EDF-only actors need an original compat patch, not an automatic import.",
                    hits=thing_hits,
                )
            )
            behavior_hits = [
                TextHit(
                    entry=thing.entry,
                    line=thing.line,
                    text=f"{thing.name}: {', '.join(thing.behavior_properties[:10])}",
                )
                for thing in eternity.edf_things
                if thing.behavior_properties
            ]
            if behavior_hits:
                checks.append(
                    CompatibilityCheck(
                        id="eternity-edf-behavior-patches",
                        severity="candidate",
                        title="EDF actor behavior needs compat patches",
                        detail="Stage 4 reports behavior fields but does not translate them into actors. Use these entries to write narrow ZScript/DECORATE shims in the compat mod.",
                        hits=behavior_hits[:40],
                    )
                )
        if eternity and eternity.emapinfo_extradata_refs:
            checks.append(
                CompatibilityCheck(
                    id="eternity-extradata-references",
                    severity="info",
                    title="EMAPINFO references ExtraData",
                    detail="Stage 3 resolves ExtraData beside the EMAPINFO source first, then globally. Validate startup logs for the resolved lump and record counts.",
                    hits=eternity.emapinfo_extradata_refs[:40],
                )
            )

    edge_surfaces = [key for key in ("edge_ddf", "edge_scripts") if key in surfaces]
    if edge_surfaces:
        checks.append(
            CompatibilityCheck(
                id="edge-classic-compat-surface",
                severity="manual",
                title="EDGE Classic compatibility surface detected",
                detail="DDF, RTS, COAL, and Lua style resources need a future EDGE Classic compat layer. The candidate should document required behavior; do not copy DDF/script content into HCDE without license review.",
            )
        )

    if "zscript" in surfaces:
        checks.append(
            CompatibilityCheck(
                id="zscript-present",
                severity="manual",
                title="ZScript is present",
                detail="ZScript compatibility usually needs engine-level review or a carefully scoped override. Generate notes only; do not copy source into HCDE unless the mod license allows it.",
            )
        )

    rail_hits = find_line_hits(texts, r"\bA_RailAttack\s*\(")
    if rail_hits:
        checks.append(
            CompatibilityCheck(
                id="decorate-railattack",
                severity="candidate",
                title="DECORATE uses A_RailAttack",
                detail="Dedicated-server rail paths have needed HCDE compatibility before. Review whether this mod needs a server-safe projectile fallback in the compat mod.",
                hits=rail_hits,
            )
        )

    replace_hits = find_line_hits(texts, r"\b(actor|class)\b.*\breplaces\b")
    if replace_hits:
        checks.append(
            CompatibilityCheck(
                id="actor-replacements",
                severity="info",
                title="Actor replacements detected",
                detail="Compat patches that replace the same actors may conflict with the mod. Review load order and use narrow patterns.",
                hits=replace_hits[:40],
            )
        )

    input_hits = find_line_hits(texts, r"\b(GetPlayerInput|PlayerNumber|ConsolePlayerNumber|Button_|BT_)\b")
    if input_hits:
        checks.append(
            CompatibilityCheck(
                id="player-input-assumptions",
                severity="candidate",
                title="Player input / player index assumptions detected",
                detail="Dedicated servers may expose player-0 assumptions. Review whether this mod needs an HCDE engine-side compat flag or ACS/ZScript-facing behavior adjustment.",
                hits=input_hits[:40],
            )
        )

    if "dehacked" in surfaces:
        checks.append(
            CompatibilityCheck(
                id="dehacked-present",
                severity="info",
                title="DeHackEd/DEHEXTRA style data is present",
                detail="Check whether the mod expects vanilla, Boom, MBF, MBF21, or DEHEXTRA behavior and document the intended compatibility profile.",
            )
        )

    if "mapinfo" in surfaces and "maps" in surfaces:
        checks.append(
            CompatibilityCheck(
                id="mapinfo-with-maps",
                severity="info",
                title="MAPINFO-style metadata with map lumps",
                detail="Review map names, next/secret exits, cluster definitions, and compatibility flags before adding HCDE-specific overrides.",
            )
        )

    return checks


def scan_mod(path: Path, slug_override: str | None = None) -> ScanResult:
    archive_type, entries, texts, warnings = read_archive(path)
    surfaces = classify_surfaces(entries)
    eternity = build_eternity_profile(surfaces, texts)
    checks = build_checks(surfaces, texts, eternity)
    return ScanResult(
        source_path=str(path.resolve()),
        archive_type=archive_type,
        sha256=sha256_file(path),
        slug=slug_override or make_slug(path),
        entries=entries,
        surfaces=surfaces,
        checks=checks,
        warnings=warnings,
        eternity=eternity,
    )


def render_report(result: ScanResult) -> str:
    lines: list[str] = []
    lines.append(f"# HCDE Compat Candidate: {result.slug}")
    lines.append("")
    lines.append("This is a generated review report. It does not import third-party mod code or assets.")
    lines.append("")
    lines.append("## Source")
    lines.append("")
    lines.append(f"- Path: `{result.source_path}`")
    lines.append(f"- Archive type: `{result.archive_type}`")
    lines.append(f"- SHA-256: `{result.sha256}`")
    lines.append(f"- Entries: `{len(result.entries)}`")
    lines.append("")

    lines.append("## Detected Surfaces")
    lines.append("")
    if result.surfaces:
        for key, names in result.surfaces.items():
            preview = ", ".join(f"`{name}`" for name in names[:12])
            if len(names) > 12:
                preview += f", ... ({len(names)} total)"
            lines.append(f"- {key}: {preview}")
    else:
        lines.append("- No known compatibility surfaces detected.")
    lines.append("")

    if result.eternity is not None:
        lines.extend(render_eternity_profile_section(result.eternity))

    lines.append("## Candidate Checks")
    lines.append("")
    if result.checks:
        for check in result.checks:
            lines.append(f"### {check.title}")
            lines.append("")
            lines.append(f"- Id: `{check.id}`")
            lines.append(f"- Severity: `{check.severity}`")
            lines.append(f"- Detail: {check.detail}")
            if check.hits:
                lines.append("- Evidence:")
                for hit in check.hits[:20]:
                    safe = hit.text.replace("`", "'")
                    lines.append(f"  - `{hit.entry}:{hit.line}`: `{safe}`")
                if len(check.hits) > 20:
                    lines.append(f"  - ... {len(check.hits) - 20} more hits")
            lines.append("")
    else:
        lines.append("- No rule-based candidate checks fired.")
        lines.append("")

    if result.warnings:
        lines.append("## Scanner Warnings")
        lines.append("")
        for warning in result.warnings:
            lines.append(f"- {warning}")
        lines.append("")

    lines.append("## Review Steps")
    lines.append("")
    lines.append("1. Confirm the mod license before copying any source or assets.")
    lines.append("2. Use the generated stubs as TODOs; write original HCDE compatibility code by hand.")
    lines.append("3. Add a narrow matcher in `src/common/engine/hcde_mod_compat.cpp` after review.")
    lines.append("4. Prefer a split compat PK3 if the patch references classes from one specific mod.")
    lines.append("")
    return "\n".join(lines)


def render_eternity_profile_section(profile: EternityValidationProfile) -> list[str]:
    lines: list[str] = []
    lines.append("## Eternity Validation Profile")
    lines.append("")
    lines.append("Stage coverage:")
    for key, enabled in profile.stage_flags.items():
        lines.append(f"- {key}: `{'yes' if enabled else 'no'}`")
    lines.append("")

    if profile.emapinfo_maps:
        preview = ", ".join(f"`{hint.map_name}`" for hint in profile.emapinfo_maps[:20])
        if len(profile.emapinfo_maps) > 20:
            preview += f", ... ({len(profile.emapinfo_maps)} total)"
        lines.append(f"- EMAPINFO maps: {preview}")
    if profile.emapinfo_extradata_refs:
        preview = ", ".join(f"`{hit.entry}:{hit.line}`" for hit in profile.emapinfo_extradata_refs[:20])
        if len(profile.emapinfo_extradata_refs) > 20:
            preview += f", ... ({len(profile.emapinfo_extradata_refs)} total)"
        lines.append(f"- ExtraData references: {preview}")
    if profile.edf_includes:
        include_preview = ", ".join(f"`{hint.kind} {hint.target}`" for hint in profile.edf_includes[:20])
        if len(profile.edf_includes) > 20:
            include_preview += f", ... ({len(profile.edf_includes)} total)"
        lines.append(f"- EDF includes: {include_preview}")
    if profile.edf_things:
        with_numbers = sum(1 for thing in profile.edf_things if thing.doomednum is not None)
        behavior = sum(1 for thing in profile.edf_things if thing.behavior_properties)
        lines.append(f"- EDF thing definitions: `{len(profile.edf_things)}` total, `{with_numbers}` with DoomEdNums, `{behavior}` with behavior fields")
    if profile.notes:
        lines.append("")
        lines.append("Notes:")
        for note in profile.notes:
            lines.append(f"- {note}")
    lines.append("")
    return lines


def render_eternity_validation(result: ScanResult, out_dir: Path | None = None) -> str:
    profile = result.eternity
    if profile is None:
        return "# HCDE Eternity Validation\n\nNo Eternity compatibility surfaces were detected.\n"

    log_root = str(out_dir) if out_dir is not None else f"build\\compat-candidates\\{result.slug}"
    lines: list[str] = []
    lines.append(f"# HCDE Eternity Validation: {result.slug}")
    lines.append("")
    lines.append("This file is a real-mod validation checklist. It contains only scanner metadata and suggested commands; it does not copy mod code or assets.")
    lines.append("")
    lines.append("## Runtime Commands")
    lines.append("")
    lines.append("Use your local IWAD path in place of `C:\\Path\\DOOM2.WAD` when needed.")
    lines.append("")
    lines.append("```powershell")
    lines.append(f"build\\RelWithDebInfo\\hcde.exe -norun -errorlog \"{log_root}\\eternity-client-startup.log\" -iwad C:\\Path\\DOOM2.WAD -file \"{result.source_path}\"")
    lines.append(f"build\\RelWithDebInfo\\hcdeserv.exe -norun -errorlog \"{log_root}\\eternity-server-startup.log\" -iwad C:\\Path\\DOOM2.WAD -file \"{result.source_path}\" -nosound -nomusic")
    if profile.emapinfo_maps:
        first_map = profile.emapinfo_maps[0].map_name
        lines.append(f"build\\RelWithDebInfo\\hcdeserv.exe -errorlog \"{log_root}\\eternity-server-{first_map}.log\" -iwad C:\\Path\\DOOM2.WAD +map {first_map} -server 1 -file \"{result.source_path}\" -nosound -nomusic")
    lines.append("```")
    lines.append("")
    lines.append("## Expected HCDE Logs")
    lines.append("")
    lines.append("- `HCDE: Eternity compatibility resources detected:`")
    if profile.emapinfo_entries:
        lines.append("- `HCDE: parsed EMAPINFO ...`")
    if profile.emapinfo_extradata_refs or profile.extradata_entries:
        lines.append("- `HCDE: loaded ExtraData ...` when a map using ExtraData loads")
    if profile.edf_entries:
        lines.append("- `HCDE: EDF compatibility parsed ...`")
        lines.append("- `HCDE: EDF actor binding candidates=...`")
    lines.append("")
    lines.append("## Triage")
    lines.append("")
    if profile.native_mapinfo_entries and profile.emapinfo_entries:
        lines.append("- Native MAPINFO/ZMAPINFO/UMAPINFO is present with EMAPINFO. HCDE should log that native metadata wins for the same archive.")
    if profile.emapinfo_maps:
        maps = ", ".join(sorted({hint.map_name for hint in profile.emapinfo_maps})[:40])
        lines.append(f"- Try these EMAPINFO maps first: `{maps}`.")
    if profile.emapinfo_extradata_refs:
        refs = ", ".join(f"{hit.entry}:{hit.line}" for hit in profile.emapinfo_extradata_refs[:20])
        lines.append(f"- Confirm ExtraData resolution for: `{refs}`.")
    if profile.edf_things:
        with_numbers = [thing for thing in profile.edf_things if thing.doomednum is not None]
        if with_numbers:
            preview = ", ".join(f"{thing.name}={thing.doomednum}" for thing in with_numbers[:30])
            lines.append(f"- Watch EDF DoomEdNum binding for: `{preview}`.")
        missing_behavior = [thing for thing in profile.edf_things if thing.behavior_properties]
        if missing_behavior:
            preview = ", ".join(thing.name for thing in missing_behavior[:30])
            lines.append(f"- These EDF things need manual behavior patches if maps rely on them: `{preview}`.")
    if profile.notes:
        for note in profile.notes:
            lines.append(f"- {note}")
    lines.append("")
    lines.append("## EDF Thing Hints")
    lines.append("")
    if profile.edf_things:
        lines.append("| Entry | Line | Kind | Name | DoomEdNum | CompatName | Behavior Fields |")
        lines.append("| --- | ---: | --- | --- | ---: | --- | --- |")
        for thing in profile.edf_things[:80]:
            behavior = ", ".join(thing.behavior_properties[:12])
            lines.append(
                f"| `{thing.entry}` | {thing.line} | `{thing.kind}` | `{thing.name}` | "
                f"{thing.doomednum if thing.doomednum is not None else ''} | "
                f"`{thing.compatname or ''}` | `{behavior}` |"
            )
        if len(profile.edf_things) > 80:
            lines.append(f"| ... |  |  | {len(profile.edf_things) - 80} more entries |  |  |  |")
    else:
        lines.append("No EDF thingtype or thingdelta entries were recognized by the scanner.")
    lines.append("")
    return "\n".join(lines)


def render_engine_entry(result: ScanResult) -> str:
    pascal = "".join(part.capitalize() for part in result.slug.split("_") if part)
    if not pascal:
        pascal = "Mod"
    original_name = Path(result.source_path).name
    return textwrap.dedent(
        f"""\
        // Candidate only. Review before moving into src/common/engine/hcde_mod_compat.cpp.
        // Source archive: {original_name}
        // SHA-256: {result.sha256}
        static const char* const {pascal}Patterns[] =
        {{
        \t"{Path(original_name).stem}*",
        \tnullptr
        }};

        {{
        \t"{pascal} compatibility",
        \t"hcde_mod_compat_{result.slug}.pk3",
        \t{pascal}Patterns,
        \t0u
        }},
        """
    )


def render_decorate_stub(result: ScanResult) -> str:
    checks = ", ".join(check.id for check in result.checks) or "none"
    return textwrap.dedent(
        f"""\
        // HCDE mod compatibility candidate for {result.slug}.
        // Generated from local archive metadata only.
        // Source SHA-256: {result.sha256}
        // Candidate checks: {checks}
        //
        // Golden-rule reminder:
        // - Do not copy third-party mod code or assets here unless its license allows it.
        // - Write narrow HCDE-owned compatibility shims by hand.
        // - Keep mod-specific shims in a split PK3 when they depend on mod-defined classes.

        // TODO: Add reviewed DECORATE compatibility shims here.
        """
    )


def render_candidate_readme(result: ScanResult) -> str:
    return textwrap.dedent(
        f"""\
        # {result.slug} Compat Candidate

        Generated by `tools/hcde_compat_patcher/hcde_compat_patcher.py`.

        Files in this directory are review aids for HCDE's compat mod. They are
        not active until a maintainer moves reviewed code into `wadsrc_mod_compat`
        and wires a matcher in `src/common/engine/hcde_mod_compat.cpp`.

        Source archive: `{Path(result.source_path).name}`
        SHA-256: `{result.sha256}`

        Read `report.md` first. If this candidate detected Eternity resources,
        read `eternity_validation.md` next and compare its expected log lines
        against a real HCDE startup log.
        """
    )


def write_candidate(result: ScanResult, out_root: Path) -> Path:
    out_dir = out_root / result.slug
    static_dir = out_dir / "static"
    static_dir.mkdir(parents=True, exist_ok=True)

    (out_dir / "README.md").write_text(render_candidate_readme(result), encoding="utf-8", newline="\n")
    (out_dir / "report.md").write_text(render_report(result), encoding="utf-8", newline="\n")
    (out_dir / "metadata.json").write_text(
        json.dumps(asdict(result), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    if result.eternity is not None:
        (out_dir / "eternity_validation.md").write_text(
            render_eternity_validation(result, out_dir),
            encoding="utf-8",
            newline="\n",
        )
    (out_dir / "suggested_hcde_mod_compat_entry.cpp.txt").write_text(
        render_engine_entry(result),
        encoding="utf-8",
        newline="\n",
    )
    (static_dir / "decorate.txt").write_text(render_decorate_stub(result), encoding="utf-8", newline="\n")
    return out_dir


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Scan local Doom mods and generate reviewable HCDE compat-mod patch candidates."
    )
    parser.add_argument("mods", nargs="+", type=Path, help="Mod archives to scan (.wad, .pk3, .zip).")
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("build") / "compat-candidates",
        help="Output directory for generated candidate folders.",
    )
    parser.add_argument("--slug", help="Override output slug. Only valid with one input mod.")
    parser.add_argument("--json", action="store_true", help="Print scan metadata JSON to stdout.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.slug and len(args.mods) != 1:
        print("--slug may only be used with one mod archive.", file=sys.stderr)
        return 2

    outputs: list[Path] = []
    results: list[ScanResult] = []
    for mod in args.mods:
        try:
            result = scan_mod(mod, slug_override=args.slug)
            outputs.append(write_candidate(result, args.out))
            results.append(result)
        except (OSError, ValueError, zipfile.BadZipFile, struct.error) as exc:
            print(f"{mod}: {exc}", file=sys.stderr)
            return 1

    if args.json:
        print(json.dumps([asdict(result) for result in results], indent=2, sort_keys=True))

    for output in outputs:
        print(f"Wrote HCDE compat candidate: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
