# Similateur KD Web

Projet indépendant visant à faire tourner dans un navigateur le véritable simulateur KGUI de KodeOS, à partir des sources publiques de KodeDIY.

## Objectif

Obtenir une version utilisable depuis Chrome sur téléphone ou ordinateur, sans Termux ni installation Android.

Le premier jalon est de compiler le simulateur officiel KGUI en WebAssembly avec SDL2 pour navigateur. Le périmètre initial reprend les cinq écrans déclarés par le simulateur KGUI :

- `face`
- `launcher`
- `loading`
- `settings`
- `touch_test`

## Sources amont

- [KodeOS](https://github.com/kodediy/KodeOS)
- [Simulateur KGUI de KodeOS](https://github.com/kodediy/KodeOS/tree/develop/components/kgui/sim)
- [Simulateur KUI du SDK](https://github.com/kodediy/kode-sdk/tree/main/components/kui/sim)

## État du projet

**Phase actuelle : portage navigateur expérimental.**

L'architecture native officielle repose sur LVGL 9.6 et SDL2. Le portage WebAssembly doit remplacer la boucle SDL bloquante par la boucle d'événements du navigateur et utiliser SDL2 fourni par Emscripten.

À ce stade, aucune compilation WebAssembly n'est encore déclarée réussie. Le dépôt ne doit pas être considéré comme un émulateur complet du firmware ESP32-P4.

## Feuille de route

1. Adapter la boucle du simulateur et sa configuration SDL2 à Emscripten.
2. Construire automatiquement le projet dans GitHub Actions.
3. Publier une première version testable sur GitHub Pages.
4. Ajouter la sélection des écrans et les commandes tactiles.
5. Étudier ensuite l'émulation des services et périphériques du firmware.

## Licence et attribution

KodeOS et le SDK KUI restent les projets amont de KodeDIY et conservent leurs licences et avis de copyright respectifs. Ce dépôt héberge le travail indépendant de portage Web.
