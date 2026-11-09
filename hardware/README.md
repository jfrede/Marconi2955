# Hardware

Add-on boards for the Marconi 2955, as KiCad projects with Gerber files.

| Folder | Board |
|---|---|
| `step-attenuator/` | Solid-state replacement for the coarse step attenuator |
| `om345-replacement/` | Replacement for the OM345 hybrid amplifier, using a MAR-7SM+ |
| `10mhz-to-1mhz/` | Converts a 10 MHz reference signal to 1 MHz |

## Note on the MAR-7SM+ footprint

The symbol and footprint library for the MAR-7SM+ is not included, because its licence does not
allow it to be passed on as a library. It comes from SamacSys. The board file still contains the
footprint, so the board opens and can be fabricated as it is. To change the footprint, get the
library for the MAR-7SM+ yourself and add it to the project as a library called `RF`.
