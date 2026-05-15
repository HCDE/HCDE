import json
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

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
                zf.writestr(
                    "DDFTHING",
                    "#VERSION 1.29\n#INCLUDE \"ddf/weapons.ddf\"\n[IMP:BASE]\nHEALTH = 60;\nSTATES(IDLE)=TROO:A:10:NORMAL:CHASE,\n             TROO:B:10:NORMAL:CHASE;\n",
                )
                zf.writestr("ddf/weapons.ddf", "[PISTOL]\nAMMOTYPE = BULLETS;\n")
                zf.writestr("ddf/anims.ddf", "[NUKAGE1]\nTYPE=FLAT;\nFIRST=\"NUKAGE1\";\nLAST=\"NUKAGE3\";\nSPEED=8T;\n")
                zf.writestr("ddf/sounds.ddf", "[PISTOL]\nLUMP_NAME=\"DSPISTOL\";\nPRIORITY=64;\nPRIORITY=80;\n")
                zf.writestr("scripts/test.rts", "START_MAP MAP01\n")
                zf.writestr("coal/hud.coal", "function draw_all() {}\n")

            result = patcher.scan_mod(mod)
            out = patcher.write_candidate(result, root / "out")

            self.assertIn("edge_ddf", result.surfaces)
            self.assertIn("edge_scripts", result.surfaces)
            self.assertIsNotNone(result.edge)
            imp_section = next(section for section in result.edge.ddf_sections if section.name == "IMP")
            self.assertEqual(imp_section.parent, "BASE")
            self.assertIn("health", imp_section.property_keys)
            self.assertIn("states", imp_section.property_keys)
            self.assertNotIn("troo", imp_section.property_keys)
            self.assertEqual(result.edge.ddf_includes[0].target, "ddf/weapons.ddf")
            self.assertEqual(result.edge.ddf_includes[0].resolved_entry, "ddf/weapons.ddf")
            self.assertLess(result.edge.ddf_load_order.index("ddf/weapons.ddf"), result.edge.ddf_load_order.index("DDFTHING"))
            self.assertEqual({script.kind for script in result.edge.script_entries}, {"coal", "rts"})
            self.assertTrue(result.edge.stage_flags["stage2_include_order"])
            self.assertTrue(result.edge.stage_flags["stage3_neutral_manifest"])
            imp_manifest = next(item for item in result.edge.ddf_manifest_entries if item.kind == "things" and item.label == "IMP:BASE")
            self.assertEqual(imp_manifest.status, "partial")
            self.assertEqual(imp_manifest.supported_properties["health"], "60")
            self.assertIn("states", imp_manifest.manual_properties)
            pistol_manifest = next(item for item in result.edge.ddf_manifest_entries if item.kind == "weapons" and item.label == "PISTOL")
            self.assertEqual(pistol_manifest.status, "bridgeable")
            self.assertEqual(pistol_manifest.supported_properties["ammotype"], "BULLETS")
            pistol_sound_manifest = next(item for item in result.edge.ddf_manifest_entries if item.kind == "sounds" and item.label == "PISTOL")
            self.assertEqual(pistol_sound_manifest.status, "partial")
            self.assertEqual(pistol_sound_manifest.supported_properties["priority"], "80")
            self.assertIn("priority", pistol_sound_manifest.duplicate_properties)
            self.assertEqual(result.edge.ddf_duplicate_property_counts["priority"], 1)
            self.assertEqual(result.edge.translation_summary.animdefs_entries, 1)
            self.assertEqual(result.edge.translation_summary.sndinfo_entries, 1)
            self.assertTrue(any(check.id == "edge-classic-compat-surface" for check in result.checks))
            self.assertTrue(any(check.id == "edge-ddf-sections" for check in result.checks))
            self.assertTrue(any(check.id == "edge-ddf-neutral-manifest" for check in result.checks))
            self.assertTrue(any(check.id == "edge-ddf-manual-properties" for check in result.checks))
            self.assertTrue(any(check.id == "edge-ddf-duplicate-properties" for check in result.checks))
            self.assertTrue(any(check.id == "edge-ddf-candidate-translations" for check in result.checks))
            self.assertTrue(any(check.id == "edge-script-runtime-needed" for check in result.checks))
            self.assertTrue((out / "edge_validation.md").exists())
            self.assertTrue((out / "edge_manifest.json").exists())
            self.assertTrue((out / "edge_translation" / "ANIMDEFS.txt").exists())
            self.assertTrue((out / "edge_translation" / "SNDINFO.txt").exists())
            self.assertTrue((out / "edge_stage5_validation.md").exists())
            animdefs = (out / "edge_translation" / "ANIMDEFS.txt").read_text(encoding="utf-8")
            sndinfo = (out / "edge_translation" / "SNDINFO.txt").read_text(encoding="utf-8")
            validation = (out / "edge_stage5_validation.md").read_text(encoding="utf-8")
            self.assertIn("flat NUKAGE1 range NUKAGE3 tics 8", animdefs)
            self.assertIn("PISTOL DSPISTOL", sndinfo)
            self.assertIn("stage5-client-translations.log", validation)
            self.assertIn("Unknown texture", validation)
            manifest = json.loads((out / "edge_manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["stage"], "edge-classic-stage4")
            self.assertEqual(manifest["translation_summary"]["animdefs_entries"], 1)
            self.assertEqual(manifest["translation_summary"]["sndinfo_entries"], 1)
            self.assertEqual(manifest["duplicate_property_counts"]["priority"], 1)
            self.assertTrue(manifest["entries"])
            report = (out / "report.md").read_text(encoding="utf-8")
            self.assertIn("EDGE Classic Profile", report)
            self.assertIn("Duplicate DDF properties", report)

    def test_zip_scan_reads_embedded_wad_text_lumps(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            inner = root / "InnerEdge.wad"
            write_test_wad(inner, [("DDFTHING", b"[ALIEN]\nHEALTH = 90;\n")])

            mod = root / "EmbeddedEdge.pk3"
            with zipfile.ZipFile(mod, "w") as zf:
                zf.writestr("embedded/InnerEdge.wad", inner.read_bytes())

            result = patcher.scan_mod(mod)

            self.assertIn("edge_ddf", result.surfaces)
            self.assertIn("embedded/InnerEdge.wad:DDFTHING", result.surfaces["edge_ddf"])
            self.assertIsNotNone(result.edge)
            self.assertEqual(result.edge.ddf_sections[0].entry, "embedded/InnerEdge.wad:DDFTHING")
            self.assertEqual(result.edge.ddf_sections[0].name, "ALIEN")

    def test_zip_scan_skips_oversized_embedded_wads(self) -> None:
        old_limit = patcher.MAX_EMBEDDED_WAD_BYTES
        patcher.MAX_EMBEDDED_WAD_BYTES = 8
        try:
            with tempfile.TemporaryDirectory() as td:
                root = Path(td)
                mod = root / "LargeEmbedded.pk3"
                with zipfile.ZipFile(mod, "w") as zf:
                    zf.writestr("large.wad", b"not-a-real-wad-but-large")

                result = patcher.scan_mod(mod)

                self.assertTrue(any("Skipped large embedded WAD candidate large.wad" in warning for warning in result.warnings))
                self.assertNotIn("edge_ddf", result.surfaces)
        finally:
            patcher.MAX_EMBEDDED_WAD_BYTES = old_limit

    def test_directory_scan_detects_loose_edge_ddf_files(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            ddf = root / "doom_ddf"
            coal = root / "coal"
            ddf.mkdir()
            coal.mkdir()
            (ddf / "things.ddf").write_text("[SOLDIER]\nHEALTH = 20;\n", encoding="utf-8")
            (ddf / "weapons.ddf").write_text("[SHOTGUN]\nAMMOTYPE = SHELLS;\n", encoding="utf-8")
            (ddf / "rscript.rts").write_text("START_MAP MAP01\n", encoding="utf-8")
            (coal / "hud.coal").write_text("function draw_all() {}\n", encoding="utf-8")

            result = patcher.scan_mod(root)

            self.assertEqual(result.archive_type, "directory")
            self.assertIn("edge_ddf", result.surfaces)
            self.assertIn("edge_scripts", result.surfaces)
            self.assertIsNotNone(result.edge)
            self.assertEqual(result.edge.ddf_entry_kinds["doom_ddf/things.ddf"], "things")
            self.assertEqual(result.edge.ddf_entry_kinds["doom_ddf/weapons.ddf"], "weapons")
            self.assertEqual({section.kind for section in result.edge.ddf_sections}, {"things", "weapons"})
            self.assertEqual({script.kind for script in result.edge.script_entries}, {"coal", "rts"})

    def test_edge_stage2_reports_order_conflicts_and_bad_includes(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            mod = root / "EdgeStage2.pk3"
            with zipfile.ZipFile(mod, "w") as zf:
                zf.writestr(
                    "ddf/things.ddf",
                    "#INCLUDE \"extra/things.ddf\"\n#INCLUDE \"missing.ddf\"\n[IMP]\nHEALTH = 60;\n",
                )
                zf.writestr("ddf/extra/things.ddf", "#CLEARALL\n[IMP]\nHEALTH = 20;\n")
                zf.writestr("ddf/cycle_a.ddf", "#INCLUDE \"cycle_b.ddf\"\n[CYCLE_A]\n")
                zf.writestr("ddf/cycle_b.ddf", "#INCLUDE \"cycle_a.ddf\"\n[CYCLE_B]\n")

            result = patcher.scan_mod(mod)
            out = patcher.write_candidate(result, root / "out")

            self.assertIsNotNone(result.edge)
            edge = result.edge
            include = next(hint for hint in edge.ddf_includes if hint.target == "extra/things.ddf")
            self.assertEqual(include.resolved_entry, "ddf/extra/things.ddf")
            self.assertEqual(edge.ddf_missing_includes[0].target, "missing.ddf")
            self.assertTrue(edge.ddf_include_cycles)
            self.assertEqual(edge.ddf_conflicts[0].kind, "things")
            self.assertEqual(edge.ddf_conflicts[0].name, "imp")
            self.assertLess(edge.ddf_load_order.index("ddf/extra/things.ddf"), edge.ddf_load_order.index("ddf/things.ddf"))
            self.assertTrue(any(check.id == "edge-ddf-missing-includes" for check in result.checks))
            self.assertTrue(any(check.id == "edge-ddf-include-cycles" for check in result.checks))
            self.assertTrue(any(check.id == "edge-ddf-definition-conflicts" for check in result.checks))
            validation = (out / "edge_validation.md").read_text(encoding="utf-8")
            self.assertIn("DDF Definition Conflicts", validation)
            self.assertIn("missing.ddf", validation)


if __name__ == "__main__":
    unittest.main()
