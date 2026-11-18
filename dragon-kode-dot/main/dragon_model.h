/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file dragon_model.h
 * @brief Le dragon interactif : un serpent ailé qui vole vers une cible et crache du feu.
 *
 * Logique pure (aucune dépendance matérielles ni LVGL) : testable sur un
 * ordinateur, rendue par dragon_draw.h.
 */
#ifndef DRAGON_MODEL_H_
#define DRAGON_MODEL_H_

#include <stdbool.h>
#include <stdint.h>

/** How many rings the dragon's body has, head first. */
#define DRAGON_SEG_N    26

/** The most flames alive at once. */
#define DRAGON_FLAME_MAX 56

/** How long a burst of fire lasts, seconds. */
#define DRAGON_BURST_S  0.8f

/** @brief One ring of the body, rendered. */
typedef struct {
    float x;    /**< Centre, in glass pixels.      */
    float y;
    float a;    /**< Facing, radians, 0 is right.  */
    float r;    /**< Radius, in pixels.            */
} dragon_pt_t;

/** @brief One tongue of fire. */
typedef struct {
    float age;
    float life;
    float x;
    float y;
    float vx;
    float vy;
    float r;
    uint32_t seed;
} dragon_flame_t;

/** @brief The whole dragon: state of its body, its fire and where it flies. */
typedef struct {
    int      w;             /**< The glass it flies over, in pixels. */
    int      h;
    float    t;             /**< This animation's own clock, seconds.   */
    uint32_t rand;          /**< A small deterministic generator.       */

    float    speed;         /**< Cruise speed, pixels a second.         */
    float    turn_rate;     /**< Radians a second.                      */
    float    spacing;       /**< Between rings, pixels.                 */
    float    radius;        /**< The head's radius, pixels.             */
    float    wiggle_amp;    /**< Flick of the tail, pixels.             */
    float    wiggle_freq;
    float    flap_speed;
    float    orbit_r;       /**< Inside this, the dragon circles its target. */

    float    head_x;
    float    head_y;
    float    head_a;
    float    target_x;
    float    target_y;

    bool     auto_flight;   /**< Roam on its own, ignoring the target.  */
    bool     hold_fire;     /**< The button or finger still asks for fire. */
    float    burst_left;    /**< Fire left to breathe, seconds.         */
    float    jaw;           /**< 0 shut, 1 wide.                        */

    dragon_pt_t     chain[DRAGON_SEG_N];    /**< The physics: ring i follows i-1. */
    dragon_pt_t     pts[DRAGON_SEG_N];      /**< As drawn, flicks included.       */
    dragon_flame_t  flames[DRAGON_FLAME_MAX];
    int             flame_n;
} dragon_t;

/**
 * @brief Build a dragon for a glass of @p w by @p h, resting at the centre.
 */
void dragon_init(dragon_t *d, int w, int h);

/** @brief Where the dragon flies to, in glass pixels. */
void dragon_set_target(dragon_t *d, float x, float y);

/** @brief Let it roam on its own (@c true) or chase the target (@c false). */
void dragon_set_auto(dragon_t *d, bool on);

/** @brief Whether the dragon roams on its own. */
bool dragon_get_auto(const dragon_t *d);

/** @brief Hold the fire button down: breathe without a pause. */
void dragon_hold_fire(dragon_t *d, bool on);

/** @brief One short burst of fire. */
void dragon_burst(dragon_t *d);

/** @brief Advance the whole animation by @p dt seconds. */
void dragon_step(dragon_t *d, float dt);

/** @brief The rings as drawn, head first. */
const dragon_pt_t *dragon_points(const dragon_t *d);

/** @brief Which ring carries the wings. */
const dragon_pt_t *dragon_wing_anchor(const dragon_t *d);

/** @brief Where the fire comes out. */
const dragon_pt_t *dragon_mouth(const dragon_t *d);

/** @brief Whether fire is coming out right now. */
bool dragon_breathing(const dragon_t *d);

/** @brief The flames alive, and how many; @p out never NULL. */
int dragon_flames(const dragon_t *d, const dragon_flame_t **out);

/** @brief Whether the point @p x, @p y touches the dragon's body. */
bool dragon_hit(const dragon_t *d, float x, float y);

#endif /* DRAGON_MODEL_H_ */
