# Hardware specification

This is the hardware as the firmware sees it. Addresses are hexadecimal.

Most of this was worked out from the service manual and from watching the original firmware.
Where we only inferred a meaning, the text says so.

## Overview

- CPU: Intel 8085A at 5 MHz. All peripherals are memory-mapped.
- Screen: 40 columns by 32 rows of characters.
- Keyboard: 8 by 8 matrix.
- Knob: the VARIABLE control, which gives steps and a direction.

## Memory map

| Address | Contents |
|---|---|
| `0000`-`DFFF` | Program ROM, 56 KB |
| `E000`-`E7FF` | Peripherals on other boards |
| `E800`-`EFFF` | Peripherals on the processor board |
| `EC00`-`EC3F` | NOVRAM: current settings (64 bytes) |
| `F000`-`F7FF` | EEPROM (X2816, 2 KB): stored settings and calibration |
| `F800`-`FFFF` | RAM, 2 KB |

## Interrupts

| Interrupt | Source |
|---|---|
| RST 5.5 | GPIB |
| RST 6.5 | Key pressed |
| RST 7.5 | VARIABLE knob step. The SID pin gives the direction. Also used as a timer for tones. |

## RF generator and output level

| Register | Direction | Use |
|---|---|---|
| `E000` | write | Synthesizer band and divider |
| `E001` | write | Serial data to the synthesizer. Bits 6-4 strobe, bits 3-0 data. Bit 7 is one attenuator step. |
| `E016` | write | Routing: low bits follow the RF band; AM or FM; transmit bit; AF generator range |
| `E017` | write | Coarse attenuator (bits 7-5), BNC socket select (bit 4), power range (bit 3) |
| `E018` | write | Modulation tracking DAC |
| `E019` | write | IF correction DAC |
| `E01A` | write | Fine level DAC low bits (bits 7-6), output mode (low nibble) |
| `E01B` | write | Fine level DAC high bits |

Output mode in `E01A`: 4 means FM, 3 means AM, 7 means modulation off.

## Audio generator and modulation

| Register | Direction | Use |
|---|---|---|
| `E010`, `E011` | write | Audio frequency count, low and high byte |
| `E012` | write | Variable oscillator level DAC |
| `E013` | write | Fixed 1 kHz source level, for modulation |
| `E014` | write | Fixed 1 kHz source level, for the AF generator output |
| `E015` | write | Audio filter band and level range |

## Measurement switching

| Register | Bits | Use |
|---|---|---|
| `E03B` | 7-6 | Power meter range |
| | 3-2 | Audio filter: `01` band pass, `00` low pass (wide), `10` 300 Hz. Toggling bit 3 at audio rate makes the warning tone. |
| | 1-0 | Demodulator: `00` AM, `10` FM, `01` PM |
| `E03D` | 7-6 | `01` receiver test, `10` transmitter measurements |
| | 5-4 | Scope and trigger gain |
| | 3-2 | Voltmeter range |
| | 1-0 | Path: `00` volts or peak, `01` through the notch filter, `10` power |
| `E03E` | 7, 1 | Voltmeter range |
| | 6, 3, 1 | Scope and trigger gain |
| | 2 | AC coupling |
| | 0 | Peak hold |

The range and gain patterns are in the tables at the top of `fw/meter.c`.

## ADC

An ADC0809 with four inputs. Writing to an address starts a conversion on that input.
Reading returns the result of the last conversion.

| Address | Input |
|---|---|
| `E800` | Positive peak |
| `E801` | Negative peak |
| `E802` | RMS voltage, or power, depending on the path selected |
| `E803` | Horizontal shift |

## Status and control register `EE00`

Reading:

| Bit | Meaning |
|---|---|
| 7 | Screen can be written |
| 4-3 | Prescaler chosen by the hardware for the audio counter |
| 2 | Analog circuits have settled, measurement is ready |
| 0 | RF overload: BNC input tripped or the N socket pad is too hot. One input for both. |

Writing (other functions share this register, so keep a copy; the default is `3D`):

| Bit | Meaning |
|---|---|
| 1 | Stops the scope timebase while it is reprogrammed |
| 0 | Enables the RF counter gate |
| 2 | A low pulse recalls the NOVRAM's stored copy (inferred from behaviour) |

## Counters

An AMD Am9513 timer chip: data at `EA00`, command at `EA01`.

| Counter | Use |
|---|---|
| 2 | Gate time of the RF counter |
| 3 | Period of the audio signal |
| 5 | Scope timebase |

RF counter registers:

| Register | Use |
|---|---|
| `E004` | Read one decimal digit (low nibble) |
| `E005` | Digit number in the high nibble. Also receiver tuning data. |
| `E006` | Bit 0 tuning strobe, bit 1 low band, bit 2 fine resolution, bit 3 power range, bit 4 run |

## Screen

| Register | Use |
|---|---|
| `E025` | Write a character at the current position |
| `E026` | Column, 0-39 |
| `E027` | Row, 0-31 |

Wait for `EE00` bit 7 before writing. The screen memory cannot be read back.

The scope trace does **not** go through the CPU. It comes from the analog board directly into
the video signal. The firmware only draws the grid and the labels.

### Useful character codes

The character generator is a separate PROM. The codes we use:

| Code | Character |
|---|---|
| `23` | Underline |
| `22`, `24`, `27`, `28`, `29` | Lower-case z, micro, m, d, k |
| `3C`, `40`, `5B` | Box: top left, horizontal, top right |
| `3F`, `5D` | Box: left side, right tip |
| `3E`, `5C` | Box: bottom left, bottom right |
| `3B` | Top edge of reverse-field text, placed in the row above |
| `DC` | Entry cursor |
| `E0`-`EF` | Pieces of a bar chart |
| `F0`-`FF` | The same pieces, mirrored |

Reverse-field text:

| Normal | Reverse code |
|---|---|
| `A`-`Z` | Character code plus 32 |
| `0`-`9` | 3 to 12 |
| `.` | 1 |
| `-` | 0 |

## Keyboard

Write to `E023` to drive the columns. Read the rows from `E022` (active low).

The key code is `row * 8 + col + 1`.

| Row | Col 0 | Col 1 | Col 2 | Col 3 | Col 4 | Col 5 | Col 6 | Col 7 |
|---|---|---|---|---|---|---|---|---|
| 0 | TX | AF GEN | FREQ | 1 | 9 | MHz/V | 300 Hz | |
| 1 | SCOPE | SINAD S/N | | 2 | 0 | Hz/uV | SELECT | |
| 2 | HELP | AC/DC | LOW PASS | 3 | DEL | dBm | FREQ down | LEVEL down |
| 3 | TONES | SET MOD | STORE | 4 | | kHz/mV | VERT expand | HORIZ expand |
| 4 | BAR CHART | MOD ON/OFF | RECALL | 5 | . | PM | VERT compress | HORIZ compress |
| 5 | HOLD | DIST'N | BAND PASS | 6 | - | dB | FREQ up | LEVEL up |
| 6 | RX | RX=TX FREQ | LEVEL | 7 | | FM | | |
| 7 | DUPLEX | RF GEN | INCR | 8 | | AM% | REP | SINGLE |

The firmware treats row 0, column 6 as a direct 300 Hz filter key. We do not know of a
front panel key for it.

The eight keys in column 0 sit beside the screen. Menus use them as soft keys.

## GPIB

A NEC uPD7210 at `E048`-`E04F`. The address switches read from `E040`, inverted.

| Register | Read | Write |
|---|---|---|
| `E048` | Data in | Data out |
| `E049`, `E04A` | Interrupt status 1, 2 | Interrupt masks |
| `E04B` | Serial poll status | Serial poll mode |
| `E04C` | Address status | Address mode |
| `E04D` | | Auxiliary commands |
| `E04E` | | Address |
| `E04F` | | End-of-string byte |

The GPIB interface is an option. The firmware checks whether the chip answers.

## Non-volatile memory

The NOVRAM holds the current settings. Its hardware saves a copy by itself when power fails.
The firmware only has to keep `EC00`-`EC3F` up to date.
