#!/usr/bin/env python3
"""Small two-pass assembler for the Intel 8085.

Usage: asm85.py input.asm -o output.bin

Syntax: standard Intel mnemonics. Labels end in ':'. Comments start with ';'.
Directives: ORG, EQU, DB, DW, END. Numbers: decimal, 0x12, 12h, 101b, 'c'.
Expressions: terms joined by + and -, where a term is a number, a symbol or $.
DB accepts numbers and strings in double quotes ("" is a literal quote).
Unused bytes between ORG blocks are filled with FFh.
"""
import argparse
import re
import sys

REG = {r: i for i, r in enumerate("BCDEHLMA")}
PAIR = {"B": 0, "D": 1, "H": 2, "SP": 3}
STACK_PAIR = {"B": 0, "D": 1, "H": 2, "PSW": 3}
COND = {c: i for i, c in enumerate(["NZ", "Z", "NC", "C", "PO", "PE", "P", "M"])}

IMPLIED = {
    "NOP": 0x00, "RLC": 0x07, "RRC": 0x0F, "RAL": 0x17, "RAR": 0x1F, "RIM": 0x20,
    "DAA": 0x27, "CMA": 0x2F, "SIM": 0x30, "STC": 0x37, "CMC": 0x3F, "HLT": 0x76,
    "RET": 0xC9, "XTHL": 0xE3, "PCHL": 0xE9, "XCHG": 0xEB, "DI": 0xF3, "SPHL": 0xF9,
    "EI": 0xFB,
}
ALU_REG = {"ADD": 0x80, "ADC": 0x88, "SUB": 0x90, "SBB": 0x98,
           "ANA": 0xA0, "XRA": 0xA8, "ORA": 0xB0, "CMP": 0xB8}
ALU_IMM = {"ADI": 0xC6, "ACI": 0xCE, "SUI": 0xD6, "SBI": 0xDE,
           "ANI": 0xE6, "XRI": 0xEE, "ORI": 0xF6, "CPI": 0xFE}
PAIR_OPS = {"INX": 0x03, "DAD": 0x09, "DCX": 0x0B}
ADDRESS = {"SHLD": 0x22, "LHLD": 0x2A, "STA": 0x32, "LDA": 0x3A, "JMP": 0xC3, "CALL": 0xCD}
for cond, index in COND.items():
    ADDRESS["J" + cond] = 0xC2 | index << 3
    ADDRESS["C" + cond] = 0xC4 | index << 3
    IMPLIED["R" + cond] = 0xC0 | index << 3


class AsmError(Exception):
    pass


def split_operands(text):
    """Split at commas that are outside double quotes."""
    parts, current, quoted = [], "", False
    for ch in text:
        if ch == '"':
            quoted = not quoted
        if ch == "," and not quoted:
            parts.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip() or parts:
        parts.append(current.strip())
    return parts


def strip_comment(line):
    quoted = False
    for i, ch in enumerate(line):
        if ch == '"':
            quoted = not quoted
        elif ch == ";" and not quoted:
            return line[:i]
    return line


TOKEN = re.compile(r"\s*([+-]|'.'|\$|[A-Za-z_?.][\w?.]*|[0-9][0-9a-fA-FxXhHbB]*)")


def parse_number(text):
    low = text.lower()
    if low.startswith("0x"):
        return int(low[2:], 16)
    if low.endswith("h"):
        return int(low[:-1], 16)
    if low.endswith("b") and set(low[:-1]) <= {"0", "1"}:
        return int(low[:-1], 2)
    return int(low, 10)


def evaluate(text, symbols, pc, strict):
    total, sign, pos, expect_term = 0, 1, 0, True
    while pos < len(text):
        match = TOKEN.match(text, pos)
        if not match:
            raise AsmError(f"bad expression: {text}")
        token, pos = match.group(1), match.end()
        if token in "+-":
            if expect_term:
                sign = -sign if token == "-" else sign
            else:
                sign, expect_term = (1 if token == "+" else -1), True
            continue
        if not expect_term:
            raise AsmError(f"bad expression: {text}")
        if token == "$":
            value = pc
        elif token[0] == "'":
            value = ord(token[1])
        elif token[0].isdigit():
            value = parse_number(token)
        elif token in symbols:
            value = symbols[token]
        elif strict:
            raise AsmError(f"undefined symbol: {token}")
        else:
            value = 0
        total += sign * value
        sign, expect_term = 1, False
    if expect_term:
        raise AsmError(f"bad expression: {text}")
    return total


def byte(value):
    if not -128 <= value <= 255:
        raise AsmError(f"8-bit value out of range: {value}")
    return value & 0xFF


def word(value):
    if not -32768 <= value <= 65535:
        raise AsmError(f"16-bit value out of range: {value}")
    return [value & 0xFF, value >> 8 & 0xFF]


def lookup(table, name, what):
    key = name.upper()
    if key not in table:
        raise AsmError(f"bad {what}: {name}")
    return table[key]


def expect(ops, count, mnemonic):
    if len(ops) != count:
        raise AsmError(f"{mnemonic} takes {count} operand(s)")


def encode(mnemonic, ops, ev):
    """Return the bytes for one instruction; ev() evaluates an expression."""
    if mnemonic in IMPLIED:
        expect(ops, 0, mnemonic)
        return [IMPLIED[mnemonic]]
    if mnemonic == "MOV":
        expect(ops, 2, mnemonic)
        dst, src = lookup(REG, ops[0], "register"), lookup(REG, ops[1], "register")
        if dst == 6 and src == 6:
            raise AsmError("MOV M,M does not exist")
        return [0x40 | dst << 3 | src]
    if mnemonic == "MVI":
        expect(ops, 2, mnemonic)
        return [0x06 | lookup(REG, ops[0], "register") << 3, byte(ev(ops[1]))]
    if mnemonic in ("INR", "DCR"):
        expect(ops, 1, mnemonic)
        return [(0x04 if mnemonic == "INR" else 0x05) | lookup(REG, ops[0], "register") << 3]
    if mnemonic in ALU_REG:
        expect(ops, 1, mnemonic)
        return [ALU_REG[mnemonic] | lookup(REG, ops[0], "register")]
    if mnemonic in ALU_IMM:
        expect(ops, 1, mnemonic)
        return [ALU_IMM[mnemonic], byte(ev(ops[0]))]
    if mnemonic == "LXI":
        expect(ops, 2, mnemonic)
        return [0x01 | lookup(PAIR, ops[0], "register pair") << 4] + word(ev(ops[1]))
    if mnemonic in PAIR_OPS:
        expect(ops, 1, mnemonic)
        return [PAIR_OPS[mnemonic] | lookup(PAIR, ops[0], "register pair") << 4]
    if mnemonic in ("PUSH", "POP"):
        expect(ops, 1, mnemonic)
        return [(0xC5 if mnemonic == "PUSH" else 0xC1)
                | lookup(STACK_PAIR, ops[0], "register pair") << 4]
    if mnemonic in ("LDAX", "STAX"):
        expect(ops, 1, mnemonic)
        pair = lookup({"B": 0, "D": 1}, ops[0], "register pair")
        return [(0x0A if mnemonic == "LDAX" else 0x02) | pair << 4]
    if mnemonic in ADDRESS:
        expect(ops, 1, mnemonic)
        return [ADDRESS[mnemonic]] + word(ev(ops[0]))
    if mnemonic == "RST":
        expect(ops, 1, mnemonic)
        vector = ev(ops[0])
        if not 0 <= vector <= 7:
            raise AsmError(f"bad RST number: {vector}")
        return [0xC7 | vector << 3]
    if mnemonic in ("IN", "OUT"):
        expect(ops, 1, mnemonic)
        return [0xDB if mnemonic == "IN" else 0xD3, byte(ev(ops[0]))]
    raise AsmError(f"unknown mnemonic: {mnemonic}")


def db_bytes(ops, ev):
    out = []
    for op in ops:
        if op.startswith('"'):
            if len(op) < 2 or not op.endswith('"'):
                raise AsmError(f"bad string: {op}")
            out += [ord(c) for c in op[1:-1].replace('""', '"')]
        else:
            out.append(byte(ev(op)))
    return out


LABEL = re.compile(r"\s*([A-Za-z_?.][\w?.]*):(.*)$")
EQU = re.compile(r"\s*([A-Za-z_?.][\w?.]*)\s+[Ee][Qq][Uu]\s+(.+)$")


def run_pass(lines, symbols, strict):
    """Assemble once; returns {address: byte}."""
    memory, pc = {}, 0
    for number, raw in enumerate(lines, 1):
        try:
            text = strip_comment(raw).rstrip()
            label = LABEL.match(text)
            if label:
                name = label.group(1)
                if not strict and name in symbols:
                    raise AsmError(f"duplicate symbol: {name}")
                symbols[name] = pc
                text = label.group(2)
            if not text.strip():
                continue
            ev = lambda expr: evaluate(expr, symbols, pc, strict)
            equ = EQU.match(text)
            if equ:
                if not strict and equ.group(1) in symbols:
                    raise AsmError(f"duplicate symbol: {equ.group(1)}")
                symbols[equ.group(1)] = evaluate(equ.group(2), symbols, pc, True)
                continue
            parts = text.split(None, 1)
            mnemonic = parts[0].upper()
            ops = split_operands(parts[1]) if len(parts) > 1 else []
            if mnemonic == "END":
                break
            if mnemonic == "ORG":
                expect(ops, 1, mnemonic)
                pc = evaluate(ops[0], symbols, pc, True)
                continue
            if mnemonic == "DB":
                data = db_bytes(ops, ev)
            elif mnemonic == "DW":
                data = [b for op in ops for b in word(ev(op))]
            else:
                data = encode(mnemonic, ops, ev)
            for value in data:
                if pc in memory:
                    raise AsmError(f"address {pc:04X} assigned twice")
                memory[pc] = value
                pc += 1
        except (AsmError, ValueError) as error:
            raise SystemExit(f"line {number}: {error}\n  {raw.rstrip()}")
    return memory


def assemble(source):
    lines = source.splitlines()
    symbols = {}
    run_pass(lines, symbols, strict=False)
    return run_pass(lines, symbols, strict=True), symbols


def main():
    parser = argparse.ArgumentParser(description="Intel 8085 assembler")
    parser.add_argument("input")
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args()
    with open(args.input, encoding="latin-1") as handle:
        memory, symbols = assemble(handle.read())
    if not memory:
        sys.exit("no code generated")
    low, high = min(memory), max(memory)
    with open(args.output, "wb") as handle:
        handle.write(bytes(memory.get(a, 0xFF) for a in range(low, high + 1)))
    print(f"{args.output}: {high - low + 1} bytes, {low:04X}-{high:04X}, {len(symbols)} symbols")


if __name__ == "__main__":
    main()
