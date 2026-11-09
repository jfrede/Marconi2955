# Principles of operation

How each function of the 2955 works, as far as the firmware is concerned.
Register details are in [hardware-spec.md](hardware-spec.md).

## The instrument in one paragraph

The 2955 tests two-way radios. It has an RF generator with modulation (to test a receiver),
an RF power meter, frequency counter and demodulator (to test a transmitter), and an audio
generator and voltmeter. The firmware mostly sets switches and DACs, reads an ADC and counters,
does some maths and writes text to the screen.

There are four operating modes: receiver test (RX), transmitter test (TX), DUPLEX (both at once)
and audio test.

## RF generator frequency

The synthesizer works at a higher frequency than the output. Dividers and mixers bring it down.
A table in `synth.c` gives each frequency band a band code, an offset and a multiplier:

```
synthesizer frequency = offset + output frequency * multiplier
```

The result is sent as two blocks of seven decimal digits over a simple serial interface.

FM deviation depends on the synthesizer frequency, so the tracking DAC is corrected from
EEPROM tables. For AM, a different table is used, and it depends on the output level.

## RF output level

The level is set in two parts:

1. **Coarse:** a switched attenuator in 10 dB steps.
2. **Fine:** a 10-bit DAC covers the part inside one 10 dB step.

```
attenuation = -level - socket loss
```

The socket loss is 0 dB on BNC and 20 dB on N, which has a 20 dB pad. An external attenuator of
3 to 40 dB can be added for each socket. The firmware keeps the total as one value per socket
(`level_socket_loss`), so the shown level is always the level at the connected equipment.

The tens of dB choose the attenuator step. The remainder is an index into the level table,
which gives the DAC value. EEPROM values correct the table for each instrument.

Voltage entries are converted:

```
dBm = 20 * log10(V / 0.2236)         0.2236 V is 0 dBm into 50 ohms
```

If the level is shown as EMF rather than PD, subtract 6.02 dB. The EMF is twice the voltage
across a matched load.

The knob and the step keys add to an offset in dB. A value entered in volts stays in volts.

## Modulation of the RF generator

An audio source drives the modulator. There are two sources:

- a fixed 1 kHz tone
- a variable oscillator, whose frequency is a count sent to the hardware

A DAC sets the modulation level. Each has two ranges, for better resolution:

| Modulation | Low range | High range |
|---|---|---|
| FM | 25 Hz per count, below 6.375 kHz | 100 Hz per count |
| AM | below 25 %, four times finer | 25 % and above |

## AF voltmeter

The audio input goes through range switches to an RMS detector and the ADC.

```
volts = ADC reading * volts per count for the range
```

There are ten ranges, from 0.5 mV to 0.5 V per count. If the ADC reaches full scale (255), the
firmware moves up a range. If the reading is small, it moves down. The thresholds are different
in each direction, so the range does not flip back and forth.

After changing a switch, wait for the "settled" bit before trusting the next reading.

## SINAD, S/N and distortion

There is a notch filter fixed at 1 kHz. The firmware measures the signal, then the signal through
the notch. What the notch leaves is noise and distortion.

The notch path has 20 dB of extra gain.

```
SINAD      = 20 dB - 20 * log10(notch volts / total volts)
distortion = notch volts / total volts * 10 %
S/N        = 20 * log10(total volts / noise volts)
```

For S/N, the firmware switches the modulation off for a moment and measures what is left.

Because the notch is fixed, these readings only make sense with a 1 kHz tone.

Readings are smoothed by two simple low-pass filters in a row.

## Audio frequency counter

The counter measures the **period** of the audio signal against a known clock:

```
frequency = clock / count
```

The hardware picks the clock (500 kHz, 5 MHz or 50 MHz) and shows which one in the status register.
The firmware averages 16 measurements. If no signal edges arrive for 20 measurements,
it shows 0 Hz.

## Audio filters

Three filters are available: 0.3-3.4 kHz band pass, wide low pass and 300 Hz low pass.
The low pass key switches between the last two. The wide low pass is shown as 50 kHz in RX
and as 15 kHz in TX and DUPLEX.

## Bar charts

A bar chart is a column of special characters. Each character has 8 pixels of height, so 13 rows
give about 100 pixels. The firmware works out a percentage of full scale and draws:

- full cells below the top of the bar
- one partly filled cell at the top
- empty cells above

The scale steps up at 100 %, and down below about a quarter of full scale.

Some bars come in pairs, with the second one mirrored, to show positive and negative peaks.

## Scope

The trace is drawn by analog hardware and never passes through the CPU. The firmware:

- sets the vertical gain and the timebase (a counter in the Am9513)
- draws the grid and scale labels with characters
- limits the voltmeter ranges so that the display stays valid

The trigger path also feeds the audio counter. With bar charts showing, the firmware makes the
trigger gain follow the voltmeter range, so the counter keeps working.

## Transmitter test

### Tuning the receiver

The measuring receiver has a 110 kHz IF. It uses a harmonic of a local oscillator:

```
offset = frequency + 110 kHz
n = floor(offset / 13.8 MHz + 1)
word = (offset - n * 10 MHz) * 0.00376 / n
```

If `word` comes out below 5640, the low band is used instead:

```
n = floor(offset / 3.1 MHz + 1)
word = offset * 0.00376 / n
```

### Automatic or manual

In automatic mode, the firmware tunes to whatever the RF counter measures. It retunes after
three readings that are more than 5 kHz away. In manual mode you enter the frequency, and
the screen shows the offset of the measured signal from it.

### RF counter

The RF counter gives nine decimal digits, read one at a time. It starts with a coarse gate. With a
stable signal between 0.4 and 200 MHz, it moves to a fine gate. The fine gate counts 1 Hz or
10 Hz, depending on a parameter.

### Measurement cycle

The transmitter measurement repeats these steps, one per poll:

1. Read positive peak, start negative peak.
2. Set peak hold and power range, wait until settled, read negative peak, start power.
3. Optional: distortion.
4. Select peak path and read power.
5. Read the auxiliary input.

### Power

```
V = ADC * 6.052 * volts per count * EEPROM correction
P in mW = 20 * V * V
```

This is correct for a socket loss of 20 dB, the pad of the N socket. For other losses the
reading is scaled to match, so the BNC socket (0 dB) reads a factor of 100 lower, and an
external attenuator raises the reading by its value.

The correction comes from EEPROM tables, interpolated by frequency and depending on the range.

### Modulation

```
value = average of positive and negative peak * volts per count * scale
```

The scale is 0.2 for FM (kHz), 1 for AM (%) and 0.1 for phase modulation (radians).

### AF generator

The audio output has three ranges, by DAC counts per volt: 100, 400 and 1000.
It uses the fixed source for exactly 1 kHz, otherwise the variable oscillator.

## DUPLEX

DUPLEX tests a transmitter and a receiver at the same time. The RF generator stays on, as in
the receiver test. The measuring hardware is shared, so the firmware takes turns:

```
two transmitter cycles -> switch -> two receiver cycles -> switch -> ...
```

DUPLEX has its own settings, separate from RX and TX. Its generator step sizes come from RX.
Modulation and AF generator frequency are fixed at 1 kHz.

There are two connection types:

| | Two port | One port |
|---|---|---|
| Generator output | BNC | N socket, shared with the transmitter |
| Transmitter input | N | N |
| Level range | -120 to -40 dBm | -140 to -80 dBm |
| Attenuation | as BNC | -level - 26 dB |
| Power reading | as usual | times 4, with one extra correction |

The modulation type of the transmitter measurement follows the generator's type.

## Self test

`HELP`, `SELF TEST` runs 13 tests one after the other. The generator feeds the measuring circuits
internally, so no leads should be connected. In `CHANGE PARAMETERS`, the `TX` key runs the same
tests one at a time with a `NEXT TEST` key.

| Test | Checks | Limit |
|---|---|---|
| 1.0 - 1.4 | RF counter against the generator at 20, 111, 218, 340 and 480 MHz | 50 Hz |
| 2.0 - 2.3 | Power meter against the generator at 300, 849, 20 and 20 MHz | 2 dB |
| 3.0 - 3.3 | Modulation meter: FM 400 Hz and 1 kHz, AM 1 kHz, PM 1 kHz, at 210 MHz | 10 %, and 1 Hz for the frequency |

A failure shows a two-digit code. The first digit is the test group. The operating manual has the
list. The last failure code of each group is kept in the EEPROM. Each result also shows what was
measured and how far it is from the expected value, so a marginal unit can be seen.

## Calibration data menu

This menu shows and changes any memory byte or hardware register. It is the way to correct
calibration values by hand, and to switch single register bits when testing hardware. Measurements
stop while it is open, so a value written to a register stays there. Registers cannot be read back,
so the value written stays visible in the `WRITE` column. To open it, press `HELP` on the first `CHANGE PARAMETERS` page, then
`2`, `9`, `5`.

Addresses are entered as five decimal digits. The arrow keys step the address by 1 or 10. A write
needs `ENTER DATA`, up to three digits, and `ENTER DATA` again.

**Writing the wrong value can spoil the instrument's calibration. Make a backup of the EEPROM first.**

## Audio test

Pressing `AF GEN` in the receiver test turns the instrument into an audio test set. The AF generator
and the AF voltmeter run together, so a filter or amplifier can be measured between `AF GEN OUTPUT`
and `AF INPUT`. The voltmeter, filters, distortion and dB readings work as in the receiver test. SINAD
and S/N are not available. The second reading starts as dBV. The RF generator is parked at its lowest
level.

A second tone of 1 kHz can be added with `TONES`.

## Tones

The `TONES` key opens a menu. It offers different things in the receiver and transmitter tests.

**Sub-audible tone (RX).** The fixed 1 kHz source keeps modulating, and the variable oscillator adds
the low tone on top. FM uses a fine range of 10 Hz per DAC count. `SAT` appears beside the
modulation frequency. Entering a new modulation frequency switches it off.

**Two tones (TX).** The AF generator output carries the variable frequency and the fixed 1 kHz at
the same level. `2T` appears beside the AF generator.

**Sending calling tones (RX, DUPLEX).** The list holds up to 11 tone numbers from a standard. The
variable oscillator modulates the generator and its frequency is changed tone by tone. A pause
sets the level to zero. Burst sends the list once. Continuous repeats it with a pause of two tone
lengths, and shows `TS` on the main screen. The fifth tone from the end can be extended, and all
tones can be shifted by up to 9 %.

**Receiving calling tones (TX).** The firmware reads single periods from the audio counter. A tone
is a run of readings within 2.5 %. It must last at least 20 ms. 800 ms of silence, or 12 entries,
ends the measurement. Each tone is matched to the nearest number in the standard if it is within
5 %, and marked with `*` if the error is over 2 %.

**User-defined standard.** 15 frequencies and one duration, kept in the EEPROM. They replace a
standard for both sending and receiving.

## Overload warning

The input protection is done by the hardware. The BNC input disconnects itself at about 1 W, and
the N socket pad has a temperature sensor. Both report on the same status bit, so the firmware
cannot tell them apart. The extra line on the warning screen is only a guess from the active socket.

While the bit is set, everything else stops, and keys and the knob are ignored:

1. The screen shows `REMOVE RF INPUT` in large letters.
2. After about 2.3 s the warning tone starts. It is a short rising sweep, repeated. The firmware
   makes it by toggling a filter switch at audio rate. The hardware turns the volume up by itself.
3. When the bit clears, the filter and the screen are restored. Held readings stay on the screen.

## Keys and number entry

Most settings follow one pattern:

1. Choose a group: `RF GEN`, `SET MOD` or `AF GEN`.
2. Choose a field: `FREQ` or `LEVEL`. The label turns reverse-field.
3. Type digits. They appear on the screen.
4. Press a unit key to finish.

A value out of range stays on the screen and is not used.

`INCR` before the digits sets a step size for the arrow keys. The knob changes the value by
its smallest step and ignores the step size.

## Store and recall

There are 37 storage slots of 45 bytes in the EEPROM. See [data-formats.md](data-formats.md).

## Power-on

1. The NOVRAM's saved copy is loaded into its RAM.
2. The firmware keeps that data for `RECALL 00`.
3. It starts with default settings: RX, 300 MHz, -100 dBm, 1 kHz FM at 1.5 kHz deviation,
   N socket, 0.3-3.4 kHz filter.

While running, the firmware keeps the current settings in the NOVRAM. When power fails, the
NOVRAM hardware saves them by itself.

A GPIB device clear returns to the default settings, but does not change what `RECALL 00` gets.

## HELP and parameters

The HELP key opens a menu on the soft keys, and measurements stop while it is open.
`CHANGE PARAMETERS` holds settings that stay in the EEPROM. See
[data-formats.md](data-formats.md).

The external attenuators are set on the third page. A reverse-field `EXT` next to the level or
power reading shows that the socket in use has one.

Two optional accessories can be enabled there:

- **600 ohm dBm:** readings are in dBm relative to 0.7746 V. AF generator level can be entered in dBm.
- **20 dB AF attenuator:** the displayed level is one tenth of the output. A reverse-field `A` marks this.

During HOLD, the HELP key prints the page of readings, if the GPIB address switches are set
to talk-only for a printer.
