# GPIB

The instrument is controlled with two-letter commands. Numbers can follow a command.
Lines end with LF, ETX, ETB or EOI.

Number characters typed in a command act like the keypad.

## Commands that press a key

| Command | Key | Command | Key |
|---|---|---|---|
| `TX` | TX | `RX` | RX |
| `DX` | DUPLEX | `TN` | TONES |
| `BC` | BAR CHART | `SC` | SCOPE |
| `HP` | HELP | `AG` | AF GEN |
| `RG` | RF GEN | `SM` | SET MOD |
| `RT` | RX=TX FREQ | `FR` | FREQ |
| `LV` | LEVEL | `DI` | INCR |
| `ST` | STORE | `RC` | RECALL |
| `MZ`, `VL` | MHz or V | `KZ`, `MV` | kHz or mV |
| `HZ`, `UV` | Hz or uV | `DM` | dBm |
| `DB` | dB | `FM` | FM |
| `AM` | AM% | `PM` | PM |
| `FU`, `FD` | FREQ up, down | `LU`, `LD` | LEVEL up, down |
| `VD`, `VU` | VERT expand, compress | `TD`, `TU` | HORIZ expand, compress |
| `RP`, `SW` | REP, SINGLE | `XA`-`XG` | The seven mode keys, as soft keys |

## Other settings

| Command | Meaning |
|---|---|
| `AC`, `DC` | Input coupling |
| `FIn` | Filter: 0 band pass, 1 low pass, 2 300 Hz |
| `HDn` | Hold display: 0 off, 1 on |
| `MDn` | Modulation: 0 off, 1 on |
| `SNn` | Second reading: 0 dBV, 1 SINAD, 2 S/N, 3 distortion |
| `IPn` | Input socket: 0 BNC, 1 N |
| `PD`, `EM` | Show voltages as PD or EMF. Stored permanently. |
| `EV`, `DV` | Enable or disable the VARIABLE knob in remote |

## Reading

| Command | Meaning |
|---|---|
| `RDn` | One reading, see below |
| `RD39` | A whole page of settings and readings |
| `VN` | Software version |
| `ER` | Number of the last entry error |
| `SK` | Report the next key pressed, in remote |
| `SV` | A command string that restores the current settings |
| `PEm` | One memory byte. m is decimal, 57344 (`E000`) or higher. |
| `PG` | Empty the output buffer |
| `TF`, `TE` | Readings 14 to 25 give the received tones as frequencies, or as numbers and errors |
| `LF`, `EX` | End of output: LF with EOI (default), or ETX with EOI |
| `BP` | Beep |
| `UC`, `LC` | Units in upper case only, or mixed |

| n | Reading | n | Reading |
|---|---|---|---|
| 1 | RF counter | 7 | SINAD, S/N or distortion (RX) |
| 2 | Power | 8 | Distortion (TX) |
| 3 | Modulation frequency | 9, 10 | Positive, negative peak |
| 4 | Modulation level | 27, 28 | Generator frequency, level |
| 5 | AF frequency | 29, 30 | AF generator frequency, level |
| 6 | AF level | 31, 32 | Modulation frequency, level setting |
| 14-25 | Received tones 1-12, as frequencies | 26 | Tone standard |
| | | 33, 34 | RF frequency and level increment |
| | | 35, 36 | AF generator frequency and level increment |
| | | 37, 38 | Modulation frequency and level increment |

## Writing to memory

| Command | Meaning |
|---|---|
| `SE`, `SD` | Allow or block memory writes. Stored permanently. |
| `POm,n` | Write one byte |
| `DUm,n,...` | Write up to 64 bytes |

**These can overwrite calibration data.** `SV` output contains these commands. Do not send it to
an instrument unless you mean to.

A `DU` to `EC00`-`EC3F` is kept in memory and applied by the next `RS`.

## Screen

| Command | Meaning |
|---|---|
| `CS` | Clear the screen |
| `WRc,r,text` | Write text at column c, row r |
| `SP` | Stop updating the GPIB indicators |
| `RS` | Restore the normal screen |
| `DS`, `ES` | Stop or allow all writing on the screen, including `CS`, `WR` and `BX` |
| `BXn,m` | Boxes beside the mode keys. n: bit 0 is the top key. m: length, plus 64 to clear the screen first, 32 for `CONTINUE`, 128 for `RETURN`. |

## Remote state

In the remote state, the front panel is locked and the screen shows `REM`.

The HELP key returns to local, unless the controller has sent Local Lockout. A `LCL` box beside
the key shows when that is possible. `ADR` shows when the instrument is addressed.

## Status byte

The controller reads this with a serial poll.

| Bit | Meaning |
|---|---|
| 7 | Data ready |
| 6 | Service request |
| 5 | An error has occurred |
| 4 | Numeric entry error |
| 3 | Data error, such as a value out of range |
| 2 | Abnormal operation |
| 1 | Syntax error |
| 0 | Buffer overflow |

`SQn` sets when a service request is raised: 0 never, 1 on errors, 2 on errors and data ready.
`SRQ` appears on the screen until the controller polls.

Reading the data clears bits 7 and 5-0. `PG` clears them too.

## Entry error numbers

`ER` returns these. They are only recorded in the remote state.
The operating manual does not list them.

| Number | Meaning |
|---|---|
| 0 | No error |
| 4 | Level too high |
| 5 | Level too low |
| 6 | RF frequency out of range |
| 10 | Increment too small |
| 13 | Modulation or AF frequency too high |
| 14 | Modulation or AF frequency too low |
| 16 | Wrong unit for modulation (original firmware only) |
| 17 | Wrong unit for AF level |
| 18 | Modulation or AF level too high |
| 21 | RF frequency entered with a level unit |
| 25 | Wrong unit for a level increment |
| 26 | Modulation frequency entered with a level unit |

## Commands beyond the manual

These exist in the original firmware but the operating manual does not list them.

| Command | Meaning |
|---|---|
| `RV` | Steps of the VARIABLE control since the last `RV`. Negative numbers have a sign; the original answers 65535 for -1. |
| `BDn` | Non-zero: bar charts are no longer updated. `RS` undoes it. |

## Not done yet

`DR`, `BE` and `UR` are accepted but have no effect. They are also not in the manual. `DR` hides single
fields of the DUPLEX screen. `BE` and `UR` let a controller draw its own bar charts and readings.

`ET DT PR RR` and readings 11 to 13 belong to the RF directional power head, which is not supported.

A reading with no value answers `NULL`.
