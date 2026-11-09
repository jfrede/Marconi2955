#!/usr/bin/env python3
"""Generates fw/generated/level_table.h from an ISS 16 ROM image.

Usage: gen_level_table.py ROM_IMAGE OUTPUT.h

The fine level table is data from the original firmware and is not a closed formula,
so it is extracted at build time. The generated file must not be published.
"""
import sys

TABLE_ADDRESS = 0xCF2B
TABLE_SIZE = 120


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    with open(sys.argv[1], "rb") as handle:
        image = handle.read()
    table = image[TABLE_ADDRESS:TABLE_ADDRESS + TABLE_SIZE]
    with open(sys.argv[2], "w") as out:
        out.write("/* Generated from the ROM image by tools/gen_level_table.py. Do not publish. */\n")
        out.write(f"#define LEVEL_TABLE_SIZE {TABLE_SIZE}\n")
        out.write("static const unsigned char level_table[LEVEL_TABLE_SIZE] = {\n")
        for start in range(0, TABLE_SIZE, 12):
            out.write("    " + ", ".join(str(b) for b in table[start:start + 12]) + ",\n")
        out.write("};\n")


if __name__ == "__main__":
    main()
