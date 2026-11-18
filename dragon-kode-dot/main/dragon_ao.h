/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file dragon_ao.h
 * @brief L'Active Object de l'app Dragon : écran, toucher, boutons, vol.
 */
#ifndef DRAGON_AO_H_
#define DRAGON_AO_H_

#include "kode_sys.h"

#ifdef __cplusplus
extern "C" {
#endif

extern QActive * const AO_Dragon;   /**< The one instance, opaque outside. */

/** @brief Construct; call after kode_init() and before DragonAo_start(). */
void DragonAo_ctor(void);

/** @brief Give the AO its task, queue and stack, and start it. */
void DragonAo_start(void);

#ifdef __cplusplus
}
#endif

#endif /* DRAGON_AO_H_ */
