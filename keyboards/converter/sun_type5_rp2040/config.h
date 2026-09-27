// Copyright 2026 toshi-v
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* The Sun keyboard reports a 7 bit key id per event, which is used directly as
 * a virtual matrix position: row = id >> 3, col = id & 7. */
#define MATRIX_ROWS 16
#define MATRIX_COLS 8

/* ---------------------------------------------------------------------------
 * Sun serial link
 *
 * The Type 5 talks 1200 baud 8N1 over a plain TTL line, but with *inverted*
 * polarity: the line idles low and a mark bit is 0 V. The RP2040 pad logic can
 * invert a peripheral signal on its way in and out (IO_BANK0 INOVER/OUTOVER),
 * so UART0 is used as-is and no 7404 style inverter is needed - only the level
 * shifting described in readme.md.
 *
 * Waveshare RP2040-Zero pin  ->  8 pin mini DIN
 *   GP0 (UART0 TX)          ->   pin 5, keyboard RX
 *   GP1 (UART0 RX)          <-   pin 6, keyboard TX
 * -------------------------------------------------------------------------*/
#define UART_DRIVER SIOD0 // ChibiOS names RP2040's UART0 "SIOD0"
#define UART_TX_PIN GP0
#define UART_RX_PIN GP1

#ifndef SUN_NO_SIGNAL_INVERSION
#    define UART_TX_PAL_MODE (PAL_MODE_ALTERNATE_UART | PAL_RP_IOCTRL_OUTOVER_DRVINVPERI)
#    define UART_RX_PAL_MODE (PAL_MODE_ALTERNATE_UART | PAL_RP_IOCTRL_INOVER_INV)
#endif

/* Tap the RP2040-Zero's RESET button twice to drop into the UF2 bootloader,
 * so the board does not have to be unplugged to reflash it. */
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_TIMEOUT 500U

/* Emulated EEPROM. The defaults (4 KiB usable) leave very little room for
 * macros once four layers of a 128 key matrix are stored, so give it more. */
#define WEAR_LEVELING_LOGICAL_SIZE 8192
#define WEAR_LEVELING_BACKING_SIZE 16384
