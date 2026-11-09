# Data formats

## Floating point numbers

Stored numbers use four bytes:

| Byte | Meaning |
|---|---|
| 0 | Exponent, signed |
| 1, 2 | Mantissa, low byte first |
| 3 | Sign: 0 positive, otherwise negative |

```
value = mantissa * 2^exponent
```

## Frequency

Five bytes of BCD (two decimal digits per byte), low digits first, in Hz.

## Settings slot

One slot is 45 bytes.

| Offset | Size | Contents |
|---|---|---|
| 0 | 5 | RF frequency in Hz, BCD |
| 5 | 4 | RF level, float: dBm or volts |
| 9 | 1 | Level unit: 1 V, 2 mV, 3 uV (as entered), 5 dBm |
| 10 | 2 | Level offset, signed, 0.1 dB |
| 12 | 4 | Modulation frequency (RX) or AF generator frequency (TX), float, Hz |
| 16 | 4 | AF level, float |
| 20 | 4 | Modulation level, float |
| 24 | 4 | Level increment |
| 28 | 1 | Level increment kind |
| 29 | 5 | Frequency increment |
| 34 | 1 | Frequency increment kind |
| 35-40 | 6 | Packed flags, see below |
| 41-44 | 4 | Sub-audible tone level, float |

Modulation level in the slot:

- FM: kHz divided by 10
- AM: (percent + 1/8) / 39.0625

Increment kinds:

| Kind | Used for | Format |
|---|---|---|
| 1 | RF generator | Level: 16-bit tenths of dB. Frequency: BCD Hz. |
| 2 | Modulation | Float |
| 3 | AF generator | Float |

### Packed flags

| Byte | Bits | Contents |
|---|---|---|
| 35 | 6 | AC coupling |
| | 5 | Scope shown |
| | 4 | Distortion on |
| | 3-2 | Socket. RX/TX: 0 BNC, 1 N. DUPLEX: 1 two port, 2 one port. |
| | 1-0 | 0 off, 1 SINAD, 2 S/N |
| 36 | 7-4 | Scope timebase |
| | 3-0 | Scope vertical, RX or AM |
| 37 | 7-4 | Scope vertical, FM |
| | 3-0 | Scope vertical, PM |
| 38 | 7-6 | Group: 0 none, 1 modulation, 2 RF, 3 AF |
| | 5-3 | Field: 0 none, 2 level, 4 frequency |
| | 2 | Modulation off |
| 39 | 6-5 | Mode: 0 TX, 1 RX, 2 audio test, 3 DUPLEX |
| | 4-3, 2-1 | Modulation kind: 1 AM, 2 FM, 3 PM |
| 40 | 6 | Manual tuning |
| | 5 | Sub-audible tone on |
| | 3 | Repeat sweep (0 means single) |
| | 2-0 | Filter |

## The settings image

The 64 bytes at `EC00`-`EC3F` are the working copy. The first 35 bytes are the same as a slot.
After that, the packed flags are spread out, one value per byte.

The GPIB `SV` command prints this image.

## EEPROM map

| Address | Contents |
|---|---|
| `F000` | Settings slots. Slot n starts at `F000 + (n - 1) * 45`. |
| `F69C` | Power meter correction, low bands |
| `F6B8` | Non-zero: voltages shown as EMF, otherwise PD |
| `F6B9` | Non-zero: RF counter resolves 1 Hz, otherwise 10 Hz |
| `F6BA` | Bit 0 set: S/N is the default, otherwise SINAD |
| `F6BB` | Non-zero: STORE and GPIB memory writes are blocked |
| `F6BF` | Extra power meter correction |
| `F6DF` | Non-zero: the PD/EMF choice is not offered |
| `F700` | Level table offset for each 10 dB step |
| `F70C` | DAC trim for each dB |
| `F717` | AM tracking |
| `F723`, `F74A`, `F77A` | FM tracking, by frequency range |
| `F68D`-`F68E` | External attenuator at the N socket, 0.1 dB, low byte first. Our firmware only. |
| `F68F`-`F690` | The same for the BNC socket |
| `F6BC`-`F6BE` | Last self test failure code of each test group (1, 2, 3) |
| `F6E0`-`F6FD` | User-defined tones, 15 words: 0.1 Hz, or bit 15 set and whole Hz from 3276.8 Hz |
| `F6FE`-`F6FF` | User-defined tone duration, 0.1 ms |
| `F7A8` | Non-zero: 20 dB AF attenuator fitted |
| `F7A9` | Non-zero: 600 ohm dBm accessory fitted |
| `F7AA` | Power meter correction, high bands |

Wait 10 ms after each EEPROM write.

These values belong to one particular instrument. Back them up before experimenting.
