/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file dragon_ao.c
 * @brief L'Active Object de l'app Dragon : LVGL sur la vitre, le doigt pour
 *        cible, les boutons pour le feu et le pilote automatique.
 */
#include "dragon_ao.h"

#include "kscreen.h"

#include "dragon_model.h"
#include "dragon_render.h"
#include "dragon_draw.h"

#include "esp_heap_caps.h"

#include <string.h>

#define TAG "dragon"

/** A tap shorter than this fires a burst; a longer press leads the dragon. */
#define DRG_TAP_MS          300U

/** The LED while the fire is out. */
#define DRG_LED_FIRE_R      220U
#define DRG_LED_FIRE_G      50U
#define DRG_LED_FIRE_B      10U

/** The LED while it flies on its own. */
#define DRG_LED_AUTO_R      20U
#define DRG_LED_AUTO_G      120U
#define DRG_LED_AUTO_B      60U

/** The LED while it follows the finger. */
#define DRG_LED_HAND_R      90U
#define DRG_LED_HAND_G      60U
#define DRG_LED_HAND_B      10U

/** @brief This AO's private signals. Posted to itself, never published. */
enum DragonSignals {
    DRG_TICK_SIG = MAX_SIG  /**< The render pass: kscreen's one-shot. */
};

/** @brief The AO's own state, private to this file. */
typedef struct {
    QActive     super;
    kscreen_t   screen;     /**< LVGL on the glass, driven from this AO. */
    lv_obj_t  * canvas;     /**< The picture the dragon is painted into. */
    uint16_t  * fb;         /**< The canvas's own pixels, in PSRAM.      */
    int32_t     stride;     /**< uint16_t between the canvas's rows.     */
    int32_t     w;          /**< The glass, in pixels.                   */
    int32_t     h;
    dragon_t    dragon;     /**< The beast itself.                       */
    bool        fire_hold;  /**< TOP is being held: breathe without pause. */
    uint32_t    press_ms;   /**< When the finger came down.              */
    bool        finger;     /**< A finger is down.                       */
    uint8_t     led;        /**< What the LED was last told to be.       */
} DragonAo;

/* The queue and the stack, written out as the reference example does. The
 * queue's depth follows kode_sys.h's one queue rule ("Queue depth"): no
 * request, the one-shot tick, a button's press and release, and
 * TOUCH_EVT_SIG, which AO_Touch publishes at most every KODE_TOUCH_POLL_MS
 * (10 ms) while a finger moves. The stack is the launcher renderer's size:
 * LVGL renders on it, and ours only paints pixels the render pass copies. */
static QEvtPtr     l_queue[KQP_AO_QUEUE_LEN];
static StackType_t l_stack[KQP_AO_STACK_BYTES / sizeof(StackType_t)];

static DragonAo l_ao;
QActive * const AO_Dragon = &l_ao.super;

static QState Dragon_initial(DragonAo * const me, void const * const par);
static QState Dragon_running(DragonAo * const me, QEvt const * const e);
static QState Dragon_waiting(DragonAo * const me, QEvt const * const e);
static QState Dragon_drawing(DragonAo * const me, QEvt const * const e);

/* ---- What is on the glass ------------------------------------------------ */

/**
 * @brief Build the one screen: a full-glass canvas the dragon is painted into,
 *        and one line of hints at the bottom.
 *
 * Nothing here draws. LVGL marks what changed and the next pass renders it.
 *
 * @param me The AO.
 */
static void ui_build(DragonAo * const me)
{
    lv_obj_t * const scr = lv_screen_active();
    lv_obj_t * hint;

    /* Black, opaque: LVGL's theme would otherwise paint the screen itself. */
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    me->stride = (int32_t)lv_draw_buf_width_to_stride((uint32_t)me->w,
                                                      LV_COLOR_FORMAT_RGB565);
    me->fb = (uint16_t *)heap_caps_malloc((size_t)me->stride * (size_t)me->h
                                              * sizeof(uint16_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if ( me->fb == NULL )
    {
        KLOG_E(TAG, "the canvas's pixels did not come");
        return;
    }

    dragon_init(&me->dragon, (int)me->w, (int)me->h);
    dragon_set_auto(&me->dragon, true);

    me->canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(me->canvas, me->fb, me->w, me->h,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(me->canvas, 0, 0);

    hint = lv_label_create(scr);
    lv_label_set_text(hint, "Doigt: guider  TOP: feu  RIGHT: pilote");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

/**
 * @brief One step of the dragon, one paint, then kscreen's render pass.
 *
 * @param me The AO.
 */
static void frame(DragonAo * const me)
{
    uint32_t now = kode_sys_time_ms();
    static uint32_t last_ms;
    uint32_t dt_ms = now - last_ms;
    int x1, y1, x2, y2;
    static int prev_x1, prev_y1, prev_x2 = -1, prev_y2;

    last_ms = now;

    dragon_step(&me->dragon, (float)dt_ms * 0.001f);

    memset(me->fb, 0, (size_t)me->stride * (size_t)me->h * sizeof(uint16_t));
    dr_dirty_reset();
    dragon_render(&me->dragon, me->fb, me->stride, (int)me->w, (int)me->h);
    dr_dirty_get(&x1, &y1, &x2, &y2);

    if ( x1 < 0 )
    {
        x1 = 0;
    }
    if ( y1 < 0 )
    {
        y1 = 0;
    }
    if ( x2 > me->w - 1 )
    {
        x2 = me->w - 1;
    }
    if ( y2 > me->h - 1 )
    {
        y2 = me->h - 1;
    }

    /* The glass keeps every pixel of the last flush: what this round erased
     * (the dragon where it was) is square in the previous area, what it drew
     * is square in this one, so only the union is safe to flush again. */
    if ( prev_x2 >= prev_x1 )
    {
        if ( prev_x1 < x1 )
        {
            x1 = prev_x1;
        }
        if ( prev_y1 < y1 )
        {
            y1 = prev_y1;
        }
        if ( prev_x2 > x2 )
        {
            x2 = prev_x2;
        }
        if ( prev_y2 > y2 )
        {
            y2 = prev_y2;
        }
    }
    prev_x1 = (x2 >= x1) ? x1 : 0;
    prev_y1 = y1;
    prev_x2 = x2;
    prev_y2 = (x2 >= x1) ? y2 : -1;

    if ( x1 <= x2 )
    {
        lv_area_t a;

        a.x1 = (int32_t)x1;
        a.y1 = (int32_t)y1;
        a.x2 = (int32_t)x2;
        a.y2 = (int32_t)y2;
        lv_obj_invalidate_area(me->canvas, &a);
    }

    /* The LED follows the mood: red for fire, green for its own flight. */
    {
        uint8_t want;

        if ( dragon_breathing(&me->dragon) )
        {
            want = 2U;
        }
        else if ( dragon_get_auto(&me->dragon) )
        {
            want = 1U;
        }
        else
        {
            want = 0U;
        }
        if ( want != me->led )
        {
            if ( 2U == want )
            {
                (void)kode_sys_led_set_rgb(DRG_LED_FIRE_R, DRG_LED_FIRE_G,
                                           DRG_LED_FIRE_B);
            }
            else if ( 1U == want )
            {
                (void)kode_sys_led_set_rgb(DRG_LED_AUTO_R, DRG_LED_AUTO_G,
                                           DRG_LED_AUTO_B);
            }
            else
            {
                (void)kode_sys_led_set_rgb(DRG_LED_HAND_R, DRG_LED_HAND_G,
                                           DRG_LED_HAND_B);
            }
            me->led = want;
        }
    }
}

/* ---- The AO ---------------------------------------------------------------- */

void DragonAo_ctor(void)
{
    DragonAo * const me = &l_ao;

    QActive_ctor(&me->super, Q_STATE_CAST(&Dragon_initial));
    kscreen_ctor(&me->screen, &me->super, (enum_t)DRG_TICK_SIG);
    me->canvas    = NULL;
    me->fb        = NULL;
    me->stride    = 0;
    me->w         = 0;
    me->h         = 0;
    me->fire_hold = false;
    me->finger    = false;
    me->press_ms  = 0U;
    me->led       = 255U;
}

void DragonAo_start(void)
{
    QActive_setAttr(AO_Dragon, TASK_NAME_ATTR, "dragon");

    /* KODE_PRIO_USER_GUI, not KODE_PRIO_USER_LOW: it is the level the SDK keeps
     * for a renderer, just above AO_Net and below every owner. Rendering is the
     * heaviest work in the system and the least urgent, and here a slow frame
     * can only ever delay itself. */
    QACTIVE_START(AO_Dragon, KODE_PRIO_USER_GUI,
                  l_queue, Q_DIM(l_queue),
                  l_stack, sizeof(l_stack),
                  (void *)0);
}

/**
 * @brief The initial transition: subscribe, then ask.
 *
 * It runs in the thread that calls DragonAo_start(), before the AO's own task
 * exists, and that is after kode_init() has returned - the condition
 * kscreen_start()'s question needs. kscreen_start() subscribes before it asks:
 * the answer can come the instant the question is posted, and an AO that asked
 * first would miss it.
 *
 * @param me  The AO.
 * @param par Unused.
 * @return The transition to Dragon_waiting.
 */
static QState Dragon_initial(DragonAo * const me, void const * const par)
{
    (void)par;

    QActive_subscribe(&me->super, BUTTON_EVT_SIG);
    kscreen_start(&me->screen);

    KLOG_I(TAG, "up; asked AO_Display whether the panel is there");
    return Q_TRAN(&Dragon_waiting);
}

/**
 * @brief The parent of both states: what is true whether or not there is a
 *        picture. The bottom button resets the board.
 *
 * @param me The AO; unused, nothing here needs it.
 * @param e  The event.
 * @return Q_HANDLED() for what it handles, Q_SUPER() otherwise.
 */
static QState Dragon_running(DragonAo * const me, QEvt const * const e)
{
    QState status;

    (void)me;
    switch ( e->sig )
    {
        case BUTTON_EVT_SIG: {
            ButtonEvt const * const be = (ButtonEvt const *)e;

            /* Posts to AO_Boot, which resets the board a moment later: into
             * kodeOS after a launcher start, into this app again after a
             * mailbox install. */
            if ( be->pressed && (KODE_SYS_BUTTON_BOT == (uint32_t)be->id) )
            {
                KLOG_I(TAG, "resetting");
                if ( KODE_OK != kode_sys_exit_to_os() )
                {
                    KLOG_W(TAG, "AO_Boot is busy; press again");
                }
            }
            status = Q_HANDLED();
            break;
        }
        default: {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }

    return status;
}

/**
 * @brief Waiting for AO_Display's answer: LVGL is not started yet.
 *
 * @param me The AO.
 * @param e  The event.
 * @return Q_HANDLED(), a transition to Dragon_drawing, or Q_SUPER().
 */
static QState Dragon_waiting(DragonAo * const me, QEvt const * const e)
{
    QState status;

    switch ( e->sig )
    {
        case DISPLAY_STATE_SIG: {
            DisplayStateEvt const * const st = (DisplayStateEvt const *)e;

            if ( kscreen_open(&me->screen, st) == KODE_OK )
            {
                me->w = lv_display_get_horizontal_resolution(me->screen.disp);
                me->h = lv_display_get_vertical_resolution(me->screen.disp);
                ui_build(me);
                KLOG_I(TAG, "panel up; the dragon wakes");
                status = Q_TRAN(&Dragon_drawing);
            }
            else
            {
                /* kscreen has said why. Stay here: nothing will be drawn, and
                 * the bottom button still resets the board. */
                status = Q_HANDLED();
            }
            break;
        }
        default: {
            status = Q_SUPER(&Dragon_running);
            break;
        }
    }

    return status;
}

/**
 * @brief Drawing: the dragon flies, and the render pass ticks.
 *
 * @param me The AO.
 * @param e  The event.
 * @return Q_HANDLED() for what it handles, Q_SUPER() otherwise.
 */
static QState Dragon_drawing(DragonAo * const me, QEvt const * const e)
{
    QState status;

    switch ( e->sig )
    {
        /* Always to kscreen_tick(), which re-arms the one-shot: a periodic
         * tick would fill the queue behind a slow frame, and a full queue
         * asserts. The frame itself runs first, so what LVGL renders is this
         * step's picture. */
        case DRG_TICK_SIG: {
            if ( me->fb != NULL )
            {
                frame(me);
            }
            kscreen_tick(&me->screen);
            status = Q_HANDLED();
            break;
        }
        case TOUCH_EVT_SIG: {
            TouchEvt const * const te = (TouchEvt const *)e;

            kscreen_touch(&me->screen, te);
            if ( te->pressed )
            {
                me->finger   = true;
                me->press_ms = te->t_ms;
                dragon_set_auto(&me->dragon, false);
                dragon_set_target(&me->dragon, (float)te->x, (float)te->y);
            }
            else
            {
                if ( me->finger && !te->stale
                     && ((te->t_ms - me->press_ms) < DRG_TAP_MS) )
                {
                    dragon_burst(&me->dragon);
                    (void)kode_sys_haptic_play_buzz(120U, 45U);
                }
                me->finger = false;
            }
            status = Q_HANDLED();
            break;
        }
        case BUTTON_EVT_SIG: {
            ButtonEvt const * const be = (ButtonEvt const *)e;

            if ( KODE_SYS_BUTTON_TOP == (uint32_t)be->id )
            {
                dragon_hold_fire(&me->dragon, be->pressed);
                if ( be->pressed )
                {
                    (void)kode_sys_haptic_play_buzz(120U, 45U);
                }
            }
            else if ( be->pressed && (KODE_SYS_BUTTON_RIGHT == (uint32_t)be->id) )
            {
                dragon_set_auto(&me->dragon, !dragon_get_auto(&me->dragon));
                KLOG_I(TAG, "auto flight %s", dragon_get_auto(&me->dragon) ? "on" : "off");
            }
            else
            {
                /* The bottom button is the parent's. */
                status = Q_SUPER(&Dragon_running);
                break;
            }
            status = Q_HANDLED();
            break;
        }
        case DISPLAY_REPAINT_SIG: {
            kscreen_repaint(&me->screen);
            status = Q_HANDLED();
            break;
        }
        case DISPLAY_PARK_SIG: {
            kscreen_park(&me->screen, ((DisplayParkEvt const *)e)->seq);
            status = Q_HANDLED();
            break;
        }
        case POWER_CRITICAL_SIG: {
            kscreen_critical(&me->screen, (PowerCriticalEvt const *)e);
            status = Q_HANDLED();
            break;
        }
        default: {
            status = Q_SUPER(&Dragon_running);
            break;
        }
    }

    return status;
}
