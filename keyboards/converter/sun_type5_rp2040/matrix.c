// Copyright 2026 toshi-v
// SPDX-License-Identifier: GPL-2.0-or-later

#include "matrix.h"

#include "sun_protocol.h"
#include "debug.h"
#include "timer.h"
#include "uart.h"

#include <hal.h>

/*
 * A Sun keyboard is not scanned - it pushes one byte per key event down a
 * 1200 baud serial line:
 *
 *   bit 7    0 = make, 1 = break
 *   bit 6..0 key id
 *
 * The key id doubles as the position in a virtual 16x8 matrix, which is what
 * the LAYOUT macros and Vial are addressing:
 *
 *      8 bit wide
 *     +---------+
 *   0 |00 ... 07|
 *   1 |08 ... 0F|
 *   : |   ...   |
 *   E |70 ... 77|
 *   F |78 ... 7F|
 *     +---------+
 */
#define SUN_ROW(code) (((code) >> 3) & 0x0F)
#define SUN_COL(code) ((code) & 0x07)

#define SUN_BAUD 1200

/* How often to poke a silent keyboard, so that plugging the keyboard in after the
 * converter still gets the LEDs and the click setting in sync. */
#define SUN_PROBE_RETRY_MS 3000

/* The first probe waits instead of firing at boot. Converter and keyboard come up
 * on the same USB power, and the keyboard spends its first second running its own
 * power on self test with its receiver deaf, so anything sent earlier is thrown
 * away. Missing the window is not fatal, it just falls through to the retry
 * above. */
#define SUN_PROBE_FIRST_MS 1000

/* What the probe actually sends. 0x0F (layout) is a plain query: the keyboard
 * answers 0xFE plus its layout id and does nothing else. 0x01 (reset) would also
 * do the job and would hand back the keyboard id as well, but it makes the
 * keyboard re-run its self test - a beep and a flash of the lock LEDs - which on
 * a plug in is the *second* self test the user sees, because the keyboard already
 * ran one when the power came up. There is no way to have both a reset at startup
 * and a single self test, so the query wins and the keyboard id is left to the
 * SUN_RST keycode. Define SUN_RESET_AT_STARTUP to probe with a reset instead. */
#ifdef SUN_RESET_AT_STARTUP
#    define SUN_PROBE_CMD SUN_CMD_RESET
#else
#    define SUN_PROBE_CMD SUN_CMD_LAYOUT
#endif

/* How often the link report below is printed. It keeps printing for the life of
 * the firmware, because `qmk console` is usually attached well after boot and
 * anything printed before it connects is dropped on the floor. */
#define SUN_LINK_REPORT_MS 5000

/* Some responses are two bytes long; the second byte is consumed here. */
typedef enum {
    SUN_RX_EVENT,      // normal key events
    SUN_RX_RESET_ID,   // after 0xFF: the keyboard id
    SUN_RX_RESET_CODE, // after 0x7E: the self test error code
    SUN_RX_LAYOUT_ID,  // after 0xFE: the DIP switch layout id
} sun_rx_state_t;

static sun_rx_state_t rx_state       = SUN_RX_EVENT;
static bool           kbd_responding = false;
static bool           kbd_synced     = false;
static uint16_t       probe_timer    = 0;
static uint8_t        layout_id      = 0xFF;
static uint8_t        keyboard_id    = 0xFF;

/* Link statistics, for the bring up report below. */
static uint16_t    probe_delay  = SUN_PROBE_FIRST_MS;
static uint32_t    boot_timer   = 0;
static uint32_t    rx_bytes     = 0;
static uint32_t    rx_faults    = 0;
static sioevents_t rx_errors    = 0;
static uint16_t    report_timer = 0;

uint8_t sun_layout_id(void) {
    return layout_id;
}

/* Called for every byte only a real keyboard sends, so the first one ends the
 * probing. Also the only place that reports how long the link took to come up:
 * roughly SUN_PROBE_FIRST_MS plus the keyboard's answer means the first probe was
 * heard, and a multiple of SUN_PROBE_RETRY_MS on top means it was not. */
static void sun_note_response(void) {
    if (!kbd_responding) {
        kbd_responding = true;
        dprintf("sun: keyboard answered after %u ms\n", (unsigned)timer_elapsed32(boot_timer));
    }
}

/* Push everything the keyboard does not remember for itself. Done once when the
 * keyboard first answers, and again after a reset, which clears its LEDs and its
 * click setting. */
static void sun_resync(void) {
    kbd_synced = true;
    sun_leds_refresh();
    sun_settings_refresh();
}

/* Everything needed to tell a wiring fault from a firmware one, on one line: the
 * handshake result, the receive counters, and what the UART and the two pads
 * actually ended up configured as. A wrong baud divisor or a pad that did not
 * take the polarity inversion looks exactly like a miswired connector from the
 * outside, so the numbers are worth carrying even once the link works. */
static void sun_link_report(void) {
    dprintf("sun: link kbd=%u id=%02X layout=%02X bytes=%u faults=%u err=%03X rxline=%u uart=%u/%u/%03X pads=%06X/%06X\n", (unsigned)kbd_responding, (unsigned)keyboard_id, (unsigned)layout_id, (unsigned)rx_bytes, (unsigned)rx_faults, (unsigned)rx_errors, (unsigned)palReadLine(UART_RX_PIN), (unsigned)UART0->UARTIBRD, (unsigned)UART0->UARTFBRD, (unsigned)UART0->UARTCR, (unsigned)IO_BANK0->GPIO[0].CTRL, (unsigned)IO_BANK0->GPIO[1].CTRL);
    rx_errors = 0;
}

void matrix_init_custom(void) {
    uart_init(SUN_BAUD);

    /* The probe that makes the keyboard announce itself is sent from
     * matrix_scan_custom() below, once the keyboard has had time to boot. Its
     * answer is what tells us the keyboard is there. */
    probe_timer  = timer_read();
    report_timer = probe_timer;
    boot_timer   = timer_read32();
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool changed = false;

    /* Sampled before the drain below, because uart_read() clears these. A line
     * stuck in the wrong state shows up here as a flood of framing or break
     * errors, which is the one symptom that separates a polarity or wiring
     * fault from a keyboard that simply is not plugged in. */
    sioevents_t errors = sioGetAndClearErrorsX(&UART_DRIVER);
    if (errors != 0) {
        rx_errors |= errors;
        rx_faults++;
    }

    while (uart_available()) {
        uint8_t code = uart_read();

        rx_bytes++;
        dprintf("sun: rx %02X\n", code);

        switch (rx_state) {
            case SUN_RX_RESET_ID:
                rx_state    = SUN_RX_EVENT;
                keyboard_id = code;
                dprintf("sun: keyboard id %02X\n", code);
                /* A reset leaves the keyboard with its LEDs cleared and its
                 * click off, so whatever we knew about its state is stale. */
                sun_resync();
                sun_command(SUN_CMD_LAYOUT);
                continue;

            case SUN_RX_RESET_CODE:
                rx_state = SUN_RX_EVENT;
                dprintf("sun: self test failed, %02X\n", code);
                continue;

            case SUN_RX_LAYOUT_ID:
                rx_state  = SUN_RX_EVENT;
                layout_id = code;
                dprintf("sun: layout %02X\n", code);
                /* The startup probe is this query, so this is where a keyboard
                 * that came up alongside the converter gets its LEDs and its
                 * click setting. Skipped when a reset already did it. */
                if (!kbd_synced) {
                    sun_resync();
                }
                continue;

            case SUN_RX_EVENT:
                break;
        }

        switch (code) {
            case SUN_RSP_RESET_OK:
                rx_state = SUN_RX_RESET_ID;
                sun_note_response();
                continue;

            case SUN_RSP_RESET_FAIL:
                rx_state = SUN_RX_RESET_CODE;
                sun_note_response();
                continue;

            case SUN_RSP_LAYOUT:
                rx_state = SUN_RX_LAYOUT_ID;
                sun_note_response();
                continue;

            case SUN_RSP_IDLE:
                /* All keys up. Sent after a reset and whenever the keyboard
                 * settles, so it doubles as a stuck key cure. */
                sun_note_response();
                for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                    if (current_matrix[row]) {
                        current_matrix[row] = 0;
                        changed             = true;
                    }
                }
                continue;
        }

        /* Every remaining byte is a key event - and so is every stray byte a
         * mis-terminated or wrongly polarised line produces. Until the
         * keyboard has actually identified itself, drop them instead of
         * pressing keys nobody touched. */
        if (!kbd_responding) {
            continue;
        }

        uint8_t      row  = SUN_ROW(code);
        matrix_row_t mask = ((matrix_row_t)1) << SUN_COL(code);

        if (code & 0x80) { // break
            if (current_matrix[row] & mask) {
                current_matrix[row] &= ~mask;
                changed = true;
            }
        } else { // make
            if (!(current_matrix[row] & mask)) {
                current_matrix[row] |= mask;
                changed = true;
            }
        }
    }

    if (!kbd_responding && timer_elapsed(probe_timer) > probe_delay) {
        dprintf("sun: no answer yet, probing keyboard\n");
        sun_command(SUN_PROBE_CMD);
        probe_timer = timer_read();
        probe_delay = SUN_PROBE_RETRY_MS;
    }

    /* An idle Sun keyboard sends nothing at all, so without this a console build
     * that is working perfectly is indistinguishable from a dead one. */
    if (timer_elapsed(report_timer) > SUN_LINK_REPORT_MS) {
        report_timer = timer_read();
        sun_link_report();
    }

    return changed;
}
