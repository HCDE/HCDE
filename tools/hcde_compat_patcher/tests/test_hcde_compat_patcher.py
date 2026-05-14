import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import hcde_compat_patcher as patcher


def write_test_wad(path: Path, lumps: list[tuple[str, bytes]]) -> None:
    data_offset = 12
    payload = bytearray()
    directory = bytearray()

    positions: list[tuple[int, int, str]] = []
    for name, data in lumps:
        pos = data_offset + len(payload)
        payload.extend(data)
        positions.append((pos, len(data), name))

    directory_offset = data_offset + len(payload)
    for pos, size, name in positions:
        raw_name = name.encode("ascii")[:8].ljust(8, b"\x00")
        directory.extend(struct.pack("<ii8s", pos, size, raw_name))

    with path.open("wb") as fh:
        fh.write(struct.pack("<4sii", b"PWAD", len(lumps), directory_offset))
        fh.write(payload)
        fh.write(directory)


class HcdeCompatPatcherTests(unittest.TestCase):
    def test_wad_scan_detects_decorate_railattack(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            wad = Path(td) / "RailTest.wad"
            write_test_wad(
                wad,
                [
                    (
                        "DECORATE",
                        b"actor TestRail : Weapon replaces RailGun\n{\nStates\n{\nFire:\nTNT1 A 0 A_RailAttack(128)\n}\n}\n",
                    )
                ],
            )

            result = patcher.scan_mod(wad)

            self.assertEqual(result.archive_type, "PWAD")
            self.assertIn("decorate", result.surfaces)
            self.assertTrue(any(check.id == "decorate-railattack" for check in result.checks))
            self.assertTrue(any(check.id == "actor-replacements" for check in result.checks))

    def test_zip_scan_detects_zscript_and_writes_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            mod = root / "Scripted.pk3"
            with zipfile.ZipFile(mod, "w") as zf:
                zf.writestr("zscript/main.zs", "class Example : Actor {}\n")
                zf.writestr("MAPINFO", "map MAP01 \"Entry\" {}\n")

            result = patcher.scan_mod(mod)
            out = patcher.write_candidate(result, root / "out")

            self.assertEqual(result.archive_type, "zip")
            self.assertIn("zscript", result.surfaces)
            self.assertTrue(any(check.id == "zscript-present" for check in result.checks))
            self.assertTrue((out / "report.md").exists())
            self.assertTrue((out / "static" / "decorate.txt").exists())

    def test_zip_scan_detects_eternity_surfaces(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            mod = root / "EternityStyle.pk3"
            with zipfile.ZipFile(mod, "w") as zf:
                zf.writestr("EDFROOT", "include(\"actors.edf\")\n")
                zf.writestr(
                    "actors.edf",
                    "thingtype StageFiveImp : DoomImp\n{\n  doomednum = 54321\n  compatname = DoomImp\n  health = 60\n}\n",
                )
                zf.writestr("maps/MAP01/extradata.txt", "linedef { recordnum = 1 }\n")
                zf.writestr("EMAPINFO", "[MAP01]\nextradata = \"maps/MAP01/extradata.txt\"\n")

            result = patcher.scan_mod(mod)
            out = patcher.write_candidate(result, root / "out")

            self.assertIn("eternity_edf", result.surfaces)
            self.assertIn("eternity_extradata", result.surfaces)
            self.assertIn("eternity_mapinfo", result.surfaces)
            self.assertIsNotNone(result.eternity)
            self.assertEqual(result.eternity.emapinfo_maps[0].map_name, "MAP01")
            self.assertEqual(result.eternity.edf_things[0].name, "StageFiveImp")
            self.assertIn("health", result.eternity.edf_things[0].behavior_properties)
            self.assertTrue(any(check.id == "eternity-compat-surface" for check in result.checks))
            self.assertTrue(any(check.id == "eternity-edf-thingtypes" for check in result.checks))
            self.assertTrue(any(check.id == "eternity-edf-behavior-patches" for check in result.checks))
            self.assertTrue((out / "eternity_validation.md").exists())
            validation = (out / "eternity_validation.md").read_text(encoding="utf-8")
            self.assertIn('-errorlog "', validation)

    def test_zip_scan_detects_edge_classic_surfaces(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            mod = root / "EdgeStyle.pk3"
            with zipfile.ZipFile(mod, "w") as zf:
                zf.writestr("DDFTHING", "[IMP]\n")
                zf.writestr("scripts/test.rts", "START_MAP MAP01\n")
                zf.writestr("coal/hud.coal", "function draw_all() {}\n")

            result = patcher.scan_mod(mod)

            self.assertIn("edge_ddf", result.surfaces)
            self.assertIn("edge_scripts", result.surfaces)
            self.assertTrue(any(check.id == "edge-classic-compat-surface" for check in result.checks))


if __name__ == "__main__":
    unittest.main()
