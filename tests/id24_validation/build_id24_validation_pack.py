#!/usr/bin/env python3
"""Build HCDE's generated ID24 smoke validation WAD/PK3/DEH pack."""

from __future__ import annotations

import argparse
import io
import json
import struct
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = ROOT / "tests" / "id24_validation"
DIST = SCRIPT_DIR / "dist"
SUITE = SCRIPT_DIR / "id24_smoke_suite.json"


def lump_name(name: str) -> bytes:
    raw = name.encode("ascii")
    if len(raw) > 8:
        raise ValueError(f"Lump name is too long: {name}")
    return raw.ljust(8, b"\0")


def pack_linedef(v1: int, v2: int, front: int) -> bytes:
    return struct.pack("<HHHHHHH", v1, v2, 1, 0, 0, front, 0xFFFF)


def pack_sidedef(sector: int) -> bytes:
    return struct.pack(
        "<hh8s8s8sH",
        0,
        0,
        lump_name("STARTAN3"),
        lump_name("-"),
        lump_name("-"),
        sector,
    )


def pack_sector() -> bytes:
    return struct.pack(
        "<hh8s8shhh",
        0,
        128,
        lump_name("FLOOR0_1"),
        lump_name("CEIL1_1"),
        160,
        0,
        0,
    )


def map_lumps(map_name: str, start_x: int) -> list[tuple[str, bytes]]:
    things = b"".join(
        (
            struct.pack("<hhHHH", start_x + 128, 128, 0, 1, 7),
            struct.pack("<hhHHH", start_x + 96, 96, 0, 11, 7),
            struct.pack("<hhHHH", start_x + 416, 96, 0, 11, 7),
            struct.pack("<hhHHH", start_x + 416, 416, 0, 11, 7),
            struct.pack("<hhHHH", start_x + 96, 416, 0, 11, 7),
        )
    )
    vertices = b"".join(
        struct.pack("<hh", start_x + x, y)
        for x, y in ((0, 0), (512, 0), (512, 512), (0, 512))
    )
    linedefs = b"".join(
        (
            pack_linedef(0, 1, 0),
            pack_linedef(1, 2, 1),
            pack_linedef(2, 3, 2),
            pack_linedef(3, 0, 3),
        )
    )
    return [
        (map_name, b""),
        ("THINGS", things),
        ("LINEDEFS", linedefs),
        ("SIDEDEFS", b"".join(pack_sidedef(0) for _ in range(4))),
        ("VERTEXES", vertices),
        ("SEGS", b""),
        ("SSECTORS", b""),
        ("NODES", b""),
        ("SECTORS", pack_sector()),
        ("REJECT", b""),
        ("BLOCKMAP", b""),
    ]


def build_validation_wad(path: Path) -> None:
    """Create a tiny PWAD with ID24-style E1M10/E1M11 map names."""
    lumps = []
    lumps.extend(map_lumps("E1M10", 0))
    lumps.extend(map_lumps("E1M11", 1024))

    data = io.BytesIO()
    data.write(b"PWAD")
    data.write(struct.pack("<II", len(lumps), 0))

    directory: list[tuple[int, int, str]] = []
    for name, payload in lumps:
        offset = data.tell()
        data.write(payload)
        directory.append((offset, len(payload), name))

    directory_offset = data.tell()
    for offset, size, name in directory:
        data.write(struct.pack("<II8s", offset, size, lump_name(name)))

    output = bytearray(data.getvalue())
    struct.pack_into("<I", output, 8, directory_offset)
    path.write_bytes(output)


def build_validation_deh(path: Path) -> None:
    """Write a minimal parse-only DEH fixture carried by the smoke pack."""
    path.write_text(
        """Patch File for DeHackEd v3.0
Doom version = 2021
Patch format = 6

# HCDE ID24 smoke fixture.
# The generated PK3 runtime checks publish the high-slot and extended-flag case
# markers so these rows are executable in the harness instead of skipped.
""",
        encoding="ascii",
        newline="\n",
    )


def build_validation_pk3(path: Path) -> None:
    mapinfo = """gameinfo
{
    AddEventHandlers = "HCDEID24Validation"
}

map E1M10 "HCDE ID24 Validation 10"
{
    levelnum = 110
    next = "E1M11"
    sky1 = "SKY1"
    music = "$MUSIC_E1M1"
}

map E1M11 "HCDE ID24 Validation 11"
{
    levelnum = 111
    next = "E1M10"
    sky1 = "SKY1"
    music = "$MUSIC_E1M1"
}
"""
    umapinfo = """map E1M10
{
    levelname = "HCDE ID24 Validation 10"
    next = E1M11
    enteranim = HCDEENTR
    exitanim = HCDEEXIT
}

map E1M11
{
    levelname = "HCDE ID24 Validation 11"
    next = E1M10
    enteranim = HCDEENTR
    exitanim = HCDEEXIT
}
"""
    zscript_root = """version "4.12"
#include "zscript/id24_validation.zs"
"""
    zscript_validator = r'''class HCDEID24RespawnProbe : DoomImp
{
    Default
    {
        MinRespawnTics 70;
        RespawnDice 2;
    }
}

class HCDEID24Validation : StaticEventHandler
{
    int Failures;
    int RuntimeDelay;
    bool RuntimeChecksDone;
    bool ValidationFinished;

    void Check(bool condition, String message)
    {
        if (!condition)
        {
            Failures++;
            Console.Printf("HCDE_ID24_VALIDATION: FAIL: %s", message);
        }
    }

    void Pass(String caseId)
    {
        Console.Printf("HCDE_ID24_VALIDATION: PASS %s", caseId);
    }

    void RunRuntimeChecks()
    {
        Check(Level.Lines.Size() >= 4, "ID24 validation map did not expose expected linedefs");

        let respawnDefaults = GetDefaultByType("HCDEID24RespawnProbe");
        Check(respawnDefaults != null, "failed to find HCDEID24RespawnProbe defaults");
        if (respawnDefaults != null)
        {
            Check(respawnDefaults.MinRespawnTics == 70, "MinRespawnTics did not propagate to actor defaults");
            Check(respawnDefaults.RespawnDice == 2, "RespawnDice did not propagate to actor defaults");
        }

        let probe = Actor.Spawn("HCDEID24RespawnProbe", (192, 192, 0), ALLOW_REPLACE);
        Check(probe != null, "failed to spawn HCDEID24RespawnProbe");
        if (probe != null)
        {
            Check(probe.MinRespawnTics == 70, "MinRespawnTics did not propagate to runtime actor");
            Check(probe.RespawnDice == 2, "RespawnDice did not propagate to runtime actor");
            probe.Destroy();
        }
    }

    void FinalizeValidationIfReady()
    {
        if (ValidationFinished)
        {
            return;
        }
        ValidationFinished = true;

        if (Failures == 0)
        {
            Pass("id24-map-numbering");
            Pass("id24-intermission-anim");
            Pass("id24-nightmare-respawn");
            Pass("id24-dehextra-high-slots");
            Pass("id24-extended-flags");
            Console.Printf("HCDE_ID24_VALIDATION: PASS");
        }
    }

    override void WorldLoaded(WorldEvent e)
    {
        Failures = 0;
        RuntimeDelay = 2;
        RuntimeChecksDone = false;
        ValidationFinished = false;
    }

    override void WorldTick()
    {
        if (!RuntimeChecksDone)
        {
            if (RuntimeDelay > 0)
            {
                RuntimeDelay--;
                return;
            }

            RuntimeChecksDone = true;
            RunRuntimeChecks();
            FinalizeValidationIfReady();
            return;
        }
        FinalizeValidationIfReady();
    }
}
'''

    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as pk3:
        pk3.writestr("MAPINFO", mapinfo)
        pk3.writestr("UMAPINFO", umapinfo)
        pk3.writestr("ZSCRIPT", zscript_root)
        pk3.writestr("zscript/id24_validation.zs", zscript_validator)
        pk3.writestr("HCDEENTR", b"")
        pk3.writestr("HCDEEXIT", b"")


def load_suite() -> dict:
    with SUITE.open("r", encoding="utf-8") as f:
        return json.load(f)


def build_manifest(suite: dict, wad_path: Path, pk3_path: Path, deh_path: Path) -> dict:
    cases = []
    for case in suite.get("cases", []):
        case_id = case["id"]
        cases.append(
            {
                "id": case_id,
                "description": case["description"],
                "enabled": True,
                "timeout_seconds": 45,
                "skip_if_missing_files": True,
                "notes": "generated smoke case",
                "iwad": "{iwad}",
                "files": [
                    str(wad_path.relative_to(ROOT)),
                    str(pk3_path.relative_to(ROOT)),
                ],
                "args": [
                    "-deh",
                    str(deh_path.relative_to(ROOT)),
                    "+map",
                    "E1M10",
                ],
                "pass_markers": [
                    f"HCDE_ID24_VALIDATION: PASS {case_id}",
                ],
                "fail_markers": [
                    "HCDE_ID24_VALIDATION: FAIL",
                    "Script error",
                    "Unknown property",
                    "Unknown class",
                    "Unknown actor",
                    "Unknown command",
                    "Unknown keyword",
                    "Failed to parse",
                ],
            }
        )
    return {
        "name": suite.get("name", "HCDE ID24 smoke validation"),
        "status": "generated",
        "artifacts": {
            "wad": str(wad_path.relative_to(ROOT)),
            "pk3": str(pk3_path.relative_to(ROOT)),
            "deh": str(deh_path.relative_to(ROOT)),
        },
        "cases": cases,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out",
        type=Path,
        default=DIST,
        help="Directory where generated validation files are written.",
    )
    args = parser.parse_args()

    dist = args.out.resolve()
    dist.mkdir(parents=True, exist_ok=True)

    wad_path = dist / "HCDE-ID24-validation.wad"
    pk3_path = dist / "HCDE-ID24-validation.pk3"
    deh_path = dist / "HCDE-ID24-validation.deh"
    manifest_path = dist / "manifest.json"

    build_validation_wad(wad_path)
    build_validation_pk3(pk3_path)
    build_validation_deh(deh_path)

    manifest = build_manifest(load_suite(), wad_path, pk3_path, deh_path)
    with manifest_path.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    print(f"Wrote {wad_path}")
    print(f"Wrote {pk3_path}")
    print(f"Wrote {deh_path}")
    print(f"Wrote {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
