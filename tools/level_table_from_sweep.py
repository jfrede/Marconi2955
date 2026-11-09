#!/usr/bin/env python3
"""Builds fw/level_table.h from a measured sweep of the fine level DAC.

Input: a CSV file with the columns dac,dbm (the DAC high byte and the measured level), an EEPROM
image for the per-dB trim values, and the level measured when the instrument is set to 0 dBm.
"""
import sys

ENTRIES = 120
FIRST_POSITION_INDEX = 5  # the table index of step position 0 in the 0 dB attenuator step
TRIM_OFFSET = 0x70C       # EEPROM offset of the signed trim for each 1 dB position
TRIMS = 11
COUNTS_PER_TRIM = 0.25    # one trim count, in counts of the DAC high byte


def load(path):
    points = []
    with open(path) as source:
        next(source)
        for line in source:
            fields = line.strip().split(",")
            points.append((int(fields[0]), float(fields[1])))
    return sorted(points)


def dac_for_level(points, target):
    """Interpolates the fractional DAC value that gives the target level."""
    for (low_dac, low_dbm), (high_dac, high_dbm) in zip(points, points[1:]):
        if low_dbm <= target <= high_dbm and high_dbm > low_dbm:
            return low_dac + (high_dac - low_dac) * (target - low_dbm) / (high_dbm - low_dbm)
    return float(points[-1][0] if target > points[-1][1] else points[0][0])


def main(sweep_path, eeprom_path, reference_dbm, output_path):
    points = load(sweep_path)
    with open(eeprom_path, "rb") as source:
        eeprom = source.read()
    trims = [b - 256 if b >= 128 else b for b in eeprom[TRIM_OFFSET:TRIM_OFFSET + TRIMS]]
    values = []
    for index in range(ENTRIES):
        position = index - FIRST_POSITION_INDEX
        trim = trims[min(max(position, 0) // 10, TRIMS - 1)]
        dac = dac_for_level(points, reference_dbm - position * 0.1) - trim * COUNTS_PER_TRIM
        values.append(max(0, min(255, round(dac))))
    with open(output_path, "w") as out:
        out.write("/* Measured with tools/level_table_from_sweep.py. Index is 0.1 dB of fine attenuation. */\n")
        out.write("#ifndef LEVEL_TABLE_H\n#define LEVEL_TABLE_H\n\n#include <stdint.h>\n\n")
        out.write(f"#define LEVEL_TABLE_SIZE {ENTRIES}u\n\nstatic const uint8_t level_table[LEVEL_TABLE_SIZE] = {{\n")
        for start in range(0, ENTRIES, 12):
            out.write("    " + ", ".join(str(v) for v in values[start:start + 12]) + ",\n")
        out.write("};\n\n#endif\n")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit("usage: level_table_from_sweep.py SWEEP.csv EEPROM.bin REFERENCE_DBM OUTPUT.h")
    main(sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4])
