// Copyright 2026 toshi-v
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sun_protocol.h"

#include "quantum.h"
#include "eeconfig.h"
#include "host.h"
#include "led.h"
#include "debug.h"
#include "uart.h"

/* Only the click setting is persisted. The bell is a continuous tone that only
 * stops when it is switched off again, so it always comes up silent. */
typedef union {
    uint32_t raw;
    struct {
        bool click : 1;
    };
} sun_config_t;

static sun_config_t sun_config;
static bool         bell_on = false;

void sun_command(uint8_t command) {
    uart_write(command);
}

void sun_leds_refresh(void) {
    led_t led_state = host_keyboard_led_state();

    /* Sun LED byte: bit0 NumLock, bit1 Compose, bit2 ScrollLock, bit3 CapsLock.
     * All four go out regardless; a keyboard with fewer lamps - a compact board
     * has two - simply ignores the bits it cannot show. */
    uint8_t sun_leds = 0;
    if (led_state.num_lock) sun_leds |= (1 << 0);
    if (led_state.compose) sun_leds |= (1 << 1);
    if (led_state.scroll_lock) sun_leds |= (1 << 2);
    if (led_state.caps_lock) sun_leds |= (1 << 3);

    sun_command(SUN_CMD_LED);
    sun_command(sun_leds);
}

void sun_settings_refresh(void) {
    sun_command(sun_config.click ? SUN_CMD_CLICK_ON : SUN_CMD_CLICK_OFF);
    sun_command(bell_on ? SUN_CMD_BELL_ON : SUN_CMD_BELL_OFF);
}

bool led_update_kb(led_t led_state) {
    bool res = led_update_user(led_state);
    if (res) {
        sun_leds_refresh();
    }
    return res;
}

void eeconfig_init_kb(void) {
    sun_config.raw   = 0;
    sun_config.click = false;
    eeconfig_update_kb(sun_config.raw);

    eeconfig_init_user();
}

void keyboard_post_init_kb(void) {
    /* Has to be here and not in matrix_init_custom(): matrix_init() runs before
     * quantum_init(), which loads debug_config from EEPROM and would overwrite
     * this. Without it every dprintf() in matrix.c is a no-op branch and a
     * console build stays silent. Costs nothing on a build without a console. */
    debug_enable = true;

    sun_config.raw = eeconfig_read_kb();
    sun_settings_refresh();

    keyboard_post_init_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch (keycode) {
            case SUN_CLICK:
                sun_config.click = !sun_config.click;
                eeconfig_update_kb(sun_config.raw);
                sun_command(sun_config.click ? SUN_CMD_CLICK_ON : SUN_CMD_CLICK_OFF);
                return false;

            case SUN_BELL:
                bell_on = !bell_on;
                sun_command(bell_on ? SUN_CMD_BELL_ON : SUN_CMD_BELL_OFF);
                return false;

            case SUN_RST:
                sun_command(SUN_CMD_RESET);
                return false;
        }
    }

    return process_record_user(keycode, record);
}
