# Simulateur natif KodeOS — architecture cible

## Décision

La voie A est retenue : construire d'abord un simulateur natif hôte (Linux/Windows/macOS selon les dépendances), puis lui adjoindre une interface Web. Le but est d'exécuter le plus possible de code officiel KodeDIY sans prétendre émuler le processeur ESP32-P4.

Le SDK reste épinglé au commit utilisé par KodeOS :
`1841cc8c13d4b2e3e4d7288b40cdac5de207a133`.
Les sources officielles sont récupérées depuis KodeOS et ses sous-modules ; les adaptations propres au simulateur restent dans ce dépôt.

## Principe d'architecture

- **Code officiel conservé** : KGUI, KUI/LVGL, logique réutilisable de `kode`, `kaos`, `kqp` et applications lorsque leurs dépendances le permettent.
- **Backend hôte** : remplace les appels spécifiques ESP-IDF par des services POSIX/hôte ou des périphériques virtuels.
- **QP/C** : utiliser un port QP/C adapté à l'hôte pour les Active Objects. Ne pas inclure directement le port ESP-IDF/FreeRTOS dans une compilation native.
- **HAL virtuel** : implémenter les contrats utilisés par les composants officiels (horloges, files/messages, GPIO, I2C, SPI, UART, stockage, écran et tactile), au fur et à mesure que l'audit des dépendances confirme leur usage.
- **Interface** : SDL2 pour le premier exécutable natif ; WebAssembly/SDL2 ensuite si les dépendances le permettent.

## Étapes et critères d'acceptation

1. **Base native reproductible**
   - Compiler le simulateur KGUI officiel avec son CMake/SDL2 sur un runner Linux.
   - Vérifier les écrans officiellement exposés par le simulateur.
   - Ne pas modifier ni remplacer les écrans officiels par une maquette.

2. **Audit des dépendances firmware**
   - Inventorier les includes, symboles et appels ESP-IDF réellement utilisés par chaque composant.
   - Classer chaque dépendance : portable, à adapter, ou matériel non simulé.
   - Identifier le point d'entrée et le graphe de démarrage des Active Objects depuis les sources épinglées.

3. **Port QP/C hôte**
   - Ajouter une cible de compilation séparée pour QP/C hôte.
   - Fournir ordonnanceur/threads, horloges et synchronisation avec les primitives hôte.
   - Tester files d'événements, démarrage/arrêt, temporisations et transitions d'état.

4. **Services KodeOS et HAL virtuel**
   - Adapter les couches d'abstraction via un backend explicite, sans faux succès silencieux.
   - Modéliser les périphériques uniquement à partir des interfaces et contrats confirmés dans le SDK.
   - Tracer les opérations non prises en charge au lieu de les ignorer.

5. **Intégration et tests**
   - Démarrer les services et Active Objects officiels compatibles.
   - Vérifier les échanges de messages et les événements de bout en bout.
   - Ajouter des tests automatisés et des journaux inspectables.
   - Connecter l'interface Web au moteur natif seulement après stabilisation de l'API de contrôle.

## Limites à ne pas masquer

Ce simulateur natif n'est pas une émulation instruction-par-instruction du RISC-V ESP32-P4. Il exécute les composants recompilés pour l'hôte et simule les interfaces matérielles qu'ils utilisent. Les pilotes dépendant de registres, DMA, interruptions ou blocs matériels spécifiques nécessitent un modèle dédié ou restent hors périmètre.

Une émulation complète du firmware ne sera revendiquée qu'après démarrage vérifié des composants ciblés, couverture explicite des périphériques requis et tests de comportement. Le simple affichage KGUI/KUI ne suffit pas.

## Blocage technique déjà observé

Le portage Web expérimental a révélé que le chemin d'inclusion KGUI atteint le port QP/C ESP-IDF : `qp_port.h` inclut `sdkconfig.h` et `freertos/FreeRTOS.h`. Un fichier `sdkconfig.h` vide ne suffit donc pas à rendre ce port portable. La cible native doit sélectionner un véritable port QP/C hôte et isoler le port FreeRTOS au lieu de simuler artificiellement ses en-têtes.
