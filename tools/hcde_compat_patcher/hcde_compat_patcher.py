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
    "SKININFO",
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

EDGE_DDF_KIND_BY_NAME = {
    "DDFANIM": "animations",
    "DDFATK": "attacks",
    "DDFCOLM": "colormaps",
    "DDFCOLOR": "colors",
    "DDFFONT": "fonts",
    "DDFGAME": "game",
    "DDFIMAGE": "images",
    "DDFLANG": "language",
    "DDFLEVL": "levels",
    "DDFLINE": "lines",
    "DDFPLAY": "players",
    "DDFSFX": "sounds",
    "DDFSECT": "sectors",
    "DDFSTYLE": "styles",
    "DDFTHING": "things",
    "DDFWEAP": "weapons",
}

EDGE_DDF_KIND_BY_STEM = {
    "anim": "animations",
    "anims": "animations",
    "animations": "animations",
    "attack": "attacks",
    "attacks": "attacks",
    "colmap": "colormaps",
    "colm": "colormaps",
    "colormap": "colormaps",
    "colormaps": "colormaps",
    "color": "colors",
    "colors": "colors",
    "font": "fonts",
    "fonts": "fonts",
    "game": "game",
    "games": "game",
    "image": "images",
    "images": "images",
    "lang": "language",
    "language": "language",
    "level": "levels",
    "levels": "levels",
    "line": "lines",
    "lines": "lines",
    "playlist": "playlist",
    "playlists": "playlist",
    "player": "players",
    "players": "players",
    "sector": "sectors",
    "sectors": "sectors",
    "sound": "sounds",
    "sounds": "sounds",
    "sfx": "sounds",
    "style": "styles",
    "styles": "styles",
    "switch": "switches",
    "switches": "switches",
    "thing": "things",
    "things": "things",
    "weapon": "weapons",
    "weapons": "weapons",
}

EDGE_STAGE3_SUPPORTED_PROPERTIES = {
    "animations": {"type", "first", "last", "speed", "sequence"},
    "attacks": {
        "accuracy_angle",
        "accuracy_slope",
        "attack_height",
        "attackrange",
        "attacktype",
        "damage.max",
        "damage.val",
        "height",
        "radius",
        "shotcount",
        "speed",
    },
    "colormaps": {"gl_colour", "length", "lump", "special", "start"},
    "fonts": {"image", "missing", "type"},
    "game": {"def_player", "firstmap", "titlepic", "titlemusic"},
    "images": {"image_data", "scale", "special"},
    "levels": {"description", "episode", "levelname", "music", "nextmap", "sky_texture", "type"},
    "lines": {"count", "newtrignum", "tagged", "time", "type"},
    "playlist": {"title"},
    "sectors": {"ambient_light", "damage", "secret", "type"},
    "sounds": {"loop", "lump", "lump_name", "priority", "singular"},
    "styles": {"background", "cursor", "font", "text", "title"},
    "switches": {"off", "on", "time"},
    "things": {"doomednum", "height", "health", "mass", "painchance", "radius", "reactiontime", "speed"},
    "weapons": {"ammopershot", "ammotype", "clip_size", "priority", "selection_order", "slot"},
}

# EDGE commonly has several STATES(...) groups per entry; those remain manual
# adapter data, but are not duplicate scalar-property warnings.
EDGE_REPEATABLE_PROPERTY_KEYS = {"states"}

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

ASSET_NAMESPACE_PREFIXES = (
    "flats/",
    "graphics/",
    "hires/",
    "patches/",
    "sprites/",
    "textures/",
)

SOUND_NAMESPACE_PREFIXES = (
    "sounds/",
)

NON_ASSET_SUFFIXES = {
    ".acs",
    ".bcs",
    ".bex",
    ".cfg",
    ".deh",
    ".ini",
    ".txt",
    ".wad",
}

NON_ASSET_BASE_NAMES = {
    "DESKTOP.INI",
    "F_END",
    "F_START",
    "P_END",
    "P_START",
    "PNAMES",
    "PP_END",
    "PP_START",
    "TEXTURE1",
    "TEXTURE2",
    "THUMBS.DB",
}

MAX_TEXT_BYTES = 1_048_576
MAX_EMBEDDED_WAD_BYTES = 64 * 1024 * 1024

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
class EdgeDdfIncludeHint:
    entry: str
    line: int
    target: str
    resolved_entry: str | None = None


@dataclass
class EdgeDdfPropertyHint:
    entry: str
    line: int
    key: str
    value: str


@dataclass
class EdgeDdfSectionHint:
    entry: str
    line: int
    kind: str
    name: str
    parent: str | None = None
    property_keys: list[str] = field(default_factory=list)
    properties: list[EdgeDdfPropertyHint] = field(default_factory=list)


@dataclass
class EdgeScriptHint:
    entry: str
    kind: str
    size: int


@dataclass
class EdgeDdfConflictHint:
    kind: str
    name: str
    definitions: list[TextHit] = field(default_factory=list)


@dataclass
class EdgeDdfManifestEntry:
    kind: str
    label: str
    entry: str
    line: int
    status: str
    supported_properties: dict[str, str] = field(default_factory=dict)
    manual_properties: list[str] = field(default_factory=list)
    duplicate_properties: list[str] = field(default_factory=list)
    property_count: int = 0


@dataclass
class EdgeTranslationSummary:
    animdefs_entries: int = 0
    sndinfo_entries: int = 0
    skipped_entries: int = 0


@dataclass
class EdgeClassicProfile:
    ddf_entries: list[str] = field(default_factory=list)
    ddf_entry_kinds: dict[str, str] = field(default_factory=dict)
    ddf_load_order: list[str] = field(default_factory=list)
    ddf_sections: list[EdgeDdfSectionHint] = field(default_factory=list)
    ddf_includes: list[EdgeDdfIncludeHint] = field(default_factory=list)
    ddf_missing_includes: list[EdgeDdfIncludeHint] = field(default_factory=list)
    ddf_include_cycles: list[list[str]] = field(default_factory=list)
    ddf_clearall: list[TextHit] = field(default_factory=list)
    ddf_versions: list[TextHit] = field(default_factory=list)
    ddf_conflicts: list[EdgeDdfConflictHint] = field(default_factory=list)
    ddf_manifest_entries: list[EdgeDdfManifestEntry] = field(default_factory=list)
    ddf_manifest_counts: dict[str, int] = field(default_factory=dict)
    ddf_manual_property_counts: dict[str, int] = field(default_factory=dict)
    ddf_duplicate_property_counts: dict[str, int] = field(default_factory=dict)
    translation_summary: EdgeTranslationSummary = field(default_factory=EdgeTranslationSummary)
    script_entries: list[EdgeScriptHint] = field(default_factory=list)
    kind_counts: dict[str, int] = field(default_factory=dict)
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
    edge: EdgeClassicProfile | None = None


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_path(path: Path) -> str:
    if path.is_file():
        return sha256_file(path)

    digest = hashlib.sha256()
    digest.update(b"directory\n")
    for item in sorted(path.rglob("*"), key=lambda candidate: str(candidate.relative_to(path)).lower()):
        if not item.is_file():
            continue
        rel = item.relative_to(path).as_posix()
        digest.update(rel.encode("utf-8", errors="surrogateescape"))
        digest.update(b"\0")
        try:
            with item.open("rb") as fh:
                for chunk in iter(lambda: fh.read(1024 * 1024), b""):
                    digest.update(chunk)
        except OSError as exc:
            digest.update(f"<unreadable:{exc}>".encode("utf-8", errors="replace"))
        digest.update(b"\0")
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


def decode_text_with_encoding(data: bytes) -> tuple[str, str]:
    for encoding in ("utf-8-sig", "utf-8", "cp1252", "latin-1"):
        try:
            return data.decode(encoding), encoding
        except UnicodeDecodeError:
            continue
    return data.decode("latin-1", errors="replace"), "latin-1"


def is_text_candidate(name: str) -> bool:
    normalized = name.replace("\\", "/")
    base = archive_base_name(normalized)
    suffix = archive_suffix(normalized)
    if base in TEXT_LUMP_NAMES or base.startswith(("SNDINFO.", "SNDSEQ.")):
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
            if name.lower().endswith(".wad"):
                if info.file_size > MAX_EMBEDDED_WAD_BYTES:
                    warnings.append(f"Skipped large embedded WAD candidate {name} ({info.file_size} bytes).")
                    continue
                try:
                    wad_entries, wad_texts, wad_warnings, _ = read_wad_bytes(zf.read(info), name)
                except (OSError, RuntimeError, zipfile.BadZipFile, ValueError) as exc:
                    warnings.append(f"Skipped embedded WAD candidate {name}: {exc}.")
                else:
                    entries.extend(wad_entries)
                    texts.update(wad_texts)
                    warnings.extend(f"{name}: {warning}" for warning in wad_warnings)
            if is_text_candidate(name):
                if info.file_size > MAX_TEXT_BYTES:
                    warnings.append(f"Skipped large text candidate {name} ({info.file_size} bytes).")
                    continue
                texts[name] = zf.read(info)

    return entries, texts, warnings


def read_wad_bytes(data: bytes, label: str = "") -> tuple[list[ArchiveEntry], dict[str, bytes], list[str], str]:
    warnings: list[str] = []
    entries: list[ArchiveEntry] = []
    texts: dict[str, bytes] = {}

    if len(data) < 12:
        raise ValueError("File is too small to be a WAD.")

    magic, lump_count, directory_offset = struct.unpack("<4sii", data[:12])
    magic_text = magic.decode("ascii", errors="replace")
    if magic_text not in ("IWAD", "PWAD"):
        raise ValueError(f"Unsupported WAD magic {magic_text!r}.")
    if lump_count < 0 or directory_offset < 0:
        raise ValueError("WAD directory header contains negative values.")

    directory_size = lump_count * 16
    directory_end = directory_offset + directory_size
    if directory_end > len(data):
        raise ValueError("WAD directory is truncated.")

    directory = data[directory_offset:directory_end]
    for i in range(lump_count):
        chunk = directory[i * 16 : (i + 1) * 16]
        file_pos, size, raw_name = struct.unpack("<ii8s", chunk)
        lump_name = raw_name.split(b"\x00", 1)[0].decode("latin-1", errors="replace")
        name = f"{label}:{lump_name}" if label else lump_name
        entries.append(ArchiveEntry(name=name, size=max(size, 0)))
        if size <= 0 or not is_text_candidate(name):
            continue
        if size > MAX_TEXT_BYTES:
            warnings.append(f"Skipped large text candidate {name} ({size} bytes).")
            continue
        if file_pos < 0:
            warnings.append(f"Skipped text candidate {name}: negative file offset.")
            continue
        if file_pos + size > len(data):
            warnings.append(f"Skipped text candidate {name}: data extends past end of WAD.")
            continue
        texts[name] = data[file_pos : file_pos + size]

    return entries, texts, warnings, magic_text


def read_wad_archive(path: Path) -> tuple[list[ArchiveEntry], dict[str, bytes], list[str], str]:
    return read_wad_bytes(path.read_bytes())


def read_directory_archive(path: Path) -> tuple[list[ArchiveEntry], dict[str, bytes], list[str]]:
    entries: list[ArchiveEntry] = []
    texts: dict[str, bytes] = {}
    warnings: list[str] = []

    for item in sorted(path.rglob("*"), key=lambda candidate: str(candidate.relative_to(path)).lower()):
        if not item.is_file():
            continue
        name = item.relative_to(path).as_posix()
        try:
            size = item.stat().st_size
        except OSError as exc:
            warnings.append(f"Skipped {name}: {exc}.")
            continue
        entries.append(ArchiveEntry(name=name, size=size))

        if item.suffix.lower() == ".wad":
            if size > MAX_EMBEDDED_WAD_BYTES:
                warnings.append(f"Skipped large embedded WAD candidate {name} ({size} bytes).")
                continue
            try:
                wad_entries, wad_texts, wad_warnings, _ = read_wad_bytes(item.read_bytes(), name)
            except (OSError, ValueError) as exc:
                warnings.append(f"Skipped embedded WAD candidate {name}: {exc}.")
            else:
                entries.extend(wad_entries)
                texts.update(wad_texts)
                warnings.extend(f"{name}: {warning}" for warning in wad_warnings)

        if not is_text_candidate(name):
            continue
        if size > MAX_TEXT_BYTES:
            warnings.append(f"Skipped large text candidate {name} ({size} bytes).")
            continue
        try:
            texts[name] = item.read_bytes()
        except OSError as exc:
            warnings.append(f"Skipped text candidate {name}: {exc}.")

    return entries, texts, warnings


def read_archive(path: Path) -> tuple[str, list[ArchiveEntry], dict[str, bytes], list[str]]:
    if path.is_dir():
        entries, texts, warnings = read_directory_archive(path)
        return "directory", entries, texts, warnings

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
        base = archive_base_name(name)
        suffix = archive_suffix(name)

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
        if base in EDGE_SCRIPT_LUMP_NAMES or suffix in (".coal", ".lua", ".rts") or lower.startswith("coal/"):
            add_surface(surfaces, "edge_scripts", name)
        if base == "DEHACKED" or suffix in (".deh", ".bex"):
            add_surface(surfaces, "dehacked", name)
        if base == "LOADACS" or suffix == ".acs" or lower.startswith("acs/"):
            add_surface(surfaces, "acs", name)
        if base in ("SNDINFO", "SNDSEQ") or base.startswith(("SNDINFO.", "SNDSEQ.")):
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


def find_asset_namespace_conflicts(entries: Iterable[ArchiveEntry]) -> list[TextHit]:
    """Find files that HCDE may try to decode through the wrong asset loader."""
    hits: list[TextHit] = []
    for entry in sorted(entries, key=lambda candidate: candidate.name.lower()):
        normalized = entry.name.replace("\\", "/")
        lower = normalized.lower()
        base = archive_base_name(normalized)
        suffix = archive_suffix(normalized)

        if lower.startswith(ASSET_NAMESPACE_PREFIXES):
            if base in NON_ASSET_BASE_NAMES:
                hits.append(TextHit(entry=entry.name, line=1, text=f"{base} marker/system lump is inside an image namespace"))
            elif suffix in NON_ASSET_SUFFIXES:
                hits.append(TextHit(entry=entry.name, line=1, text=f"{suffix or '<no extension>'} file is inside an image namespace"))
        elif lower.startswith(SOUND_NAMESPACE_PREFIXES):
            if base in NON_ASSET_BASE_NAMES:
                hits.append(TextHit(entry=entry.name, line=1, text=f"{base} marker/system lump is inside the sound namespace"))
            elif suffix in NON_ASSET_SUFFIXES:
                hits.append(TextHit(entry=entry.name, line=1, text=f"{suffix or '<no extension>'} file is inside the sound namespace"))

    return hits


def archive_stem_name(name: str) -> str:
    normalized = name.replace("\\", "/")
    if ":" in normalized:
        normalized = normalized.rsplit(":", 1)[1]
    return Path(normalized).stem.upper()


def find_skin_sprite_prefix_conflicts(entries: Iterable[ArchiveEntry], texts: dict[str, bytes]) -> list[TextHit]:
    """Find non-sprite files whose basenames can be mistaken for SKININFO frames."""
    sprite_prefixes: set[str] = set()
    assignment_rx = re.compile(r'^\s*(?:sprite|crouchsprite)\s*=\s*"?([A-Za-z0-9_\\\-\[\]]+)"?', re.IGNORECASE | re.MULTILINE)

    for name, data in texts.items():
        if archive_stem_name(name) != "SKININFO":
            continue
        for match in assignment_rx.finditer(decode_text(data)):
            sprite = match.group(1).strip().upper()
            if len(sprite) >= 4:
                sprite_prefixes.add(sprite[:4])

    if not sprite_prefixes:
        return []

    hits: list[TextHit] = []
    for entry in sorted(entries, key=lambda candidate: candidate.name.lower()):
        normalized = entry.name.replace("\\", "/")
        lower = normalized.lower()
        stem = archive_stem_name(normalized)
        if len(stem) < 6 or stem[:4] not in sprite_prefixes:
            continue
        if lower.startswith(ASSET_NAMESPACE_PREFIXES):
            continue

        hits.append(
            TextHit(
                entry=entry.name,
                line=1,
                text=f"{stem} starts with SKININFO sprite prefix {stem[:4]} and is outside a sprite/image namespace",
            )
        )

    return hits


def find_mapinfo_unterminated_text_assignments(texts: dict[str, bytes]) -> list[TextHit]:
    """Find old MAPINFO text lists that HCDE rejects when they trail off."""
    hits: list[TextHit] = []
    text_keys = "entertext|exittext|exittextsecret|intertext|intertextsecret"
    assign_rx = re.compile(rf"^\s*({text_keys})\s*=", re.IGNORECASE)
    block_start_rx = re.compile(r"^\s*(clusterdef|episode|map)\b", re.IGNORECASE)

    for name, data in sorted(texts.items(), key=lambda kv: kv[0].lower()):
        base = archive_base_name(name)
        suffix = archive_suffix(name)
        if base not in ("MAPINFO", "ZMAPINFO", "UMAPINFO") and suffix != ".mapinfo":
            continue

        lines = decode_text(data).splitlines()
        index = 0
        while index < len(lines):
            stripped = strip_script_comment(lines[index]).strip()
            match = assign_rx.search(stripped)
            if match is None:
                index += 1
                continue

            start_line = index + 1
            key = match.group(1)
            if ";" in stripped:
                index += 1
                continue

            probe = index + 1
            while probe < len(lines):
                probe_line = strip_script_comment(lines[probe]).strip()
                if ";" in probe_line:
                    break
                if probe_line == "}" or block_start_rx.search(probe_line):
                    previous_text = ""
                    for previous in range(probe - 1, index - 1, -1):
                        previous_text = strip_script_comment(lines[previous]).strip()
                        if previous_text:
                            break
                    reason = (
                        f"{key} text list has a trailing comma before line {probe + 1}"
                        if previous_text.endswith(",")
                        else f"{key} assignment reaches line {probe + 1} without a clear terminator"
                    )
                    hits.append(
                        TextHit(
                            entry=name,
                            line=start_line,
                            text=reason,
                        )
                    )
                    break
                probe += 1
            else:
                hits.append(
                        TextHit(
                            entry=name,
                            line=start_line,
                            text=f"{key} assignment reaches end of file without a clear terminator",
                        )
                )

            index = max(index + 1, probe)

    return hits


def archive_base_name(name: str) -> str:
    normalized = name.replace("\\", "/")
    if ":" in normalized:
        normalized = normalized.rsplit(":", 1)[1]
    path = Path(normalized)
    if path.suffix.lower() == ".lmp":
        return path.stem.upper()
    return path.name.upper()


def archive_suffix(name: str) -> str:
    normalized = name.replace("\\", "/")
    if ":" in normalized:
        normalized = normalized.rsplit(":", 1)[1]
    return Path(normalized).suffix.lower()


def split_script_comment(line: str) -> tuple[str, str]:
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
            return line[:index], line[index:]
    return line, ""


def strip_script_comment(line: str) -> str:
    code, _ = split_script_comment(line)
    return code


def strip_slash_comment(line: str) -> str:
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
        if char == "/" and index + 1 < len(line) and line[index + 1] == "/":
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


def archive_member_name(name: str) -> str:
    normalized = name.replace("\\", "/")
    if ":" in normalized:
        normalized = normalized.rsplit(":", 1)[1]
    return normalized


def edge_ddf_kind(name: str) -> str:
    member = archive_member_name(name)
    base = Path(member).name.upper()
    if base in EDGE_DDF_KIND_BY_NAME:
        return EDGE_DDF_KIND_BY_NAME[base]

    stem = Path(member).stem
    if stem.upper() in EDGE_DDF_KIND_BY_NAME:
        return EDGE_DDF_KIND_BY_NAME[stem.upper()]
    if stem.lower() in EDGE_DDF_KIND_BY_STEM:
        return EDGE_DDF_KIND_BY_STEM[stem.lower()]

    lower = member.lower()
    if lower.startswith("ddf/"):
        first = lower.split("/", 2)[1]
        if first.endswith(".ddf"):
            first = first[:-4]
        named = f"DDF{first.upper()}"
        return EDGE_DDF_KIND_BY_NAME.get(named, EDGE_DDF_KIND_BY_STEM.get(first, first.replace("-", "_")))
    if "/doom_ddf/" in f"/{lower}" or lower.startswith("doom_ddf/"):
        first = Path(lower).stem
        return EDGE_DDF_KIND_BY_STEM.get(first, first.replace("-", "_"))

    return "ddf"


def edge_script_kind(name: str) -> str:
    member = archive_member_name(name)
    base = Path(member).name.upper()
    suffix = Path(member).suffix.lower()
    lower = member.lower()

    if base == "COAL" or suffix == ".coal" or lower.startswith("coal/"):
        return "coal"
    if base == "LUA" or suffix == ".lua":
        return "lua"
    if base in {"RADTRIG", "RADTRIGS"}:
        return "radtrig"
    if base == "RTS" or suffix == ".rts":
        return "rts"
    return "script"


def edge_entry_key(name: str) -> str:
    return name.replace("\\", "/").strip("/").lower()


def edge_section_label(section: EdgeDdfSectionHint) -> str:
    if section.parent:
        return f"{section.name}:{section.parent}"
    return section.name


def normalize_edge_property_key(key: str) -> str:
    return key.strip().lower()


def normalize_edge_property_value(value: str) -> str:
    cleaned = value.strip().rstrip(";").strip()
    return cleaned[:240]


def edge_property_continues(line: str) -> bool:
    return not line.rstrip().endswith(";")


def edge_unquote(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
        return value[1:-1]
    return value


def edge_speed_to_tics(value: str) -> str | None:
    cleaned = edge_unquote(value).strip()
    match = re.match(r"^([0-9]+)(?:\s*T)?$", cleaned, flags=re.IGNORECASE)
    if match is None:
        return None
    return match.group(1)


def edge_split_sequence(value: str) -> list[str]:
    values: list[str] = []
    current: list[str] = []
    in_quote = False
    quote_char = ""
    escape = False
    for char in value:
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
        if char in {"'", '"'}:
            in_quote = True
            quote_char = char
            continue
        if char == ",":
            item = "".join(current).strip()
            if item:
                values.append(edge_unquote(item))
            current = []
            continue
        current.append(char)
    item = "".join(current).strip()
    if item:
        values.append(edge_unquote(item))
    return values


def parse_edge_property_hint(name: str, line_no: int, line: str) -> EdgeDdfPropertyHint | None:
    prop = re.match(r"^\s*([A-Za-z_][A-Za-z0-9_./-]*)(?:\([^)]*\))?\s*(?:=|:)\s*(.*?)\s*;?\s*$", line)
    if prop is None:
        return None
    return EdgeDdfPropertyHint(
        entry=name,
        line=line_no,
        key=normalize_edge_property_key(prop.group(1)),
        value=normalize_edge_property_value(prop.group(2)),
    )


def apply_edge_property_hint(section: EdgeDdfSectionHint, hint: EdgeDdfPropertyHint) -> None:
    if hint.key not in section.property_keys:
        section.property_keys.append(hint.key)
    section.properties.append(hint)


def is_edge_ddf_name(name: str) -> bool:
    member = archive_member_name(name)
    base = Path(member).name.upper()
    suffix = Path(member).suffix.lower()
    return base in EDGE_DDF_LUMP_NAMES or suffix == ".ddf" or member.lower().startswith("ddf/")


def parse_edge_include_hint(name: str, line_no: int, line: str) -> EdgeDdfIncludeHint | None:
    match = re.match(r"^\s*#?\s*include\b\s*(?:=|\s)?\s*(.+?)\s*;?\s*$", line, flags=re.IGNORECASE)
    if match is None:
        return None
    target = match.group(1).strip()
    target = target.strip("\"'<>")
    if not target:
        return None
    return EdgeDdfIncludeHint(entry=name, line=line_no, target=target)


def resolve_edge_include_target(hint: EdgeDdfIncludeHint, ddf_entries: Iterable[str]) -> str | None:
    entry_by_key = {edge_entry_key(entry): entry for entry in ddf_entries}
    target = hint.target.replace("\\", "/").strip().strip("/")
    if not target:
        return None

    candidates: list[str] = []
    if not re.match(r"^[A-Za-z]:/", target):
        parent = hint.entry.replace("\\", "/").rsplit("/", 1)[0] if "/" in hint.entry else ""
        if parent:
            candidates.append(f"{parent}/{target}")
    candidates.append(target)
    candidates.append(Path(target).name)

    for candidate in candidates:
        match = entry_by_key.get(edge_entry_key(candidate))
        if match is not None:
            return match

    target_name = Path(target).name.lower()
    suffix_matches = [
        entry
        for entry in ddf_entries
        if Path(entry.replace("\\", "/")).name.lower() == target_name
    ]
    if len(suffix_matches) == 1:
        return suffix_matches[0]
    return None


def resolve_edge_includes(profile: EdgeClassicProfile) -> None:
    for hint in profile.ddf_includes:
        hint.resolved_entry = resolve_edge_include_target(hint, profile.ddf_entries)
    profile.ddf_missing_includes = [hint for hint in profile.ddf_includes if hint.resolved_entry is None]


def build_edge_load_order(profile: EdgeClassicProfile) -> None:
    includes_by_entry: dict[str, list[str]] = {entry: [] for entry in profile.ddf_entries}
    for hint in profile.ddf_includes:
        if hint.resolved_entry is not None:
            includes_by_entry.setdefault(hint.entry, []).append(hint.resolved_entry)

    # Build dependency-first order so generated reports describe the same layering
    # an EDGE-style include chain would use, while still reporting cycles below.
    order: list[str] = []
    permanent: set[str] = set()
    temporary: set[str] = set()
    stack: list[str] = []
    seen_cycles: set[tuple[str, ...]] = set()

    def visit(entry: str) -> None:
        if entry in permanent:
            return
        if entry in temporary:
            try:
                start = stack.index(entry)
            except ValueError:
                cycle = [entry]
            else:
                cycle = stack[start:] + [entry]
            key = tuple(edge_entry_key(item) for item in cycle)
            if key not in seen_cycles:
                profile.ddf_include_cycles.append(cycle)
                seen_cycles.add(key)
            return

        temporary.add(entry)
        stack.append(entry)
        for dependency in includes_by_entry.get(entry, []):
            visit(dependency)
        stack.pop()
        temporary.remove(entry)
        permanent.add(entry)
        if entry not in order:
            order.append(entry)

    for entry in profile.ddf_entries:
        visit(entry)

    profile.ddf_load_order = order


def build_edge_conflicts(profile: EdgeClassicProfile) -> None:
    order_index = {entry: index for index, entry in enumerate(profile.ddf_load_order)}
    sections = sorted(
        profile.ddf_sections,
        key=lambda section: (order_index.get(section.entry, len(order_index)), section.entry.lower(), section.line),
    )
    grouped: dict[tuple[str, str], list[EdgeDdfSectionHint]] = {}
    for section in sections:
        key = (section.kind.lower(), edge_section_label(section).lower())
        grouped.setdefault(key, []).append(section)

    conflicts: list[EdgeDdfConflictHint] = []
    for (kind, name), matches in sorted(grouped.items()):
        if len(matches) < 2:
            continue
        conflicts.append(
            EdgeDdfConflictHint(
                kind=kind,
                name=name,
                definitions=[
                    TextHit(
                        entry=section.entry,
                        line=section.line,
                        text=f"{section.kind} [{edge_section_label(section)}]",
                    )
                    for section in matches
                ],
            )
        )
    profile.ddf_conflicts = conflicts


def build_edge_neutral_manifest(profile: EdgeClassicProfile) -> None:
    order_index = {entry: index for index, entry in enumerate(profile.ddf_load_order)}
    ordered_sections = sorted(
        profile.ddf_sections,
        key=lambda section: (order_index.get(section.entry, len(order_index)), section.entry.lower(), section.line),
    )

    entries: list[EdgeDdfManifestEntry] = []
    status_counts: dict[str, int] = {}
    manual_counts: dict[str, int] = {}
    duplicate_counts: dict[str, int] = {}

    for section in ordered_sections:
        supported_keys = EDGE_STAGE3_SUPPORTED_PROPERTIES.get(section.kind, set())
        supported: dict[str, str] = {}
        manual: list[str] = []
        key_counts: dict[str, int] = {}

        # Keep duplicate keys visible instead of silently turning them into
        # last-writer-wins data. Later adapters need to decide whether these
        # represent overrides, alternatives, or accidental stale code.
        for prop in section.properties:
            key_counts[prop.key] = key_counts.get(prop.key, 0) + 1
            if prop.key in supported_keys:
                supported[prop.key] = prop.value
            elif prop.key not in manual:
                manual.append(prop.key)
                manual_counts[prop.key] = manual_counts.get(prop.key, 0) + 1

        duplicates = sorted(
            key for key, count in key_counts.items() if count > 1 and key not in EDGE_REPEATABLE_PROPERTY_KEYS
        )
        for key in duplicates:
            duplicate_counts[key] = duplicate_counts.get(key, 0) + 1

        if supported and (manual or duplicates):
            status = "partial"
        elif supported:
            status = "bridgeable"
        else:
            status = "manual"

        entry = EdgeDdfManifestEntry(
            kind=section.kind,
            label=edge_section_label(section),
            entry=section.entry,
            line=section.line,
            status=status,
            supported_properties=dict(sorted(supported.items())),
            manual_properties=sorted(manual),
            duplicate_properties=duplicates,
            property_count=len(section.properties),
        )
        entries.append(entry)
        status_counts[status] = status_counts.get(status, 0) + 1

    profile.ddf_manifest_entries = entries
    profile.ddf_manifest_counts = dict(sorted(status_counts.items()))
    profile.ddf_manual_property_counts = dict(sorted(manual_counts.items(), key=lambda item: (-item[1], item[0])))
    profile.ddf_duplicate_property_counts = dict(sorted(duplicate_counts.items(), key=lambda item: (-item[1], item[0])))


def edge_animdefs_line(item: EdgeDdfManifestEntry) -> str | None:
    # Stage 4 is a candidate translation only; Stage 5 checks whether these
    # texture/flat names actually exist in the selected IWAD or mod bundle.
    props = item.supported_properties
    anim_type = edge_unquote(props.get("type", "")).lower()
    if anim_type not in {"flat", "texture"}:
        return None
    tics = edge_speed_to_tics(props.get("speed", ""))
    if tics is None:
        return None

    keyword = "flat" if anim_type == "flat" else "texture"
    first = edge_unquote(props.get("first", ""))
    last = edge_unquote(props.get("last", ""))
    if first and last:
        return f"{keyword} {first} range {last} tics {tics}"

    sequence = edge_split_sequence(props.get("sequence", ""))
    if sequence:
        lines = [f"{keyword} {item.label}"]
        lines.extend(f"    pic {pic} tics {tics}" for pic in sequence)
        return "\n".join(lines)
    return None


def edge_sndinfo_line(item: EdgeDdfManifestEntry) -> str | None:
    props = item.supported_properties
    lump = edge_unquote(props.get("lump_name") or props.get("lump", ""))
    if not lump:
        return None
    return f"{item.label} {lump}"


def build_edge_translation_summary(profile: EdgeClassicProfile) -> None:
    summary = EdgeTranslationSummary()
    for item in profile.ddf_manifest_entries:
        if item.kind == "animations" and edge_animdefs_line(item) is not None:
            summary.animdefs_entries += 1
        elif item.kind == "sounds" and edge_sndinfo_line(item) is not None:
            summary.sndinfo_entries += 1
        elif item.kind in {"animations", "sounds"}:
            summary.skipped_entries += 1
    profile.translation_summary = summary


def parse_edge_ddf_hints(
    name: str, text: str, kind: str
) -> tuple[list[EdgeDdfSectionHint], list[EdgeDdfIncludeHint], list[TextHit], list[TextHit]]:
    sections: list[EdgeDdfSectionHint] = []
    includes: list[EdgeDdfIncludeHint] = []
    clearall: list[TextHit] = []
    versions: list[TextHit] = []
    current: EdgeDdfSectionHint | None = None
    continuation_key: str | None = None

    for line_no, raw_line in enumerate(text.splitlines(), start=1):
        line = strip_slash_comment(raw_line).strip()
        if not line:
            continue

        include = parse_edge_include_hint(name, line_no, line)
        if include is not None:
            includes.append(include)
            continue

        if re.match(r"^\s*#?\s*clearall\b", line, flags=re.IGNORECASE):
            clearall.append(TextHit(entry=name, line=line_no, text=line[:240]))
            continue

        version = re.match(r"^\s*#?\s*version\b\s*(?:=|\s)?\s*(.+?)\s*;?\s*$", line, flags=re.IGNORECASE)
        if version is not None:
            versions.append(TextHit(entry=name, line=line_no, text=version.group(1).strip()[:240]))
            continue

        section = re.match(r"^\s*\[([^\]:]+)(?::([^\]]+))?\]\s*(.*)$", line)
        if section is not None:
            continuation_key = None
            if current is not None:
                sections.append(current)
            current = EdgeDdfSectionHint(
                entry=name,
                line=line_no,
                kind=kind,
                name=section.group(1).strip(),
                parent=section.group(2).strip() if section.group(2) else None,
            )
            trailing = section.group(3).strip()
            if trailing:
                prop_hint = parse_edge_property_hint(name, line_no, trailing)
                if prop_hint is not None:
                    apply_edge_property_hint(current, prop_hint)
                    if edge_property_continues(trailing):
                        continuation_key = prop_hint.key
            continue

        if current is None:
            continue

        # EDGE state/value lists often continue across comma-terminated lines.
        # Those frame rows are data for the prior property, not new DDF keys.
        if continuation_key is not None:
            if not edge_property_continues(line):
                continuation_key = None
            continue

        prop_hint = parse_edge_property_hint(name, line_no, line)
        if prop_hint is None:
            continue
        apply_edge_property_hint(current, prop_hint)
        if edge_property_continues(line):
            continuation_key = prop_hint.key

    if current is not None:
        sections.append(current)
    return sections, includes, clearall, versions


def build_edge_profile(
    surfaces: dict[str, list[str]], texts: dict[str, bytes], entries: Iterable[ArchiveEntry]
) -> EdgeClassicProfile | None:
    if "edge_ddf" not in surfaces and "edge_scripts" not in surfaces:
        return None

    profile = EdgeClassicProfile()
    entry_sizes = {entry.name: entry.size for entry in entries}
    profile.ddf_entries = list(surfaces.get("edge_ddf", []))
    profile.ddf_entry_kinds = {name: edge_ddf_kind(name) for name in profile.ddf_entries}

    for name in surfaces.get("edge_scripts", []):
        profile.script_entries.append(
            EdgeScriptHint(entry=name, kind=edge_script_kind(name), size=entry_sizes.get(name, len(texts.get(name, b""))))
        )

    for name, data in sorted(texts.items(), key=lambda kv: kv[0].lower()):
        if not is_edge_ddf_name(name):
            continue
        kind = edge_ddf_kind(name)
        text = decode_text(data)
        sections, includes, clearall, versions = parse_edge_ddf_hints(name, text, kind)
        profile.ddf_sections.extend(sections)
        profile.ddf_includes.extend(includes)
        profile.ddf_clearall.extend(clearall)
        profile.ddf_versions.extend(versions)

    resolve_edge_includes(profile)
    build_edge_load_order(profile)
    build_edge_conflicts(profile)
    build_edge_neutral_manifest(profile)
    build_edge_translation_summary(profile)

    counts: dict[str, int] = {}
    for name in profile.ddf_entries:
        counts[profile.ddf_entry_kinds.get(name, "ddf")] = counts.get(profile.ddf_entry_kinds.get(name, "ddf"), 0) + 1
    for section in profile.ddf_sections:
        counts[section.kind] = max(counts.get(section.kind, 0), 0)
    profile.kind_counts = dict(sorted(counts.items()))

    profile.stage_flags = {
        "stage1_manifest": True,
        "stage2_include_order": True,
        "stage2_conflict_report": True,
        "stage3_neutral_manifest": True,
        "stage4_candidate_translation": True,
        "ddf_runtime_adapter": False,
        "coal_lua_runtime_adapter": False,
    }

    if profile.ddf_entries and not profile.ddf_sections:
        profile.notes.append("DDF resources were found, but no bracketed DDF sections were recognized by the scanner.")
    if profile.ddf_includes and not profile.ddf_missing_includes and not profile.ddf_include_cycles:
        profile.notes.append("DDF include directives were resolved into a deterministic load order.")
    if profile.ddf_missing_includes:
        profile.notes.append(f"{len(profile.ddf_missing_includes)} DDF include target(s) could not be resolved from the scanned files.")
    if profile.ddf_include_cycles:
        profile.notes.append(f"{len(profile.ddf_include_cycles)} DDF include cycle(s) were detected.")
    if profile.ddf_clearall:
        profile.notes.append("DDF reset directives were found; HCDE must report merge order instead of silently replacing base definitions.")
    if profile.ddf_conflicts:
        profile.notes.append(f"{len(profile.ddf_conflicts)} duplicate DDF section definition(s) need explicit merge decisions.")
    if profile.ddf_manifest_entries:
        bridgeable = profile.ddf_manifest_counts.get("bridgeable", 0)
        partial = profile.ddf_manifest_counts.get("partial", 0)
        manual = profile.ddf_manifest_counts.get("manual", 0)
        profile.notes.append(f"Neutral DDF manifest entries: {bridgeable} bridgeable, {partial} partial, {manual} manual.")
    if profile.translation_summary.animdefs_entries or profile.translation_summary.sndinfo_entries:
        profile.notes.append(
            "Stage 4 candidate translations: "
            f"{profile.translation_summary.animdefs_entries} ANIMDEFS line(s), "
            f"{profile.translation_summary.sndinfo_entries} SNDINFO mapping(s)."
        )
    if profile.script_entries:
        profile.notes.append("COAL, Lua, RTS, or RadTrig resources are report-only until HCDE has an explicit sandbox/API compatibility layer.")
    behavior_kinds = {section.kind for section in profile.ddf_sections}
    if behavior_kinds.intersection({"things", "weapons", "attacks", "lines", "sectors", "sounds"}):
        profile.notes.append("Gameplay-facing DDF categories are present and need narrow HCDE-owned shims or a reviewed adapter subset.")

    return profile


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
    entries: Iterable[ArchiveEntry],
    eternity: EternityValidationProfile | None = None,
    edge: EdgeClassicProfile | None = None,
) -> list[CompatibilityCheck]:
    checks: list[CompatibilityCheck] = []

    asset_conflict_hits = find_asset_namespace_conflicts(entries)
    if asset_conflict_hits:
        checks.append(
            CompatibilityCheck(
                id="pk3-asset-namespace-conflicts",
                severity="candidate",
                title="PK3 has non-asset files inside asset namespaces",
                detail="HCDE and other Doom-family ports use path namespaces to pick asset loaders. Text files, WADs, marker lumps, or system files inside image/sound folders can be selected as textures or sounds by short-name lookup; move or remove them in a local fixed copy before writing an engine compat shim.",
                hits=asset_conflict_hits[:80],
            )
        )

    skin_prefix_hits = find_skin_sprite_prefix_conflicts(entries, texts)
    if skin_prefix_hits:
        checks.append(
            CompatibilityCheck(
                id="skininfo-prefix-collisions",
                severity="candidate",
                title="SKININFO sprite prefixes collide with non-sprite files",
                detail="HCDE's skin loader scans archive entries by the first four characters of each SKININFO sprite name. DECORATE helpers, sounds, or other non-image files with the same prefix can be opened as skin sprites and fail texture decoding; rename the helper path or move the non-sprite file in a local fixed copy.",
                hits=skin_prefix_hits[:80],
            )
        )

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
                detail="HCDE now generates EDGE Stage 1-4 outputs for DDF, RTS, COAL, and Lua resources, including include order, definition conflicts, a neutral DDF manifest, and reviewed candidate ANIMDEFS/SNDINFO translations. Treat them as design input; do not copy DDF/script content into HCDE without license review.",
            )
        )
        if edge and edge.ddf_sections:
            checks.append(
                CompatibilityCheck(
                    id="edge-ddf-sections",
                    severity="candidate",
                    title="EDGE DDF definitions detected",
                    detail="These DDF blocks describe gameplay, map, sound, or UI definitions that need an HCDE-owned adapter subset or narrow compatibility shims.",
                    hits=[
                        TextHit(
                            entry=section.entry,
                            line=section.line,
                            text=f"{section.kind} [{edge_section_label(section)}] properties={', '.join(section.property_keys[:8])}",
                        )
                        for section in edge.ddf_sections[:40]
                    ],
                )
            )
        if edge and edge.ddf_manifest_entries:
            checks.append(
                CompatibilityCheck(
                    id="edge-ddf-neutral-manifest",
                    severity="info",
                    title="EDGE neutral DDF manifest generated",
                    detail="Stage 3 parsed DDF sections into HCDE-owned manifest entries. Bridgeable properties are separated from manual properties; no runtime translation is enabled yet.",
                    hits=[
                        TextHit(
                            entry=item.entry,
                            line=item.line,
                            text=f"{item.kind} [{item.label}] status={item.status} supported={', '.join(item.supported_properties.keys())}",
                        )
                        for item in edge.ddf_manifest_entries[:40]
                    ],
                )
            )
            manual_hits = [
                TextHit(
                    entry=item.entry,
                    line=item.line,
                    text=f"{item.kind} [{item.label}] manual={', '.join(item.manual_properties[:10])}",
                )
                for item in edge.ddf_manifest_entries
                if item.manual_properties
            ]
            if manual_hits:
                checks.append(
                    CompatibilityCheck(
                        id="edge-ddf-manual-properties",
                        severity="candidate",
                        title="EDGE DDF properties need manual adapter decisions",
                        detail="These DDF sections use properties outside the Stage 3 neutral bridge subset. Keep them visible until a later adapter supports them.",
                        hits=manual_hits[:40],
                    )
                )
            duplicate_hits = [
                TextHit(
                    entry=item.entry,
                    line=item.line,
                    text=f"{item.kind} [{item.label}] duplicates={', '.join(item.duplicate_properties[:10])}",
                )
                for item in edge.ddf_manifest_entries
                if item.duplicate_properties
            ]
            if duplicate_hits:
                checks.append(
                    CompatibilityCheck(
                        id="edge-ddf-duplicate-properties",
                        severity="candidate",
                        title="EDGE DDF duplicate properties need adapter decisions",
                        detail="These DDF sections repeat a property key. The manifest preserves the final value for candidate generation and reports duplicates so an adapter can decide whether the repeat is intentional.",
                        hits=duplicate_hits[:40],
                    )
                )
        if edge and (edge.translation_summary.animdefs_entries or edge.translation_summary.sndinfo_entries):
            checks.append(
                CompatibilityCheck(
                    id="edge-ddf-candidate-translations",
                    severity="candidate",
                    title="EDGE DDF candidate translations generated",
                    detail="Stage 4 writes candidate ANIMDEFS and SNDINFO files for narrow, data-only DDF animation and sound mappings. They are review outputs, not active resources.",
                    hits=[
                        TextHit(
                            entry="edge_translation",
                            line=1,
                            text=(
                                f"ANIMDEFS={edge.translation_summary.animdefs_entries}, "
                                f"SNDINFO={edge.translation_summary.sndinfo_entries}, "
                                f"skipped={edge.translation_summary.skipped_entries}"
                            ),
                        )
                    ],
                )
            )
        if edge and edge.ddf_includes:
            checks.append(
                CompatibilityCheck(
                    id="edge-ddf-include-order",
                    severity="info",
                    title="EDGE DDF include order resolved",
                    detail="Stage 2 resolved DDF include directives into the reported load order where possible. Missing includes and cycles are reported separately.",
                    hits=[
                        TextHit(
                            entry=hint.entry,
                            line=hint.line,
                            text=f"{hint.target} -> {hint.resolved_entry or '<unresolved>'}",
                        )
                        for hint in edge.ddf_includes[:40]
                    ],
                )
            )
        if edge and edge.ddf_missing_includes:
            checks.append(
                CompatibilityCheck(
                    id="edge-ddf-missing-includes",
                    severity="candidate",
                    title="EDGE DDF includes could not be resolved",
                    detail="These include targets were not found in the scanned archive/folder. The mod may require additional files or a different root folder.",
                    hits=[
                        TextHit(entry=hint.entry, line=hint.line, text=hint.target)
                        for hint in edge.ddf_missing_includes[:40]
                    ],
                )
            )
        if edge and edge.ddf_include_cycles:
            checks.append(
                CompatibilityCheck(
                    id="edge-ddf-include-cycles",
                    severity="candidate",
                    title="EDGE DDF include cycles detected",
                    detail="Cycles prevent a simple one-pass merge order. Future runtime adapters must break or explicitly model these relationships.",
                    hits=[
                        TextHit(entry=cycle[0], line=1, text=" -> ".join(cycle))
                        for cycle in edge.ddf_include_cycles[:20]
                    ],
                )
            )
        if edge and edge.ddf_clearall:
            checks.append(
                CompatibilityCheck(
                    id="edge-ddf-reset-order",
                    severity="candidate",
                    title="EDGE DDF reset directives detected",
                    detail="Reset directives can replace base definitions. HCDE should report this explicitly before enabling any runtime DDF adapter.",
                    hits=edge.ddf_clearall[:40],
                )
            )
        if edge and edge.ddf_conflicts:
            checks.append(
                CompatibilityCheck(
                    id="edge-ddf-definition-conflicts",
                    severity="candidate",
                    title="EDGE DDF duplicate definitions need merge decisions",
                    detail="More than one DDF section defines the same category/name. HCDE must report or resolve these explicitly before translating definitions.",
                    hits=[
                        TextHit(
                            entry=conflict.definitions[-1].entry,
                            line=conflict.definitions[-1].line,
                            text=f"{conflict.kind} [{conflict.name}] definitions={len(conflict.definitions)}",
                        )
                        for conflict in edge.ddf_conflicts[:40]
                    ],
                )
            )
        if edge and edge.script_entries:
            checks.append(
                CompatibilityCheck(
                    id="edge-script-runtime-needed",
                    severity="manual",
                    title="EDGE COAL/Lua/RTS scripts need a sandbox layer",
                    detail="Script files are detected and listed, but HCDE should not execute or emulate them until the accepted API surface and conflict rules are designed.",
                    hits=[
                        TextHit(entry=script.entry, line=1, text=f"{script.kind} script, {script.size} bytes")
                        for script in edge.script_entries[:40]
                    ],
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

    invasion_cvar_hits = find_line_hits(
        texts,
        r"\bsv_invasion(?:basebudget|budgetstep|cleanuptime|countdowntime|intermissiontime|resulttime|waves|"
        r"perplayer|spawnburst|spawninterval|spotfallback|spotusemaptags|bosswaveevery|bossbonus|spawntime)\b",
    )
    invasion_spot_hits = find_line_hits(texts, r"\bCustomMonsterInvasionSpot\b")
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

    if invasion_cvar_hits or invasion_spot_hits:
        checks.append(
            CompatibilityCheck(
                id="invasion-mode-indicators",
                severity="manual",
                title="Invasion-mode indicators detected",
                detail="Compat scan found mod symbols that likely indicate invasion gameplay intent. "
                       "Treat this as invasion-mode-capable and confirm runtime behavior in networked tests.",
                hits=(invasion_cvar_hits[:40] + invasion_spot_hits[:40])[:40],
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

    mapinfo_text_hits = find_mapinfo_unterminated_text_assignments(texts)
    if mapinfo_text_hits:
        checks.append(
            CompatibilityCheck(
                id="mapinfo-unterminated-text-list",
                severity="blocker",
                title="MAPINFO text list has an invalid ending",
                detail="HCDE rejects MAPINFO text assignments that run into a closing brace or new block with a trailing comma or no clear terminator. Remove the final trailing comma or repair the local mod copy before launch.",
                hits=mapinfo_text_hits[:40],
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
    edge = build_edge_profile(surfaces, texts, entries)
    checks = build_checks(surfaces, texts, entries, eternity, edge)
    return ScanResult(
        source_path=str(path.resolve()),
        archive_type=archive_type,
        sha256=sha256_path(path),
        slug=slug_override or make_slug(path),
        entries=entries,
        surfaces=surfaces,
        checks=checks,
        warnings=warnings,
        eternity=eternity,
        edge=edge,
    )


AUTO_RELOCATE_CHECK_IDS = {
    "pk3-asset-namespace-conflicts",
    "skininfo-prefix-collisions",
}
AUTO_MAPINFO_FIX_CHECK_ID = "mapinfo-unterminated-text-list"
AUTO_RELOCATE_PREFIX = "hcde_compat/non_asset"

HCDE_ENGINE_CHANGE_HINTS = {
    "invasion-mode-indicators": "If this mod is intended as invasion-style gameplay, test with sv_gametype=4 and verify "
                                "map metadata/spawn behavior. Add compatibility glue if required rather than assuming defaults.",
    "zscript-present": "Review HCDE's ZScript VM/runtime compatibility surface and add an engine or compat-layer gate for unsupported APIs.",
    "decorate-railattack": "Review dedicated-server rail attack handling and fallback logic in gameplay/network paths.",
    "player-input-assumptions": "Review player index/input behavior for dedicated server mode and ACS/ZScript helpers.",
    "dehacked-present": "Confirm the expected DeHackEd/BEX/MBF/MBF21 profile and adjust parser/runtime compatibility flags as needed.",
    "mapinfo-with-maps": "Review map metadata load order and map-name transitions (MAPxx/E#M# and custom map names).",
    "eternity-compat-surface": "Extend Eternity compatibility layer support and add focused runtime validation for detected resources.",
    "eternity-native-mapinfo-precedence": "Adjust EMAPINFO/native MAPINFO precedence rules only if the mod expects alternate priority.",
    "eternity-edf-thingtypes": "Add or extend EDF-to-HCDE actor binding behavior in the compatibility layer.",
    "eternity-edf-behavior-patches": "Implement manual EDF behavior shims in HCDE compat code (do not import third-party definitions directly).",
    "eternity-extradata-references": "Extend ExtraData/XLAT resolution rules if startup logs show unresolved references.",
    "edge-classic-compat-surface": "Design/extend EDGE compatibility adapter boundaries before runtime enablement.",
    "edge-ddf-sections": "Extend DDF adapter coverage for detected section kinds and validate merge behavior.",
    "edge-ddf-neutral-manifest": "Use the neutral manifest to select safe bridge fields and track unsupported properties.",
    "edge-ddf-manual-properties": "Implement explicit adapter rules for manual DDF properties before enabling them.",
    "edge-ddf-duplicate-properties": "Define duplicate-property conflict policy in the adapter (do not silently discard intent).",
    "edge-ddf-candidate-translations": "Review generated ANIMDEFS/SNDINFO candidates and integrate only validated lines.",
    "edge-ddf-include-order": "Validate DDF include order behavior against HCDE load layering.",
    "edge-ddf-missing-includes": "Add missing include root resolution logic or require additional mod payload files.",
    "edge-ddf-include-cycles": "Implement cycle-safe include handling in any runtime DDF adapter.",
    "edge-ddf-reset-order": "Model reset directive ordering explicitly in adapter logic.",
    "edge-ddf-definition-conflicts": "Implement conflict resolution policy for duplicate DDF definitions.",
    "edge-script-runtime-needed": "COAL/Lua/RTS require an explicit sandbox/API layer before runtime support.",
}


def normalize_archive_entry_name(name: str) -> str:
    return name.replace("\\", "/").lstrip("/")


def check_hits_by_id(result: ScanResult, check_id: str) -> list[TextHit]:
    for check in result.checks:
        if check.id == check_id:
            return check.hits
    return []


def write_hcde_engine_change_report(result: ScanResult, out_dir: Path, handled_check_ids: set[str]) -> None:
    # Keep the report focused on checks that still need manual/engine work after
    # any automatic transforms have already been applied.
    unresolved_engine_checks: list[CompatibilityCheck] = []
    unresolved_other_checks: list[CompatibilityCheck] = []
    for check in result.checks:
        if check.id in handled_check_ids:
            continue
        if check.id in HCDE_ENGINE_CHANGE_HINTS:
            unresolved_engine_checks.append(check)
        elif check.severity in {"manual", "candidate", "blocker"}:
            unresolved_other_checks.append(check)

    report_data = {
        "slug": result.slug,
        "source_path": result.source_path,
        "source_sha256": result.sha256,
        "auto_handled_check_ids": sorted(handled_check_ids),
        "engine_changes_needed": [
            {
                "id": check.id,
                "severity": check.severity,
                "title": check.title,
                "detail": check.detail,
                "hcde_change_hint": HCDE_ENGINE_CHANGE_HINTS.get(check.id, ""),
                "evidence": [asdict(hit) for hit in check.hits[:20]],
            }
            for check in unresolved_engine_checks
        ],
        "other_unresolved_checks": [
            {
                "id": check.id,
                "severity": check.severity,
                "title": check.title,
                "detail": check.detail,
                "evidence": [asdict(hit) for hit in check.hits[:20]],
            }
            for check in unresolved_other_checks
        ],
    }

    (out_dir / "hcde_engine_change_report.json").write_text(
        json.dumps(report_data, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )

    lines: list[str] = []
    lines.append(f"# HCDE Engine Change Report: {result.slug}")
    lines.append("")
    lines.append("This report lists checks that the auto-attempt patcher did not directly transform.")
    lines.append("It highlights likely HCDE engine/compatibility-layer work that should happen next.")
    lines.append("")
    lines.append(f"- Source: `{Path(result.source_path).name}`")
    lines.append(f"- SHA-256: `{result.sha256}`")
    lines.append(f"- Auto-handled checks: `{', '.join(sorted(handled_check_ids)) if handled_check_ids else 'none'}`")
    lines.append("")

    lines.append("## HCDE Changes Needed")
    lines.append("")
    if unresolved_engine_checks:
        for check in unresolved_engine_checks:
            lines.append(f"### {check.title}")
            lines.append("")
            lines.append(f"- Id: `{check.id}`")
            lines.append(f"- Severity: `{check.severity}`")
            lines.append(f"- Why it remains: {check.detail}")
            lines.append(f"- Suggested HCDE work: {HCDE_ENGINE_CHANGE_HINTS.get(check.id, 'Review engine compatibility layer behavior and add targeted support.')}")
            if check.hits:
                lines.append("- Evidence:")
                for hit in check.hits[:12]:
                    safe = hit.text.replace("`", "'")
                    lines.append(f"  - `{hit.entry}:{hit.line}`: `{safe}`")
            lines.append("")
    else:
        lines.append("No unresolved checks were mapped to HCDE engine changes.")
        lines.append("")

    lines.append("## Other Unresolved Checks")
    lines.append("")
    if unresolved_other_checks:
        for check in unresolved_other_checks:
            lines.append(f"- `{check.id}` ({check.severity}): {check.title}")
    else:
        lines.append("- none")
    lines.append("")

    (out_dir / "hcde_engine_change_report.md").write_text(
        "\n".join(lines),
        encoding="utf-8",
        newline="\n",
    )


def terminate_mapinfo_assignment_line(line: str) -> tuple[str, bool]:
    code, comment = split_script_comment(line)
    trimmed = code.rstrip()
    if not trimmed:
        return line, False
    if trimmed.endswith(";"):
        return line, False
    if trimmed.endswith(","):
        trimmed = trimmed[:-1].rstrip() + ";"
    else:
        trimmed += ";"
    suffix = comment
    if suffix and not suffix.startswith((" ", "\t")):
        suffix = " " + suffix
    return trimmed + suffix, True


def auto_fix_mapinfo_text(text: str) -> tuple[str, list[int]]:
    text_keys = "entertext|exittext|exittextsecret|intertext|intertextsecret"
    assign_rx = re.compile(rf"^\s*({text_keys})\s*=", re.IGNORECASE)
    block_start_rx = re.compile(r"^\s*(clusterdef|episode|map)\b", re.IGNORECASE)

    lines = text.splitlines()
    had_newline = text.endswith("\n")
    changed_lines: list[int] = []
    index = 0

    while index < len(lines):
        stripped = strip_script_comment(lines[index]).strip()
        match = assign_rx.search(stripped)
        if match is None:
            index += 1
            continue

        if ";" in stripped:
            index += 1
            continue

        probe = index + 1
        while probe < len(lines):
            probe_line = strip_script_comment(lines[probe]).strip()
            if ";" in probe_line:
                break

            if probe_line == "}" or block_start_rx.search(probe_line):
                target = None
                for previous in range(probe - 1, index - 1, -1):
                    if strip_script_comment(lines[previous]).strip():
                        target = previous
                        break
                if target is None:
                    target = index

                updated_line, changed = terminate_mapinfo_assignment_line(lines[target])
                if changed:
                    lines[target] = updated_line
                    changed_lines.append(target + 1)
                break
            probe += 1
        else:
            target = None
            for previous in range(len(lines) - 1, index - 1, -1):
                if strip_script_comment(lines[previous]).strip():
                    target = previous
                    break
            if target is None:
                target = index

            updated_line, changed = terminate_mapinfo_assignment_line(lines[target])
            if changed:
                lines[target] = updated_line
                changed_lines.append(target + 1)

        index = max(index + 1, probe)

    rebuilt = "\n".join(lines)
    if had_newline:
        rebuilt += "\n"
    return rebuilt, changed_lines


def read_payload_for_auto_attempt(path: Path) -> tuple[dict[str, bytes], str]:
    suffix = path.suffix.lower()
    payload: dict[str, bytes] = {}

    if path.is_dir():
        for entry in sorted(path.rglob("*")):
            if not entry.is_file():
                continue
            rel = entry.relative_to(path).as_posix()
            try:
                payload[rel] = entry.read_bytes()
            except OSError:
                continue
        return payload, "directory"

    if suffix in {".pk3", ".zip", ".pkz"}:
        with zipfile.ZipFile(path, "r") as archive:
            for info in archive.infolist():
                if info.is_dir():
                    continue
                payload[info.filename] = archive.read(info.filename)
        return payload, "zip"

    return payload, "unsupported"


def unique_relocated_entry_name(source_name: str, used: set[str]) -> str:
    normalized = normalize_archive_entry_name(source_name).replace(":", "_")
    candidate = f"{AUTO_RELOCATE_PREFIX}/{normalized}"
    current = candidate
    serial = 2
    while current.lower() in used:
        if "." in candidate:
            base, ext = candidate.rsplit(".", 1)
            current = f"{base}_{serial}.{ext}"
        else:
            current = f"{candidate}_{serial}"
        serial += 1
    used.add(current.lower())
    return current


def write_auto_patch_attempt(result: ScanResult, source_path: Path, out_dir: Path) -> tuple[list[str], set[str]]:
    notes: list[str] = []
    handled_check_ids: set[str] = set()
    payload, payload_kind = read_payload_for_auto_attempt(source_path)
    if payload_kind == "unsupported":
        notes.append("Auto attempt skipped: only directory/.pk3/.zip inputs are supported.")
        return notes, handled_check_ids
    if not payload:
        notes.append("Auto attempt skipped: no readable payload entries were found.")
        return notes, handled_check_ids

    lookup = {
        normalize_archive_entry_name(name).lower(): name
        for name in payload.keys()
    }
    # Track relocation candidates by originating check id so the engine-change
    # report can hide only checks that were actually transformed.
    relocate_entries: set[str] = set()
    relocate_sources_by_check: dict[str, set[str]] = {check_id: set() for check_id in AUTO_RELOCATE_CHECK_IDS}
    for check_id in AUTO_RELOCATE_CHECK_IDS:
        for hit in check_hits_by_id(result, check_id):
            normalized = normalize_archive_entry_name(hit.entry).lower()
            resolved = lookup.get(normalized)
            if resolved is not None:
                relocate_entries.add(resolved)
                relocate_sources_by_check[check_id].add(resolved)

    mapinfo_targets: set[str] = set()
    for hit in check_hits_by_id(result, AUTO_MAPINFO_FIX_CHECK_ID):
        normalized = normalize_archive_entry_name(hit.entry).lower()
        resolved = lookup.get(normalized)
        if resolved is not None:
            mapinfo_targets.add(resolved)

    if not relocate_entries and not mapinfo_targets:
        notes.append("Auto attempt found no safe transformations for this mod.")
        return notes, handled_check_ids

    fixed_payload: dict[str, bytes] = {}
    used_names: set[str] = set()
    actions: list[dict[str, object]] = []
    mapinfo_fix_count = 0

    for source_name in sorted(payload.keys(), key=lambda item: item.lower()):
        target_name = source_name
        if source_name in relocate_entries:
            target_name = unique_relocated_entry_name(source_name, used_names)
            for check_id, sources in relocate_sources_by_check.items():
                if source_name in sources:
                    handled_check_ids.add(check_id)
            actions.append(
                {
                    "action": "relocate_non_asset",
                    "source": source_name,
                    "target": target_name,
                }
            )
        else:
            used_names.add(normalize_archive_entry_name(target_name).lower())

        content = payload[source_name]
        if source_name in mapinfo_targets:
            text, encoding = decode_text_with_encoding(content)
            fixed_text, changed_lines = auto_fix_mapinfo_text(text)
            if changed_lines:
                mapinfo_fix_count += len(changed_lines)
                handled_check_ids.add(AUTO_MAPINFO_FIX_CHECK_ID)
                try:
                    content = fixed_text.encode(encoding)
                except UnicodeEncodeError:
                    content = fixed_text.encode("utf-8")
                actions.append(
                    {
                        "action": "terminate_mapinfo_text_assignment",
                        "entry": target_name,
                        "lines": changed_lines,
                    }
                )

        fixed_payload[target_name] = content

    auto_dir = out_dir / "auto_patch_attempt"
    auto_dir.mkdir(parents=True, exist_ok=True)
    fixed_archive = auto_dir / f"{result.slug}.hcde-autoattempt.pk3"
    with zipfile.ZipFile(fixed_archive, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(fixed_payload.keys(), key=lambda item: item.lower()):
            archive.writestr(name, fixed_payload[name])

    summary = {
        "source": str(source_path.resolve()),
        "source_sha256": result.sha256,
        "output_archive": str(fixed_archive.resolve()),
        "transforms": actions,
        "counts": {
            "relocated_entries": sum(1 for item in actions if item["action"] == "relocate_non_asset"),
            "mapinfo_line_fixes": mapinfo_fix_count,
        },
    }
    (auto_dir / "autopatch_report.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    (auto_dir / "README.md").write_text(
        textwrap.dedent(
            f"""\
            # Auto Attempt Patch: {result.slug}

            This folder contains an automatically patched local test copy.
            It is generated from your local mod input for quick smoke tests and
            is not meant to be committed to HCDE source control.

            Output archive:
            - `{fixed_archive.name}`

            Transform counts:
            - relocated non-asset namespace entries: `{summary["counts"]["relocated_entries"]}`
            - MAPINFO text terminator fixes: `{summary["counts"]["mapinfo_line_fixes"]}`

            Detailed transform metadata is in `autopatch_report.json`.
            """
        ),
        encoding="utf-8",
        newline="\n",
    )

    notes.append(f"Auto attempt output: {fixed_archive}")
    notes.append(f"Auto attempt report: {auto_dir / 'autopatch_report.json'}")
    return notes, handled_check_ids


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

    if result.edge is not None:
        lines.extend(render_edge_profile_section(result.edge))

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


def render_edge_profile_section(profile: EdgeClassicProfile) -> list[str]:
    lines: list[str] = []
    lines.append("## EDGE Classic Profile")
    lines.append("")
    lines.append("Stage coverage:")
    for key, enabled in profile.stage_flags.items():
        lines.append(f"- {key}: `{'yes' if enabled else 'no'}`")
    lines.append("")

    if profile.ddf_entries:
        kind_preview = ", ".join(f"`{kind}`={count}" for kind, count in profile.kind_counts.items())
        lines.append(f"- DDF entries: `{len(profile.ddf_entries)}` total ({kind_preview})")
    if profile.ddf_load_order:
        order_preview = ", ".join(f"`{entry}`" for entry in profile.ddf_load_order[:16])
        if len(profile.ddf_load_order) > 16:
            order_preview += f", ... ({len(profile.ddf_load_order)} total)"
        lines.append(f"- DDF load order: {order_preview}")
    if profile.ddf_sections:
        section_preview = ", ".join(
            f"`{section.kind}:{edge_section_label(section)}`" for section in profile.ddf_sections[:20]
        )
        if len(profile.ddf_sections) > 20:
            section_preview += f", ... ({len(profile.ddf_sections)} total)"
        lines.append(f"- DDF sections: {section_preview}")
    if profile.ddf_includes:
        include_preview = ", ".join(
            f"`{hint.target}` -> `{hint.resolved_entry or '<unresolved>'}`"
            for hint in profile.ddf_includes[:20]
        )
        if len(profile.ddf_includes) > 20:
            include_preview += f", ... ({len(profile.ddf_includes)} total)"
        lines.append(f"- DDF includes: {include_preview}")
    if profile.ddf_missing_includes:
        missing_preview = ", ".join(f"`{hint.target}`" for hint in profile.ddf_missing_includes[:20])
        if len(profile.ddf_missing_includes) > 20:
            missing_preview += f", ... ({len(profile.ddf_missing_includes)} total)"
        lines.append(f"- Missing DDF includes: {missing_preview}")
    if profile.ddf_include_cycles:
        cycle_preview = ", ".join(f"`{' -> '.join(cycle)}`" for cycle in profile.ddf_include_cycles[:5])
        if len(profile.ddf_include_cycles) > 5:
            cycle_preview += f", ... ({len(profile.ddf_include_cycles)} total)"
        lines.append(f"- DDF include cycles: {cycle_preview}")
    if profile.ddf_conflicts:
        conflict_preview = ", ".join(
            f"`{conflict.kind}:{conflict.name}` x{len(conflict.definitions)}"
            for conflict in profile.ddf_conflicts[:20]
        )
        if len(profile.ddf_conflicts) > 20:
            conflict_preview += f", ... ({len(profile.ddf_conflicts)} total)"
        lines.append(f"- Duplicate DDF definitions: {conflict_preview}")
    if profile.ddf_manifest_entries:
        status_preview = ", ".join(f"`{status}`={count}" for status, count in profile.ddf_manifest_counts.items())
        lines.append(f"- Neutral DDF manifest: `{len(profile.ddf_manifest_entries)}` entries ({status_preview})")
    if profile.translation_summary.animdefs_entries or profile.translation_summary.sndinfo_entries:
        lines.append(
            "- Candidate translations: "
            f"`ANIMDEFS`={profile.translation_summary.animdefs_entries}, "
            f"`SNDINFO`={profile.translation_summary.sndinfo_entries}, "
            f"`skipped`={profile.translation_summary.skipped_entries}"
        )
    if profile.ddf_manual_property_counts:
        manual_preview = ", ".join(
            f"`{key}`={count}" for key, count in list(profile.ddf_manual_property_counts.items())[:16]
        )
        if len(profile.ddf_manual_property_counts) > 16:
            manual_preview += f", ... ({len(profile.ddf_manual_property_counts)} total)"
        lines.append(f"- Manual DDF properties: {manual_preview}")
    if profile.ddf_duplicate_property_counts:
        duplicate_preview = ", ".join(
            f"`{key}`={count}" for key, count in list(profile.ddf_duplicate_property_counts.items())[:16]
        )
        if len(profile.ddf_duplicate_property_counts) > 16:
            duplicate_preview += f", ... ({len(profile.ddf_duplicate_property_counts)} total)"
        lines.append(f"- Duplicate DDF properties: {duplicate_preview}")
    if profile.ddf_versions:
        version_preview = ", ".join(f"`{hit.text}`" for hit in profile.ddf_versions[:10])
        if len(profile.ddf_versions) > 10:
            version_preview += f", ... ({len(profile.ddf_versions)} total)"
        lines.append(f"- DDF versions: {version_preview}")
    if profile.script_entries:
        script_preview = ", ".join(f"`{script.kind}:{script.entry}`" for script in profile.script_entries[:20])
        if len(profile.script_entries) > 20:
            script_preview += f", ... ({len(profile.script_entries)} total)"
        lines.append(f"- Script entries: {script_preview}")
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


def render_edge_validation(result: ScanResult, out_dir: Path | None = None) -> str:
    profile = result.edge
    if profile is None:
        return "# HCDE EDGE Classic Validation\n\nNo EDGE Classic compatibility surfaces were detected.\n"

    log_root = str(out_dir) if out_dir is not None else f"build\\compat-candidates\\{result.slug}"
    lines: list[str] = []
    lines.append(f"# HCDE EDGE Classic Validation: {result.slug}")
    lines.append("")
    lines.append("This file is a Stage 1-4 compatibility manifest. It contains scanner metadata, DDF include order, conflict reports, neutral DDF bridge entries, and candidate translation outputs only; it does not import or execute EDGE DDF, COAL, Lua, RTS, or RadTrig behavior.")
    lines.append("")
    lines.append("## Runtime Commands")
    lines.append("")
    lines.append("Use your local IWAD path in place of `C:\\Path\\DOOM2.WAD` when needed.")
    lines.append("")
    lines.append("```powershell")
    lines.append(f"build\\RelWithDebInfo\\hcde.exe -norun -errorlog \"{log_root}\\edge-client-startup.log\" -iwad C:\\Path\\DOOM2.WAD -file \"{result.source_path}\"")
    lines.append(f"build\\RelWithDebInfo\\hcdeserv.exe -norun -errorlog \"{log_root}\\edge-server-startup.log\" -iwad C:\\Path\\DOOM2.WAD -file \"{result.source_path}\" -nosound -nomusic")
    lines.append("```")
    lines.append("")
    lines.append("## Current Boundary")
    lines.append("")
    lines.append("- Stage 1 is detection and manifest generation.")
    lines.append("- Stage 2 resolves DDF includes into a deterministic load order and reports duplicate definitions, cycles, resets, and missing include targets.")
    lines.append("- Stage 3 separates DDF properties into neutral bridgeable data and manual adapter decisions.")
    lines.append("- Stage 4 writes candidate ANIMDEFS and SNDINFO translations for narrow data-only DDF animation and sound mappings.")
    lines.append("- DDF definitions are not translated into HCDE actors, weapons, specials, sounds, or UI yet.")
    lines.append("- COAL, Lua, RTS, and RadTrig resources are not executed by HCDE.")
    lines.append("- Any future adapter must report includes, resets, and conflicts instead of silently taking the last definition.")
    lines.append("")
    lines.append("## DDF Load Order")
    lines.append("")
    if profile.ddf_load_order:
        for index, entry in enumerate(profile.ddf_load_order, start=1):
            lines.append(f"{index}. `{entry}`")
    else:
        lines.append("No DDF load order was produced.")
    lines.append("")

    if profile.ddf_includes:
        lines.append("## DDF Includes")
        lines.append("")
        lines.append("| Entry | Line | Target | Resolved Entry |")
        lines.append("| --- | ---: | --- | --- |")
        for hint in profile.ddf_includes[:120]:
            lines.append(f"| `{hint.entry}` | {hint.line} | `{hint.target}` | `{hint.resolved_entry or '<unresolved>'}` |")
        if len(profile.ddf_includes) > 120:
            lines.append(f"| ... |  | {len(profile.ddf_includes) - 120} more includes |  |")
        lines.append("")

    if profile.ddf_missing_includes:
        lines.append("Missing includes:")
        for hint in profile.ddf_missing_includes[:80]:
            lines.append(f"- `{hint.entry}:{hint.line}` -> `{hint.target}`")
        if len(profile.ddf_missing_includes) > 80:
            lines.append(f"- ... {len(profile.ddf_missing_includes) - 80} more missing includes")
        lines.append("")

    if profile.ddf_include_cycles:
        lines.append("Include cycles:")
        for cycle in profile.ddf_include_cycles[:40]:
            lines.append(f"- `{' -> '.join(cycle)}`")
        if len(profile.ddf_include_cycles) > 40:
            lines.append(f"- ... {len(profile.ddf_include_cycles) - 40} more cycles")
        lines.append("")

    if profile.ddf_conflicts:
        lines.append("## DDF Definition Conflicts")
        lines.append("")
        lines.append("| Kind | Name | Definitions |")
        lines.append("| --- | --- | --- |")
        for conflict in profile.ddf_conflicts[:100]:
            defs = "<br>".join(f"`{hit.entry}:{hit.line}`" for hit in conflict.definitions[:8])
            if len(conflict.definitions) > 8:
                defs += f"<br>`... {len(conflict.definitions) - 8} more`"
            lines.append(f"| `{conflict.kind}` | `{conflict.name}` | {defs} |")
        if len(profile.ddf_conflicts) > 100:
            lines.append(f"| ... | {len(profile.ddf_conflicts) - 100} more conflicts |  |")
        lines.append("")

    if profile.ddf_manifest_entries:
        lines.append("## Neutral DDF Manifest")
        lines.append("")
        if profile.ddf_manifest_counts:
            counts = ", ".join(f"`{status}`={count}" for status, count in profile.ddf_manifest_counts.items())
            lines.append(f"Manifest status counts: {counts}")
            lines.append("")
        lines.append("| Entry | Line | Kind | Label | Status | Supported Properties | Manual Properties | Duplicate Properties |")
        lines.append("| --- | ---: | --- | --- | --- | --- | --- | --- |")
        for item in profile.ddf_manifest_entries[:120]:
            supported = ", ".join(f"{key}={value}" for key, value in item.supported_properties.items())
            manual = ", ".join(item.manual_properties[:12])
            duplicates = ", ".join(item.duplicate_properties[:12])
            lines.append(
                f"| `{item.entry}` | {item.line} | `{item.kind}` | `{item.label}` | `{item.status}` | "
                f"`{supported}` | `{manual}` | `{duplicates}` |"
            )
        if len(profile.ddf_manifest_entries) > 120:
            lines.append(f"| ... |  |  | {len(profile.ddf_manifest_entries) - 120} more entries |  |  |  |  |")
        lines.append("")

    if profile.ddf_manual_property_counts:
        lines.append("Top manual properties:")
        for key, count in list(profile.ddf_manual_property_counts.items())[:40]:
            lines.append(f"- `{key}`: `{count}` section(s)")
        lines.append("")

    if profile.ddf_duplicate_property_counts:
        lines.append("Duplicate properties:")
        for key, count in list(profile.ddf_duplicate_property_counts.items())[:40]:
            lines.append(f"- `{key}`: `{count}` section(s)")
        lines.append("")

    lines.append("## DDF Sections")
    lines.append("")
    if profile.ddf_sections:
        lines.append("| Entry | Line | Kind | Section | Parent | Property Keys |")
        lines.append("| --- | ---: | --- | --- | --- | --- |")
        for section in profile.ddf_sections[:100]:
            props = ", ".join(section.property_keys[:12])
            lines.append(
                f"| `{section.entry}` | {section.line} | `{section.kind}` | `{edge_section_label(section)}` | "
                f"`{section.parent or ''}` | `{props}` |"
            )
        if len(profile.ddf_sections) > 100:
            lines.append(f"| ... |  |  | {len(profile.ddf_sections) - 100} more sections |  |  |")
    else:
        lines.append("No bracketed DDF sections were recognized by the scanner.")
    lines.append("")

    if profile.script_entries:
        lines.append("## Scripts")
        lines.append("")
        for script in profile.script_entries[:80]:
            lines.append(f"- `{script.entry}`: `{script.kind}`, `{script.size}` bytes")
        if len(profile.script_entries) > 80:
            lines.append(f"- ... {len(profile.script_entries) - 80} more scripts")
        lines.append("")

    if profile.notes:
        lines.append("Notes:")
        for note in profile.notes:
            lines.append(f"- {note}")
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
        against a real HCDE startup log. If it detected EDGE Classic resources,
        read `edge_validation.md`, `edge_manifest.json`, and `edge_translation/`
        before designing any DDF/Lua/COAL adapter, then use
        `edge_stage5_validation.md` for runtime smoke checks.
        """
    )


def render_edge_manifest(result: ScanResult) -> str:
    profile = result.edge
    if profile is None:
        manifest: object = {
            "stage": "edge-classic-stage4",
            "source_path": result.source_path,
            "sha256": result.sha256,
            "entries": [],
        }
    else:
        manifest = {
            "stage": "edge-classic-stage4",
            "source_path": result.source_path,
            "sha256": result.sha256,
            "ddf_load_order": profile.ddf_load_order,
            "status_counts": profile.ddf_manifest_counts,
            "manual_property_counts": profile.ddf_manual_property_counts,
            "duplicate_property_counts": profile.ddf_duplicate_property_counts,
            "translation_summary": asdict(profile.translation_summary),
            "entries": [asdict(entry) for entry in profile.ddf_manifest_entries],
        }
    return json.dumps(manifest, indent=2, sort_keys=True) + "\n"


def render_edge_animdefs(result: ScanResult) -> str:
    profile = result.edge
    lines = [
        "// HCDE EDGE Classic candidate ANIMDEFS translation.",
        "// Generated from neutral manifest data only; review before moving into wadsrc_mod_compat.",
        f"// Source SHA-256: {result.sha256}",
        "",
    ]
    if profile is None:
        return "\n".join(lines + ["// No EDGE profile was generated.", ""])

    emitted = 0
    for item in profile.ddf_manifest_entries:
        if item.kind != "animations":
            continue
        line = edge_animdefs_line(item)
        if line is None:
            continue
        lines.append(f"// {item.entry}:{item.line} [{item.label}]")
        lines.append(line)
        lines.append("")
        emitted += 1
    if emitted == 0:
        lines.append("// No safe animation translations were found.")
        lines.append("")
    return "\n".join(lines)


def render_edge_sndinfo(result: ScanResult) -> str:
    profile = result.edge
    lines = [
        "// HCDE EDGE Classic candidate SNDINFO translation.",
        "// Generated from neutral manifest data only; review before moving into wadsrc_mod_compat.",
        f"// Source SHA-256: {result.sha256}",
        "",
    ]
    if profile is None:
        return "\n".join(lines + ["// No EDGE profile was generated.", ""])

    emitted = 0
    for item in profile.ddf_manifest_entries:
        if item.kind != "sounds":
            continue
        line = edge_sndinfo_line(item)
        if line is None:
            continue
        lines.append(f"// {item.entry}:{item.line} [{item.label}]")
        lines.append(line)
        lines.append("")
        emitted += 1
    if emitted == 0:
        lines.append("// No safe sound translations were found.")
        lines.append("")
    return "\n".join(lines)


def render_edge_stage5_validation(result: ScanResult, out_dir: Path | None = None) -> str:
    profile = result.edge
    log_root = str(out_dir) if out_dir is not None else f"build\\compat-candidates\\{result.slug}"
    translation_path = f"{log_root}\\edge_translation"
    lines: list[str] = []
    lines.append(f"# HCDE EDGE Classic Stage 5 Validation: {result.slug}")
    lines.append("")
    lines.append("Stage 5 validates the generated Stage 4 candidate translations with HCDE startup logs. It does not enable translations automatically.")
    lines.append("")
    lines.append("## Runtime Commands")
    lines.append("")
    lines.append("Use your local IWAD path in place of `C:\\Path\\DOOM2.WAD` when needed.")
    lines.append("")
    lines.append("```powershell")
    lines.append(f"build\\RelWithDebInfo\\hcde.exe -norun -errorlog \"{log_root}\\stage5-client-translations.log\" -iwad C:\\Path\\DOOM2.WAD -file \"{translation_path}\"")
    lines.append(f"build\\RelWithDebInfo\\hcdeserv.exe -norun -errorlog \"{log_root}\\stage5-server-translations.log\" -iwad C:\\Path\\DOOM2.WAD -file \"{translation_path}\" -nosound -nomusic")
    lines.append("```")
    lines.append("")
    lines.append("## Validation Criteria")
    lines.append("")
    lines.append("- HCDE should load `edge_translation` as resource data without fatal script errors.")
    lines.append("- The logs should show the translation folder being added.")
    lines.append("- Treat `Script error`, `Unknown texture`, `Unknown flat`, `Unknown sound`, `Can't find`, and `Fatal` as blockers.")
    lines.append("- DDF animation translations are asset-sensitive. If the selected IWAD or mod does not contain every texture/flat in a range, split or filter the candidate before merging.")
    lines.append("- SNDINFO translations are also asset-sensitive, but missing sound lumps are usually easier to triage by checking the source WAD/PK3 lump list.")
    lines.append("")
    lines.append("## Candidate Output Counts")
    lines.append("")
    if profile is None:
        lines.append("- No EDGE Classic profile was generated.")
    else:
        lines.append(f"- ANIMDEFS candidates: `{profile.translation_summary.animdefs_entries}`")
        lines.append(f"- SNDINFO candidates: `{profile.translation_summary.sndinfo_entries}`")
        lines.append(f"- Skipped animation/sound entries: `{profile.translation_summary.skipped_entries}`")
        if profile.ddf_manual_property_counts:
            manual_preview = ", ".join(
                f"{key}={count}" for key, count in list(profile.ddf_manual_property_counts.items())[:12]
            )
            lines.append(f"- Top manual properties still needing adapter work: `{manual_preview}`")
        if profile.ddf_duplicate_property_counts:
            duplicate_preview = ", ".join(
                f"{key}={count}" for key, count in list(profile.ddf_duplicate_property_counts.items())[:12]
            )
            lines.append(f"- Duplicate DDF properties needing adapter decisions: `{duplicate_preview}`")
    lines.append("")
    lines.append("## Result Template")
    lines.append("")
    lines.append("| Command | Result | Notes |")
    lines.append("| --- | --- | --- |")
    lines.append("| Client translation startup | pending |  |")
    lines.append("| Server translation startup | pending |  |")
    lines.append("")
    lines.append("## Next Decisions")
    lines.append("")
    lines.append("- If startup passes, copy only the reviewed candidate lines into `wadsrc_mod_compat` or a split compat PK3.")
    lines.append("- If startup reports missing textures/flats, filter the ANIMDEFS candidate by IWAD/mod assets before merging.")
    lines.append("- If startup reports missing sounds, confirm whether the sound lump is shipped by the mod or should remain a manual compatibility note.")
    lines.append("- Keep DDF things, weapons, attacks, line specials, sectors, COAL, Lua, RTS, and RadTrig out of runtime translation until their adapter rules are explicit.")
    lines.append("")
    return "\n".join(lines)


def write_candidate(result: ScanResult, out_root: Path, source_path: Path | None = None, auto_attempt: bool = False) -> Path:
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
    if result.edge is not None:
        edge_translation_dir = out_dir / "edge_translation"
        edge_translation_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "edge_validation.md").write_text(
            render_edge_validation(result, out_dir),
            encoding="utf-8",
            newline="\n",
        )
        (out_dir / "edge_manifest.json").write_text(
            render_edge_manifest(result),
            encoding="utf-8",
            newline="\n",
        )
        (edge_translation_dir / "ANIMDEFS.txt").write_text(
            render_edge_animdefs(result),
            encoding="utf-8",
            newline="\n",
        )
        (edge_translation_dir / "SNDINFO.txt").write_text(
            render_edge_sndinfo(result),
            encoding="utf-8",
            newline="\n",
        )
        (out_dir / "edge_stage5_validation.md").write_text(
            render_edge_stage5_validation(result, out_dir),
            encoding="utf-8",
            newline="\n",
        )
    (out_dir / "suggested_hcde_mod_compat_entry.cpp.txt").write_text(
        render_engine_entry(result),
        encoding="utf-8",
        newline="\n",
    )
    (static_dir / "decorate.txt").write_text(render_decorate_stub(result), encoding="utf-8", newline="\n")
    handled_check_ids: set[str] = set()
    if auto_attempt and source_path is not None:
        notes, auto_handled = write_auto_patch_attempt(result, source_path, out_dir)
        if notes:
            (out_dir / "auto_patch_attempt.md").write_text(
                "\n".join(f"- {line}" for line in notes) + "\n",
                encoding="utf-8",
                newline="\n",
            )
        handled_check_ids = set(auto_handled)
    write_hcde_engine_change_report(result, out_dir, handled_check_ids)
    return out_dir


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Scan local Doom mods and generate reviewable HCDE compat-mod patch candidates."
    )
    parser.add_argument("mods", nargs="+", type=Path, help="Mod archives or unpacked folders to scan (.wad, .pk3, .zip, directory).")
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("build") / "compat-candidates",
        help="Output directory for generated candidate folders.",
    )
    parser.add_argument(
        "--auto-attempt",
        action="store_true",
        help="Generate a local auto-attempt patched PK3 copy for safe heuristics (review output only).",
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
            outputs.append(write_candidate(result, args.out, source_path=mod, auto_attempt=args.auto_attempt))
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
