# Références techniques KodeDIY

Ce document recense les sources amont consultées pour le Similateur KD Web. Les dépôts officiels et le commit de sous-module réellement utilisé par KodeOS priment sur les anciens exemples matériels.

## Sources de référence

- [KodeOS — dépôt officiel](https://github.com/kodediy/KodeOS)
- [KodeOS — documentation générale](https://docs.kode.diy/internals/overview)
- [KodeOS — construction et flash](https://docs.kode.diy/internals/building-and-flashing)
- [KodeOS — architecture de démarrage](https://docs.kode.diy/internals/boot-model)
- [KodeOS — interface graphique](https://docs.kode.diy/internals/gui/overview)
- [kode-sdk — dépôt officiel](https://github.com/kodediy/kode-sdk)
- [SDK — documentation](https://docs.kode.diy/sdk/overview)
- [KUI — documentation](https://docs.kode.diy/sdk/kui)
- [KUI — simulateur desktop officiel](https://github.com/kodediy/kode-sdk/tree/main/components/kui/sim)
- [KGUI — simulateur officiel](https://github.com/kodediy/KodeOS/tree/develop/components/kgui/sim)
- [Exemple officiel d'application](https://github.com/kodediy/kode-app-template)
- [Exemples officiels du SDK](https://github.com/kodediy/kode-sdk/tree/main/examples)

## Versions effectivement épinglées

La branche `develop` de KodeOS consultée épingle le sous-module `sdk` au commit :

`1841cc8c13d4b2e3e4d7288b40cdac5de207a133`

Le simulateur Web doit construire ce commit épinglé, et non prendre automatiquement la branche `main` du SDK. Cela garantit que les sources KGUI et KUI sont cohérentes entre elles.

Les sources épinglées décrivent :
- cible matérielle Kode Dot : ESP32-P4 ;
- environnement firmware : ESP-IDF 6.1 ;
- kit graphique KUI : LVGL 9.6 ;
- simulateur desktop officiel : SDL2 ;
- résolution du panneau Kode Dot : 410 × 502 pixels, RGB565.

## Architecture à respecter

KodeOS est construit à partir du SDK. Le dossier `components/kgui` contient l'interface et ses écrans ; le sous-module `sdk` contient les couches communes et les composants KUI. Le simulateur KGUI officiel inclut le simulateur KUI depuis `sdk/components/kui/sim`.

Le portage navigateur doit donc adapter le simulateur SDL2 existant à Emscripten, sans réécrire les écrans en HTML/CSS. L'interface du navigateur sert uniquement d'hôte au binaire WebAssembly.

## Périmètre et limites

Le simulateur officiel fournit actuellement cinq écrans de test : `face`, `launcher`, `loading`, `settings` et `touch_test`.

Cela permet de valider le rendu KGUI/KUI et les interactions graphiques. Cela ne simule pas encore le firmware complet, les Active Objects QP/C, le bootloader, les partitions flash, les coprocesseurs ni les périphériques physiques.

## Attention aux anciennes pages publiques

Le dépôt historique `kodediy/docs` contient encore des pages qui décrivent une ancienne génération matérielle ESP32-S3 et LVGL 9.3. Elles restent utiles pour comprendre les anciens exemples d'écran, mais ne doivent pas être utilisées comme spécification de la génération actuelle ESP32-P4 / ESP-IDF 6.1 / KUI LVGL 9.6.

Pour toute divergence, utiliser le README et les sources du commit de SDK épinglé par KodeOS, puis la documentation actuelle liée depuis ces dépôts.
