# HCDE Personal Extras

HCDE can stage local personal extras without importing their binary assets into
the GPL core data packages.

## Boko Announcer And RBC Skins

The current optional extra set uses:

- `\\192.168.1.2\Main_Data\WADS\BokoAnnouncerV3.zip`
- `\\192.168.1.2\Main_Data\WADS\RBCSkins12_NOSTDN.pk3`

The staging helper extracts `BokoAnnouncerV3.wad` from the announcer ZIP and
copies the full RBC skin pack. The skin pack includes `Boko the Chocobo` and
`Chobi the Chocobo`, plus the rest of the bundled `SKININFO` skins.

## Stage Manually

```powershell
python tools\stage_hcde_personal_extras.py --output build\RelWithDebInfo\extras
```

## Stage During A CMake Build

```powershell
cmake -S . -B build -DHCDE_STAGE_PERSONAL_EXTRAS=ON
cmake --build build --config RelWithDebInfo --target hcde_personal_extras
```

The source paths can be overridden with:

```powershell
-DHCDE_BOKO_ANNOUNCER_ZIP=C:\Path\BokoAnnouncerV3.zip
-DHCDE_BOKO_SKIN_PK3=C:\Path\RBCSkins12_NOSTDN.pk3
```

## Loading

Manual load:

```powershell
build\RelWithDebInfo\hcde.exe -file build\RelWithDebInfo\extras\BokoAnnouncerV3.wad build\RelWithDebInfo\extras\RBCSkins12_NOSTDN.pk3 +cl_stannounce 1
```

Skin autoload:

```text
skins\RBCSkins12_NOSTDN.pk3
```

HCDE now scans both `.wad` and `.pk3` files in the skins directory.

## Support Notes

- `SKININFO` skin packs are recognized and converted into HCDE player skins at
  startup.
- The `dstaunt` skin sound key is mapped to the player taunt sound.
- `cl_stannounce` enables the Skulltag/Zandronum `ANCRINFO` bridge for common
  deathmatch announcements: fight, first frag, sprees, spree loss, and
  multikills.
- CTF/team/rune medal announcements need separate game-mode hooks before HCDE
  can trigger them automatically.

## Golden Rules

These assets are not part of `hcde.pk3`, `game_support.pk3`, or
`hcde_mod_compat.pk3`. Keep them staged as optional extras unless their
provenance and redistribution terms are audited.
