# Changelog

## 1.2.0

- Treasure instead of septims: the gold of Nordic barrows (boss chests, dragon priests, draugr, the 20 coins of ordinary chests, loose change in chests and alcove corpses) and of Dwemer ruins becomes silver and gold ingots, ore and gems (Dwarven metal in the ruins) of the same average value, at the same levels. Solstheim's boss chests can give stalhrim. To keep septims, remove the gold replacements in the config.
- Barrow chests draw their arrows from `LootDraugrArrows15` / `LootDraugrArrows100`, missed since 1.0: they now give draugr arrows instead of iron and steel ones.
- Solstheim's Dwemer ruins (Dragonborn's `DLC2LootDwarven*` lists) are covered by the Dwemer family.
- Created lists accept treasure (misc items), a count per piece, and `"pick": "highest"` (one piece among the highest level reached, like Bethesda's boss gold).

## 1.1.0

- Ancient Nord and Dwarven weapons that a mod puts in the generic lists (Reforging - To the Masses does) now stay in tombs: weapons with an era material keyword found in the replaced lists are added to the era list (a copy made at launch; the era list carried by draugr is not changed). `"era_keywords"` in each family of the config; `[]` turns it off.
- Checked against the Address Library databases of SE 1.5.97 and 1.7.99: every address the plugin uses exists there.

## 1.0.0

First release.

- Nordic barrows: generic weapons, armor, arrows, enchanted gear and special loot become Ancient Nord (Bethesda's draugr lists, Ancient Nord armor lists made at launch, enchanted draugr boss weapons). Nordic carved gear is removed; stalhrim outside Solstheim becomes the draugr lords' gear.
- Solstheim's barrows: the same, plus stalhrim at level 38.
- Dwemer ruins: generic gear becomes Dwarven, enchanted gear becomes enchanted Dwarven gear.
- Ebony at level 38 in Skyrim's barrows, skipped when Open World Loot or MorrowLoot Ultimate is loaded (they ban random ebony).
- Supports Ancient Nord Armoury EXTREME (its pieces and enchanted pieces are used; its mixed lists are replaced in tombs only), MorrowLoot Ultimate (Ancient Nord Hero shield), Prvti's Dragon Cult Weapons (sub-lists found by themselves).
- Chests and mod sub-lists used only by tombs are found by themselves; lists shared with the rest of the world are never touched.
- Short report in the log; detailed survey with `"survey": "full"`.
