# Butin d'époque dans les tombeaux — fiche de conception

Nom de travail : **TombLoot**. Noms possibles pour Nexus : *Sealed Tombs*, *Ancient Hoards*, *Nothing New Under the Barrow*.

Statut : fiche à valider avant tout code (2026-11-20). Les noms et FormID cités ont été relevés directement dans `Skyrim.esm` le 2026-11-20 ; les DLC et les mods restent à relever.

## 1. L'idée

Une crypte nordique scellée depuis des millénaires ne devrait pas contenir une armure de fer forgée l'an dernier. Le mod change l'**époque** du butin des tombeaux, pas sa **quantité** : on ne retire rien, on remplace par ce qui a sa place là.

| Lieu | Butin cohérent |
|---|---|
| Crypte nordique (draugr) | Armes nordiques anciennes (normales, affûtées, enchantées de givre), ébène pour les plus hauts niveaux, bijoux, gemmes, pierres d'âme, parchemins, potions |
| Ruine dwemer | Armes et armures dwemer, lingots et pièces dwemer, bijoux, gemmes |
| Ruche falmer | Voir § 6 : les Falmers pillent des voyageurs d'aujourd'hui, le butin moderne s'y défend |

Les occupants actuels d'une ruine (bandits, nécromanciens, aventuriers morts) gardent leur équipement : ils vivent aujourd'hui. Seuls les **coffres funéraires** et les **morts anciens** changent.

Même principe qu'Every Crumb Counts : SKSE seul, aucun ESP, aucun script, tout en mémoire au lancement, réversible en désinstallant.

### Place à côté des autres outils (2026-11-20)

Ce mod ne remplace ni **Open World Loot** (zones de rencontre, progression, paliers de butin et coffres de boss), ni **DynamicContainerLoot** (fusion des conteneurs au démarrage), ni le **Bashed Patch** (fusion des listes nivelées et du reste). C'est une **couche de cohérence** qui passe après eux, au démarrage :

- il ne touche ni aux niveaux, ni aux quantités, ni aux chances : la progression d'Open World Loot reste intacte ;
- il remplace seulement l'**époque** (ou plus tard la **faction**) d'une entrée générique dans les listes propres à un lieu ;
- il ne dépend pas des tags Wrye Bash ni de l'ordre de chargement : le résultat est le même quel que soit le patch ;
- il doit passer **après** DynamicContainerLoot (les plugins SKSE se chargent par ordre alphabétique de DLL : vérifier que `DynamicContainerLoot.dll` reçoit `kDataLoaded` avant nous).

Plus tard, la même couche peut couvrir d'autres incohérences (coffres de bandits, de Parjures, de Falmers…) et rejoindre Every Crumb Counts dans une famille de mods « butin cohérent ».

## 2. Ce que montre `Skyrim.esm`

### Le défaut vient d'un seul endroit

Les coffres funéraires passent par des **listes enveloppes** propres à leur faction, mais ces enveloppes pointent vers les **listes génériques** de tout le jeu :

```
TreasDraugrChest [020670]
  LootDraugrWeapon15 [068837]   (85 % de chance de rien)
    LItemWeaponSword [016578]   → IronSword, SteelSword, OrcishSword, DwarvenSword, ElvenSword, GlassSword, EbonySword, Daedric…
    LItemWeaponWarAxe, LItemWeaponBattleAxe, LItemWeaponGreatSword, LItemWeaponMace,
    LItemWeaponWarhammer, LItemWeaponDagger, LItemWeaponBow, LItemArrowsAll
  LootDraugrArmor25 [06883B]
    LItemArmorCuirassAny [016739] → fer, acier, plaques, orque, ébène, cuir, elfique, verre, écailles de dragon…
    LItemArmorBootsAny, LItemArmorGauntletsAny, LItemArmorHelmetAny, LItemArmorShieldAny
```

Les coffres dwemer font pareil (`LootDwarvenWeapon15` → les mêmes listes génériques, plus `LItemDwarvenOnlyWeapons`), tout comme les coffres de boss (`LootDraugrChestBossBase`, `LootDraugrChestBossMagicItem25` → `LItemEnchWeaponAny`, `LItemEnchArmorHeavyAll`…).

### Les enveloppes ne servent qu'aux tombeaux

`LootDraugrWeapon15` et `LootDraugrArmor25` ne sont utilisées que par des conteneurs draugr : `TreasDraugrChest`, les dix `TreasDraugrAmbushCorpse*`, trois coffres `DEMODraugrChest*` et `dunGeirmundCorpse`. `LootDwarvenWeapon15` ne sert qu'aux trois coffres dwemer.

**Conséquence : on peut corriger les enveloppes elles-mêmes, sans copie.** Tout conteneur qui demande « du butin draugr » reçoit du butin draugr, y compris les cryptes ajoutées par des mods qui réutilisent ces coffres ou ces listes. C'est plus simple et plus sûr que les copies d'Every Crumb Counts.

### Les listes d'époque existent déjà, sauf pour l'armure nordique

| Besoin | Liste vanilla | Contenu |
|---|---|---|
| Armes nordiques anciennes | `LItemDraugr02Weapon1H` [0242FC], `…2H` [024300], `…WeaponBow` [024302] | épées et haches draugr, normales et affûtées |
| Armes anciennes enchantées | `LItemDraugr02BossWeapon1HMagic` [0DDD62], `…2HMagic` [0DDD63] | `EnchDraugrSwordFrost01`… (givre, trois rangs) |
| Haut niveau (seigneurs draugr) | `LItemDraugr05Weapon1H` [023C10] | `EbonySword`, `EbonyWarAxe` |
| Flèches anciennes | `BaseArrowDraugr75` [037C17] | flèches nordiques anciennes |
| Bouclier | `LItemDraugrShield` [018ECB], `LItemDraugrEbonyShield50` [0559FB] | |
| Armes et armures dwemer | `LItemDwarvenOnlyWeapons` [070C21], `LItemDwarvenOnlyArmor` [070C20] | |
| Armes falmer | `LItemFalmerWeapon` | épées et haches falmer |
| **Armure nordique ancienne** | **aucune** | à créer au lancement (§ 4) |

Bethesda elle-même équipe ses seigneurs draugr en **ébène** : l'ébène est donc « ancien » d'après le jeu. On s'appuie sur ces paliers (02, 04, 05) plutôt que d'inventer une progression.

### Piège : l'armure nordique ancienne porte le mot-clé daedrique

Dans `Skyrim.esm`, les pièces `ArmorDraugrCuirass` [018388], `ArmorDraugrBoots` [056A9D], `ArmorDraugrGauntlets` [056B17], `ArmorDraugrHelmet` [056A9E] portent **`ArmorMaterialDaedric`**. Pour les armures, le mot-clé de matériau ne suffit donc pas à reconnaître l'époque : il faut une liste d'objets explicite. Pour les armes, il marche (`WeapMaterialDraugr` [0C5C01], `WeapMaterialDraugrHoned` [0C5C02]). À vérifier : un correctif comme l'USSEP change peut-être ce mot-clé (le mod lit les valeurs finales, après tous les plugins).

## 3. Ce que fait le mod

Au chargement des données (`kDataLoaded`), et de nouveau à chaque chargement de partie (comme Every Crumb Counts) :

1. **Repérer les enveloppes à corriger.** Liste par défaut dans la config (enveloppes `LootDraugr*`, `LootDwarven*` et coffres de boss relevés au § 2), plus détection automatique : toute liste atteinte depuis un coffre funéraire connu (`TreasDraugrChest*`, `TreasDwarvenChest*`, `TreasDraugrAmbushCorpse*`).
2. **Remplacer les entrées génériques par leur équivalent d'époque.** Table de correspondance dans la config :

   | Entrée générique | Crypte nordique | Ruine dwemer |
   |---|---|---|
   | `LItemWeaponSword`, `…WarAxe`, `…Mace`, `…Dagger` | `LItemDraugr02Weapon1H` | `LItemDwarvenOnlyWeapons` |
   | `LItemWeaponBattleAxe`, `…GreatSword`, `…Warhammer` | `LItemDraugr02Weapon2H` | `LItemDwarvenOnlyWeapons` |
   | `LItemWeaponBow` | `LItemDraugr02WeaponBow` | `LItemDwarvenOnlyWeapons` |
   | `LItemArrowsAll` | `BaseArrowDraugr75` | `BaseArrowDwarven75` |
   | `LItemArmor*Any` | liste nordique ancienne créée (§ 4) | `LItemDwarvenOnlyArmor` |
   | `LItemEnchWeaponAny` | `LItemDraugr02BossWeapon1HMagic` / `2HMagic` | `LItemEnchDwarven*` (armes dwemer enchantées) |
   | `LItemEnchArmor*All` | `LItemDraugr02BossWeapon1HMagic` / `2HMagic` (§ 7) | sous-listes `SublistEnchArmorDwarven*` (§ 7) |

   Seule l'entrée change (son objet) : le niveau, le nombre et la chance de rien restent ceux de Bethesda, donc la quantité de butin ne bouge pas.
3. **Garder les valeurs d'origine** et les remettre avant chaque nouvelle application : la config relue au chargement d'une partie s'applique proprement, et désinstaller rend le butin vanilla.
4. **Log** : pour chaque enveloppe, les entrées remplacées et leurs remplaçantes ; les listes atteintes depuis un coffre funéraire mais laissées telles quelles, avec la raison. Durée de chaque étape (`[Startup]`, leçon de la 1.1.2 d'Every Crumb Counts).

Rien n'est fait pendant le jeu : aucun hook, aucun événement. Coût nul hors démarrage.

## 4. L'armure nordique ancienne

Aucune liste nivelée vanilla ne la distribue en coffre. Le mod en crée une au lancement (même fabrique que les copies d'Every Crumb Counts), avec les pièces jouables : `ArmorDraugrCuirass`, `ArmorDraugrBoots`, `ArmorDraugrGauntlets`, `ArmorDraugrHelmet`, et le bouclier par `LItemDraugrShield`. Les variantes `…Unplayable` [1019CA–1019CD] sont exclues.

Liste de mots-clés et d'objets dans la config, pour que les mods qui ajoutent des armures anciennes (variantes, retextures avec nouveaux objets) puissent y être ajoutés.

Point à vérifier : une liste créée au lancement a un FormID dynamique (plage FF). Every Crumb Counts l'a validé pour les cadavres (copies intactes après chargement), mais ici la liste est référencée par des **coffres** dont le contenu tiré est sauvegardé : vérifier qu'un coffre ouvert, sauvegardé puis rechargé garde ses objets, et que rien ne casse si le mod est retiré.

## 5. Les mods d'équipement

| Comment le mod ajoute ses objets | Résultat |
|---|---|
| Dans les listes génériques (`LItemWeaponSword`…) : Immersive Weapons, Immersive Armors, Heavy Armory, la plupart des « lootifications » | Absents des tombeaux, présents partout ailleurs. C'est le but. |
| Dans les listes d'époque (`LItemDraugr*`, `LItemDwarvenOnly*`) | Présents dans les tombeaux automatiquement. |
| Dans les enveloppes (`LootDraugr*`) directement | Leurs entrées ne pointent pas vers une liste générique : elles restent. |
| Objets anciens rangés dans les listes génériques | Perdus dans les tombeaux par défaut. Option (§ 7) : les repérer par mot-clé de matériau d'arme et les ajouter aux listes d'époque. |
| Distributeurs au lancement (KID, et ceux qui modifient les listes au démarrage) | **À vérifier** : on doit passer après eux, sinon on ne voit ni leurs objets ni leurs mots-clés. |

Les mods qui ajoutent leurs **propres** cryptes avec leurs propres coffres et listes ne sont pas touchés, sauf s'ils réutilisent les coffres ou enveloppes vanilla. La config permet d'ajouter leurs coffres.

## 6. Ce qu'on ne touche pas

- **Objets de quête et uniques** : posés à la main ou donnés par script, jamais tirés de ces listes. `LItemSpecialLoot10` / `LItemSpecialLoot100` (butin spécial rare des coffres) sont laissées telles quelles par défaut.
- **Occupants actuels** : les listes `LootBandit*`, `LootWarlock*`, `LootVampire*`, `LootForsworn*` ne sont jamais modifiées, même dans une ruine.
- **Équipement porté par les draugr** : déjà d'époque (`LItemDraugr*`). Leurs objets de mort (`DeathItemDraugr`) ne contiennent pas d'équipement : rien à faire.
- **Falmers** : désactivé par défaut. Les Falmers attaquent des voyageurs d'aujourd'hui : du fer dans une ruche est plausible. Option pour ne garder que l'équipement falmer et dwemer.
- **Or, potions, parchemins, gemmes, pierres d'âme, bijoux** : inchangés. (Des septims dans une crypte antérieure aux Septim, c'est anachronique, mais aucune pièce ancienne n'existe en vanilla : on laisse.)
- **Objets posés au sol dans les cryptes** : hors périmètre de la v1 (presque tous déjà d'époque).

## 6 bis. Le culte des dragons

Relevé dans `Skyrim.esm` :

- **Masques de prêtres-dragons** (`ArmorDragonPriestMask*Helmet`, dix masques), bâtons nommés (`dunForelhostDragonPriestStaff`, `dunValthumeDragonPriestStaff`, `MQ303DragonPriestStaff`) : uniques ou liés à des quêtes, posés sur les prêtres. **Jamais touchés.**
- **`DragonPriestDagger`** [01C1FE] : arme rituelle du culte, posée à quelques endroits.
- **Objets de mort des prêtres** : `DeathItemDragonPriest` [03AD7E] (or de boss draugr) et des bâtons de magie génériques (`LItemDragonPriestStaff50` / `100`). Les bâtons ne sont pas datés : on les laisse.
- Les **coffres** des repaires de prêtres-dragons sont les mêmes que ceux des autres cryptes (`TreasDraugrChestBoss`) : ils recevront déjà le butin d'époque de la v1.

Ce qui manque pour que le culte ait **son** butin : un trésor plus riche dans les repaires de prêtres-dragons (mot-clé de lieu `LocTypeDragonPriestLair` [0130E1]) que dans une crypte ordinaire. Par exemple, une arme ancienne enchantée assurée, l'ébène plus tôt, des bijoux, la dague du culte en tirage rare.

Difficulté : le butin est attaché au **type** de coffre, pas au lieu. Un même `TreasDraugrChestBoss` sert dans une petite crypte et chez un prêtre-dragon. Pour distinguer le repaire, il faut agir sur chaque coffre posé au moment où il se charge (le hook de chargement d'Every Crumb Counts), en lui donnant un coffre de base « culte des dragons » créé au lancement. Faisable, mais c'est une technique nouvelle à valider (sauvegarde, rechargement, désinstallation) : **version 2**, après les cryptes et les ruines dwemer.

## 6 ter. Ce que fait déjà la liste de l'utilisateur (relevé du 2026-11-20)

Plugins lus directement (sans le jeu) : `AncientNordArmo(u)ryEXTREMESE.esp`, `Prvti_DragonCultWeapons.esp`, `King-Priest.esp`, `DragonCultDraugr.esp`, `Open World Loot.esp` (version FR), `Bashed Patch, 0.esp`.

| Plugin | Ce qu'il fait aux listes des tombeaux |
|---|---|
| **Ancient Nord Armoury EXTREME** (ordre 206) | Remplace tout le contenu de `LootDraugrWeapon*` / `LootDraugrArmor*` / `LootDraugrEnch*` / `LootDraugrJewelry*` par des listes anciennes (`LItemDraugr02*`, `LItemDraugrandNordic*`, amulettes nordiques anciennes). Ajoute aussi l'équipement nordique gravé et le stalhrim de Dragonborn. **C'est déjà ce que veut ce mod pour les cryptes.** |
| **Open World Loot** | Réduit `LootDwarvenWeapon*` / `LootDwarvenArmor*` aux seules listes dwemer (`LItemDwarvenOnly*`). **C'est déjà ce que veut ce mod pour les ruines dwemer.** |
| **Armes du Culte du Dragon** (Prvti, ordre 373) | Garde les listes génériques vanilla dans `LootDraugrWeapon*` / `LootDraugrArmor*` et y ajoute ses sous-listes du culte (`PI_SublistLootDragonCult*`). Arme des seigneurs draugr (`PI_LItemDraugrLord*`). |
| **King-Priest** | Prêtres-dragons : armes du culte (`KP_DragonPriest*`), dague du culte dans leurs objets de mort (`KP_LItemDragonPriestDagger50`). |
| **Bashed Patch** (dernier) | Fusionne tout : **les listes génériques reviennent** (`LItemWeaponSword`, `LItemArmorCuirassAny`…) à côté des listes anciennes, dans les cryptes comme dans les ruines dwemer. |

**Conclusion** : le fer et l'acier vus dans les cryptes viennent de la fusion du Bashed Patch. Les retraits faits par Ancient Nord Armoury et Open World Loot sont perdus parce qu'un plugin chargé plus tard (Prvti) liste encore les entrées vanilla, et que le Bashed Patch additionne les listes.

Correction possible sans nouveau mod : le tag Wrye Bash **`Delev`** sur `AncientNordArmo(u)ryEXTREMESE.esp` et `Open World Loot.esp`, pour que le Bashed Patch garde leurs retraits, puis reconstruire le patch. À vérifier ensuite en relisant le Bashed Patch (même script).

Leçon pour ce mod : les **mots-clés de matériau des mods sont incohérents**. Les armures draugr de `DragonCultDraugr` portent `ArmorMaterialSteel`, les armes du culte de Prvti `WeapMaterialEbony`, celles de King-Priest `WeapMaterialDraugrHoned`, les armures d'Ancient Nord Armoury `DLC2ArmorMaterialNordic*`. Il faut donc juger un objet aussi par **sa liste d'origine et son plugin** (« tout ce que ce plugin ajoute aux listes des tombeaux est d'époque »), pas seulement par son mot-clé. Les sous-listes ajoutées par un mod dans une enveloppe de tombeau (`PI_SublistLootDragonCult*`, `LItemDraugrandNordic*`) sont **gardées** : seules les listes génériques vanilla sont remplacées.

À vérifier : **DynamicContainerLoot** (plugin SKSE actif dans la liste, mod Nexus 172018) agit peut-être aussi sur le contenu des coffres au moment du jeu.

## 7. Décisions (2026-11-20)

1. **Paliers de niveau** : **ébène seul par défaut, daedrique en option** (`timeless_materials` dans la config). Ancien et affûté en bas niveau, ébène en haut niveau, comme les seigneurs draugr de Bethesda. Le daedrique n'est pas récent (ébène lié à un Daedra), mais il n'appartient pas à la culture des Nords anciens, qui servaient le culte des dragons : il reste en option pour qui veut plus de récompenses à très haut niveau.
2. **Objets enchantés des cryptes nordiques** : **armes enchantées d'époque**. Les armures enchantées génériques (`LItemEnchArmor*All`) sont remplacées par les armes anciennes enchantées (`LItemDraugr02BossWeapon1HMagic` / `2HMagic`, givre), puisqu'aucune armure nordique ancienne enchantée n'existe.
3. **Objets enchantés des ruines dwemer** : armes enchantées d'époque. Le relevé montre que `Skyrim.esm` a des listes complètes d'**armes dwemer enchantées** (`LItemEnchDwarvenSword`, `…WarAxe`, `…Mace`, `…Dagger`, `…GreatSword`, `…Battleaxe`, `…Warhammer`, `…Bow`, et leurs variantes `…Boss`) et des sous-listes d'**armures dwemer enchantées** (`SublistEnchArmorDwarvenCuirass02/03/04`, `…Boots`, `…Gauntlets`, `…Helmet`, `…Shield`). Les ruines dwemer gardent donc aussi des armures enchantées, mais dwemer.
4. **Mots-clés d'armes** (§ 5) : **dès la v1**. Les armes portant `WeapMaterialDraugr` / `WeapMaterialDraugrHoned` (ou `WeapMaterialDwarven` pour les ruines dwemer) trouvées dans les listes génériques sont ajoutées aux listes d'époque. Les armures restent sur une liste d'objets explicite (piège du mot-clé daedrique, § 2).
5. **Dawnguard et Dragonborn** : relevés **dès l'étape 1**, avec la liste de mods de l'utilisateur. À ne pas confondre : l'équipement nordique gravé de Dragonborn est un équipement *d'aujourd'hui*, pas de l'ancien nordique.

## 8. Étapes proposées

1. **Relevé** : DLC et liste de l'utilisateur (USSEP, mods d'armes et d'armures, distributeurs) : quelles enveloppes et listes, après tous les plugins. Un log seul, sans rien changer.
2. **Cryptes nordiques**, armes seulement : remplacement des entrées, restauration, log. Test : un coffre de tombeau ouvert à plusieurs niveaux.
3. **Armure nordique ancienne** : liste créée au lancement, test sauvegarde / rechargement / désinstallation.
4. **Ruines dwemer.**
5. **Coffres de boss et enchantements** selon les réponses du § 7.
6. **Falmers** (option) et **mots-clés d'armes** (option).
7. Publication : même chaîne qu'Every Crumb Counts (GPL, FOMOD, source).
