# Similateur Kode Dot Web

Projet indépendant pour exécuter le véritable firmware KodeOS ESP32-P4 dans Chrome, avec les sources publiques KodeDIY et le moteur officiel ESP-EMU. Ce dépôt n'est pas un produit officiel KodeDIY.

## Objectif

Utiliser le simulateur directement dans le navigateur, sur téléphone ou ordinateur, sans Termux ni installation Android.

## Ce qui fonctionne dans cette version

- L'image flash KodeOS est construite depuis le commit KodeOS épinglé et le commit exact de son SDK.
- ESP-EMU v1.0.0 exécute le firmware ESP32-P4 dans le navigateur.
- Le code réel KGUI/LVGL du firmware dessine dans deux framebuffers RGB565 en PSRAM. Le patch headless remplace uniquement la sortie physique CO5300/MIPI-DSI, qui n'existe pas dans le navigateur.
- Les adresses des symboles mémoire sont extraites du véritable ELF. Le navigateur lit le framebuffer directement dans la mémoire émulée avec l'API `read_memory` d'ESP-EMU, puis affiche ces pixels sur un canvas 410×502. Le canvas n'est pas une maquette de l'interface : il présente les pixels produits par KGUI.
- Un composant virtuel WebAssembly modélise les périphériques I²C TCA6408A (0x20) et CST820 (0x15) nécessaires au chemin d'affichage et d'entrée.
- Après le chargement des ressources KGUI, l'heure UTC du navigateur est envoyée à KodeOS par la console UART0. Le navigateur demande ensuite l'état de l'horloge au firmware et répète la synchronisation toutes les 30 secondes.

## Limites connues

- Ce n'est pas encore une émulation complète de tous les composants du Kode Dot. Le contrôleur MIPI-DSI est contourné ; l'audio, la carte SD, l'IMU, le NFC/RFID, les vibrations, l'IR et plusieurs autres périphériques ne sont pas encore modélisés.
- Le firmware ESP32-C5 est inclus dans l'image de stockage, mais le coprocesseur C5 n'est pas encore exécuté comme une seconde puce indépendante dans le navigateur.
- La capacité PSRAM du modèle navigateur doit encore être vérifiée par rapport aux 32 Mo du matériel réel. Le framebuffer utilise moins d'un mégaoctet, mais cette différence peut compter pour des applications plus gourmandes.
- La lecture du framebuffer est actuellement effectuée par snapshots complets quatre fois par seconde ; ce compromis privilégie la cohérence des pixels pendant que le pont mémoire est validé.

## Sources officielles et versions épinglées

- [KodeOS](https://github.com/kodediy/KodeOS) — commit `9bcfdb71b763ee7b5c14106358643ea611fc97db`
- [Kode SDK](https://github.com/kodediy/kode-sdk) — commit `1841cc8c13d4b2e3e4d7288b40cdac5de207a133`
- [ESP-EMU](https://github.com/espressif/esp-emulator) — v1.0.0

Les licences et avis de copyright des projets amont restent inchangés.
