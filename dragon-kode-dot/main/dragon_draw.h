/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file dragon_draw.h
 * @brief Un petit rasteriseur RGB565 pour dessiner le dragon soi-même.
 *
 * Chaque forme marque le rectangle qu'elle touche, que l'app lit pour
 * n'invalider que ce qui a bougé.
 */
#ifndef DRAGON_DRAW_H_
#define DRAGON_DRAW_H_

#include <stdint.h>

/** @brief Forget what the last round of drawing touched. */
void dr_dirty_reset(void);

/** @brief What the round touched so far, clamped inside the glass. */
void dr_dirty_get(int *x1, int *y1, int *x2, int *y2);

/** @brief Red, green, blue into RGB565. */
uint16_t dr_rgb(uint8_t r, uint8_t g, uint8_t b);

/** @brief Mix two RGB565 colours, @p k parts of @p b out of 8. */
uint16_t dr_mix(uint16_t a, uint16_t b, uint8_t k);

/** @brief A whole rectangle of an opaque colour. */
void dr_rect(uint16_t *fb, int stride, int w, int h,
             int x, int y, int rw, int rh, uint16_t c);

/** @brief A filled circle, opaque. */
void dr_disc(uint16_t *fb, int stride, int w, int h,
             float cx, float cy, float r, uint16_t c);

/** @brief A filled circle, one alpha over what is there. */
void dr_blend_disc(uint16_t *fb, int stride, int w, int h,
                   float cx, float cy, float r, uint16_t c, uint8_t alpha);

/** @brief A disc that fades to nothing at its rim - a glow, a flame. */
void dr_glow_disc(uint16_t *fb, int stride, int w, int h,
                  float cx, float cy, float r, uint16_t c, uint8_t alpha);

/** @brief A round-ended line - a horn, a wing bone. */
void dr_capsule(uint16_t *fb, int stride, int w, int h,
                float x0, float y0, float x1, float y1, float r, uint16_t c);

/** @brief A filled polygon of up to 12 points. */
void dr_poly(uint16_t *fb, int stride, int w, int h,
             const float *xs, const float *ys, int n, uint16_t c);

/** @brief A filled ellipse turned by @p ang radians - the skull. */
void dr_ellipse(uint16_t *fb, int stride, int w, int h,
                float cx, float cy, float rx, float ry, float ang, uint16_t c);

#endif /* DRAGON_DRAW_H_ */
