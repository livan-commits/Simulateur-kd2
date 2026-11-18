/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file app_main.c
 * @brief Le point d'entree de l'app Dragon : monter le systeme, demarrer l'AO.
 */
#include "kode.h"

#include "dragon_ao.h"

/* The name on the launcher's pill, and which categories it files this under. */
KODE_APP_MANIFEST("Dragon", "games");

/**
 * @brief ESP-IDF's entry point.
 */
void app_main(void)
{
    /* First, always: nothing may post to an Active Object before its queue exists. */
    kode_init();

    DragonAo_ctor();
    DragonAo_start();
}
