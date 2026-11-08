# Marconi 2955 – offene Firmware

Ziel: Funktionsgleicher, offener Nachbau der Firmware des Marconi 2955 Funkmessplatzes
mit Bugfixes und Komfortfunktionen (Frequenz-Schnelleingabe, eigener Einschalt-Default,
Klartext-Fehlermeldungen im Selbsttest).

## Struktur
- `docs/`   – Hardware- und Verhaltensspezifikation (eigene Texte, veroeffentlichbar)
- `emu/`    – 8085-Emulator und Instrumenten-Peripherie (C)
- `fw/`     – neue Firmware (C, plattformneutral mit Hardware-Abstraktionsschicht)
- `tools/`  – Hilfsskripte (OCR, Plaene zusammensetzen, ...)
- `private/` – Original-ROMs, Handbuecher, Ghidra-Projekt, Disassembly. **Nicht im Repository.**

## Regeln
- Original-ROMs, deren Disassembly und die Handbuch-PDFs bleiben privat (siehe `.gitignore`).
- Oeffentlich sind nur eigener Code und selbst formulierte Spezifikation.
- Vor jedem Test am Geraet: Backup von EEPROM IC13 und den NOVRAMs (Kalibrierdaten!).
