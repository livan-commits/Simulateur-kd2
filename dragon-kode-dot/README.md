# Dragon

Un dragon volant qui suit votre doigt sur l'écran du Kode Dot et crache du feu.
Serpent ailé dessiné pixel par pixel (aucune image embarquée), avec ondulation
du corps, battement des ailes et particules de flamme.

## What you need

- Un **Kode Dot** avec kodeOS.

## Using it

1. Installez **Dragon** depuis le catalogue Kode, ou flashez `build/dragon.bin`.
2. Au lancement, le dragon vole tout seul : pilote automatique en courbe.
3. Posez un doigt sur la vitre et glissez : il vous suit et tourne autour de
   votre doigt quand vous vous arrêtez.
4. Les commandes :
   - **Tap** sur la vitre : une bouffée de feu (+ vibration).
   - **TOP** (maintenu) : souffle continu.
   - **RIGHT** : pilote automatique on/off.
   - **BOT** (maintenu) : retour à kodeOS.

## What it does

- **Il suit le doigt.** La tête vole vers le point touché, le corps suit
  anneau par anneau, la queue ondule.
- **Il crache du feu.** Jusqu'à 56 particules de flamme, du jaune au rouge,
  avec un halo au museau. La LED passe au rouge pendant le feu.
- **Il vole seul.** Pilote automatique, LED verte ; LED ambre en manuel.
- **Il vit bien dans kodeOS.** L'app répond au park de l'écran et à la
  batterie critique comme le demande `kscreen`, et le bouton BOT rend la main.

## Not there yet

- Dessin sans anticrénelage : le trait est net, façon rétro, volontairement.
- Une seule apparence ; les couleurs se règlent dans `dragon_render.c`.

---

## Building

- ESP-IDF **v6.1**.
- Le kode SDK, épinglé dans `main/idf_component.yml` (v0.1.2) et récupéré par
  le component manager au premier build.

```bash
. ~/.espressif/v6.1/esp-idf/export.sh
idf.py build
```

L'image est `build/dragon.bin`. Pour l'installer sur le Dot :

```bash
idf.py -p PORT flash
```

## Structure

| Fichier | Rôle |
|---|---|
| `main/app_main.c` | Le point d'entrée : `kode_init()`, puis l'Active Object. |
| `main/dragon_ao.c/.h` | L'AO : kscreen (LVGL), toucher, boutons, canvas. |
| `main/dragon_model.c/.h` | La logique pure : vol, corps, feu. Testable sur PC. |
| `main/dragon_draw.c/.h` | Le rasteriseur RGB565 : disques, polygones, capsules, halos. |
| `main/dragon_render.c/.h` | La peinture du dragon à partir du modèle. |

Licence : GPL-3.0-or-later, comme le SDK.
