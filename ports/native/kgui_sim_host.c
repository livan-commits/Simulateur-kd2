/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Host profile: official KGUI screen builders, without firmware-only screens. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kui_sim.h"
#include "kgui_screens.h"

#define SUBJECT_TEXT_LEN 256U

lv_obj_t *kgui_screen_dark = NULL;
lv_obj_t *kgui_screen_loading = NULL;
lv_obj_t *kgui_screen_settings = NULL;

lv_subject_t *kgui_subject_app_count = NULL;
lv_subject_t *kgui_subject_empty_text = NULL;
lv_subject_t *kgui_subject_loading_text = NULL;
lv_subject_t *kgui_subject_tp_text = NULL;

static char empty_text[SUBJECT_TEXT_LEN], empty_prev[SUBJECT_TEXT_LEN];
static char loading_text[SUBJECT_TEXT_LEN], loading_prev[SUBJECT_TEXT_LEN];
static char tp_text[SUBJECT_TEXT_LEN], tp_prev[SUBJECT_TEXT_LEN];

static lv_obj_t *get_face(void) { return kgui_screen_face; }
static lv_obj_t *get_launcher(void) { return kgui_screen_launcher; }
static lv_obj_t *get_touch_test(void) { return kgui_screen_touch_test; }

const kui_sim_screen_t kui_sim_screens[] = {
    { "face", get_face },
    { "launcher", get_launcher },
    { "touch_test", get_touch_test },
};
const uint32_t kui_sim_screen_count =
    (uint32_t)(sizeof(kui_sim_screens) / sizeof(kui_sim_screens[0]));

static lv_subject_t *int_subject(int32_t value)
{
    lv_subject_t *subject = lv_subject_create(LV_SUBJECT_TYPE_INT);
    if (subject != NULL) lv_subject_set_int(subject, value);
    return subject;
}

static lv_subject_t *string_subject(char *buf, char *prev, const char *value)
{
    lv_subject_t *subject = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    if (subject != NULL) {
        lv_subject_set_string_buffer_static(subject, buf, prev, SUBJECT_TEXT_LEN);
        lv_subject_set_string(subject, value);
    }
    return subject;
}

bool kgui_clock_known(int32_t hour, int32_t minute)
{
    return hour >= 0 && hour < KGUI_CLOCK_HOURS &&
           minute >= 0 && minute < KGUI_CLOCK_MINUTES;
}

int32_t kgui_clock_hour(int32_t hour, bool h12)
{
    if (!h12) return hour;
    int32_t value = hour % KGUI_CLOCK_HOURS_12;
    return value == 0 ? KGUI_CLOCK_HOURS_12 : value;
}

void kgui_screens_init(void)
{
    if (kgui_subject_app_count != NULL) return;
    kgui_subject_app_count = int_subject(0);
    kgui_subject_empty_text = string_subject(
        empty_text, empty_prev, "No apps yet.        ");
    kgui_subject_loading_text = string_subject(
        loading_text, loading_prev, "Installing...                             ");
    kgui_subject_tp_text = string_subject(
        tp_text, tp_prev, "Tap the target  (1/5)                     ");

    /* These are the unchanged KodeOS screen builders. */
    kgui_screen_face_build();
    kgui_screen_launcher_build();
    kgui_screen_touch_test_build();
}

void kui_sim_setup(void)
{
    kgui_screens_init();
}

static bool parse_int(const char *value, int32_t *out)
{
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0') return false;
    *out = (int32_t)parsed;
    return true;
}

bool kui_sim_set(const char *subject, const char *value)
{
    int32_t number = 0;
    if (strcmp(subject, "app_count") == 0) {
        if (!parse_int(value, &number)) return false;
        lv_subject_set_int(kgui_subject_app_count, number);
        return true;
    }
    if (strcmp(subject, "empty_text") == 0) {
        lv_subject_set_string(kgui_subject_empty_text, value);
        return true;
    }
    if (strcmp(subject, "tp_text") == 0) {
        lv_subject_set_string(kgui_subject_tp_text, value);
        return true;
    }
    return false;
}
