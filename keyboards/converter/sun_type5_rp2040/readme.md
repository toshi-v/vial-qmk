# Sun Type 5 to USB converter (Waveshare RP2040-Zero)

Turns a serial Sun keyboard - Type 4, 5, 5c or 6 - into a USB keyboard, with live
remapping over [Vial](https://get.vial.today): layers, macros, combos and tap
dance are edited in the Vial GUI and stored in the RP2040's flash, so the
firmware only has to be built and flashed once.

Bell, key click and the lock LEDs of the original keyboard all work. The LEDs
follow the host; the bell and the click are driven by keycodes that only the
`vial` keymap binds, so on the `default` keymap they stay as they were.

    Keyboard Maintainer: toshi-v
    Hardware Supported:  Waveshare RP2040-Zero + any Sun Type 4/5/6 protocol keyboard
    Hardware Availability: self built

Built and tested against a **Fujitsu Takamisawa FKB8579-T001**, a US layout
compact board that speaks PS/2, Mac ADB or Sun Type 6 depending on how its DIP
switches are set - here, Sun Type 6. See "Compatibility" below for what that does
and does not imply about other keyboards.

## Compatibility

The serial Sun keyboards - **Type 4, Type 5, Type 5c and Type 6** - all speak one
protocol: 1200 baud 8N1 inverted TTL over the 8 pin mini DIN, the same command
set, the same response bytes, and one shared table of key ids. They all answer a
reset with the id `0x04`, which is literally "Type 4/5". So the converter's link
layer and key handling are family-wide, not specific to one model.

Some keyboards are not Sun keyboards all the time. Multi protocol boards - the
FKB8579 is one - select PS/2, Mac ADB or Sun with DIP switches, and have to be
switched into Sun mode before any of this applies. A board in the wrong mode will
sit there silent, which reads in the log exactly like a keyboard that is not
plugged in: `kbd=0 bytes=0 faults=0 rxline=1`.

What that does **not** cover:

* **USB Sun keyboards** (the later Type 6 and Type 7) are ordinary USB HID and
  have nothing to do with this - plug them straight into the host.
* **Type 3 and earlier** use a different key id table. The AVR converter this one
  derives from keeps `type3` and `type5` as separate keymaps for exactly that
  reason; porting Type 3 here means adding its table, not just rewiring.
* **The drawing, not the function.** `keyboard.json` only carries full size Type 5
  layouts, so a compact or a Type 4 remaps correctly but shows keys in Vial that
  it does not physically have. See "Layout variants" at the bottom.
* **Lock LEDs.** A compact board has two where a full size Type 5 has four. The
  converter always sends all four bits and the keyboard ignores the ones it has no
  lamp for, so nothing needs configuring either way.

One caveat specific to how this firmware starts up: the probe is `0x0F` (layout),
and key events are ignored until the keyboard has answered *something*. Every
keyboard tested answers `0x0F`, but if yours does not, the `0x7F` idle byte sent
when the last key comes up also counts - so the very first keystroke after plug in
would be swallowed and everything after it would work. If you hit that, build with
`OPT_DEFS += -DSUN_RESET_AT_STARTUP`, which probes with `0x01` instead and gets
the keyboard id back, at the cost of a second self test per plug in.

Only the FKB8579-T001 above has actually been tested. Everything else here is what
the protocol documentation says, so if you try another board, the console build
will tell you in seconds whether it holds.

## Hardware

### The link

A Sun keyboard is a serial device, not a matrix: it sends one byte per key
event at **1200 baud, 8N1**, and accepts single byte commands back for the
LEDs, the bell and the click. The signalling is TTL, but **inverted** - the
line idles low and a mark bit is 0 V - and it runs at **5 V**.

Inversion is handled inside the RP2040: its pads can invert a peripheral signal
on the way in and on the way out (the `INOVER` / `OUTOVER` fields of
`IO_BANK0`), which `config.h` switches on for the two UART pins. No 7404 or
MAX232 is needed, unlike the AVR version of this converter.

What *is* needed is level shifting, because RP2040 pins are not 5 V tolerant.

### Connector

Sun keyboards use an 8 pin mini DIN. Looking into the socket you solder to your
converter (the keyboard's plug is the mirror image of this):

       ___ ___
      /  |_|  \
     / 8  7  6 \
    | 5    4  3 |
     \_ 2   1 _/
       \_____/
    (receptacle)

| mini DIN | signal                | RP2040-Zero                        |
| -------- | --------------------- | ---------------------------------- |
| 1        | GND                   | GND                                |
| 2        | GND                   | GND                                |
| 3        | +5 V                  | 5V                                 |
| 4        | mouse TX (to host)    | not connected - see below          |
| 5        | keyboard RX (in)      | GP0 (UART0 TX), via level shifter  |
| 6        | keyboard TX (out)     | GP1 (UART0 RX), via level shifter  |
| 7        | GND                   | GND                                |
| 8        | +5 V                  | 5V                                 |

Meter the connector before applying power: pins 3 and 8 must read +5 V against
pins 1, 2 and 7.

### Level shifting

Use a 4 channel bidirectional logic level converter module (a BSS138 board
with `HV`/`GND`/`HV1-4` on one side and `LV`/`GND`/`LV1-4` on the other) - it
is simpler and more robust than discrete parts, and the two signal lines each
need only one auto-sensing channel. Some vendors label the channels `A1-A4` /
`B1-B4` instead; the high voltage side is the one whose supply pin you tie to
5 V.

| Module pin | Connects to                        |
| ---------- | ----------------------------------- |
| `HV`       | +5 V (mini DIN pin 3 or 8)          |
| `GND`      | common ground, both sides           |
| `LV`       | RP2040-Zero `3V3` pin               |
| `HV1`      | mini DIN pin 6 (keyboard TX, out)   |
| `LV1`      | GP1 (UART0 RX)                      |
| `HV2`      | mini DIN pin 5 (keyboard RX, in)    |
| `LV2`      | GP0 (UART0 TX)                      |

```
Sun kbd pin 6 (TX) ---- HV1   LV1 ---- GP1 (UART0 RX)
Sun kbd pin 5 (RX) ---- HV2   LV2 ---- GP0 (UART0 TX)
Sun kbd +5V (3/8)  ---- HV          LV ---- RP2040 3V3
common GND         ---- GND (HV side)  GND (LV side) ---- RP2040 GND
```

**The two signal lines cross.** Both names above are from the point of view of
the device they belong to, so the keyboard's TX goes to the converter's RX and
vice versa - `pin 6 -> GP1`, `pin 5 -> GP0`. On the RP2040-Zero the silkscreen
number *is* the GP number, so that is keyboard TX to the pad marked `1` and
keyboard RX to the pad marked `0`. Wiring them straight through instead leaves
GP1 hanging off the keyboard's input, where nothing drives it - which on this
inverted link reads as a permanent break, and the log below fills with
`sun: rx 00`.

`HV3`/`LV3` and `HV4`/`LV4` are free - use one for the mouse pass-through (see
below) or leave them unconnected.

The module only translates voltage, not logic polarity, so it has no bearing
on the RP2040 pad inversion described above - leave that switched on.

If you would rather use discrete parts instead of a module: a 1k/2k resistor
divider on the keyboard -> RP2040 line is enough at 1200 baud (the keyboard's
input is TTL, so GP0 can drive pin 5 directly without any shifting at all).
Putting a 74HCT125 or 74HCT14 in that line also works, but a 74HCT14 inverts,
so either invert twice or turn the firmware's own inversion off by adding
`OPT_DEFS += -DSUN_NO_SIGNAL_INVERSION` to your keymap's `rules.mk` (that also
drops the inversion on the receive side, so invert both lines in hardware if
you go that way).

### Power

The keyboard runs from USB VBUS through the `5V` pad of the RP2040-Zero. A Type
5 draws roughly 100-300 mA depending on the LEDs and the bell, which is inside
the budget of any normal USB port, but do not power it from an unpowered hub
that is already loaded.

### The mouse

Pin 4 carries the pass through data of a Sun mouse plugged into the keyboard,
in the Mouse Systems 5 byte protocol. This converter ignores it. If you want it
later, GP5 (UART1 RX) is free and needs the same level shifting as the two
lines above (a spare channel on the same module, or another 1k/2k divider).

Confirm the mouse's baud rate against your own hardware before writing any of
that: the sources disagree. The TMK wiki linked below gives 1200 baud, the same
as the keyboard, while [usb3sun](https://github.com/delan/usb3sun) drives Sun
workstations at 9600 baud by default and treats 4800/2400/1200 as the settings
that NeXTSTEP and Plan 9 need. It may well be a difference between mouse
generations - the Type 5 mouse was replaced by the Compact 1 in 1995.

### Onboard RGB LED

The RP2040-Zero's WS2812 on GP16 is not used by this firmware. It is available
if you want a lock light: enable `RGBLIGHT` and drive it from `led_update_kb`.

## Building and flashing

    qmk compile -kb converter/sun_type5_rp2040 -km vial

Put the board into its bootloader - hold **BOOT** while plugging the USB cable
in, or double tap **RESET**, which this firmware enables - and copy
`converter_sun_type5_rp2040_vial.uf2` onto the `RPI-RP2` drive that appears.

`qmk flash -kb converter/sun_type5_rp2040 -km vial` builds and then copies the
file for you, but you still have to put the board into the bootloader yourself -
it waits for the drive to appear.

The `default` keymap is the same layout without Vial, for a fixed firmware.

## Bring up

Work through this before trusting the converter, in this order - the first two
steps are what stop a mirrored connector from destroying the RP2040.

1. **Meter the connector with the keyboard unplugged.** Pins 3 and 8 must read
   +5 V against pins 1, 2 and 7. If the diagram above is read from the wrong
   side every pin mirrors, which puts 5 V straight onto GP1.
2. **Check that pin 6 idles low.** The link is inverted, so a keyboard that is
   powered and idle holds its TX line at 0 V. A pin 6 that sits high means you
   are either mirrored or on the wrong pin - stop and re-check before wiring
   the level shifter.
3. **Confirm the handshake.** The converter sends `0x0F` (layout) about a second
   after startup and then every three seconds until the keyboard replies; a
   healthy board answers `0xFE` plus its layout id. The one second wait is
   deliberate - the keyboard spends its first second running its own power on
   self test and is not listening, so anything sent earlier is thrown away.
4. **Type.** If keys land but the wrong characters come out, the link is fine
   and it is the scan code table that needs work, not the wiring.

### Seeing the debug output

`matrix.c` logs the whole bring up over `dprintf` - but **the Vial firmware
compiles those away**, because `builddefs/build_vial.mk` adds `-DNO_DEBUG` to
every Vial keymap. To watch the handshake you need a console build of the
`default` keymap, which does not pull that file in:

    qmk compile -kb converter/sun_type5_rp2040 -km default -e CONSOLE_ENABLE=yes

Flash that, read it with `qmk console`, and do the protocol bring up there. Once
the keyboard is talking, flash the Vial firmware over it.

A healthy link looks like this - the handshake, then a report every five
seconds for as long as the firmware runs (captured from a Type 6 compatible
compact board):

    sun: no answer yet, probing keyboard
    sun: rx FE
    sun: keyboard answered after 1021 ms
    sun: rx 22
    sun: layout 22
    sun: link kbd=1 id=FF layout=22 bytes=2 faults=0 err=000 rxline=1 uart=6510/27/301 pads=000102/010002

The report repeats forever on purpose. `qmk console` is almost always attached
after the board has booted, and anything printed before it connects is dropped,
so a one-shot startup banner is invisible in practice - and an idle Sun keyboard
sends nothing at all, which makes a console build that works indistinguishable
from a dead one.

Read the report like this:

| Field | Healthy | What it means |
| ----- | ------- | ------------- |
| `kbd` | `1` | The keyboard has answered. This is the field to read for "is the link up" - `0` means nothing recognisable has ever arrived. |
| `id` | `FF` normally | Keyboard id, and only a reset returns it, which startup no longer sends (see below). Press `SUN_RST` and it becomes `04` - the whole Type 4/5/6 family reports that. |
| `layout` | `22` here | Layout id, see below. `FF` means the keyboard never answered the probe. |
| `bytes` | climbing as you type | Bytes received. Stuck at `0` means nothing is arriving at all. |
| `faults` / `err` | `0` / `000` | UART receive errors. Climbing means framing or break errors - see below. |
| `rxline` | `1` | GP1 **after** the pad inversion, so `1` is the idle/mark state. A steady `0` means the physical pin is high on a link that idles low. |
| `uart` | `6510/27/301` | `IBRD`/`FBRD`/`UARTCR` - 1200 baud off the 125 MHz peripheral clock, with `RXE`, `TXE` and `UARTEN` set. |
| `pads` | `000102/010002` | GP0/GP1 `IO_BANK0` CTRL. `FUNCSEL=2` (UART0) on both, `OUTOVER=1` on TX and `INOVER=1` on RX. `000002/000002` means the polarity inversion did not take. |

So:

| What you see | What it means |
| ------------ | ------------- |
| `kbd=0 bytes=0 faults=0 rxline=1` | The UART is happy and the line is idle - nothing is arriving. The keyboard is not powered, GP1 is not connected to mini DIN pin 6, or the level shifter's `LV` pin is not on 3V3. |
| `rxline=0`, `faults` climbing, `err=440` or similar, a flood of `sun: rx 00` | GP1 is physically **high** while this inverted link expects it low, so the UART sees a permanent break and invents `0x00` bytes. The usual cause is the two signal lines wired straight through instead of crossed; also a mirrored connector, or a level shifter whose pull-up wins because pin 6 is not driving it. |
| `bytes` climbing with plausible-looking values but `kbd=0` | Framing is close but not right: check `uart=` against the expected divisors, and try the polarity the other way round (below). |
| Nothing at all, not even a report every five seconds | The board is not running a console build. Either `CONSOLE_ENABLE=yes` was missing, or an older `.uf2` is still on the board - the Vial firmware has no console interface whatsoever, so `qmk console` would not even say "connected". |

`sun: rx XX` is every raw byte as it arrives, so step 4 above is readable too -
a Type 5 sends the key id on make and the id with bit 7 set on break.

### Why startup does not reset the keyboard

A Sun keyboard runs a self test - a beep and a flash of the lock LEDs - both when
its power comes up **and** every time it is sent `0x01` (reset). So a converter
that resets the keyboard at startup always produces *two* self tests per plug in,
about a second apart, however the timing is arranged. There is no window in which
the two can be merged; the keyboard has already finished its own before its
receiver is listening.

**One** beep and flash per plug in is therefore the expected, healthy behaviour,
and it comes from the keyboard itself - for the first second the converter sends
nothing at all, so there is nothing else it could be. There is no command to
suppress it; `0x03` (bell off) only silences a bell that `0x02` turned on. Beware
that most Sun documentation about keyboard LEDs describes a *workstation*
signalling its own POST through them, which does not apply here - there is no Sun
host in this picture.

The converter therefore probes with `0x0F` (layout) instead, which is a plain
query: the keyboard answers `0xFE` plus its layout id and does nothing else. One
self test per plug in, and the LEDs and the click setting are still pushed as soon
as that answer arrives.

The cost is the keyboard id, which only a reset returns, so `id=FF` in the link
report is normal. The `SUN_RST` keycode still resets the keyboard on demand - that
is what it is for - and will fill the field in.

To go back to resetting at startup, add this to your keymap's `rules.mk`:

    OPT_DEFS += -DSUN_RESET_AT_STARTUP

### The layout byte

`0x0F` asks the keyboard which layout it is wired for, and it answers `0xFE`
plus one byte. On a full size Type 4/5/5c that byte is the setting of the DIP
switches under the keyboard, read as `bit 7 6 5 4 3 2 1 0` = `x 2 3 4 5 6 7 8`
(switch 1 always reads 0 on a Type 5), which gives up to 64 country layouts.

**Compact boards have no layout DIP switches and answer a fixed `0x22`**, which
is also the value a full size board's switches are set to for the US UNIX
layout. So `layout=22` means `LAYOUT_us_unix` - the layout both keymaps already
use - is the right variant, and there is nothing to change. It is not a country
code you can read anything else into.

"No layout DIP switches" is not the same as "no DIP switches". A multi protocol
board such as the FKB8579 has them, but they choose which *host* it talks to -
PS/2, Mac ADB or Sun - and the country layout is fixed in the model number
instead. It still answers the fixed `0x22`.

Nothing in the firmware acts on this byte; `sun_layout_id()` exposes it for a
keymap that wants to branch on it.

### Proving the firmware before blaming the wiring

Unplug the keyboard, take a jumper wire and short **GP0 to GP1 directly** - no
level shifter, both ends are 3.3 V. The converter's own probe byte then comes
straight back into its receiver, once a second at first and then every three:

    sun: no answer yet, probing keyboard
    sun: rx 0F
    sun: link kbd=0 id=FF layout=FF bytes=1 faults=0 err=000 rxline=1 ...

Both pads invert, so the double inversion cancels and the byte reads back
unchanged. If the loopback works, the UART, the baud rate, the pad setup and
the console are all fine, and the fault is in the connector, the level shifter
or the keyboard. If the loopback shows nothing, the problem is in the firmware
or the build, and no amount of rewiring will help.

### If the polarity turns out to be wrong

The Sun protocol is documented as negative logic and this firmware inverts both
pads to match, which is also what the AVR converter's external 7404 does. If
your hardware already inverts somewhere (a 74HCT14 in the line, or an adapter
of unknown provenance), build with the inversion off and compare:

    qmk compile -kb converter/sun_type5_rp2040 -km default \
        -e CONSOLE_ENABLE=yes -e OPT_DEFS=-DSUN_NO_SIGNAL_INVERSION

If that build is the one that talks, add `OPT_DEFS += -DSUN_NO_SIGNAL_INVERSION`
to `keymaps/vial/rules.mk` so the Vial firmware matches your wiring.

## Remapping with Vial

Open the [Vial app](https://get.vial.today/download/); the converter shows up as
"Sun Type 5 USB Converter" with the US UNIX layout drawn out. Remaps take effect
immediately and survive unplugging. Some operations (macros, assigning the
bootloader key) ask you to unlock the keyboard first: hold **both Shift keys**
until the prompt clears.

Three keyboard specific keycodes are on the "Custom keycodes" tab. Vial shows
the short name; the second column is what you write in a `keymap.c` if you are
binding them by hand:

| In Vial   | In C        | Effect                                               |
| --------- | ----------- | ---------------------------------------------------- |
| `SUN_CLK` | `SUN_CLICK` | toggles the key click, remembered across power cycles |
| `SUN_BEL` | `SUN_BELL`  | toggles the bell (it is a continuous tone, not a beep) |
| `SUN_RST` | `SUN_RST`   | re-runs the keyboard's self test and resyncs its LEDs |

Neither keymap binds them to a physical key, so on the `default` firmware the
click and the bell can only be changed by editing `keymaps/default/keymap.c`.

## Layout variants

The key ids a Sun keyboard sends do not depend on which variant you own, so
remapping works on any of them; only the picture differs. `keyboard.json`
carries `LAYOUT_us_unix` (the default), `LAYOUT_ansi` and `LAYOUT_jp_unix`, and
Vial draws the US UNIX board. To draw one of the others, change the `LAYOUT_`
macro in `keymaps/vial/keymap.c` and edit `keymaps/vial/vial.json` to match.

All three are full size boards with a numeric keypad and the left hand Sun
function column. **On a compact keyboard Vial will therefore draw keys that do
not physically exist**, and the keys that do exist still remap correctly because
Vial addresses them by matrix position, not by the drawing. Building a matching
compact layout is a matter of deleting the absent positions from a `LAYOUT_`
macro and from `vial.json`; the console build tells you which ids your board
actually has - press every key in turn and collect the `sun: rx XX` values.

## Notes

* The converter probes the keyboard at startup and every three seconds until it
  answers, so the keyboard may be plugged in after the converter and still ends
  up with the right LED and click state.
* Key events are ignored until that handshake succeeds, so that a line sitting
  in the wrong state cannot press keys nobody touched. The `0x7F` idle byte a
  keyboard sends whenever the last key comes up counts as a handshake too, so a
  board that ignored `0x01` entirely would still unlock on the first keystroke.
* Key ids double as positions in a 16x8 virtual matrix (`row = id >> 3`,
  `col = id & 7`), which is what Vial and the `LAYOUT_` macros address.
* Protocol details, including the scan code tables, the DIP switch bit order and
  the fixed `0x22` of a compact board, are on the
  [TMK wiki](https://github.com/tmk/tmk_keyboard/wiki/Sun-Keyboard-Protocol).
  The original AVR converter this is derived from lives in
  `keyboards/converter/sun_usb`.
