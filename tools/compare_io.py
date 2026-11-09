#!/usr/bin/env python3
"""Compares the hardware control writes of two scenario runs.

Usage: compare_io.py reference.io candidate.io

Input lines are "ADDRESS VALUE" in hexadecimal, as written by emu/run_scenarios.sh.
Only registers that set up the instrument are compared. For each register the sequence
of changed values must match, so repeated writes and timing differences do not count.
Measurement cycles, keyboard scanning and screen access are ignored.
"""
import sys

CONTROL = {0xE000, 0xE001} | set(range(0xE010, 0xE01C)) | {0xE03B, 0xE03D, 0xE03E}


def transitions(path):
    changes = {}
    with open(path) as handle:
        for line in handle:
            addr, value = (int(field, 16) for field in line.split())
            if addr in CONTROL:
                seq = changes.setdefault(addr, [])
                if not seq or seq[-1] != value:
                    seq.append(value)
    return changes


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    reference, candidate = transitions(sys.argv[1]), transitions(sys.argv[2])
    failures = 0
    for addr in sorted(set(reference) | set(candidate)):
        ref, cand = reference.get(addr, []), candidate.get(addr, [])
        if ref != cand:
            failures += 1
            pos = next((i for i, (a, b) in enumerate(zip(ref, cand)) if a != b), min(len(ref), len(cand)))
            print(f"{addr:04X}: differs at change {pos}: reference has {len(ref)} changes, candidate {len(cand)}")
    total = sum(len(seq) for seq in reference.values())
    print(f"{len(reference)} registers, {total} value changes in the reference, {failures} registers differ")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
