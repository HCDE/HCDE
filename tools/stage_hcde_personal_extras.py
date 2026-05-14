#!/usr/bin/env python3
"""Stage local personal HCDE extras without importing them into source control."""

from __future__ import annotations

import argparse
import shutil
import zipfile
from pathlib import Path


DEFAULT_ANNOUNCER_ZIP = r"\\192.168.1.2\Main_Data\WADS\BokoAnnouncerV3.zip"
DEFAULT_SKIN_PK3 = r"\\192.168.1.2\Main_Data\WADS\RBCSkins12_NOSTDN.pk3"


def extract_announcer(source: Path, output: Path) -> Path:
    if not source.is_file():
        raise FileNotFoundError(f"announcer archive not found: {source}")

    with zipfile.ZipFile(source) as archive:
        wad_names = [info.filename for info in archive.infolist() if info.filename.lower().endswith(".wad")]
        if not wad_names:
            raise RuntimeError(f"announcer archive contains no WAD: {source}")

        preferred = next((name for name in wad_names if Path(name).name.lower() == "bokoannouncerv3.wad"), wad_names[0])
        destination = output / Path(preferred).name
        destination.write_bytes(archive.read(preferred))
        return destination


def copy_skin_pack(source: Path, output: Path) -> Path:
    if not source.is_file():
        raise FileNotFoundError(f"skin pack not found: {source}")

    destination = output / source.name
    shutil.copy2(source, destination)
    return destination


def write_readme(output: Path, announcer: Path, skin_pack: Path) -> None:
    readme = f"""HCDE Personal Extras

These files are staged from local personal asset archives and are intentionally
kept outside the GPL core data packages.

Files:
- {announcer.name}: Boko Skulltag/Zandronum announcer sounds and ANCRINFO metadata.
- {skin_pack.name}: RBC skin pack, including Boko the Chocobo and Chobi the Chocobo.

Load manually:
  hcde -file extras/{announcer.name} extras/{skin_pack.name} +cl_stannounce 1

Skin autoload:
  Put {skin_pack.name} in the HCDE skins directory to autoload the skins.

Note:
  cl_stannounce enables common Skulltag/Zandronum announcer events. CTF, team,
  and rune medal announcements still need game-mode hooks before HCDE can
  trigger them automatically.
"""
    (output / "README.txt").write_text(readme, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Stage optional local Boko HCDE extras.")
    parser.add_argument("--output", required=True, type=Path, help="Directory where extras should be staged.")
    parser.add_argument("--announcer-zip", default=DEFAULT_ANNOUNCER_ZIP, type=Path, help="Boko announcer ZIP source.")
    parser.add_argument("--skin-pk3", default=DEFAULT_SKIN_PK3, type=Path, help="RBC skin PK3 source.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output = args.output
    output.mkdir(parents=True, exist_ok=True)

    announcer = extract_announcer(args.announcer_zip, output)
    skin_pack = copy_skin_pack(args.skin_pk3, output)
    write_readme(output, announcer, skin_pack)

    print(f"Staged HCDE personal extras in {output}")
    print(f"  {announcer.name}")
    print(f"  {skin_pack.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
