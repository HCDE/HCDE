# HCDE Mod Compatibility Patches

This folder contains HCDE-owned compatibility shims for specific mods. Keep each
patch in its own source folder and add the PK3 target to `CMakeLists.txt`.

Current organized patches:

| Source folder | Output PK3 | Purpose |
| --- | --- | --- |
| `combined` | `hcde_mod_compat_combined.pk3` | Shared Brutal Doom and The Island compatibility aliases/shims. |
| `pink_valley_eng` | `hcde_mod_compat_pink_valley_eng.pk3` | Pink Valley map transition and event handler compatibility. |
| `armageddon2_test` | `hcde_mod_compat_armageddon2_test.pk3` | Armageddon2 Skulltag invasion spot/resource compatibility. |
| `boa_c31_4` | `hcde_mod_compat_boa_c31_4.pk3` | Blade of Agony ZScript compatibility fixes for dedicated-server parsing. |

The release script packages these PK3s into the separate
`HCDE-<version>-compat-mods.zip` asset and writes a generated `PATCHES.md`
manifest with sizes and SHA-256 hashes. Third-party mod data should not be
checked into this folder or shipped in the compat zip unless its license has
been reviewed.
