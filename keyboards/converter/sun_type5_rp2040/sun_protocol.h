// Copyright 2026 toshi-v
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "keycodes.h"

/* Commands the host sends to the keyboard. */
enum sun_command {
    SUN_CMD_RESET     = 0x01,
    SUN_CMD_BELL_ON   = 0x02,
    SUN_CMD_BELL_OFF  = 0x03,
    SUN_CMD_CLICK_ON  = 0x0A,
    SUN_CMD_CLICK_OFF = 0x0B,
    SUN_CMD_LED       = 0x0E,
    SUN_CMD_LAYOUT    = 0x0F,
};

/* Bytes the keyboard sends back that are not key events. */
enum sun_response {
    SUN_RSP_IDLE       = 0x7F, // no keys held
    SUN_RSP_RESET_FAIL = 0x7E, // followed by an error code
    SUN_RSP_LAYOUT     = 0xFE, // followed by the DIP switch layout id
    SUN_RSP_RESET_OK   = 0xFF, // followed by the keyboard id (0x04 = Type 4/5)
};

#define SUN_KEYBOARD_ID_TYPE4_5 0x04

/* Keycodes exposed to Vial as "custom keycodes"; the order must match the
 * customKeycodes array in keymaps/vial/vial.json. */
enum sun_keycodes {
    SUN_CLICK = QK_KB_0, // toggle the keyboard's key click
    SUN_BELL,            // toggle the keyboard's bell
    SUN_RST,             // re-run the keyboard's self test and resync its LEDs
};

/* Send a single command byte to the keyboard. */
void sun_command(uint8_t command);

/* Push the host's lock LED state to the keyboard. */
void sun_leds_refresh(void);

/* Re-apply click and bell state; used after the keyboard has been reset. */
void sun_settings_refresh(void);

/* Layout id reported by the keyboard's DIP switches, 0xFF until it answers. */
uint8_t sun_layout_id(void);
