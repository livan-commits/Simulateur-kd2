# Simulateur KGUI dans le navigateur

Cette page est construite automatiquement par le workflow **Build and publish KGUI Web**.

## Moteur graphique

- Le moteur est le simulateur KGUI/KUI amont, compilé en WebAssembly avec Emscripten.
- Les écrans, composants LVGL, ressources graphiques et le cadre du Kode Dot proviennent des sources officielles épinglées dans `.github/workflows/build-web.yml`.
- Le fichier HTML généré par Emscripten sert de conteneur au binaire ; il ne remplace pas KGUI par une maquette HTML.

## Commandes

Le cadre officiel intègre les zones tactiles des six commandes. Sur ordinateur, le simulateur amont accepte les flèches du clavier pour le pavé directionnel, Entrée pour TOP et Échap pour BOT. Sur mobile, toucher directement les commandes dessinées sur le boîtier.

## Portée actuelle

Ce moteur exécute le simulateur graphique KGUI/LVGL. Il ne constitue pas, à lui seul, une émulation complète du firmware ESP32-P4/KodeOS : les Active Objects, pilotes et services matériels absents du profil hôte ne doivent pas être considérés comme émulés. Le laboratoire ESP-EMU est une cible séparée.

Les applications tierces ne sont exécutables que lorsqu'un environnement de chargement compatible avec leur format et leurs dépendances aura été raccordé ; leur simple présence dans le catalogue ne suffit pas.
