# The Waveshare RP2040-Zero is a plain RP2040 board; the QMK default for this
# MCU is the Pro Micro RP2040, whose pin defaults do not apply here.
BOARD = GENERIC_RP_RP2040

# The "matrix" is fed by the serial link instead of being scanned, so only the
# lite half of the custom matrix API is implemented; QMK keeps ownership of the
# matrix state itself.
CUSTOM_MATRIX = lite

UART_DRIVER_REQUIRED = yes

SRC += matrix.c sun_protocol.c
