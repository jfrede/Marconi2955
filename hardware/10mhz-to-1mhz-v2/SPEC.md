# 10 MHz to 1 MHz converter, version 2

A small board that turns an external 10 MHz reference into the 1 MHz signal that the Marconi
2955 wants at its EXT STD input. It is fitted inside the instrument, in front of that input.

The first version (`../10mhz-to-1mhz/`) works and is fitted in the instrument, but it is built
from through-hole parts that have become hard to get (74LS90, 2N2369, BC308B) and it needs
-12 V for its input stage. Version 2 does the same job with parts that are easy to get, from
+5 V alone, on a board of about the same size.

This document is the specification of the board and describes board v2.0 as laid out
(`10MHz_to_1MHz_v2.kicad_pro`, Gerber files in `gerber/`).

Status: laid out, not built or tested yet. The figures for the switching threshold come from a
calculation, not from a measurement.

## What the board has to do

- Accept 10 MHz, sine or square wave, from -15 dBm to +20 dBm.
- Deliver 1 MHz with a 50 % duty cycle at a 5 V logic level.
- Deliver nothing when there is no input signal, so that the instrument falls back to its
  internal standard.
- Run from the instrument's +5 V alone.

## Signals

### Input, J1

| | |
|---|---|
| Frequency | 10 MHz |
| Waveform | sine or square wave, AC coupled |
| Level | -15 dBm to +20 dBm, referred to 50 ohm |
| Switching threshold | about -19 dBm typical, about -17 dBm at the comparator's largest offset (calculated) |
| Impedance | 50 ohm with JP1 closed, about 2.5 k at 10 MHz with JP1 open |
| Without clamping | up to +19 dBm with JP1 closed, up to +13 dBm with JP1 open |
| Above that | the comparator's protection diodes clamp; R2 keeps their current below 3 mA at +20 dBm, 10 mA are allowed |
| Limit | +20 dBm, set by R1 (0805, 0.125 W) |

Common references deliver between 0 dBm and +13 dBm. 1 V RMS into 50 ohm, which is +13 dBm, is
the customary upper level, and makers allow up to 2 dB more. Some examples, from the makers' data:

| Source | Level into 50 ohm |
|---|---|
| Rubidium standard, SRS FS725 | 0.5 V RMS, about +7 dBm |
| GPS disciplined oscillator, Trimble Thunderbolt E | +7 dBm, 2 dB either way |
| GPS clock, Leo Bodnar Precision GPS Reference Clock | 3.3 V CMOS square wave, +7.7 to +13.3 dBm depending on the drive setting |
| Distribution amplifiers, SRS FS730, Wenzel LNDA | +13 dBm, 1 or 2 dB either way |

A 3.3 V CMOS square wave stays inside the range without clamping, with and without the
termination.

JP1 is a solder jumper that is closed as delivered. Leave it closed when the board has a
splitter or amplifier output of its own. Cut it open when the board taps, through a T-piece, a
line that is terminated somewhere else. With JP1 open and nothing connected the input is a
high-impedance node, so do not run the board like that.

### Output, J2

| | |
|---|---|
| Frequency | 1 MHz, the input divided by ten |
| Waveform | square wave, 50 % duty cycle |
| Level | 0 V and 5 V behind 220 ohm |
| Short circuit | allowed, R10 limits the current to about 20 mA |

The 2955's EXT STD input takes 1 MHz at 100 mV to 3 V RMS and loads the source with about 10 k
in parallel with about 100 pF (operating manual). Into that load the board delivers about 4.9 V
peak to peak, which is 2.4 V RMS.

### Supply

+5 V on TP1 and ground on TP2, from the instrument. The board takes about 8 mA with a signal and
about 4 mA without. -12 V is no longer needed.

## Circuit

```
J1 ─┬─ C1 ─ R2 ─┬──────── U1 -IN
    │           R3
   JP1          │
    │    VREF ──┴─ R4 ─┬─ U1 +IN
    R1                 R5
    │                  │
   GND                 └─ U1 OUT ─ R8 ─ U2 1CLKB [÷5] 1QD ─ 1CLKA [÷2] 1QA ─ R10 ─ J2

VREF: R6 from +5V_CMP, R7 and C2 to ground
```

### Input and comparator

U1 is a TLV3501, a comparator with 4.5 ns delay and a rail-to-rail output. It works as an
inverting Schmitt trigger around half its supply voltage.

- R1 terminates the line with 51 ohm when JP1 is closed.
- C1 couples the signal to the inverting input. R2 limits the current of the comparator's
  protection diodes when the signal swings beyond the supply.
- R6, R7 and C2 make the reference VREF, half the comparator's supply. R3 holds the inverting
  input at VREF. Because the input rests in the middle of the supply, a large signal is clipped
  equally on both sides and the duty cycle stays at 50 %.
- R4 and R5 set the hysteresis at the non-inverting input: 4.8 V x 1 k / 101 k, which is 48 mV,
  on top of the 6 mV inside the TLV3501. The signal has to swing about 27 mV to either side of
  VREF at the comparator before the output moves. A signal smaller than that, and noise, leave
  the output where it is.
- R3 must go to VREF, not to the non-inverting input. Tied to the non-inverting input, it lets
  the inverting input follow the hysteresis within about 22 us; without a signal the output is
  then left to the comparator's offset and can oscillate.
- Pin 6, SHDN, is tied to ground. A high level there switches the comparator off.
- C1 is small on purpose. With 1 nF the inverting input follows a slow change of VREF within
  23 us, much faster than VREF itself can move (2.3 ms), so the two inputs stay together when
  the supply or the output's duty cycle changes.

Why a comparator and not a Schmitt-trigger gate: a 74LVC1G17 has 0.58 to 1.04 V of hysteresis at
5 V (TI data sheet), more than the 0.63 V peak to peak of a 0 dBm signal into 50 ohm. An
unbuffered inverter as an amplifier in front of it would have to be biased in its linear region,
where a 74LVC1GU04 draws about 65 mA at 5 V and a 74AHC1GU04 about 17 mA for 18 dB of gain (TI
application report SZZA043).

### Divider

U2 is a 74HC390, two decade counters in one package. Only the first is used. Each counter has a
divide-by-5 section and a divide-by-2 section that count on the falling edge.

- The comparator drives 1CLKB, the divide-by-5 section, through R8. R8 damps the fast edge.
- 1QD, the 2 MHz output of that section, drives 1CLKA, the divide-by-2 section.
- 1QA is the output: 1 MHz with a 50 % duty cycle. With the two sections the other way round
  the output would be high for only 20 % of the time.
- 1CLR and the three inputs of the unused counter are tied to ground. The unused outputs stay
  open.

A 74HCT390 works as well.

### Output

R10 feeds J2. It protects the output against a short circuit and slows the edges a little on
the cable.

### Supply

FB1 and C6 keep the board's switching currents off the instrument's 5 V wiring. R9, C3 and C4
give the comparator a filtered supply of its own, +5V_CMP, about 0.2 V below the rail. The
TLV3501 must never see more than 5.5 V.

### Nets

The pin numbers of U1 are those of the SOT-23-6 package, the pin numbers of U2 those of the
SO-16 package. Pin 1 of J1 and J2 is the centre contact, pin 2 the shell.

| Net | Connected |
|---|---|
| IN | J1.1, JP1.2, C1.1 |
| TERM | JP1.1, R1.1 |
| IN_AC | C1.2, R2.1 |
| CMP_IN | R2.2, R3.2, U1.1 (-IN) |
| VREF | R3.1, R4.1, R6.2, R7.1, C2.2 |
| CMP_P | R4.2, R5.1, U1.3 (+IN) |
| CMP_OUT | U1.5 (OUT), R5.2, R8.1 |
| CLK10M | R8.2, TP3, U2.4 (1CLKB) |
| CLK2M | U2.7 (1QD), U2.1 (1CLKA) |
| Q1M | U2.3 (1QA), R10.1 |
| OUT | R10.2, J2.1 |
| +5V_IN | TP1, FB1.1 |
| +5V | FB1.2, C6.2, C5.2, R9.1, U2.16 (VCC) |
| +5V_CMP | R9.2, C3.2, C4.2, R6.1, U1.4 (V+) |
| GND | TP2, J1.2, J2.2, R1.2, R7.2, C2.1, C3.1, C4.1, C5.1, C6.1, U1.2 (V-), U1.6 (SHDN), U2.2 (1CLR), U2.8 (GND), U2.12 (2CLKB), U2.14 (2CLR), U2.15 (2CLKA) |
| not connected | U2.5, U2.6, U2.9, U2.10, U2.11, U2.13 |

## Parts

Passive parts no smaller than 0603, with hand-soldering pads.

| Ref | Part | Package | Notes |
|---|---|---|---|
| U1 | TLV3501AIDBV | SOT-23-6 | |
| U2 | 74HC390 | SO-16, 3.9 mm wide | KiCad symbol `74xx:74LS390`, which has the same pins |
| R1 | 51 R | 0805 | termination, 0.125 W |
| R2, R4 | 1 k | 0603 | |
| R3 | 22 k | 0603 | |
| R5 | 100 k | 0603 | |
| R6, R7 | 4.7 k | 0603 | |
| R8, R9 | 33 R | 0603 | |
| R10 | 220 R | 0603 | |
| C1 | 1 nF, C0G, 50 V | 0603 | |
| C2 | 1 uF, X7R, 10 V or more | 0603 | |
| C3 | 2.2 uF, 10 V or more | 0603 | |
| C4, C5 | 100 nF, X7R | 0603 | |
| C6 | 10 uF, 10 V or more | 0805 | |
| FB1 | ferrite bead, about 600 ohm at 100 MHz, 200 mA or more | 0805 | 0 R works if no bead is at hand |
| JP1 | solder jumper, bridged | | cut to remove the termination |
| J1, J2 | SMA jack, right angle, through hole, Amphenol 901-143 | | footprint changed in the project: the library version has an edge cut that fails the design rule check |
| TP1, TP2 | +5 V and ground, 1.0 mm hole | | for the supply wires |
| TP3 | test pad on CLK10M | | |

## Mechanics

- Outline 55 x 30 mm, two layers, 1.6 mm.
- Four M3 holes, 3.2 mm, without pads, 4 mm from the edges, so 47 mm and 22 mm apart.
- J1, the input, on the left edge, J2, the output, on the right edge, both 15 mm from the top
  edge.
- TP1 (+5 V) and TP2 (ground) along the bottom edge.

The board is about the same size as the first version, but not drop-in: its holes are 47 x
22 mm apart instead of 46.2 x 21.8 mm.

## Layout notes

- U1 sits next to J1, with the nets CMP_IN and CMP_P short. Stray capacitance on these two nets
  costs sensitivity.
- C3 and C4 decouple U1, C5 and C6 decouple U2.
- Ground fill on both sides, stitched with vias.
- When the PCB is updated from the schematic, leave "Delete footprints with no symbols" off.
  The mounting holes and the logos are only in the layout.

## Bring-up

On the bench, from a 5 V supply with its current limit at 50 mA:

1. Without a signal the board takes about 4 mA. J2 rests at 0 V or at 5 V and does not move.
   VREF is about 2.4 V.
2. With 10 MHz at 0 dBm the board takes about 8 mA. TP3 shows 10 MHz, J2 shows 1 MHz with a
   50 % duty cycle.
3. Lower the level until the output stops, and note it. The calculation gives about -19 dBm
   with JP1 closed and about 6 dB less with JP1 open.
4. Raise the level to +20 dBm. The output must stay at 1 MHz.
5. Try the reference that will be used, with every output level it offers.

Then in the instrument: with the reference connected, the 2955 has to lock to it. A frequency
measurement of a signal that comes from the same reference then reads exactly.

## Still to be done

- Build one board and measure the switching threshold, the highest level and the supply
  current.
