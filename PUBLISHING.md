# Publier TombLoot sur Nexus Mods

Même chaîne qu'Every Crumb Counts (`G:\Skyrim\Disette-EN\PUBLISHING.md`). Tout ce qui va sur Nexus est en anglais ; le texte prêt est dans `NEXUS_PAGE.txt`.

## Faits vérifiés (2026-11-20)

- **Testé en jeu** sur Skyrim AE **1.6.1170** (Steam), dans deux profils MO2 :
  - la liste de l'auteur (Open World Loot, Ancient Nord Armoury EXTREME, Prvti Dragon Cult Weapons, DynamicContainerLoot, Bashed Patch, TrueHel…) : aucun objet hors d'époque, sauf deux choix d'auteurs (épée de verre posée par Open World Loot dans `DEMODraugrChestLarge02`, coffre mixte bandits/draugr de `TrueHel.esp`) ;
  - un profil minimal `TombLoot test` (Address Library, SKSE, USSEP, TombLoot) : replis sans Ancient Nord Armoury, ébène au niveau 38, aucun objet moderne.
  - coffre ouvert, sauvegardé, rechargé : objets conservés.
- **MorrowLoot Ultimate** : lu hors ligne (`MLU.esp`), compatible, jamais lancé en jeu.
- **Pas testé** : SE 1.5.97, autres versions AE, VR (non pris en charge).
- **Dépendances** : SKSE64, Address Library. Aucun ESP, aucun script, rien dans la sauvegarde.
- **Licence** : GPL-3.0-or-later (imposée par CommonLibSSE-NG v13). `COPYING`, `THIRD_PARTY_LICENSES.txt`.

## Construire les archives

Commiter d'abord (l'archive source vient du dernier commit), puis :

```
cmake --build --preset release --target package_mod
cmake --build --preset release --target package_source
```

→ `G:/Skyrim/_build/TombLoot/TombLoot-<version>.zip` et `TombLoot-<version>-source.zip`. La version vient de `project(... VERSION ...)` dans `CMakeLists.txt`.

## Étapes

1. Relire `NEXUS_PAGE.txt`, régler les lignes `[À DÉCIDER]` / `[À VÉRIFIER]`.
2. Captures d'écran : un coffre de crypte ouvert (équipement nordique antique), un coffre dwemer.
3. Créer la page, téléverser les deux archives (Main file, puis Miscellaneous pour la source).
4. Après publication : noter l'adresse de la page dans `THIRD_PARTY_LICENSES.txt` (ligne « Source code ») pour la version suivante.

## Support

Demander les lignes `[Summary]` de `TombLoot.log` : elles donnent, pour chaque objet hors d'époque, le chemin « coffre > liste > sous-liste » qui l'amène.
