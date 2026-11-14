# Recherche externe — simulateur Kode Dot / KodeOS

Date : 2026-11-14

Ce document conserve les pistes publiques utiles au projet `similateur-kd`. Les statuts distinguent les faits vérifiés des hypothèses.

## Découvertes prioritaires

### 1. Simulateur graphique officiel KodeDIY : `kgui_sim`

Sources officielles :
- KodeOS : https://github.com/kodediy/KodeOS
- Entrée CMake : https://github.com/kodediy/KodeOS/blob/develop/components/kgui/sim/CMakeLists.txt
- Adaptateur des écrans : https://github.com/kodediy/KodeOS/blob/develop/components/kgui/sim/kgui_sim.c
- SDK KUI simulator : https://github.com/kodediy/kode-sdk/tree/1841cc8c13d4b2e3e4d7288b40cdac5de207a133/components/kui/sim
- README amont : https://github.com/kodediy/kode-sdk/blob/develop/components/kui/sim/README.md

Constats vérifiés dans les sources :
- Le binaire consommateur KodeOS s'appelle `kgui_sim`.
- `kui_sim` est le socle de simulation du SDK utilisé par le consommateur KGUI.
- Le README amont donne les commandes :
  `cmake -S components/kgui/sim -B build_kgui_sim`
  `cmake --build build_kgui_sim -j8`
  puis `build_kgui_sim/kgui_sim <screen> [--snapshot OUT.png] [--set SUBJECT=VALUE]... [--device] [--bezel]`.
- L'implémentation officielle utilise LVGL 9.6 et SDL2.
- Les cinq écrans enregistrés dans `kgui_sim.c` sont : `face`, `launcher`, `loading`, `settings`, `touch_test`.
- Le simulateur peut afficher une fenêtre ou écrire une capture PNG.
- Le CMake amont exclut explicitement plusieurs extensions/consommateurs non disponibles dans ce simulateur (notamment certaines bibliothèques LVGL optionnelles, les scènes Kody générées et le parseur de packs KUI). Cela confirme qu'il s'agit d'un simulateur graphique ciblé, pas du firmware KodeOS complet.

### 2. Émulateur ESP32 officiel Espressif : piste potentiellement majeure

Dépôt : https://github.com/espressif/esp-emulator
Documentation source : https://github.com/espressif/esp-emulator/blob/main/README.md
Démo navigateur : https://espressif.github.io/esp-emulator/
Release vérifiée : https://github.com/espressif/esp-emulator/releases/tag/v1.0.0

Constats vérifiés :
- Release `v1.0.0` publiée le 13 novembre 2026.
- Le projet annonce la prise en charge de l'ESP32-P4, en plus d'autres SoC ESP32.
- L'émulateur exécute des images flash fusionnées construites avec ESP-IDF.
- Il fournit un build navigateur WebAssembly ; l'image firmware est chargée localement dans le navigateur.
- Le projet annonce l'émulation du CPU, mémoire et de nombreux périphériques (couverture dépendante de la puce et des périphériques explicitement implémentés).
- Il possède des interfaces de diagnostic (console, registres, mémoire, GPIO, etc.).

Intérêt pour similateur-kd :
- À évaluer comme deuxième voie, distincte de `kgui_sim` : exécuter une vraie image firmware ESP32-P4 au lieu de recompiler les composants sur l'hôte.
- Ne remplace pas le simulateur KGUI : l'émulateur de SoC n'apporte pas automatiquement les assets, l'environnement ni les services propres à KodeOS.
- Condition bloquante à vérifier : obtenir une image flash KodeOS compatible, ainsi que les composants/ROM/configurations attendus. La disponibilité publique d'une image officielle KodeOS complète n'a pas été établie pendant cette recherche.
- Il faut tester précisément le support du périphérique écran / contrôleur utilisé par le Kode Dot ; la prise en charge du SoC ESP32-P4 ne garantit pas à elle seule l'émulation de tout le matériel spécifique de la carte.

## Autres dépôts examinés

### Kyworn/kode-dot
https://github.com/Kyworn/kode-dot

Projet Python indépendant orienté pipeline Hermes/RF et contrats hôte. Son README indique explicitement que ses simulations/test suites valident des contrats logiciels et données synthétiques, pas un firmware KodeOS démarré sur un ESP32-P4. Ce n'est donc pas la preuve d'un émulateur Kode Dot/KodeOS. À garder comme référence éventuelle de pratiques de simulation contractuelle, pas comme base du simulateur graphique.

## État de notre dépôt au moment de la recherche

Dépôt : https://github.com/livan-commits/similateur-kd

Le README et l'audit d'intégration confirment :
- objectif : porter le véritable `kgui_sim` officiel vers le navigateur, pas refaire les écrans en HTML/CSS ;
- SDK épinglé : `1841cc8c13d4b2e3e4d7288b40cdac5de207a133` ;
- workflow Web : checkout de KodeOS/develop avec sous-modules récursifs, vérification du gitlink SDK, adaptation Emscripten, compilation puis publication Pages ;
- l'audit du dépôt rapporte que la compilation Web échouait autour de 99 % sur une inclusion QP/C (`kqp.h` -> `qpc.h`) et que le déploiement n'avait donc pas été atteint à cette vérification ;
- la voie native vise un port QP/C hôte explicite ; ne pas masquer le port FreeRTOS/ESP-IDF par de faux en-têtes.

Liens internes :
- https://github.com/livan-commits/similateur-kd/blob/main/docs/integration-audit.md
- https://github.com/livan-commits/similateur-kd/blob/main/docs/native-host-simulator.md
- https://github.com/livan-commits/similateur-kd/blob/main/.github/workflows/build-web.yml

## Recommandation technique provisoire

Maintenir deux voies complémentaires et ne pas les confondre :

1. **Voie UI immédiate** : réparer et compiler le simulateur officiel `kgui_sim` avec le SDK épinglé, puis publier sa version WebAssembly. C'est la voie la plus proche d'un résultat visuel contrôlable depuis Chrome.
2. **Voie firmware/SoC** : réaliser un prototype indépendant avec Espressif `esp-emulator` et une image firmware ESP32-P4 réellement disponible. Valider écran, tactile, stockage, radio et autres périphériques nécessaires avant d'en faire la base du projet.
3. **Voie OS hôte** : poursuivre seulement après audit des points d'entrée KodeOS, Active Objects QP/C et services. Cette voie recompile du code pour l'hôte et n'est pas une émulation instruction-par-instruction du processeur.

Ne déclarer « émulation complète » que lorsque le boot, les Active Objects, les services et les périphériques nécessaires ont été démarrés et testés. Un affichage KGUI seul ne suffit pas.

## Limites de recherche

- Les dépôts publics et pages publiquement indexées ont été examinés.
- Aucun accès aux salons Discord privés ni aux messages non publics n'est disponible ici.
- Aucun dépôt communautaire vérifié dans cette passe ne démontre à lui seul un démarrage complet de KodeOS sur PC.
