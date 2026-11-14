# Marconi 2955 – open firmware

Goal: a functionally equivalent, open re-implementation of the firmware of the Marconi 2955
radio communications test set, with bug fixes and convenience features (quick frequency entry,
user-defined power-on default, plain-text self-test error messages).

## Layout
- `docs/`   – hardware and behaviour specification (own text, publishable)
- `emu/`    – 8085 emulator and instrument peripherals (C)
- `fw/`     – new firmware (C, platform-neutral with a hardware abstraction layer)
- `tools/`  – helper scripts for building and comparing
- `hardware/` – KiCad projects of the add-on boards
- `private/` – original ROMs, manuals, Ghidra project, disassembly. **Not in the repository.**

## Documentation
Start with [docs/architecture.md](docs/architecture.md). The other files in `docs/` explain how
each function works, the registers, the stored data, GPIB, and what is still missing.

## Rules
- Original ROMs, their disassembly and the manual PDFs stay private (see `.gitignore`).
- Only own code and self-written specification are published.
- Before any test on the instrument: back up EEPROM IC13. It holds the calibration data, which
  cannot be recreated. The NOVRAMs hold only the current settings; GPIB `SV` reads them out if
  you want to keep them.

## Licence

- `fw/`, `emu/` and `tools/`: GNU General Public License, version 3 or later (GPL-3.0-or-later). See [LICENSE](LICENSE).
- `docs/`: Creative Commons Attribution 4.0 International (CC BY 4.0). See [docs/LICENSE](docs/LICENSE).
- `hardware/`: CERN Open Hardware Licence Version 2, Strongly Reciprocal (CERN-OHL-S-2.0). See [hardware/LICENSE](hardware/LICENSE).

Original ROMs and manuals are not part of this project and are not covered by these licences.
