#!/usr/bin/env python3
"""Build HCDE's generated MBF21 runtime validation WAD and PK3."""

from __future__ import annotations

import argparse
import io
import os
import struct
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DIST = ROOT / "tests" / "mbf21_validation" / "dist"


def lump_name(name: str) -> bytes:
    """Return a Doom directory lump name padded to exactly eight bytes."""
    raw = name.encode("ascii")
    if len(raw) > 8:
        raise ValueError(f"Lump name is too long: {name}")
    return raw.ljust(8, b"\0")


def pack_linedef(v1: int, v2: int, flags: int, front: int) -> bytes:
    """Pack a Doom-format linedef with no special and no back side."""
    return struct.pack("<HHHHHHH", v1, v2, flags, 0, 0, front, 0xFFFF)


def pack_sidedef(sector: int) -> bytes:
    """Pack a plain sidedef that uses stock DOOM2 textures."""
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
    """Pack one ordinary sector large enough to spawn the player."""
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


def build_validation_wad(path: Path) -> None:
    """Create a tiny Doom-format PWAD with MBF21 extended linedef flags."""
    things = struct.pack("<hhHHH", 128, 128, 0, 1, 7)
    vertices = b"".join(
        struct.pack("<hh", x, y)
        for x, y in ((0, 0), (512, 0), (512, 512), (0, 512))
    )

    # Line 0: MBF21 block-land-monsters bit.
    # Line 1: MBF21 block-players bit.
    # Line 2: reserved bit plus both MBF21 bits; should be masked when enabled.
    # Line 3: both MBF21 bits without the reserved bit; should translate.
    linedefs = b"".join(
        (
            pack_linedef(0, 1, 0x1000, 0),
            pack_linedef(1, 2, 0x2000, 1),
            pack_linedef(2, 3, 0x3800, 2),
            pack_linedef(3, 0, 0x3000, 3),
        )
    )

    lumps: list[tuple[str, bytes]] = [
        ("MAP01", b""),
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


def build_validation_pk3(path: Path) -> None:
    """Create the PK3 side of the MBF21 validation pack."""
    dehacked = """Patch File for DeHackEd v3.0
Doom version = 21
Patch format = 6

Thing 12 (HCDE MBF21 Probe)
Dropped item = 64
Infighting group = 7
Projectile group = 11
Splash group = 13
Fast speed = 18
Melee range = 5242880
Rip sound = misc/chat
MBF21 Bits = LOGRAV

Weapon 1 (HCDE MBF21 Weapon Probe)
MBF21 Bits = NOTHRUST

Frame 1
Duration = 1
Next frame = 2
MBF21 Bits = SKILL5FAST

Frame 2
Duration = 1
Next frame = 3
Unknown 1 = 64
Unknown 2 = 0

Frame 3
Duration = 1
Next frame = 0
Args1 = 2
Args2 = 1
Args3 = 1

[CODEPTR]
Frame 1 = A_WeaponAlert
Frame 2 = A_Spawn
Frame 3 = A_JumpIfFlagsSet
"""

    options = """comp_reservedlineflag 1
comp_friendlyspawn 1
comp_ledgeblock 1
comp_staylift 1
"""

    mapinfo = """gameinfo
{
    AddEventHandlers = "HCDEMBF21Validation"
}

map MAP01 "HCDE MBF21 Validation"
{
    levelnum = 1
    next = "MAP01"
    sky1 = "SKY1"
    music = "$MUSIC_RUNNIN"
}
"""

    zscript_root = """version "4.12"
#include "zscript/mbf21_validation.zs"
"""

    zscript_validator = r'''class HCDEMBF21Validation : StaticEventHandler
{
    int Failures;
    int RuntimeDelay;
    bool RuntimeChecksDone;

    void Check(bool condition, String message)
    {
        if (!condition)
        {
            Failures++;
            Console.Printf("HCDE_MBF21_VALIDATION: FAIL: %s", message);
        }
    }

    bool IsNear(Vector3 pos, Actor other, double maxDistance)
    {
        double dx = other.Pos.X - pos.X;
        double dy = other.Pos.Y - pos.Y;
        return dx * dx + dy * dy <= maxDistance * maxDistance;
    }

    int CountNearbyByClass(Vector3 pos, class<Actor> type, double maxDistance)
    {
        int count = 0;
        let it = Level.CreateActorIterator(0, type);
        Actor found;
        while ((found = it.Next()) != null)
        {
            if (IsNear(pos, found, maxDistance))
            {
                count++;
            }
        }
        return count;
    }

    void RunRuntimeChecks()
    {
        // Validate dehacked default propagation on a known stock monster.
        let impDefaults = GetDefaultByType("DoomImp");

        Check(abs(impDefaults.MeleeRange - 60.0) < 0.01,
            "DEHACKED melee range was not applied to DoomImp");
        Check(abs(impDefaults.FastSpeed - 18.0) < 0.01,
            "DEHACKED fast speed was not applied to DoomImp");
        Check(abs(impDefaults.Gravity - 0.125) < 0.0001,
            "DEHACKED MBF21 LOGRAV bit was not applied to DoomImp");

        // Validate friendliness inheritance for HCDE's MBF A_Spawn wrapper.
        let spawner = Actor.Spawn("DoomImp", (160, 160, 0), ALLOW_REPLACE);
        Check(spawner != null,
            "failed to spawn runtime probe DoomImp for A_Spawn friendliness test");
        if (spawner != null)
        {
            spawner.A_SetFriendly(true);
            let [spawnedOk, spawned] = spawner.HCDE_MBFSpawnItem("DoomImp", 32, 0, false, false);
            Check(spawnedOk && spawned != null,
                "HCDE_MBFSpawnItem failed to spawn a child actor");
            if (spawned != null)
            {
                Check(spawned.bFriendly,
                    "HCDE_MBFSpawnItem did not inherit friendliness when comp_friendlyspawn is enabled");
            }
        }

        // Validate dropped-item behavior by forcing a normal death and checking for
        // a nearby clip drop from the patched DoomImp drop table.
        let dropProbe = Actor.Spawn("DoomImp", (256, 256, 0), ALLOW_REPLACE);
        Check(dropProbe != null,
            "failed to spawn runtime probe DoomImp for dropped-item test");
        if (dropProbe != null)
        {
            let dropPos = dropProbe.Pos;
            let clipsBefore = CountNearbyByClass(dropPos, "Clip", 96);
            dropProbe.DamageMobj(null, null, dropProbe.health, "Melee", DMG_FORCED);
            Check(dropProbe.health <= 0,
                "drop probe DoomImp survived forced death in dropped-item test");

            let clipsAfter = CountNearbyByClass(dropPos, "Clip", 96);
            Check(clipsAfter >= clipsBefore + 1,
                "DEHACKED dropped item did not increase nearby Clip count after DoomImp death");
        }
    }

    override void WorldLoaded(WorldEvent e)
    {
        Failures = 0;
        RuntimeDelay = 2;
        RuntimeChecksDone = false;

        Check((Level.compatflags2 & COMPATF2_RESERVEDLINEFLAG) != 0,
            "comp_reservedlineflag did not set COMPATF2_RESERVEDLINEFLAG");
        Check((Level.compatflags2 & COMPATF2_NOFRIENDLYSPAWN) == 0,
            "comp_friendlyspawn left COMPATF2_NOFRIENDLYSPAWN enabled");

        Check(Level.Lines.Size() >= 4,
            "validation map did not expose the expected linedefs");

        if (Level.Lines.Size() >= 4)
        {
            Check((Level.Lines[0].flags2 & Line.ML2_BLOCKLANDMONSTERS) != 0,
                "line flag 0x1000 did not become ML2_BLOCKLANDMONSTERS");
            Check((Level.Lines[1].flags & Line.ML_BLOCK_PLAYERS) != 0,
                "line flag 0x2000 did not become ML_BLOCK_PLAYERS");
            Check((Level.Lines[2].flags2 & Line.ML2_BLOCKLANDMONSTERS) == 0,
                "reserved line flag failed to mask ML2_BLOCKLANDMONSTERS");
            Check((Level.Lines[2].flags & Line.ML_BLOCK_PLAYERS) == 0,
                "reserved line flag failed to mask ML_BLOCK_PLAYERS");
            Check((Level.Lines[3].flags2 & Line.ML2_BLOCKLANDMONSTERS) != 0,
                "line flag 0x3000 missed ML2_BLOCKLANDMONSTERS");
            Check((Level.Lines[3].flags & Line.ML_BLOCK_PLAYERS) != 0,
                "line flag 0x3000 missed ML_BLOCK_PLAYERS");
        }
    }

    override void WorldTick()
    {
        // Defer runtime checks briefly so map actors and defaults are fully ready.
        if (RuntimeChecksDone)
        {
            return;
        }

        if (RuntimeDelay > 0)
        {
            RuntimeDelay--;
            return;
        }

        RuntimeChecksDone = true;
        RunRuntimeChecks();

        if (Failures == 0)
        {
            Console.Printf("HCDE_MBF21_VALIDATION: PASS");
        }
    }
}
'''

    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as pk3:
        pk3.writestr("DEHACKED", dehacked)
        pk3.writestr("OPTIONS", options)
        pk3.writestr("MAPINFO", mapinfo)
        pk3.writestr("ZSCRIPT", zscript_root)
        pk3.writestr("zscript/mbf21_validation.zs", zscript_validator)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dist",
        type=Path,
        default=DIST,
        help="Directory where generated validation files are written.",
    )
    args = parser.parse_args()

    dist = args.dist.resolve()
    os.makedirs(dist, exist_ok=True)

    wad_path = dist / "HCDE-MBF21-validation.wad"
    pk3_path = dist / "HCDE-MBF21-validation.pk3"

    build_validation_wad(wad_path)
    build_validation_pk3(pk3_path)

    print(f"Wrote {wad_path}")
    print(f"Wrote {pk3_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
