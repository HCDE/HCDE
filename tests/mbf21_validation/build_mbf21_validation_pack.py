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
    # Include both a single-player start and several deathmatch starts so the
    # validation map can boot in either path.
    things = b"".join(
        (
            struct.pack("<hhHHH", 128, 128, 0, 1, 7),   # Player 1 start
            struct.pack("<hhHHH", 96, 96, 0, 11, 7),    # Deathmatch start
            struct.pack("<hhHHH", 416, 96, 0, 11, 7),   # Deathmatch start
            struct.pack("<hhHHH", 416, 416, 0, 11, 7),  # Deathmatch start
            struct.pack("<hhHHH", 96, 416, 0, 11, 7),   # Deathmatch start
        )
    )
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


def build_validation_pk3(path: Path, reservedlineflag_enabled: bool) -> None:
    """Create one PK3 variant of the MBF21 validation pack."""
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

Frame 4
Duration = 1
Next frame = 5
Args1 = 2
Args2 = 1
Args3 = 0
Args4 = 0
Args5 = 0

Frame 5
Duration = 1
Next frame = 0
Args1 = 0

[CODEPTR]
Frame 1 = A_WeaponAlert
Frame 2 = A_Spawn
Frame 3 = A_JumpIfFlagsSet
Frame 4 = A_SeekerMissile
Frame 5 = A_Tracer2
"""

    option_lines = [
        "comp_friendlyspawn 1",
        "comp_ledgeblock 1",
        "comp_staylift 1",
    ]
    # The reserved-off variant intentionally omits this key so runtime command
    # line overrides can force compat_reservedlineflag=0 for the alternate case.
    if reservedlineflag_enabled:
        option_lines.insert(0, "comp_reservedlineflag 1")
    options = "\n".join(option_lines) + "\n"

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
    bool ValidationFinished;
    bool PendingProjectileGroupCheck;
    int ProjectileGroupCheckTicks;
    Actor ProjectileGroupTarget;
    int ProjectileGroupTargetHealth;

    void Check(bool condition, String message)
    {
        if (!condition)
        {
            Failures++;
            Console.Printf("HCDE_MBF21_VALIDATION: FAIL: %s", message);
        }
    }

    void FinalizeValidationIfReady()
    {
        if (ValidationFinished || PendingProjectileGroupCheck)
        {
            return;
        }

        if (Failures == 0)
        {
            Console.Printf("HCDE_MBF21_VALIDATION: PASS");
        }
        ValidationFinished = true;
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
        Check(impDefaults.GetInfightingGroup() == 7,
            "DEHACKED infighting group was not applied to DoomImp");
        Check(impDefaults.GetProjectileGroup() == 11,
            "DEHACKED projectile group was not applied to DoomImp");
        Check(impDefaults.GetSplashGroup() == 13,
            "DEHACKED splash group was not applied to DoomImp");

        // Behavior-level check for MBF21 infighting groups: same non-zero group
        // should block target switching.
        let groupedA = Actor.Spawn("DoomImp", (224, 320, 0), ALLOW_REPLACE);
        let groupedB = Actor.Spawn("DoomImp", (288, 320, 0), ALLOW_REPLACE);
        Check(groupedA != null && groupedB != null,
            "failed to spawn DoomImp pair for infighting-group behavior test");
        if (groupedA != null && groupedB != null)
        {
            Check(!groupedA.OkayToSwitchTarget(groupedB),
                "MBF21 infighting group did not block same-group target switching");
        }

        // Runtime steering check for MBF21 alias-backed seeker actions.
        // These verify live behavior (angle correction), not just parser mapping.
        let seekerTarget = Actor.Spawn("DoomImp", (160, 416, 0), ALLOW_REPLACE);
        let tracer2Probe = Actor.Spawn("DoomImpBall", (96, 352, 32), ALLOW_REPLACE);
        let seekerProbe = Actor.Spawn("DoomImpBall", (256, 352, 32), ALLOW_REPLACE);
        Check(seekerTarget != null && tracer2Probe != null && seekerProbe != null,
            "failed to spawn seeker/tracer probes for MBF21 steering checks");
        if (seekerTarget != null && tracer2Probe != null && seekerProbe != null)
        {
            tracer2Probe.tracer = seekerTarget;
            tracer2Probe.Angle = tracer2Probe.AngleTo(seekerTarget) + 45.0;
            double tracer2DeltaBefore = abs(Actor.Normalize180(tracer2Probe.AngleTo(seekerTarget) - tracer2Probe.Angle));
            tracer2Probe.A_Tracer2(19.6875);
            double tracer2DeltaAfter = abs(Actor.Normalize180(tracer2Probe.AngleTo(seekerTarget) - tracer2Probe.Angle));
            Check(tracer2DeltaAfter < tracer2DeltaBefore,
                "A_Tracer2 did not steer a tracer missile toward its target");

            seekerProbe.tracer = seekerTarget;
            seekerProbe.Angle = seekerProbe.AngleTo(seekerTarget) + 45.0;
            double seekerDeltaBefore = abs(Actor.Normalize180(seekerProbe.AngleTo(seekerTarget) - seekerProbe.Angle));
            seekerProbe.A_SeekerMissile(90, 90, 0, 0, 0);
            double seekerDeltaAfter = abs(Actor.Normalize180(seekerProbe.AngleTo(seekerTarget) - seekerProbe.Angle));
            Check(seekerDeltaAfter < seekerDeltaBefore,
                "A_SeekerMissile did not steer a tracer missile toward its target");
        }

        // Behavior-level check for splash groups: same non-zero group should
        // block radius damage from same-group sources.
        let splashSource = Actor.Spawn("DoomImp", (352, 416, 0), ALLOW_REPLACE);
        let splashTarget = Actor.Spawn("DoomImp", (400, 416, 0), ALLOW_REPLACE);
        Check(splashSource != null && splashTarget != null,
            "failed to spawn DoomImp pair for splash-group behavior test");
        if (splashSource != null && splashTarget != null)
        {
            int splashHealthBefore = splashTarget.health;
            splashSource.RadiusAttack(splashSource, 128, 96.0, 'none', RADF_HURTSOURCE, 0.0, "None");
            Check(splashTarget.health == splashHealthBefore,
                "MBF21 splash group immunity failed for same-group actors");
        }

        // Behavior-level check for projectile groups. This needs a few ticks so
        // the spawned missile can travel and resolve collision.
        let projectileSource = Actor.Spawn("DoomImp", (352, 224, 0), ALLOW_REPLACE);
        let projectileTarget = Actor.Spawn("DoomImp", (448, 224, 0), ALLOW_REPLACE);
        Check(projectileSource != null && projectileTarget != null,
            "failed to spawn DoomImp pair for projectile-group behavior test");
        if (projectileSource != null && projectileTarget != null)
        {
            ProjectileGroupTarget = projectileTarget;
            ProjectileGroupTargetHealth = projectileTarget.health;
            PendingProjectileGroupCheck = false;

            let projectile = projectileSource.SpawnMissile(projectileTarget, "DoomImpBall", projectileSource);
            Check(projectile != null,
                "failed to spawn projectile for projectile-group immunity test");
            if (projectile != null)
            {
                PendingProjectileGroupCheck = true;
                ProjectileGroupCheckTicks = 35;
            }
        }

        // Validate friendliness inheritance for HCDE's MBF A_Spawn wrapper.
        let spawner = Actor.Spawn("DoomImp", (160, 160, 0), ALLOW_REPLACE);
        Check(spawner != null,
            "failed to spawn runtime probe DoomImp for A_Spawn friendliness test");
        if (spawner != null)
        {
            spawner.A_SetFriendly(true);
            // Use a clear offset so spawned monster hitboxes do not overlap.
            let [spawnedOk, spawned] = spawner.HCDE_MBFSpawnItem("DoomImp", 96, 0, false, false);
            Check(spawnedOk && spawned != null,
                "HCDE_MBFSpawnItem failed to spawn a child actor");
            if (spawned != null)
            {
                Check(spawned.bFriendly,
                    "HCDE_MBFSpawnItem did not inherit friendliness when comp_friendlyspawn is enabled");
            }
        }

        // Validate dropped-item DEH wiring directly from defaults. Runtime
        // drop spawn depends on death-state timing and is covered elsewhere.
        DropItem dropDef = impDefaults.GetDropItems();
        Check(dropDef != null,
            "DEHACKED dropped item list is missing on DoomImp");
        if (dropDef != null)
        {
            Check(dropDef.Name == "Clip",
                "DEHACKED dropped item on DoomImp was not set to Clip");
            Check(dropDef.Probability == 255,
                "DEHACKED dropped item probability on DoomImp was not 255");
        }
    }

    override void WorldLoaded(WorldEvent e)
    {
        Failures = 0;
        RuntimeDelay = 2;
        RuntimeChecksDone = false;
        ValidationFinished = false;
        PendingProjectileGroupCheck = false;
        ProjectileGroupCheckTicks = 0;
        ProjectileGroupTarget = null;
        ProjectileGroupTargetHealth = 0;

        bool reservedLineMaskEnabled = (Level.compatflags2 & COMPATF2_RESERVEDLINEFLAG) != 0;
        bool expectedReservedLineMaskEnabled = __EXPECT_RESERVED_MASK__;
        Check(reservedLineMaskEnabled == expectedReservedLineMaskEnabled,
            "comp_reservedlineflag mode mismatch for this validation pack");
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
            if (reservedLineMaskEnabled)
            {
                Check((Level.Lines[2].flags2 & Line.ML2_BLOCKLANDMONSTERS) == 0,
                    "reserved line flag failed to mask ML2_BLOCKLANDMONSTERS");
                Check((Level.Lines[2].flags & Line.ML_BLOCK_PLAYERS) == 0,
                    "reserved line flag failed to mask ML_BLOCK_PLAYERS");
            }
            else
            {
                Check((Level.Lines[2].flags2 & Line.ML2_BLOCKLANDMONSTERS) != 0,
                    "ML2_BLOCKLANDMONSTERS was unexpectedly masked when reservedlineflag is disabled");
                Check((Level.Lines[2].flags & Line.ML_BLOCK_PLAYERS) != 0,
                    "ML_BLOCK_PLAYERS was unexpectedly masked when reservedlineflag is disabled");
            }
            Check((Level.Lines[3].flags2 & Line.ML2_BLOCKLANDMONSTERS) != 0,
                "line flag 0x3000 missed ML2_BLOCKLANDMONSTERS");
            Check((Level.Lines[3].flags & Line.ML_BLOCK_PLAYERS) != 0,
                "line flag 0x3000 missed ML_BLOCK_PLAYERS");
        }
    }

    override void WorldTick()
    {
        // Defer runtime checks briefly so map actors and defaults are fully ready.
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

        if (PendingProjectileGroupCheck)
        {
            ProjectileGroupCheckTicks--;
            if (ProjectileGroupCheckTicks > 0)
            {
                return;
            }

            Check(ProjectileGroupTarget != null,
                "projectile-group immunity check lost target actor");
            if (ProjectileGroupTarget != null)
            {
                Check(ProjectileGroupTarget.health == ProjectileGroupTargetHealth,
                    "MBF21 projectile group immunity failed for same-group actors");
            }

            PendingProjectileGroupCheck = false;
            ProjectileGroupTarget = null;
            ProjectileGroupTargetHealth = 0;
            FinalizeValidationIfReady();
            return;
        }
        FinalizeValidationIfReady();
    }
}
'''
    zscript_validator = zscript_validator.replace(
        "__EXPECT_RESERVED_MASK__",
        "true" if reservedlineflag_enabled else "false",
    )

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
    pk3_reserved_off_path = dist / "HCDE-MBF21-validation-reserved-off.pk3"

    build_validation_wad(wad_path)
    build_validation_pk3(pk3_path, reservedlineflag_enabled=True)
    build_validation_pk3(pk3_reserved_off_path, reservedlineflag_enabled=False)

    print(f"Wrote {wad_path}")
    print(f"Wrote {pk3_path}")
    print(f"Wrote {pk3_reserved_off_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
