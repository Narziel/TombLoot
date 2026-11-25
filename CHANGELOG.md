# Changelog

## 1.3.0

- Gear laid out by hand: the iron helmet on a barrow altar, the gauntlets on the floor of a Dwemer ruin. Weapons and armor placed by Bethesda (Skyrim, Update, Dawnguard, HearthFires, Dragonborn) in Nordic barrows, dragon priest lairs, Solstheim's barrows and Dwemer ruins become an era piece of the same kind as they load, through the family's replacements: a helmet for a helmet, a sword for a sword, enchanted only if the original was, within the player's level. The piece depends on the object, so it is the same every time; nothing goes in the save, and removing TombLoot brings the original back. Persistent (quest) objects, owned objects and pieces enchanted on their own are left alone, and so are pieces with no era equivalent of their kind. `"placed"` in each family of the config; remove it to turn it off.
- Nordic boss chests: their 10 % chance of an enchanted mage robe (`LootEnchRobes10`) now gives an enchanted piece of the era instead (`@ancient_robes`).
- Addresses checked against SE 1.5.97 and AE 1.6.353 to 1.6.1179 and the 2026 update (1.7.99).

## 1.2.2

- Ruin urns: their loose change (1 to 9 septims, in `LootUrnSmallTreasure30`) becomes silver ore of the same average value. Swapped only inside the urn list: the same gold list stays in burnt corpses and spider cocoons.

## 1.2.1

- C.O.I.N. - Coins of Interesting Nature: when `C.O.I.N.esp` is loaded, the treasure replacements are skipped and tomb gold is left to its ancient coins (`"treasure_unless"`). Era gear is still replaced.
- `"treasure": false` at the top of the config keeps septims in tombs; the gold replacements are marked `"treasure": true`.
- Roleplaying in Skyrim - Loot Overhaul: its barrow and Dwemer chest gold lists (`rr_Loot_Gold_Draugr`, `rr_Loot_Gold_Dwarven`) are tomb lists, so their septims become treasure too. Skipped when it is not loaded.

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
