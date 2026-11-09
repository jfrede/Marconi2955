#!/usr/bin/env bash
# Plays every scenario and writes screen/settings output and the sequence of I/O writes.
# Usage: run_scenarios.sh ROM EEPROM OUTDIR [GOLDENDIR]
# With GOLDENDIR the results are compared with a stored reference.
# The .io files hold "address value" pairs, without cycle counts or program addresses,
# so that a different firmware implementation can be compared with them.
set -u

if [ $# -lt 3 ]; then
    echo "usage: $0 ROM EEPROM OUTDIR [GOLDENDIR]" >&2
    exit 2
fi
rom=$1 eeprom=$2 out=$3 golden=${4:-}
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
status=0

for scenario in "$here"/scenarios/*.scn; do
    name=$(basename "$scenario" .scn)
    trace=$(mktemp)
    if ! "$here/build/emu2955" --rom "$rom" --eeprom "$eeprom" --script "$scenario" \
        --trace "$trace" > "$out/$name.screen"; then
        echo "$name: emulator failed"
        status=1
    fi
    awk '{print $3, $4}' "$trace" > "$out/$name.io"
    rm -f "$trace"
    if [ -n "$golden" ]; then
        if cmp -s "$out/$name.screen" "$golden/$name.screen" && cmp -s "$out/$name.io" "$golden/$name.io"; then
            echo "$name: PASS"
        else
            echo "$name: FAIL"
            status=1
        fi
    else
        echo "$name: recorded ($(wc -l < "$out/$name.io") I/O writes)"
    fi
done
exit $status
