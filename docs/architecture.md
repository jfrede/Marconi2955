# Architecture

This project is a new firmware for the Marconi 2955 radio test set, written in C.
It aims to do what the original firmware does, not to copy how it does it.

## The rules we work by

- **Same functions, not a copy.** If the original rounds badly, we do the correct maths instead.
  We do not rebuild its oddities. Differences of a few percent or one pixel do not matter.
- **Original ROMs stay private.** Only our own code and our own descriptions are published.
- **Calibration comes from the instrument.** Read it from the EEPROM. Never hard-code a
  value measured on one instrument.
- **Be careful with the real instrument.** Writing to its EEPROM can destroy calibration data.
  Make a backup first.

## The pieces

```
fw/      the new firmware
emu/     an 8085 emulator with a model of the instrument's hardware
tools/   helper scripts
docs/    these files
```

The emulator can run the original ROM (you need your own copy). The new firmware can run
against the same hardware model. That lets us press the same keys in both and compare the
screen and the registers.

## Firmware modules

| Module | What it does |
|---|---|
| `app.c` | The receiver test screen, key handling, mode changes, store and recall |
| `tx.c` | The transmitter test, and the transmitter half of DUPLEX |
| `meter.c` | The AF voltmeter, SINAD, S/N, distortion, range switching |
| `level.c` | RF output level: attenuator and fine DAC |
| `synth.c` | RF generator frequency |
| `modulation.c` | FM and AM of the RF generator |
| `rfcounter.c` | The RF frequency counter |
| `afcounter.c` | The audio frequency counter |
| `bargraph.c` | Bar charts made of characters |
| `scope.c` | Scope controls and graticule |
| `gpib.c` | GPIB interface and commands |
| `readings.c` | The latest text of each reading, for GPIB |
| `help.c` | The HELP menu and parameter pages |
| `overload.c` | The `REMOVE RF INPUT` warning |
| `service.c` | The calibration data menu |
| `regmenu.c` | The hardware register menu |
| `selftest.c` | The 13 self tests |
| `tones.c` | The TONES menu, sub-audible tone, two tones, calling tones |
| `settings.c` | The format of stored settings |
| `params.c` | Permanent parameters in the EEPROM |
| `screen.c`, `format.c` | Writing characters and formatting numbers |
| `keys.c` | Finding which key is pressed |
| `control.c` | Shared write register at `EE00` |
| `hal.h` | The connection to the hardware |

## How the firmware runs

There are no threads and the firmware does not wait for measurements to finish.
Each measurement is a small state machine that does one step and returns.

A target has to do this:

```c
app_power_on();          /* once, after reset */
for (;;) {
    app_poll();          /* as often as possible */
    /* on a key press:   app_key(code);        */
    /* on a knob step:   app_knob(+1 or -1);   */
}
```

`app_poll` first services GPIB. Then it runs one step of the measurement for the current
mode and updates the screen if there is a new reading.

There are two ways to press a key:

- `app_key` is a front panel key. It is ignored in the GPIB remote state.
- `app_command_key` is a key sent by a GPIB command. It always works.

## The hardware layer

The firmware only reaches the hardware through `hal.h`:

| Function | Purpose |
|---|---|
| `hal_read(addr)`, `hal_poke(addr, value)` | Access a register or memory byte. `hal_write` in `hal_common.c` calls `hal_poke` and remembers what was written. |
| `hal_screen_wait()` | Wait until the screen memory can be written |
| `hal_delay_ms(ms)`, `hal_delay_us(us)` | Wait, for example after an EEPROM write or for the warning tone |
| `hal_millis()` | Milliseconds since start, for timing calling tones |

To move to new hardware, write these functions for it. Register addresses stay the same
because the firmware uses the instrument's own memory map.

No target that runs on the real instrument exists yet. We expect to use an RP2350 connected
to the 8085 socket. `fw/target8085` only measures code size.

## Building and testing

```
cd emu && make        # the emulator
cd fw  && make        # builds build/test_fw, see below
```

The firmware does not need the original ROM. Its RF level table, `fw/level_table.h`, was measured
on an instrument with `tools/level_table_from_sweep.py` from the data in `tools/level_sweep_300MHz.csv`.
You only need your own copy of the ROM to compare against the original in the emulator.

`build/test_fw` has several modes. The most useful for comparing is:

```
build/test_fw script EEPROM_FILE [SETTINGS_FILE] < script
```

It reads the same script commands as the emulator (see the top of `emu/main.c`) and gives
the same output. Run a script on both and compare:

```
key 7 1            # press a key: row, column
run 12000000       # let it run for this many CPU cycles
peek E017          # print a register
rawscreen 0 31     # print screen rows as character codes
```

Screens and register values should match, apart from the differences in
[deviations.md](deviations.md). Some registers change during a measurement cycle, so compare
those over several samples.

`emu/run_scenarios.sh` plays the stored key sequences in `emu/scenarios`.

## Where to read next

- [principles-of-operation.md](principles-of-operation.md): how each function works
- [hardware-spec.md](hardware-spec.md): registers and memory
- [data-formats.md](data-formats.md): stored data
- [gpib.md](gpib.md): remote control
- [deviations.md](deviations.md): differences, missing parts and test status
