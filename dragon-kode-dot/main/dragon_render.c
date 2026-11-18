/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file dragon_render.c
 * @brief The dragon, painted shape by shape: wings, body, head, then the fire.
 */
#include "dragon_render.h"

#include "dragon_draw.h"

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

static uint16_t BODY;
static uint16_t BODY_HI;
static uint16_t BODY_LO;
static uint16_t HORN;
static uint16_t EYE;
static uint16_t MOUTH;

static void palette(void)
{
    static bool done = false;

    if ( !done )
    {
        BODY    = dr_rgb(196, 40, 50);
        BODY_HI = dr_rgb(214, 64, 62);
        BODY_LO = dr_rgb(122, 18, 32);
        HORN    = dr_rgb(232, 199, 107);
        EYE     = dr_rgb(255, 211, 77);
        MOUTH   = dr_rgb(42, 10, 10);
        done = true;
    }
}

static void poly3(uint16_t *fb, int stride, int w, int h,
                  float x0, float y0, float x1, float y1,
                  float x2, float y2, uint16_t c)
{
    const float xs[3] = { x0, x1, x2 };
    const float ys[3] = { y0, y1, y2 };

    dr_poly(fb, stride, w, h, xs, ys, 3, c);
}

static void draw_wing(const dragon_t *d, uint16_t *fb, int stride, int w, int h, int side)
{
    const dragon_pt_t *b = dragon_wing_anchor(d);
    const float perp = b->a + (float)side * (float)M_PI * 0.5f;
    const float sweep = sinf(d->t * d->flap_speed - 0.9f) * 0.22f * (float)side;
    const float base_ang = perp + sweep;
    const float len = d->radius * 8.6f * (0.72f + 0.28f * cosf(d->t * d->flap_speed));
    const float ca = cosf(base_ang);
    const float sa = sinf(base_ang);
    const float back_x = b->x - ca * len * 0.2f;
    const float back_y = b->y - sa * len * 0.2f;
    float tip_x[3];
    float tip_y[3];
    float xs[8];
    float ys[8];
    int f;

    for ( f = 0; f < 3; f++ )
    {
        float fa = base_ang + (float)side * (-0.12f - (float)f * 0.68f);
        float fl = len * (1.05f - 0.25f * (float)f);

        tip_x[f] = b->x + cosf(fa) * fl;
        tip_y[f] = b->y + sinf(fa) * fl;
    }

    /* The membrane: base, the three finger tips, and a point behind. */
    xs[0] = b->x;        ys[0] = b->y;
    xs[1] = tip_x[0];    ys[1] = tip_y[0];
    xs[2] = (tip_x[0] + tip_x[1]) * 0.5f + (b->x - tip_x[0]) * 0.22f;
    ys[2] = (tip_y[0] + tip_y[1]) * 0.5f + (b->y - tip_y[0]) * 0.22f;
    xs[3] = tip_x[1];    ys[3] = tip_y[1];
    xs[4] = (tip_x[1] + tip_x[2]) * 0.5f + (b->x - tip_x[1]) * 0.22f;
    ys[4] = (tip_y[1] + tip_y[2]) * 0.5f + (b->y - tip_y[1]) * 0.22f;
    xs[5] = tip_x[2];    ys[5] = tip_y[2];
    xs[6] = back_x;      ys[6] = back_y;
    dr_poly(fb, stride, w, h, xs, ys, 7,
            dr_rgb(216, 92, 68));

    for ( f = 0; f < 3; f++ )
    {
        dr_capsule(fb, stride, w, h, b->x, b->y, tip_x[f], tip_y[f],
                   d->radius * (0.3f - 0.06f * (float)f), dr_rgb(140, 26, 36));
    }
}

static void draw_body(const dragon_t *d, uint16_t *fb, int stride, int w, int h)
{
    const dragon_pt_t *pts = dragon_points(d);
    int i;

    for ( i = DRAGON_SEG_N - 1; i >= 0; i-- )
    {
        const dragon_pt_t *p = &pts[i];
        uint16_t c = dr_mix(BODY, BODY_LO, (uint8_t)(i * 8U / DRAGON_SEG_N));

        dr_disc(fb, stride, w, h, p->x, p->y, p->r, c);
    }

    /* The dorsal ridge. */
    for ( i = 2; i < DRAGON_SEG_N - 3; i += 2 )
    {
        const dragon_pt_t *p = &pts[i];
        float back = p->a + (float)M_PI;
        float out = p->a + (float)M_PI * 0.5f;
        float wdt = p->r * 0.55f;
        float apex_x = p->x + cosf(back) * p->r * 1.5f;
        float apex_y = p->y + sinf(back) * p->r * 1.5f;

        poly3(fb, stride, w, h,
              apex_x, apex_y,
              p->x + cosf(out) * wdt, p->y + sinf(out) * wdt,
              p->x - cosf(out) * wdt, p->y - sinf(out) * wdt,
              BODY_LO);
    }

    /* The tail fin. */
    {
        const dragon_pt_t *tail = &pts[DRAGON_SEG_N - 1];
        const dragon_pt_t *mid = &pts[DRAGON_SEG_N - 3];
        float ta = atan2f(tail->y - mid->y, tail->x - mid->x);
        float tl = d->radius * 2.4f;
        const float xs[6] = {
            tail->x,
            tail->x + cosf(ta + 0.9f) * tl,
            tail->x + cosf(ta + 0.35f) * tl * 1.35f,
            tail->x + cosf(ta) * tl * 0.55f,
            tail->x + cosf(ta - 0.35f) * tl * 1.35f,
            tail->x + cosf(ta - 0.9f) * tl
        };
        const float ys[6] = {
            tail->y,
            tail->y + sinf(ta + 0.9f) * tl,
            tail->y + sinf(ta + 0.35f) * tl * 1.35f,
            tail->y + sinf(ta) * tl * 0.55f,
            tail->y + sinf(ta - 0.35f) * tl * 1.35f,
            tail->y + sinf(ta - 0.9f) * tl
        };

        dr_poly(fb, stride, w, h, xs, ys, 6, BODY);
    }
}

static void draw_head(const dragon_t *d, uint16_t *fb, int stride, int w, int h)
{
    const dragon_pt_t *hpt = dragon_points(d);
    const float R_ = d->radius;
    const float ca = cosf(hpt->a);
    const float sa = sinf(hpt->a);
    const float pa = -sa;
    const float pb = ca;
    float lx;   /* a point in the head's local frame, x forward */
    float ly;
    int side;

    (void)lx;
    (void)ly;

    /* The horns: two capsules a side, thicker at the root. */
    for ( side = -1; side <= 1; side += 2 )
    {
        float s = (float)side;
        float sx = hpt->x + ca * R_ * 0.2f + pa * s * R_ * 0.75f;
        float sy = hpt->y + sa * R_ * 0.2f + pb * s * R_ * 0.75f;
        float mx = hpt->x - ca * R_ * 0.9f + pa * s * R_ * 1.5f;
        float my = hpt->y - sa * R_ * 0.9f + pb * s * R_ * 1.5f;
        float ex = hpt->x - ca * R_ * 1.75f + pa * s * R_ * 1.15f;
        float ey = hpt->y - sa * R_ * 1.75f + pb * s * R_ * 1.15f;

        dr_capsule(fb, stride, w, h, sx, sy, mx, my, R_ * 0.28f, HORN);
        dr_capsule(fb, stride, w, h, mx, my, ex, ey, R_ * 0.15f, HORN);
    }

    /* The skull, then the snout. */
    dr_ellipse(fb, stride, w, h, hpt->x + ca * R_ * 0.45f, hpt->y + sa * R_ * 0.45f,
               R_ * 1.4f, R_ * 0.98f, hpt->a, BODY_HI);

    {
        float n0x = hpt->x + pa * (-R_ * 0.55f) + ca * R_ * 0.9f;
        float n0y = hpt->y + pb * (-R_ * 0.55f) + sa * R_ * 0.9f;
        float n1x = hpt->x + pa * (-R_ * 0.5f) + ca * (R_ * 2.21f);
        float n1y = hpt->y + pb * (-R_ * 0.5f) + sa * (R_ * 2.21f);
        float n2x = hpt->x + pa * (-R_ * 0.22f) + ca * (R_ * 2.6f);
        float n2y = hpt->y + pb * (-R_ * 0.22f) + sa * (R_ * 2.6f);
        float n3x = hpt->x + pa * (R_ * 0.22f) + ca * (R_ * 2.6f);
        float n3y = hpt->y + pb * (R_ * 0.22f) + sa * (R_ * 2.6f);
        float n4x = hpt->x + pa * (R_ * 0.5f) + ca * (R_ * 2.21f);
        float n4y = hpt->y + pb * (R_ * 0.5f) + sa * (R_ * 2.21f);
        float n5x = hpt->x + pa * (R_ * 0.55f) + ca * R_ * 0.9f;
        float n5y = hpt->y + pb * (R_ * 0.55f) + sa * R_ * 0.9f;
        const float xs[6] = { n0x, n1x, n2x, n3x, n4x, n5x };
        const float ys[6] = { n0y, n1y, n2y, n3y, n4y, n5y };

        dr_poly(fb, stride, w, h, xs, ys, 6, BODY_HI);
    }

    /* The open mouth, the wider the louder. */
    if ( d->jaw > 0.05f )
    {
        float jw = R_ * 0.34f * d->jaw;
        float m0x = hpt->x + ca * R_ * 1.0f + pa * (-R_ * 0.1f);
        float m0y = hpt->y + sa * R_ * 1.0f + pb * (-R_ * 0.1f);
        float m1x = hpt->x + ca * R_ * 2.6f + pa * (-jw);
        float m1y = hpt->y + sa * R_ * 2.6f + pb * (-jw);
        float m2x = hpt->x + ca * R_ * 2.6f + pa * jw;
        float m2y = hpt->y + sa * R_ * 2.6f + pb * jw;
        float m3x = hpt->x + ca * R_ * 1.0f + pa * (R_ * 0.1f);
        float m3y = hpt->y + sa * R_ * 1.0f + pb * (R_ * 0.1f);

        poly3(fb, stride, w, h, m0x, m0y, m1x, m1y, m2x, m2y, MOUTH);
        poly3(fb, stride, w, h, m0x, m0y, m2x, m2y, m3x, m3y, MOUTH);
    }

    /* The nostrils. */
    {
        float nx = hpt->x + ca * R_ * 2.48f;
        float ny = hpt->y + sa * R_ * 2.48f;

        dr_disc(fb, stride, w, h, nx + pa * R_ * 0.15f, ny + pb * R_ * 0.15f, R_ * 0.075f, MOUTH);
        dr_disc(fb, stride, w, h, nx - pa * R_ * 0.15f, ny - pb * R_ * 0.15f, R_ * 0.075f, MOUTH);
    }

    /* The eyes: a glow and a core. */
    for ( side = -1; side <= 1; side += 2 )
    {
        float s = (float)side;
        float ex = hpt->x + ca * R_ * 0.5f + pa * s * R_ * 0.72f;
        float ey = hpt->y + sa * R_ * 0.5f + pb * s * R_ * 0.72f;

        dr_glow_disc(fb, stride, w, h, ex, ey, R_ * 0.55f, EYE, 200U);
        dr_disc(fb, stride, w, h, ex, ey, R_ * 0.16f, EYE);
    }
}

static void draw_fire(const dragon_t *d, uint16_t *fb, int stride, int w, int h)
{
    const dragon_flame_t *fl = NULL;
    int n = dragon_flames(d, &fl);
    int i;

    for ( i = 0; i < n; i++ )
    {
        const dragon_flame_t *f = &fl[i];
        float k = 1.0f - f->age / f->life;
        float rr = f->r * (0.5f + 1.1f * (1.0f - fabsf(k - 0.45f) * 1.4f));
        uint16_t c = dr_mix(dr_rgb(255, 90, 20), dr_rgb(255, 220, 90),
                            (uint8_t)(k * 8.0f + 0.5f));

        if ( rr < 0.5f )
        {
            rr = 0.5f;
        }
        dr_glow_disc(fb, stride, w, h, f->x, f->y, rr, c,
                     (uint8_t)(190.0f * k + 0.5f));
    }

    if ( dragon_breathing(d) )
    {
        const dragon_pt_t *m = dragon_mouth(d);

        dr_glow_disc(fb, stride, w, h, m->x, m->y, d->radius * 3.2f,
                     dr_rgb(255, 160, 40), 70U);
    }
}

void dragon_render(const dragon_t *d, uint16_t *fb, int stride, int w, int h)
{
    palette();

    draw_wing(d, fb, stride, w, h, -1);
    draw_wing(d, fb, stride, w, h, 1);
    draw_body(d, fb, stride, w, h);
    draw_head(d, fb, stride, w, h);
    draw_fire(d, fb, stride, w, h);
}
