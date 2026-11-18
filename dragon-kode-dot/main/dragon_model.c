/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file dragon_model.c
 * @brief The dragon's flight, its body and its fire. Pure C, floats only.
 */
#include "dragon_model.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/** @brief The way to turn from @p a to @p b, into (-pi, pi]. */
static float ang_diff(float a, float b)
{
    float d = fmodf(b - a, 2.0f * M_PI);

    if ( d > M_PI )
    {
        d -= 2.0f * M_PI;
    }
    if ( d < -M_PI )
    {
        d += 2.0f * M_PI;
    }
    return d;
}

/** @brief The dragon's own generator: a xorshift, so the fire never repeats. */
static uint32_t dragon_rand(dragon_t *d)
{
    uint32_t x = d->rand;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    d->rand = (x != 0U) ? x : 0x9e3779b9U;
    return d->rand;
}

static float dragon_frac(dragon_t *d)
{
    return (float)(dragon_rand(d) >> 8) * (1.0f / 16777216.0f);
}

static void ring_init(dragon_pt_t *p, float x, float y)
{
    p->x = x;
    p->y = y;
    p->a = 0.0f;
    p->r = 0.0f;
}

static void flames_update(dragon_t *d, float dt);

void dragon_init(dragon_t *d, int w, int h)
{
    const float cx = (float)w * 0.5f;
    const float cy = (float)h * 0.5f;
    int i;

    memset(d, 0, sizeof(*d));
    d->w = w;
    d->h = h;
    d->radius     = 12.0f;
    d->spacing    = 8.5f;
    d->speed      = 250.0f;
    d->turn_rate  = 3.1f;
    d->wiggle_amp = 8.0f;
    d->wiggle_freq = 3.0f;
    d->flap_speed = 6.0f;
    d->orbit_r    = d->radius * 6.0f;
    d->head_x     = cx;
    d->head_y     = cy;
    d->head_a     = 0.0f;
    d->target_x   = cx;
    d->target_y   = cy;
    d->rand       = 0x1234567U;

    for ( i = 0; i < DRAGON_SEG_N; i++ )
    {
        ring_init(&d->chain[i], cx - (float)i * d->spacing, cy);
    }
    dragon_step(d, 1.0f / 60.0f);
}

void dragon_set_target(dragon_t *d, float x, float y)
{
    d->target_x = x;
    d->target_y = y;
}

void dragon_set_auto(dragon_t *d, bool on)
{
    d->auto_flight = on;
}

bool dragon_get_auto(const dragon_t *d)
{
    return d->auto_flight;
}

void dragon_hold_fire(dragon_t *d, bool on)
{
    d->hold_fire = on;
}

void dragon_burst(dragon_t *d)
{
    d->burst_left = DRAGON_BURST_S;
}

static void flames_update(dragon_t *d, float dt)
{
    int i = 0;

    while ( i < d->flame_n )
    {
        dragon_flame_t *f = &d->flames[i];
        float curl = sinf(d->t * 12.0f + (float)f->seed * 0.001f) * 60.0f * dt;

        f->age += dt;
        if ( f->age >= f->life )
        {
            d->flames[i] = d->flames[d->flame_n - 1];
            d->flame_n--;
            continue;
        }
        f->vx *= (1.0f - dt * 2.2f);
        f->vy *= (1.0f - dt * 2.2f);
        f->x += f->vx * dt + curl * 0.3f;
        f->y += f->vy * dt + curl;
        i++;
    }
}

void dragon_step(dragon_t *d, float dt)
{
    const float dt_max = 0.05f;
    float step;
    int i;

    if ( dt > dt_max )
    {
        step = dt_max;
    }
    else if ( dt <= 0.0f )
    {
        step = 0.0001f;
    }
    else
    {
        step = dt;
    }

    d->t += step;

    /* Where it flies: its own roam, or the finger. */
    if ( d->auto_flight )
    {
        d->target_x = (float)d->w * 0.5f
                      + sinf(d->t * 0.55f) * (float)d->w * 0.33f
                      + sinf(d->t * 1.3f) * (float)d->w * 0.05f;
        d->target_y = (float)d->h * 0.5f
                      + sinf(d->t * 1.1f) * (float)d->h * 0.28f;
    }

    {
        float lo = d->radius * 3.0f;
        float hi = (float)d->w - lo;

        d->target_x = clampf(d->target_x, lo, (hi > lo) ? hi : lo);
    }
    {
        float lo = d->radius * 2.0f;
        float hi = (float)d->h - d->radius * 8.0f;

        d->target_y = clampf(d->target_y, lo, (hi > lo) ? hi : lo);
    }

    /* Steer: turn at most turn_rate, and circle the target once settled. */
    {
        float dx   = d->target_x - d->head_x;
        float dy   = d->target_y - d->head_y;
        float dist = hypotf(dx, dy);
        float to   = atan2f(dy, dx);
        float want = to;
        float diff;
        float sp;

        if ( dist < d->orbit_r )
        {
            want += (M_PI * 0.5f) * (1.0f - dist / d->orbit_r);
        }
        diff = ang_diff(d->head_a, want);
        d->head_a += clampf(diff, -d->turn_rate * step, d->turn_rate * step);

        sp = d->speed * clampf(dist / d->orbit_r, 0.4f, 1.0f);
        d->head_x = clampf(d->head_x + cosf(d->head_a) * sp * step,
                           -d->radius * 4.0f, (float)d->w + d->radius * 4.0f);
        d->head_y = clampf(d->head_y + sinf(d->head_a) * sp * step,
                           -d->radius * 4.0f, (float)d->h + d->radius * 4.0f);
    }

    /* The body: each ring follows the one before it at exactly one spacing. */
    d->chain[0].x = d->head_x;
    d->chain[0].y = d->head_y;
    for ( i = 1; i < DRAGON_SEG_N; i++ )
    {
        dragon_pt_t *p = &d->chain[i];
        const dragon_pt_t *prev = &d->chain[i - 1];
        float dx = prev->x - p->x;
        float dy = prev->y - p->y;
        float len = hypotf(dx, dy);
        float fx, fy;

        if ( len < 0.0001f )
        {
            dx = 1.0f;
            dy = 0.0f;
            len = 1.0f;
        }
        fx = dx / len;
        fy = dy / len;
        p->x = prev->x - fx * d->spacing;
        p->y = prev->y - fy * d->spacing;
        p->a = atan2f(fy, fx);
    }

    /* As drawn: a flick along the body, growing towards the tail. */
    {
        float amp = d->wiggle_amp * (dragon_breathing(d) ? 0.6f : 1.0f);

        for ( i = 0; i < DRAGON_SEG_N; i++ )
        {
            dragon_pt_t *dst = &d->pts[i];
            const dragon_pt_t *src = &d->chain[i];
            float k = (float)i / (float)DRAGON_SEG_N;
            float off = sinf(d->t * d->wiggle_freq - (float)i * 0.55f) * amp * powf(k, 1.3f);
            float tail_k = (float)i / (float)(DRAGON_SEG_N - 1);

            dst->x = src->x + cosf(src->a + (float)M_PI * 0.5f) * off;
            dst->y = src->y + sinf(src->a + (float)M_PI * 0.5f) * off;
            dst->a = src->a;
            dst->r = d->radius * (1.0f - 0.78f * powf(tail_k, 1.4f));
        }
        for ( i = 1; i < DRAGON_SEG_N; i++ )
        {
            d->pts[i].a = atan2f(d->pts[i - 1].y - d->pts[i].y,
                                 d->pts[i - 1].x - d->pts[i].x);
        }
    }

    /* The fire. */
    if ( d->burst_left > 0.0f )
    {
        d->burst_left -= step;
        if ( d->burst_left < 0.0f )
        {
            d->burst_left = 0.0f;
        }
    }
    if ( dragon_breathing(d) )
    {
        const dragon_pt_t *m = dragon_mouth(d);
        int n;

        for ( n = 0; n < 5; n++ )
        {
            dragon_flame_t *f;
            float a;
            float sp;

            if ( d->flame_n >= DRAGON_FLAME_MAX )
            {
                break;
            }
            a  = m->a + (dragon_frac(d) - 0.5f) * 0.45f;
            sp = d->speed * 1.1f * (0.55f + dragon_frac(d) * 0.7f);
            f  = &d->flames[d->flame_n++];
            f->age  = 0.0f;
            f->life = 0.35f + dragon_frac(d) * 0.40f;
            f->x    = m->x + (dragon_frac(d) - 0.5f) * d->radius;
            f->y    = m->y + (dragon_frac(d) - 0.5f) * d->radius;
            f->vx   = cosf(a) * sp;
            f->vy   = sinf(a) * sp;
            f->r    = d->radius * (0.35f + dragon_frac(d) * 0.35f);
            f->seed = dragon_rand(d) & 0xFFFFU;
        }
    }
    flames_update(d, step);

    d->jaw += ((dragon_breathing(d) ? 1.0f : 0.0f) - d->jaw)
              * clampf(step * 12.0f, 0.0f, 1.0f);
}

const dragon_pt_t *dragon_points(const dragon_t *d)
{
    return d->pts;
}

const dragon_pt_t *dragon_wing_anchor(const dragon_t *d)
{
    return &d->pts[5];
}

const dragon_pt_t *dragon_mouth(const dragon_t *d)
{
    /* The gape sits a snout ahead of the head ring. */
    static dragon_pt_t m;
    const dragon_pt_t *h = &d->pts[0];
    float reach = d->radius * 2.6f;

    m.a = h->a;
    m.r = h->r;
    m.x = h->x + cosf(h->a) * reach;
    m.y = h->y + sinf(h->a) * reach;
    return &m;
}

bool dragon_breathing(const dragon_t *d)
{
    return d->hold_fire || (d->burst_left > 0.0f);
}

int dragon_flames(const dragon_t *d, const dragon_flame_t **out)
{
    *out = d->flames;
    return d->flame_n;
}

bool dragon_hit(const dragon_t *d, float x, float y)
{
    int i;

    for ( i = 0; i < DRAGON_SEG_N; i++ )
    {
        const dragon_pt_t *p = &d->pts[i];
        float dx = x - p->x;
        float dy = y - p->y;
        float rr = p->r + 2.0f;

        if ( (dx * dx + dy * dy) < rr * rr )
        {
            return true;
        }
    }
    return false;
}
