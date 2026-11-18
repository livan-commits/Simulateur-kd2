/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file dragon_render.h
 * @brief Le dragon peint dans un tampon RGB565, à partir de dragon_model.h.
 */
#ifndef DRAGON_RENDER_H_
#define DRAGON_RENDER_H_

#include "dragon_model.h"
#include <stdint.h>

/**
 * @brief Paint the dragon and its fire over a buffer that is already black.
 *
 * @param d      The dragon.
 * @param fb     The RGB565 pixels; rows are @p stride uint16_t apart.
 * @param stride Columns between rows.
 * @param w      The glass, in pixels.
 * @param h
 */
void dragon_render(const dragon_t *d, uint16_t *fb, int stride, int w, int h);

#endif /* DRAGON_RENDER_H_ */
