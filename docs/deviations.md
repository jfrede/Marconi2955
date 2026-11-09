# Status and deviations

## Test status

**The new firmware has never run on the real instrument.** There is no hardware target yet.

The checks so far are:

| Function | Compared with the original in the emulator | Original firmware checked on the real instrument |
|---|---|---|
| RX screen, keys, voltmeter, SINAD | Yes | Voltmeter and SINAD against a signal generator |
| Bar charts and scope controls | Yes | Screen compared by eye |
| TX screen, tuning, measurements | Yes | RF counter, power and FM at a few frequencies |
| GPIB commands | Yes | `RD39` page layout, `SV` |
| Status byte, serial poll, return to local | Against our own hardware model only | No |
| Store and recall | Yes | No |
| HELP and parameters | Yes | No |
| DUPLEX | Yes, without real signals | No |
| Overload warning | Screen and restore, with a simulated status bit | No |
| Calibration data menu | Reading and writing, with our own access keys | No |
| Self test | Screens and error codes, with simulated readings | No |
| AUDIO TEST | Screens and registers; readings with the audio model | No |
| TONES menus, sub-audible tone, two tones, sending | Screens and registers | No |
| TONES decoding | No (the emulator has no audio signal); tested with simulated samples | No |
| Printing during HOLD | No (talk-only mode is not settable in the emulator) | No |

The emulator's hardware model is a simplification. It has no real analog circuits.

## Deliberate differences from the original

### Better maths

| Item | Original | New firmware |
|---|---|---|
| Rounding of readings | 16-bit arithmetic, often truncated | Exact constants, correct rounding |
| AM of 0 % | A small residual DAC value | Exactly 0 |
| Zero with a sign | Can show `-0.0` | Shows `0.0` |
| Voltage display near a digit change | Truncates | Rounds, so 999.96 uV shows `1.000mV` |

These can change a displayed value in the last digit.

### Added or changed

| Item | Change |
|---|---|
| `LV4.5FM` | Accepted as kHz of deviation. In the original, the FM key does not finish the entry. |
| `RECALL` of a low pass filter | The stored filter is restored. In the emulator, the original does not. |
| HELP text | Own wording, and no manufacturer's copyright line |
| Calling tones | Timed from a millisecond clock in the main loop rather than an interrupt. A loop that takes long will make tones uneven. |
| EIA tones | Not offered. The firmware we studied only has EEA. |
| Self test | Uses the DUPLEX measuring cycle instead of a separate routine. Same frequencies, levels and limits as the manual. |
| Self test screen | Shows the measured value and the deviation next to the result. The column layout differs for that. |
| Calibration data menu | Own access keys. Shows the address and data in hex as well. Starts at `F000`. |
| External attenuator | New. Set per socket on a third parameter page. Changes the RF level, its limits and the power reading. |
| Overload warning | One extra line under the large letters says which input is the likely cause. The letters and the tone are our own design. |

### Small display differences

- After a rejected step, a typed modulation frequency stays as typed.
- After leaving DUPLEX, a typed TX frequency is shown as typed.

## Not done yet

| Item | Notes |
|---|---|
| Some GPIB commands | See [gpib.md](gpib.md) |
| Hardware target | RP2350 planned, with level shifting between 5 V and 3.3 V |

## Before publishing

The RF level table is extracted from the original ROM at build time and must not be published.
It should be replaced with a table built from measurements.
