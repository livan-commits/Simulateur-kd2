#!/usr/bin/env python3
"""Apply the simulator-only headless framebuffer backend to the pinned KodeOS SDK.

The upstream SDK remains pinned and untouched in Git. This overlay replaces only
the physical CO5300/DSI output path with two real RGB565 PSRAM framebuffers.
KodeOS/KGUI still render on the emulated ESP32-P4; the existing KodeOS
"screenshot dst=console" command exports those pixels through the USB console.
"""
from pathlib import Path
import sys

def once(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one patch point, found {count}")
    return source.replace(old, new, 1)

def after_open(source: str, signature: str, body: str, label: str) -> str:
    pos = source.find(signature)
    if pos < 0:
        raise SystemExit(f"{label}: function not found")
    brace = source.find("{", pos + len(signature))
    if brace < 0:
        raise SystemExit(f"{label}: opening brace not found")
    return source[:brace + 1] + "\n#if KODEOS_WEB_HEADLESS_DISPLAY\n" + body + "\n#endif\n" + source[brace + 1:]

def main(root: Path) -> None:
    display = root / "sdk/components/kdrv/display/src/kdrv_display.c"
    if not display.is_file():
        raise SystemExit(f"not a KodeOS checkout: {display} is missing")

    source = display.read_text()
    source = once(
        source,
        '#include "sdkconfig.h"\n',
        '#include "sdkconfig.h"\n#include <stdint.h>\n\n/* Simulator overlay: retain KodeOS/KGUI rendering without a physical DSI panel. */\n#define KODEOS_WEB_HEADLESS_DISPLAY 1\n',
        "headless build switch",
    )

    # Allocate the same two full-size RGB565 framebuffers the physical panel
    # driver normally obtains from esp_lcd_dpi_panel_get_frame_buffer().
    helper_anchor = "static bool                     s_ready;"
    helper = r"""

#if KODEOS_WEB_HEADLESS_DISPLAY
volatile uint32_t g_kode_web_fb_front = 0U;
volatile uint32_t g_kode_web_fb_seq = 0U;
volatile uint32_t g_kode_web_fb_dirty_y1 = 0U;
volatile uint32_t g_kode_web_fb_dirty_y2 = KBSP_LCD_V_RES;

static bool web_headless_buffers_init(void)
{
    if ( (NULL != s_fb[0]) && (NULL != s_fb[1]) )
    {
        return true;
    }
    s_fb[0] = khal_alloc_psram(KDRV_DISPLAY_CACHE_LINE, KDRV_DISPLAY_FB_BYTES);
    s_fb[1] = khal_alloc_psram(KDRV_DISPLAY_CACHE_LINE, KDRV_DISPLAY_FB_BYTES);
    if ( (NULL == s_fb[0]) || (NULL == s_fb[1]) )
    {
        khal_free(s_fb[0]);
        khal_free(s_fb[1]);
        s_fb[0] = NULL;
        s_fb[1] = NULL;
        return false;
    }
    (void)memset(s_fb[0], 0, KDRV_DISPLAY_FB_BYTES);
    (void)memset(s_fb[1], 0, KDRV_DISPLAY_FB_BYTES);
    s_back         = 1U;
    s_frame_open   = false;
    s_flip_pending = false;
    s_dirty_y1     = KBSP_LCD_V_RES;
    s_dirty_y2     = 0;
    s_prev_y1      = 0;
    s_prev_y2      = KBSP_LCD_V_RES;
    g_kode_web_fb_front = (uint32_t)(uintptr_t)s_fb[0];
    g_kode_web_fb_dirty_y1 = 0U;
    g_kode_web_fb_dirty_y2 = KBSP_LCD_V_RES;
    g_kode_web_fb_seq = 2U;
    return true;
}

static void web_framebuffer_publish(uint8_t *front, int32_t y1, int32_t y2)
{
    if ( NULL == front )
    {
        return;
    }
    (void)y1;
    (void)y2;
    (void)esp_cache_msync(front, KDRV_DISPLAY_FB_BYTES, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    ++g_kode_web_fb_seq; /* odd: publishing */
    g_kode_web_fb_front = (uint32_t)(uintptr_t)front;
    /* The browser polls more slowly than LVGL flushes. Publish a full-frame
       snapshot so several intermediate dirty rectangles cannot be lost. */
    g_kode_web_fb_dirty_y1 = 0U;
    g_kode_web_fb_dirty_y2 = KBSP_LCD_V_RES;
    ++g_kode_web_fb_seq; /* even: stable */
}
#endif
"""
    source = once(source, helper_anchor, helper_anchor + helper, "framebuffer allocator")

    init_anchor = "    uint8_t percent = 0U;\n\n    if ( panel_note_take(&percent) )"
    init_block = """#if KODEOS_WEB_HEADLESS_DISPLAY
    if ( !web_headless_buffers_init() )
    {
        KLOG_E(TAG, "web framebuffer allocation failed (%u bytes x2)",
               (unsigned)KDRV_DISPLAY_FB_BYTES);
        return KODE_NO_RESOURCES;
    }
    s_video_owed = false;
    s_asleep     = false;
    s_keep_lit   = false;
    s_held       = false;
    s_held_pct   = 100U;
    s_last_pct   = 100U;
    s_up_ms      = khal_time_ms();
    s_ready      = true;
    s_brought_up = true;
    KLOG_I(TAG, "web framebuffer ready: %ux%u RGB565; physical MIPI-DSI bypassed",
           (unsigned)KBSP_LCD_H_RES, (unsigned)KBSP_LCD_V_RES);
    return KODE_OK;
#endif

"""
    source = once(source, init_anchor, init_block + init_anchor, "headless display init")

    inplace_anchor = "    s_flipping = true;\n    err = esp_lcd_panel_draw_bitmap(s_panel, 0, (int)sync_y1,"
    inplace_block = """#if KODEOS_WEB_HEADLESS_DISPLAY
    s_prev_y1      = y1;
    s_prev_y2      = y2;
    s_back         = (uint8_t)(shown ^ 1U);
    s_flip_pending = false;
    web_framebuffer_publish(s_fb[shown], y1, y2);
#if CONFIG_KODEOS_SCREENSHOT
    shown_rows(y1, y2);
#endif
    khal_sem_signal(s_flush_done);
    return KODE_OK;
#endif
"""
    source = once(source, inplace_anchor, inplace_block + inplace_anchor, "in-place framebuffer flip")

    external_anchor = "    /* The flip. Handing draw_bitmap a pointer INSIDE a framebuffer makes\n"
    external_block = """#if KODEOS_WEB_HEADLESS_DISPLAY
    if ( s_dirty_y2 > s_dirty_y1 )
    {
        s_prev_y1      = s_dirty_y1;
        s_prev_y2      = s_dirty_y2;
        s_back         = (uint8_t)(s_back ^ 1U);
        s_flip_pending = false;
        web_framebuffer_publish(s_fb[s_back ^ 1U], s_dirty_y1, s_dirty_y2);
#if CONFIG_KODEOS_SCREENSHOT
        shown_rows(s_dirty_y1, s_dirty_y2);
#endif
    }
    s_dirty_y1   = KBSP_LCD_V_RES;
    s_dirty_y2   = 0;
    s_frame_open = false;
    khal_sem_signal(s_flush_done);
    return KODE_OK;
#endif

"""
    source = once(source, external_anchor, external_block + external_anchor, "external-buffer framebuffer flip")

    # Brightness has no physical target in this build, but retain the state
    # expected by the rest of AO_Display.
    source = after_open(
        source,
        "kode_status_t kdrv_display_brightness(uint8_t percent)",
        """    if ( !s_ready )
    {
        return KODE_DENIED;
    }
    if ( BRIGHT_PCT_MAX < percent )
    {
        percent = (uint8_t)BRIGHT_PCT_MAX;
    }
    s_last_pct = percent;
    return KODE_OK;""",
        "headless brightness",
    )

    source = after_open(
        source,
        "void kdrv_display_link(kdrv_display_link_t *out)",
        """    if ( NULL == out )
    {
        return;
    }
    (void)memset(out, 0, sizeof(*out));
    if ( s_ready )
    {
        uint64_t const elapsed_ms = (uint64_t)(khal_time_ms() - s_up_ms);
        out->frames = (uint32_t)((elapsed_ms * 60ULL) / 1000ULL);
    }
    return;""",
        "headless virtual scanout metrics",
    )

    # Low-power link operations must not tear down the RAM framebuffer.
    source = after_open(
        source,
        "kode_status_t kdrv_display_link_set(bool up)",
        """    (void)up;
    return s_ready ? KODE_OK : KODE_DENIED;""",
        "headless link control",
    )
    source = after_open(
        source,
        "kode_status_t kdrv_display_panel_sleep(bool sleep)",
        """    if ( !s_ready )
    {
        return KODE_DENIED;
    }
    s_asleep = sleep;
    return KODE_OK;""",
        "headless panel sleep",
    )
    source = after_open(
        source,
        "kode_status_t kdrv_display_keep_for_restart(void)",
        """    return KODE_DENIED;""",
        "headless keep-for-restart",
    )

    display.write_text(source)

    # kgui_ao.c assumes the radio link is always compiled into kodeOS: its
    # Wi-Fi and BLE row painters call helpers that live inside
    # CONFIG_KODEOS_WIFI / CONFIG_KODEOS_BLE blocks. With the radio held in
    # reset (no C5 exists in the emulator) those guards close, so the three
    # unguarded call sites need radio-less spellings that read "Off", the
    # same word kgui_radio_words() picks for a stack that is not running.
    kgui = root / "components/kgui/kgui_ao.c"
    if not kgui.is_file():
        raise SystemExit(f"not a KodeOS checkout: {kgui} is missing")
    kgui_source = kgui.read_text()
    kgui_source = once(
        kgui_source,
        """        case GUI_MORE_WIFI: {
            const char * const w = gui_radio_words(me, me->wifi_on, me->wifi_linked,
                                                   me->wifi_reconnecting);
            const char *say = me->wifi_ssid;

            if ( NULL != w )
            {
                say = w;
            }
            (void)snprintf(out, cap, "%s", say);
            break;
        }""",
        """        case GUI_MORE_WIFI: {
#if CONFIG_KODEOS_WIFI
            const char * const w = gui_radio_words(me, me->wifi_on, me->wifi_linked,
                                                   me->wifi_reconnecting);
            const char *say = me->wifi_ssid;

            if ( NULL != w )
            {
                say = w;
            }
            (void)snprintf(out, cap, "%s", say);
#else
            (void)snprintf(out, cap, "%s", "Off");
#endif
            break;
        }""",
        "kgui Wi-Fi row without a radio",
    )
    kgui_source = once(
        kgui_source,
        """        case GUI_MORE_BLE: {
            (void)snprintf(out, cap, "%s", gui_radio_words(me, me->ble_on, false, false));
            break;
        }""",
        """        case GUI_MORE_BLE: {
#if CONFIG_KODEOS_BLE
            (void)snprintf(out, cap, "%s", gui_radio_words(me, me->ble_on, false, false));
#else
            (void)snprintf(out, cap, "%s", "Off");
#endif
            break;
        }""",
        "kgui BLE row without a radio",
    )
    kgui_source = once(
        kgui_source,
        """            if ( me->ble_name_owed && kode_up() )
            {
                me->ble_name_owed = false;
                gui_ble_name_send(me);
            }""",
        """#if CONFIG_KODEOS_BLE
            if ( me->ble_name_owed && kode_up() )
            {
                me->ble_name_owed = false;
                gui_ble_name_send(me);
            }
#endif""",
        "kgui BLE name push without a radio",
    )
    kgui.write_text(kgui_source)

    # The JPEG mirror depends on PPA/JPEG hardware and is not needed: the
    # screenshot verb already serializes the actual RGB565 framebuffer as BMP.
    defaults = root / "sdkconfig.defaults"
    config = defaults.read_text() if defaults.is_file() else ""
    for key, value in (
        # ESP-EMU's browser terminal is UART0. Keep the console bidirectional
        # there instead of selecting the USB-Serial/JTAG endpoint.
        ("CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG", "n"),
        ("CONFIG_ESP_CONSOLE_UART_DEFAULT", "y"),
        # The dock CDC-ACM port claims the USB OTG console; browser input is UART0.
        ("CONFIG_KHAL_CONSOLE_DOCK", "n"),
        # No ST25R3916 on the emulated SPI2: its floating IRQ line would fire
        # without end and the nfc_irq servicer would eat CPU 0 whole before
        # the console ever runs.
        ("CONFIG_KODEOS_NFC", "n"),
        # No C5 on the emulated SDIO bus: esp-hosted retries sdmmc_card_init
        # every ~120 emulated ms forever, and that storm keeps the emulated
        # firmware compute-bound at a fraction of real time even on a native
        # host. Radio off holds the C5 in reset; WiFi cannot work here anyway.
        ("CONFIG_KODEOS_RADIO", "n"),
        # AO_TestDot (the E5 bench test) requires the radio link; there is no
        # station on the other end of a browser, so its AO is dead weight here.
        ("CONFIG_KODE_TEST_DOT", "n"),
    ):
        lines = [line for line in config.splitlines()
                 if not line.startswith(key + "=")
                 and line != f"# {key} is not set"]
        lines.append(f"{key}={value}")
        config = "\n".join(lines) + "\n"
    defaults.write_text(config)

    print(f"Applied headless framebuffer overlay to {display}")
    print(f"Selected the bidirectional UART0 console in {defaults}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: headless_display.py <KodeOS checkout root>")
    main(Path(sys.argv[1]).resolve())
