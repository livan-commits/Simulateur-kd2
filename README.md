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

**Phase actuelle : deux moteurs publiés, intégration matérielle en cours.**

- Le simulateur KGUI officiel est compilé en WebAssembly et publié sur GitHub Pages.
- ESP-EMU v1.0.0 est intégré comme moteur ESP32 complémentaire dans le site.
- Le workflow vérifie le commit SDK épinglé par KodeOS et le SHA-256 du paquet ESP-EMU ; il publie aussi un manifeste de provenance.
- Ces deux moteurs restent distincts : le KGUI Web n'est pas encore relié au firmware ESP32 émulé.
- L'émulation complète du firmware KodeOS, de ses Active Objects et de tous les périphériques du Kode Dot n'est pas encore réalisée.

Le profil matériel vérifié et les limites d'intégration sont décrits dans [docs/hardware-model.md](docs/hardware-model.md). L'audit technique est dans [docs/integration-audit.md](docs/integration-audit.md).

## Feuille de route

1. Décrire précisément les bus et composants de la carte depuis le SDK épinglé.
2. Implémenter et tester les premiers modèles Virtual Parts (TCA6408A, tactile et capteurs I2C).
3. Créer une page navigateur Kode Dot personnalisée utilisant l'API officielle ESP-EMU.
4. Obtenir une image flash KodeOS P4 reproductible et tester son démarrage.
5. Étudier les interfaces non couvertes par les Virtual Parts actuelles : MIPI-DSI, I2S, SDMMC et SDIO P4–C5.
6. Relier progressivement les services KodeOS, Active Objects et le rendu KGUI.

## Licence et attribution

KodeOS et le SDK KUI restent les projets amont de KodeDIY et conservent leurs licences et avis de copyright respectifs. Ce dépôt héberge le travail indépendant de portage Web.
