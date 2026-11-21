# TombLoot

SKSE plugin for Skyrim Anniversary Edition: era-appropriate loot in ancient tombs. Nordic barrows give Ancient Nord gear instead of iron and steel, Solstheim's barrows add stalhrim, and Dwemer ruins give Dwarven gear. No ESP, no Papyrus script; everything is applied in memory at launch, after every other plugin and loot patch.

Copyright (C) 2026 narziel.

## How it works

At data load (and again at each game load), for each family of tombs (`nordic`, `solstheim`, `dwemer`):

1. The **generic lists** found in the tomb loot lists (`LItemWeaponSword`, `LItemArmorCuirassAny`, `LItemEnchWeaponAny`…) are replaced by an **era list** (`LItemDraugr02Weapon1H`, a list of Ancient Nord armor made at launch, Dwarven lists…). Only the entry's object changes: level, count and chance none stay as the load order set them.
2. The **chests** that hold a tomb list directly are found by themselves, and their special loot (shared with other chests) is replaced in those chests only.
3. **Mod sub-lists** used only by tomb lists and chests, that hold generic lists themselves, are found by themselves too. A list shared with the rest of the world (NPC inventories, death items, outfits, other chests) is never touched.
4. **Era weapons in generic lists**: weapons carrying an era material keyword (`"era_keywords"`, `WeapMaterialDraugr`…) that a mod put in the replaced generic lists are added to a copy of the era list made at launch, so they are not lost in tombs. The era list itself, also carried by draugr, is not changed.
5. **Treasure instead of septims**: the tombs' gold lists, the coins put straight in their chests and their loose change point to lists of ingots, ore and gems of the same average value, made at launch (`ancient_treasure`, `dwemer_treasure`…).
6. The original entries are kept and put back before each new application: a changed config applies cleanly, and removing the mod gives the old loot back.

A short report in `Documents/My Games/Skyrim Special Edition/SKSE/TombLoot.log` tells what each family's chests can give, and the path of any gear of the wrong era.

## Config

`Data/SKSE/Plugins/TombLoot/TombLoot.json`, commented. Forms are written `"Plugin.esp|0xLOCALID"`; a form whose plugin is not loaded is skipped.

## License

TombLoot is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See `COPYING`.

It statically links CommonLibSSE-NG (GPL-3.0-or-later with a Modding Exception), which requires this license, and several MIT/BSD libraries. Their notices are in `THIRD_PARTY_LICENSES.txt`.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

## Building

Requirements: Windows, Visual Studio 2022 (C++ workload), CMake 3.25 or later, and vcpkg with the `VCPKG_ROOT` environment variable set.

1. In `CMakePresets.json`, set `binaryDir` and `FETCHCONTENT_BASE_DIR` to folders on your machine (they point to the author's `G:/Skyrim/_build` folder).
2. Configure and build:
   ```
   cmake --preset vs2022
   cmake --build --preset release
   ```
   CommonLibSSE-NG is downloaded at configure time, pinned to v13.0.0. vcpkg installs the other libraries listed in `vcpkg.json`, pinned by its `builtin-baseline`.
3. Optional:
   - `cmake --build --preset release --target package_mod` builds the release archive.
   - `cmake --build --preset release --target package_source` builds the source archive from the last commit.
   - `-DTOMBLOOT_DEPLOY=ON -DTOMBLOOT_DEPLOY_DIR=<your MO2 mod folder>` copies the DLL and the config to a Mod Organizer 2 mod after each build.

## Layout

```
src/
  main.cpp       SKSE loading, logs, steps at data load and game load
  Config.*       JSON reading
  Forms.*        "Plugin|0xID" references, labels for the log
  Swap.*         lists made at launch, swap of tomb lists and chests, detection, restore
  Survey.*       report in the log (summary or full survey)
config/
  TombLoot.json  default config, installed to Data/SKSE/Plugins/TombLoot/
```

`CONCEPTION.md` (in French) is the design document.
