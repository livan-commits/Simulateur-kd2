/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file dragon_draw.c
 * @brief The rasterizer: discs, polygons, capsules and glows into RGB565.
 */
#include "dragon_draw.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

/* ---- What the round touched ----------------------------------------------- */

static int s_x1;
static int s_y1;
static int s_x2;
static int s_y2;

void dr_dirty_reset(void)
{
    s_x1 = 0x7FFFFFFF;
    s_y1 = 0x7FFFFFFF;
    s_x2 = -1;
    s_y2 = -1;
}

static void mark(float ax, float ay, float bx, float by)
{
    int ix1 = (int)floorf(ax) - 1;
    int iy1 = (int)floorf(ay) - 1;
    int ix2 = (int)ceilf(bx) + 1;
    int iy2 = (int)ceilf(by) + 1;

    if ( ix1 < s_x1 )
    {
        s_x1 = ix1;
    }
    if ( iy1 < s_y1 )
    {
        s_y1 = iy1;
    }
    if ( ix2 > s_x2 )
    {
        s_x2 = ix2;
    }
    if ( iy2 > s_y2 )
    {
        s_y2 = iy2;
    }
}

void dr_dirty_get(int *x1, int *y1, int *x2, int *y2)
{
    if ( s_x2 < s_x1 )
    {
        *x1 = 0;
        *y1 = 0;
        *x2 = -1;
        *y2 = -1;
        return;
    }
    *x1 = s_x1;
    *y1 = s_y1;
    *x2 = s_x2;
    *y2 = s_y2;
}

/* ---- Pixels ---------------------------------------------------------------- */

uint16_t dr_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r >> 3) << 11)
                      | ((uint16_t)(g >> 2) << 5)
                      | (uint16_t)(b >> 3));
}

uint16_t dr_mix(uint16_t a, uint16_t b, uint8_t k)
{
    uint16_t ar = (uint16_t)(((a >> 11) & 0x1FU) * (8U - k));
    uint16_t ag = (uint16_t)(((a >> 5) & 0x3FU) * (8U - k));
    uint16_t ab = (uint16_t)((a & 0x1FU) * (8U - k));
    uint16_t br = (uint16_t)(((b >> 11) & 0x1FU) * k);
    uint16_t bg = (uint16_t)(((b >> 5) & 0x3FU) * k);
    uint16_t bb = (uint16_t)((b & 0x1FU) * k);

    return (uint16_t)(((ar + br) / 8U) << 11)
           | (uint16_t)((((ag + bg) / 8U) & 0x3FU) << 5)
           | (uint16_t)(((ab + bb) / 8U) & 0x1FU);
}

static void put(uint16_t *fb, int stride, int w, int h, int x, int y, uint16_t c)
{
    if ( (x >= 0) && (x < w) && (y >= 0) && (y < h) )
    {
        fb[(uint32_t)y * (uint32_t)stride + (uint32_t)x] = c;
    }
}

static void put_alpha(uint16_t *fb, int stride, int w, int h,
                      int x, int y, uint16_t c, uint8_t a)
{
    uint32_t i;

    if ( (x < 0) || (x >= w) || (y < 0) || (y >= h) )
    {
        return;
    }
    if ( a >= 250U )
    {
        i = (uint32_t)y * (uint32_t)stride + (uint32_t)x;
        fb[i] = c;
        return;
    }
    if ( a == 0U )
    {
        return;
    }
    i = (uint32_t)y * (uint32_t)stride + (uint32_t)x;
    fb[i] = dr_mix(fb[i], c, (uint8_t)(a >> 5));
}

/* ---- Forms ------------------------------------------------------------------ */

void dr_rect(uint16_t *fb, int stride, int w, int h,
             int x, int y, int rw, int rh, uint16_t c)
{
    int yy;

    if ( x < 0 )
    {
        rw += x;
        x = 0;
    }
    if ( y < 0 )
    {
        rh += y;
        y = 0;
    }
    if ( x + rw > w )
    {
        rw = w - x;
    }
    if ( y + rh > h )
    {
        rh = h - y;
    }
    if ( (rw <= 0) || (rh <= 0) )
    {
        return;
    }
    for ( yy = y; yy < y + rh; yy++ )
    {
        uint16_t *row = fb + (uint32_t)yy * (uint32_t)stride;
        int xx;

        for ( xx = x; xx < x + rw; xx++ )
        {
            row[xx] = c;
        }
    }
    mark((float)x, (float)y, (float)(x + rw - 1), (float)(y + rh - 1));
}

void dr_disc(uint16_t *fb, int stride, int w, int h,
             float cx, float cy, float r, uint16_t c)
{
    int y1 = (int)floorf(cy - r);
    int y2 = (int)ceilf(cy + r);
    float r2 = r * r;
    int y;

    for ( y = y1; y <= y2; y++ )
    {
        float dy = (float)y - cy;
        float dx = sqrtf((r2 - dy * dy) > 0.0f ? r2 - dy * dy : 0.0f);
        int xa = (int)ceilf(cx - dx);
        int xb = (int)floorf(cx + dx);
        int xx;

        for ( xx = xa; xx <= xb; xx++ )
        {
            put(fb, stride, w, h, xx, y, c);
        }
    }
    mark(cx - r, cy - r, cx + r, cy + r);
}

void dr_blend_disc(uint16_t *fb, int stride, int w, int h,
                   float cx, float cy, float r, uint16_t c, uint8_t alpha)
{
    int y;
    int x;
    float r2 = r * r;

    if ( alpha == 0U )
    {
        return;
    }
    for ( y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++ )
    {
        float dy = (float)y - cy;

        for ( x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++ )
        {
            float dx = (float)x - cx;

            if ( (dx * dx + dy * dy) <= r2 )
            {
                put_alpha(fb, stride, w, h, x, y, c, alpha);
            }
        }
    }
    mark(cx - r, cy - r, cx + r, cy + r);
}

void dr_glow_disc(uint16_t *fb, int stride, int w, int h,
                  float cx, float cy, float r, uint16_t c, uint8_t alpha)
{
    int y;
    int x;
    float r2 = r * r;

    if ( alpha == 0U )
    {
        return;
    }
    for ( y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++ )
    {
        float dy = (float)y - cy;

        for ( x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++ )
        {
            float dx  = (float)x - cx;
            float d2  = dx * dx + dy * dy;

            if ( d2 <= r2 )
            {
                float fall = 1.0f - d2 / r2;

                put_alpha(fb, stride, w, h, x, y, c,
                          (uint8_t)(alpha * fall * fall + 0.5f));
            }
        }
    }
    mark(cx - r, cy - r, cx + r, cy + r);
}

void dr_capsule(uint16_t *fb, int stride, int w, int h,
                float x0, float y0, float x1, float y1, float r, uint16_t c)
{
    /* A line, walked a half radius at a step, drawn as discs: cheap and
     * round-ended, which is all a horn or a wing bone asks. */
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = hypotf(dx, dy);
    float steps;
    int i;

    if ( len < 0.5f )
    {
        dr_disc(fb, stride, w, h, x0, y0, r, c);
        return;
    }
    steps = ceilf(len / (r * 0.5f));
    for ( i = 0; i <= (int)steps; i++ )
    {
        float k = (float)i / steps;

        dr_disc(fb, stride, w, h, x0 + dx * k, y0 + dy * k, r, c);
    }
    {
        float xlo = (x0 < x1 ? x0 : x1) - r;
        float ylo = (y0 < y1 ? y0 : y1) - r;
        float xhi = (x0 > x1 ? x0 : x1) + r;
        float yhi = (y0 > y1 ? y0 : y1) + r;

        mark(xlo, ylo, xhi, yhi);
    }
}

void dr_poly(uint16_t *fb, int stride, int w, int h,
             const float *xs, const float *ys, int n, uint16_t c)
{
    float ymin = 1e9f;
    float ymax = -1e9f;
    int i;
    int y;

    /* Walk every row, gather the edges' crossings, fill between pairs. Small
     * polygons only (a wing, a spike, a snout), so the scan stays cheap. */
    for ( i = 0; i < n; i++ )
    {
        if ( ys[i] < ymin )
        {
            ymin = ys[i];
        }
        if ( ys[i] > ymax )
        {
            ymax = ys[i];
        }
    }
    ymin -= 1.0f;
    ymax += 1.0f;

    for ( y = (int)floorf(ymin); y <= (int)ceilf(ymax); y++ )
    {
        float py = (float)y + 0.5f;
        float xs_at[12];
        int hits = 0;

        for ( i = 0; (i < n) && (hits < 12); i++ )
        {
            int j = (i + 1) % n;
            float ya = ys[i];
            float yb = ys[j];

            if ( (ya <= py) != (yb <= py) )
            {
                float t = (py - ya) / (yb - ya);

                xs_at[hits++] = xs[i] + (xs[j] - xs[i]) * t;
            }
        }
        /* Insertion sort: never more than a dozen crossings. */
        for ( i = 1; i < hits; i++ )
        {
            float v = xs_at[i];
            int j = i - 1;

            while ( (j >= 0) && (xs_at[j] > v) )
            {
                xs_at[j + 1] = xs_at[j];
                j--;
            }
            xs_at[j + 1] = v;
        }
        for ( i = 0; (i + 1) < hits; i += 2 )
        {
            int xa = (int)ceilf(xs_at[i]);
            int xb = (int)floorf(xs_at[i + 1]);
            int xx;

            for ( xx = xa; xx <= xb; xx++ )
            {
                put(fb, stride, w, h, xx, y, c);
            }
        }
    }
    ymin += 1.0f;
    ymax -= 1.0f;
    {
        float xlo = 1e9f;
        float xhi = -1e9f;

        for ( i = 0; i < n; i++ )
        {
            if ( xs[i] < xlo )
            {
                xlo = xs[i];
            }
            if ( xs[i] > xhi )
            {
                xhi = xs[i];
            }
        }
        mark(xlo, ymin, xhi, ymax);
    }
}

void dr_ellipse(uint16_t *fb, int stride, int w, int h,
                float cx, float cy, float rx, float ry, float ang, uint16_t c)
{
    float ca = cosf(ang);
    float sa = sinf(ang);
    float reach = (rx > ry ? rx : ry);
    int y;
    int x;

    for ( y = (int)floorf(cy - reach); y <= (int)ceilf(cy + reach); y++ )
    {
        float dy = (float)y - cy;

        for ( x = (int)floorf(cx - reach); x <= (int)ceilf(cx + reach); x++ )
        {
            float dx = (float)x - cx;
            float u = (dx * ca + dy * sa) / rx;
            float v = (-dx * sa + dy * ca) / ry;

            if ( (u * u + v * v) <= 1.0f )
            {
                put(fb, stride, w, h, x, y, c);
            }
        }
    }
    mark(cx - reach, cy - reach, cx + reach, cy + reach);
}
