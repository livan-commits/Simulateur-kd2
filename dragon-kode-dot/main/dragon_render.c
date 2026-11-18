/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file dragon_render.c
 * @brief The dragon, painted shape by shape: wings, body, head, then the fire.
 *
 * Every shape is soft at its rim, the body carries a top light and a warm
 * belly, the wing membranes are translucent, and the fire adds its light.
 */
#include "dragon_render.h"

#include "dragon_draw.h"

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

static uint16_t BODY, BODY_HI, BODY_LO, HORN, EYE, MOUTH;
static uint16_t BODY_DARK, BELLY, MEMB, BONE, HORN_TIP, TEETH;
static uint16_t FIRE_CORE, FIRE_MID, FIRE_OUT;

static void palette(void)
{
    static bool done = false;

    if (!done) {
        BODY      = dr_rgb(178, 36, 48);
        BODY_DARK = dr_rgb(96, 14, 26);
        BODY_HI   = dr_rgb(224, 88, 72);
        BELLY     = dr_rgb(226, 148, 100);
        MEMB      = dr_rgb(150, 34, 52);
        BONE      = dr_rgb(120, 22, 32);
        HORN      = dr_rgb(238, 210, 150);
        HORN_TIP  = dr_rgb(250, 236, 186);
        EYE       = dr_rgb(255, 198, 66);
        MOUTH     = dr_rgb(30, 6, 10);
        TEETH     = dr_rgb(240, 234, 208);
        FIRE_CORE = dr_rgb(255, 238, 190);
        FIRE_MID  = dr_rgb(255, 196, 70);
        FIRE_OUT  = dr_rgb(232, 92, 16);
        done = true;
    }
}

static void draw_wing(const dragon_t *d, uint16_t *fb, int stride, int w, int h, int side);
static void draw_body(const dragon_t *d, uint16_t *fb, int stride, int w, int h);
static void draw_head(const dragon_t *d, uint16_t *fb, int stride, int w, int h);
static void draw_fire(const dragon_t *d, uint16_t *fb, int stride, int w, int h);

void dragon_render(const dragon_t *d, uint16_t *fb, int stride, int w, int h)
{
    palette();
    draw_wing(d, fb, stride, w, h, -1);
    draw_wing(d, fb, stride, w, h, 1);
    draw_body(d, fb, stride, w, h);
    draw_head(d, fb, stride, w, h);
    draw_fire(d, fb, stride, w, h);
}

static void draw_wing(const dragon_t *d, uint16_t *fb, int stride, int w, int h, int side)
{
    const dragon_pt_t *b = dragon_wing_anchor(d);
    const float s = (float)side;
    const float perp = b->a + s * (float)M_PI * 0.5f;
    const float flap = sinf(d->t * d->flap_speed - 0.9f);
    const float base_ang = perp + flap * 0.22f * s;
    const float len = d->radius * 8.6f * (0.72f + 0.28f * cosf(d->t * d->flap_speed));
    float tip_x[3], tip_y[3];
    float xs[7], ys[7];
    int f;

    for (f = 0; f < 3; f++) {
        float fa = base_ang + s * (-0.12f - (float)f * 0.68f);
        float fl = len * (1.05f - 0.25f * (float)f);
        float flut = sinf(d->t * d->flap_speed * 2.0f + (float)f * 1.31f) * d->radius * 0.12f;

        tip_x[f] = b->x + cosf(fa) * fl + cosf(perp) * flut;
        tip_y[f] = b->y + sinf(fa) * fl + sinf(perp) * flut;
    }

    xs[0] = b->x;     ys[0] = b->y;
    xs[1] = tip_x[0]; ys[1] = tip_y[0];
    xs[2] = (tip_x[0] + tip_x[1]) * 0.5f + (b->x - tip_x[0]) * 0.22f;
    ys[2] = (tip_y[0] + tip_y[1]) * 0.5f + (b->y - tip_y[0]) * 0.22f;
    xs[3] = tip_x[1]; ys[3] = tip_y[1];
    xs[4] = (tip_x[1] + tip_x[2]) * 0.5f + (b->x - tip_x[1]) * 0.22f;
    ys[4] = (tip_y[1] + tip_y[2]) * 0.5f + (b->y - tip_y[1]) * 0.22f;
    xs[5] = tip_x[2]; ys[5] = tip_y[2];
    xs[6] = b->x - cosf(base_ang) * len * 0.34f;
    ys[6] = b->y - sinf(base_ang) * len * 0.34f;
    dr_aa_poly(fb, stride, w, h, xs, ys, 7, MEMB, 176U);
    dr_aa_poly(fb, stride, w, h, xs, ys, 5, dr_mix(MEMB, BODY, 4U), 58U);

    for (f = 0; f < 3; f++) {
        dr_taper(fb, stride, w, h, b->x, b->y, d->radius * (0.30f - 0.07f * (float)f),
                 tip_x[f], tip_y[f], d->radius * (0.11f - 0.02f * (float)f),
                 BONE, 255U);
    }
    dr_taper(fb, stride, w, h,
             b->x - cosf(base_ang) * d->radius * 0.4f,
             b->y - sinf(base_ang) * d->radius * 0.4f,
             d->radius * 0.34f,
             tip_x[0], tip_y[0], d->radius * 0.10f, BONE, 255U);
    {
        float claw_a = base_ang + s * (-0.12f - 2.0f * 0.68f);
        float cx = tip_x[2] + cosf(claw_a) * d->radius * 0.30f;
        float cy = tip_y[2] + sinf(claw_a) * d->radius * 0.30f;
        float ca = cosf(claw_a), sa = sinf(claw_a);
        float cb = -sa, sb = ca;
        const float px[3] = { tip_x[2] - ca * d->radius * 0.05f,
                              cx + cb * d->radius * 0.12f,
                              tip_x[2] + ca * d->radius * 0.28f };
        const float py[3] = { tip_y[2] - sa * d->radius * 0.05f,
                              cy + sb * d->radius * 0.12f,
                              tip_y[2] + sa * d->radius * 0.28f };

        dr_aa_poly(fb, stride, w, h, px, py, 3, HORN, 255U);
    }
}

static void draw_body(const dragon_t *d, uint16_t *fb, int stride, int w, int h)
{
    const dragon_pt_t *pts = dragon_points(d);
    int i;

    for (i = DRAGON_SEG_N - 1; i >= 0; i--) {
        const dragon_pt_t *p = &pts[i];
        uint16_t c = dr_mix(BODY, BODY_DARK, (uint8_t)(i * 8U / DRAGON_SEG_N));

        dr_soft_disc(fb, stride, w, h, p->x, p->y, p->r, c, 255U);
    }
    for (i = DRAGON_SEG_N - 2; i >= 1; i--) {
        const dragon_pt_t *p = &pts[i];

        dr_soft_disc(fb, stride, w, h, p->x + p->r * 0.42f, p->y + p->r * 0.42f,
                     p->r * 0.78f, BODY_DARK, 74U);
        dr_soft_disc(fb, stride, w, h, p->x, p->y + p->r * 0.40f,
                     p->r * 0.52f, BELLY, 110U);
        dr_soft_disc(fb, stride, w, h, p->x - p->r * 0.22f, p->y - p->r * 0.34f,
                     p->r * 0.62f, BODY_HI, 92U);
    }
    {
        const dragon_pt_t *p = &pts[5];

        dr_soft_disc(fb, stride, w, h, p->x - p->r * 0.24f, p->y - p->r * 0.36f,
                     p->r * 0.30f, dr_rgb(250, 210, 170), 120U);
    }
    for (i = 2; i < DRAGON_SEG_N - 3; i += 2) {
        const dragon_pt_t *p = &pts[i];
        float back = p->a + (float)M_PI;
        float out = p->a + (float)M_PI * 0.5f;
        float wdt = p->r * 0.52f;
        float sway = sinf(d->t * d->flap_speed * 0.5f + (float)i) * p->r * 0.18f;
        float apex_x = p->x + cosf(back) * (p->r * 1.45f + sway);
        float apex_y = p->y + sinf(back) * (p->r * 1.45f + sway);
        const float xs[3] = { apex_x,
                              p->x + cosf(out) * wdt,
                              p->x - cosf(out) * wdt };
        const float ys[3] = { apex_y,
                              p->y + sinf(out) * wdt,
                              p->y - sinf(out) * wdt };

        dr_aa_poly(fb, stride, w, h, xs, ys, 3, dr_mix(BODY_DARK, BODY, 3U), 255U);
    }
    {
        const dragon_pt_t *tail = &pts[DRAGON_SEG_N - 1];
        const dragon_pt_t *mid = &pts[DRAGON_SEG_N - 3];
        float ta = atan2f(tail->y - mid->y, tail->x - mid->x);
        float tl = d->radius * 2.4f;
        float sway = sinf(d->t * d->flap_speed + 2.0f) * d->radius * 0.5f;
        const float xs[6] = {
            tail->x,
            tail->x + cosf(ta + 1.15f + sway * 0.05f) * tl,
            tail->x + cosf(ta + 0.35f) * (tl * 1.35f + sway),
            tail->x + cosf(ta) * tl * 0.55f,
            tail->x + cosf(ta - 0.35f) * (tl * 1.35f - sway),
            tail->x + cosf(ta - 1.15f - sway * 0.05f) * tl
        };
        const float ys[6] = {
            tail->y,
            tail->y + sinf(ta + 1.15f + sway * 0.05f) * tl,
            tail->y + sinf(ta + 0.35f) * (tl * 1.35f + sway),
            tail->y + sinf(ta) * tl * 0.55f,
            tail->y + sinf(ta - 0.35f) * (tl * 1.35f - sway),
            tail->y + sinf(ta - 1.15f - sway * 0.05f) * tl
        };

        dr_aa_poly(fb, stride, w, h, xs, ys, 6, MEMB, 176U);
    }
    (void)BODY_LO;
}

static void draw_head(const dragon_t *d, uint16_t *fb, int stride, int w, int h)
{
    const dragon_pt_t *hpt = dragon_points(d);
    const float R_ = d->radius;
    const float ca = cosf(hpt->a);
    const float sa = sinf(hpt->a);
    const float pa = -sa;
    const float pb = ca;
    int side;

    for (side = -1; side <= 1; side += 2) {
        float s = (float)side;
        float rx = hpt->x - ca * R_ * 0.9f + pa * s * R_ * 1.05f;
        float ry = hpt->y - sa * R_ * 0.9f + pb * s * R_ * 1.05f;
        float ra = hpt->a + (float)M_PI + s * 0.85f;
        const float xs[3] = { rx,
                              rx + cosf(ra) * R_ * 0.85f + pa * s * R_ * 0.34f,
                              rx + cosf(ra - s * 0.35f) * R_ * 0.20f };
        const float ys[3] = { ry,
                              ry + sinf(ra) * R_ * 0.85f + pb * s * R_ * 0.34f,
                              ry + sinf(ra - s * 0.35f) * R_ * 0.20f };

        dr_aa_poly(fb, stride, w, h, xs, ys, 3, MEMB, 176U);
    }
    for (side = -1; side <= 1; side += 2) {
        float s = (float)side;
        float sx = hpt->x + ca * R_ * 0.2f + pa * s * R_ * 0.75f;
        float sy = hpt->y + sa * R_ * 0.2f + pb * s * R_ * 0.75f;
        float mx = hpt->x - ca * R_ * 0.9f + pa * s * R_ * 1.5f;
        float my = hpt->y - sa * R_ * 0.9f + pb * s * R_ * 1.5f;
        float ex = hpt->x - ca * R_ * 1.75f + pa * s * R_ * 1.15f;
        float ey = hpt->y - sa * R_ * 1.75f + pb * s * R_ * 1.15f;
        float curve = cosf(d->t * d->flap_speed * 0.5f) * 0.05f;

        dr_taper(fb, stride, w, h, sx, sy, R_ * 0.26f,
                 mx, my + curve * R_, R_ * 0.15f, HORN, 255U);
        dr_taper(fb, stride, w, h, mx, my + curve * R_, R_ * 0.15f,
                 ex, ey, R_ * 0.05f, HORN_TIP, 255U);
    }
    dr_aa_ellipse(fb, stride, w, h,
                  hpt->x + ca * R_ * 0.45f, hpt->y + sa * R_ * 0.45f,
                  R_ * 1.4f, R_ * 0.98f, hpt->a, BODY, 255U);
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
        const float tx[6] = { n0x, n1x, n2x - pa * R_ * 0.06f,
                              n3x - pa * R_ * 0.06f, n4x, n5x };
        const float ty[6] = { n0y, n1y, n2y - pb * R_ * 0.06f,
                              n3y - pb * R_ * 0.06f, n4y, n5y };

        dr_aa_poly(fb, stride, w, h, xs, ys, 6, dr_mix(BODY, BODY_HI, 3U), 255U);
        dr_aa_poly(fb, stride, w, h, tx, ty, 6, BODY_DARK, 90U);
    }
    if (d->jaw > 0.05f) {
        float jw = R_ * 0.34f * d->jaw;
        float m0x = hpt->x + ca * R_ * 1.0f + pa * (-R_ * 0.1f);
        float m0y = hpt->y + sa * R_ * 1.0f + pb * (-R_ * 0.1f);
        float m1x = hpt->x + ca * R_ * 2.6f + pa * (-jw);
        float m1y = hpt->y + sa * R_ * 2.6f + pb * (-jw);
        float m2x = hpt->x + ca * R_ * 2.6f + pa * jw;
        float m2y = hpt->y + sa * R_ * 2.6f + pb * jw;
        float m3x = hpt->x + ca * R_ * 1.0f + pa * (R_ * 0.1f);
        float m3y = hpt->y + sa * R_ * 1.0f + pb * (R_ * 0.1f);
        const float mx[4] = { m0x, m1x, m2x, m3x };
        const float my[4] = { m0y, m1y, m2y, m3y };
        int t;

        if (dragon_breathing(d)) {
            dr_glow_add(fb, stride, w, h, (m0x + m1x) * 0.5f, (m0y + m1y) * 0.5f,
                        R_ * 1.4f, FIRE_OUT, 90U);
        }
        dr_aa_poly(fb, stride, w, h, mx, my, 4, MOUTH, 255U);
        for (t = 0; t < 3; t++) {
            float k = (t + 1) / 4.0f;
            float bx = m0x + (m1x - m0x) * k;
            float by = m0y + (m1y - m0y) * k;
            float tx = m3x + (m2x - m3x) * k;
            float ty = m3y + (m2y - m3y) * k;
            const float fx[3] = { bx, (bx + tx) * 0.5f + (tx - bx) * 0.12f, tx };
            const float fy[3] = { by, (by + ty) * 0.5f + (ty - by) * 0.12f, ty };

            dr_aa_poly(fb, stride, w, h, fx, fy, 3, TEETH,
                       (uint8_t)(255U * (1.0f - k * 0.75f)));
        }
    }
    {
        float nx = hpt->x + ca * R_ * 2.48f;
        float ny = hpt->y + sa * R_ * 2.48f;

        dr_aa_ellipse(fb, stride, w, h, nx + pa * R_ * 0.16f, ny + pb * R_ * 0.16f,
                      R_ * 0.10f, R_ * 0.06f, hpt->a, MOUTH, 255U);
        dr_aa_ellipse(fb, stride, w, h, nx - pa * R_ * 0.16f, ny - pb * R_ * 0.16f,
                      R_ * 0.10f, R_ * 0.06f, hpt->a, MOUTH, 255U);
    }
    for (side = -1; side <= 1; side += 2) {
        float s = (float)side;
        float ex = hpt->x + ca * R_ * 0.5f + pa * s * R_ * 0.72f;
        float ey = hpt->y + sa * R_ * 0.5f + pb * s * R_ * 0.72f;

        dr_soft_disc(fb, stride, w, h, ex, ey, R_ * 0.42f, BODY_DARK, 255U);
        dr_glow_disc(fb, stride, w, h, ex, ey, R_ * 0.34f, EYE, 220U);
        dr_soft_disc(fb, stride, w, h, ex, ey, R_ * 0.20f, EYE, 255U);
        dr_aa_ellipse(fb, stride, w, h, ex, ey, R_ * 0.06f, R_ * 0.16f,
                      hpt->a, dr_rgb(16, 6, 4), 255U);
        dr_soft_disc(fb, stride, w, h,
                     ex + pa * s * R_ * 0.07f - ca * R_ * 0.05f,
                     ey + pb * s * R_ * 0.07f - sa * R_ * 0.05f,
                     R_ * 0.05f, dr_rgb(255, 250, 230), 230U);
        dr_taper(fb, stride, w, h,
                 ex + ca * R_ * 0.30f + pa * s * R_ * 0.34f,
                 ey + sa * R_ * 0.30f + pb * s * R_ * 0.34f, R_ * 0.18f,
                 ex - ca * R_ * 0.24f + pa * s * R_ * 0.30f,
                 ey - sa * R_ * 0.24f + pb * s * R_ * 0.30f, R_ * 0.10f,
                 BONE, 255U);
    }
}

static void draw_fire(const dragon_t *d, uint16_t *fb, int stride, int w, int h)
{
    const dragon_flame_t *fl = NULL;
    int n = dragon_flames(d, &fl);
    int i;

    for (i = 0; i < n; i++) {
        const dragon_flame_t *f = &fl[i];
        float k = 1.0f - f->age / f->life;
        float rr = f->r * (0.5f + 1.1f * (1.0f - fabsf(k - 0.45f) * 1.4f));
        uint8_t ak  = (uint8_t)(200.0f * k + 0.5f);
        uint8_t ak2 = (uint8_t)(150.0f * k + 0.5f);
        uint8_t ak3 = (uint8_t)(120.0f * k + 0.5f);

        if (rr < 0.5f) { rr = 0.5f; }
        dr_glow_add(fb, stride, w, h, f->x, f->y, rr * 1.15f, FIRE_OUT, ak3);
        dr_glow_add(fb, stride, w, h, f->x, f->y, rr * 0.72f, FIRE_MID, ak2);
        dr_glow_add(fb, stride, w, h, f->x, f->y, rr * 0.38f, FIRE_CORE, ak);
    }
    if (dragon_breathing(d)) {
        const dragon_pt_t *m = dragon_mouth(d);

        dr_glow_add(fb, stride, w, h, m->x, m->y, d->radius * 3.2f,
                    dr_rgb(255, 150, 40), 80U);
    }
}
