/* Intel 8085 CPU core: documented instructions, RIM/SIM and hardware interrupts. */
#include "cpu8085.h"

#include <string.h>

enum { B, C, D, E, H, L, M, A };

static uint8_t rd(cpu8085 *c, uint16_t a) { return c->read(c->ctx, a); }
static void wr(cpu8085 *c, uint16_t a, uint8_t v) { c->write(c->ctx, a, v); }

static uint8_t fetch(cpu8085 *c) { return rd(c, c->pc++); }

static uint16_t fetch16(cpu8085 *c)
{
    uint16_t lo = fetch(c);
    return (uint16_t)(lo | fetch(c) << 8);
}

static uint16_t hl(const cpu8085 *c) { return (uint16_t)(c->reg[H] << 8 | c->reg[L]); }

static uint8_t get_r(cpu8085 *c, int r) { return r == M ? rd(c, hl(c)) : c->reg[r]; }

static void set_r(cpu8085 *c, int r, uint8_t v)
{
    if (r == M)
        wr(c, hl(c), v);
    else
        c->reg[r] = v;
}

/* Register pairs in opcode order: BC, DE, HL, SP. */
static uint16_t get_rp(const cpu8085 *c, int p)
{
    if (p == 3)
        return c->sp;
    return (uint16_t)(c->reg[p * 2] << 8 | c->reg[p * 2 + 1]);
}

static void set_rp(cpu8085 *c, int p, uint16_t v)
{
    if (p == 3) {
        c->sp = v;
    } else {
        c->reg[p * 2] = (uint8_t)(v >> 8);
        c->reg[p * 2 + 1] = (uint8_t)v;
    }
}

static void push16(cpu8085 *c, uint16_t v)
{
    wr(c, --c->sp, (uint8_t)(v >> 8));
    wr(c, --c->sp, (uint8_t)v);
}

static uint16_t pop16(cpu8085 *c)
{
    uint16_t lo = rd(c, c->sp++);
    return (uint16_t)(lo | rd(c, c->sp++) << 8);
}

static void szp(cpu8085 *c, uint8_t v)
{
    c->s = v & 0x80;
    c->z = v == 0;
    c->p = !__builtin_parity(v);
}

static uint8_t psw(const cpu8085 *c)
{
    return (uint8_t)(c->s << 7 | c->z << 6 | c->ac << 4 | c->p << 2 | 1 << 1 | c->cy);
}

static void set_psw(cpu8085 *c, uint8_t f)
{
    c->s = f & 0x80;
    c->z = f & 0x40;
    c->ac = f & 0x10;
    c->p = f & 0x04;
    c->cy = f & 0x01;
}

static void add(cpu8085 *c, uint8_t v, bool carry)
{
    unsigned a = c->reg[A], r = a + v + carry;
    c->ac = (a & 15) + (v & 15) + carry > 15;
    c->cy = r > 255;
    c->reg[A] = (uint8_t)r;
    szp(c, (uint8_t)r);
}

/* Subtraction as the hardware does it: A + ~v + !borrow; CY is the inverted carry. */
static uint8_t sub(cpu8085 *c, uint8_t v, bool borrow)
{
    unsigned a = c->reg[A], nv = (uint8_t)~v, r = a + nv + !borrow;
    c->ac = (a & 15) + (nv & 15) + !borrow > 15;
    c->cy = r <= 255;
    szp(c, (uint8_t)r);
    return (uint8_t)r;
}

static void logic(cpu8085 *c, int op, uint8_t v)
{
    switch (op) {
    case 4: c->reg[A] &= v; c->ac = true; break; /* 8085: AND always sets AC */
    case 5: c->reg[A] ^= v; c->ac = false; break;
    default: c->reg[A] |= v; c->ac = false; break;
    }
    c->cy = false;
    szp(c, c->reg[A]);
}

static void alu(cpu8085 *c, int op, uint8_t v)
{
    switch (op) {
    case 0: add(c, v, false); break;
    case 1: add(c, v, c->cy); break;
    case 2: c->reg[A] = sub(c, v, false); break;
    case 3: c->reg[A] = sub(c, v, c->cy); break;
    case 7: sub(c, v, false); break;
    default: logic(c, op, v); break;
    }
}

static void daa(cpu8085 *c)
{
    uint8_t a = c->reg[A], adjust = 0;
    bool carry = c->cy;
    if (c->ac || (a & 15) > 9)
        adjust = 0x06;
    if (c->cy || a > 0x99) {
        adjust |= 0x60;
        carry = true;
    }
    c->ac = (a & 15) + (adjust & 15) > 15;
    a = (uint8_t)(a + adjust);
    c->reg[A] = a;
    szp(c, a);
    c->cy = carry;
}

static void rotate(cpu8085 *c, int y)
{
    uint8_t a = c->reg[A];
    switch (y) {
    case 0: c->cy = a & 0x80; a = (uint8_t)(a << 1 | c->cy); break;       /* RLC */
    case 1: c->cy = a & 1; a = (uint8_t)(a >> 1 | c->cy << 7); break;     /* RRC */
    case 2: { bool o = c->cy; c->cy = a & 0x80; a = (uint8_t)(a << 1 | o); break; } /* RAL */
    default: { bool o = c->cy; c->cy = a & 1; a = (uint8_t)(a >> 1 | o << 7); break; } /* RAR */
    }
    c->reg[A] = a;
}

static bool cond(const cpu8085 *c, int y)
{
    switch (y) {
    case 0: return !c->z;
    case 1: return c->z;
    case 2: return !c->cy;
    case 3: return c->cy;
    case 4: return !c->p;
    case 5: return c->p;
    case 6: return !c->s;
    default: return c->s;
    }
}

void cpu_reset(cpu8085 *c)
{
    cpu_read_fn r = c->read;
    cpu_write_fn w = c->write;
    void *ctx = c->ctx;
    memset(c, 0, sizeof *c);
    c->read = r;
    c->write = w;
    c->ctx = ctx;
    c->mask = 7;
}

void cpu_pulse_rst75(cpu8085 *c) { c->latch75 = true; }

static int accept_interrupt(cpu8085 *c)
{
    uint16_t vector;
    if (c->latch75 && !(c->mask & 4)) {
        c->latch75 = false;
        c->irq_count[2]++;
        vector = 0x3C;
    } else if (c->line65 && !(c->mask & 2)) {
        c->irq_count[1]++;
        vector = 0x34;
    } else if (c->line55 && !(c->mask & 1)) {
        c->irq_count[0]++;
        vector = 0x2C;
    } else {
        return 0;
    }
    c->ie = false;
    c->halted = false;
    push16(c, c->pc);
    c->pc = vector;
    return 12;
}

static void illegal(cpu8085 *c, uint16_t pc)
{
    c->bad_opcode = true;
    c->bad_pc = pc;
}

static int execute(cpu8085 *c)
{
    uint16_t start = c->pc, w;
    uint8_t op = fetch(c), v;
    int x = op >> 6, y = op >> 3 & 7, z = op & 7, p = y >> 1, q = y & 1;
    bool take;

    if (x == 1) {
        if (op == 0x76) {
            c->halted = true;
            return 5;
        }
        set_r(c, y, get_r(c, z));
        return (y == M || z == M) ? 7 : 4;
    }
    if (x == 2) {
        alu(c, y, get_r(c, z));
        return z == M ? 7 : 4;
    }
    if (x == 0) {
        switch (z) {
        case 0:
            if (y == 0)
                return 4;
            if (y == 4) { /* RIM */
                c->reg[A] = (uint8_t)(c->sid << 7 | c->latch75 << 6 | c->line65 << 5 | c->line55 << 4
                                      | c->ie << 3 | c->mask);
                return 4;
            }
            if (y == 6) { /* SIM */
                v = c->reg[A];
                if (v & 0x08)
                    c->mask = v & 7;
                if (v & 0x10)
                    c->latch75 = false;
                if (v & 0x40)
                    c->sod = v & 0x80;
                return 4;
            }
            illegal(c, start);
            return 4;
        case 1:
            if (q == 0) {
                set_rp(c, p, fetch16(c));
                return 10;
            }
            {
                unsigned r = (unsigned)hl(c) + get_rp(c, p);
                c->cy = r > 0xFFFF;
                set_rp(c, 2, (uint16_t)r);
                return 10;
            }
        case 2:
            switch (op) {
            case 0x02: wr(c, get_rp(c, 0), c->reg[A]); return 7;
            case 0x12: wr(c, get_rp(c, 1), c->reg[A]); return 7;
            case 0x0A: c->reg[A] = rd(c, get_rp(c, 0)); return 7;
            case 0x1A: c->reg[A] = rd(c, get_rp(c, 1)); return 7;
            case 0x22: w = fetch16(c); wr(c, w, c->reg[L]); wr(c, (uint16_t)(w + 1), c->reg[H]); return 16;
            case 0x2A: w = fetch16(c); c->reg[L] = rd(c, w); c->reg[H] = rd(c, (uint16_t)(w + 1)); return 16;
            case 0x32: w = fetch16(c); wr(c, w, c->reg[A]); return 13;
            default: w = fetch16(c); c->reg[A] = rd(c, w); return 13;
            }
        case 3:
            set_rp(c, p, (uint16_t)(get_rp(c, p) + (q ? -1 : 1)));
            return 6;
        case 4:
            v = (uint8_t)(get_r(c, y) + 1);
            c->ac = (v & 15) == 0;
            szp(c, v);
            set_r(c, y, v);
            return y == M ? 10 : 4;
        case 5:
            v = (uint8_t)(get_r(c, y) - 1);
            c->ac = (v & 15) != 15;
            szp(c, v);
            set_r(c, y, v);
            return y == M ? 10 : 4;
        case 6:
            set_r(c, y, fetch(c));
            return y == M ? 10 : 7;
        default:
            switch (y) {
            case 4: daa(c); break;
            case 5: c->reg[A] = (uint8_t)~c->reg[A]; break;
            case 6: c->cy = true; break;
            case 7: c->cy = !c->cy; break;
            default: rotate(c, y); break;
            }
            return 4;
        }
    }

    switch (z) {
    case 0:
        if (cond(c, y)) {
            c->pc = pop16(c);
            return 12;
        }
        return 6;
    case 1:
        if (q == 0) {
            w = pop16(c);
            if (p == 3) {
                c->reg[A] = (uint8_t)(w >> 8);
                set_psw(c, (uint8_t)w);
            } else {
                set_rp(c, p, w);
            }
            return 10;
        }
        switch (p) {
        case 0: c->pc = pop16(c); return 10;
        case 2: c->pc = hl(c); return 6;
        case 3: c->sp = hl(c); return 6;
        default: illegal(c, start); return 4;
        }
    case 2:
        w = fetch16(c);
        if (cond(c, y)) {
            c->pc = w;
            return 10;
        }
        return 7;
    case 3:
        switch (y) {
        case 0: c->pc = fetch16(c); return 10;
        case 2: fetch(c); return 10; /* OUT: nothing is attached */
        case 3: fetch(c); c->reg[A] = 0xFF; return 10; /* IN */
        case 4:
            w = hl(c);
            v = rd(c, c->sp);
            wr(c, c->sp, c->reg[L]);
            c->reg[L] = v;
            v = rd(c, (uint16_t)(c->sp + 1));
            wr(c, (uint16_t)(c->sp + 1), (uint8_t)(w >> 8));
            c->reg[H] = v;
            return 16;
        case 5:
            w = get_rp(c, 1);
            set_rp(c, 1, hl(c));
            set_rp(c, 2, w);
            return 4;
        case 6: c->ie = false; c->ei_delay = 0; return 4;
        case 7: c->ei_delay = 2; return 4;
        default: illegal(c, start); return 4;
        }
    case 4:
        w = fetch16(c);
        take = cond(c, y);
        if (take) {
            push16(c, c->pc);
            c->pc = w;
            return 18;
        }
        return 9;
    case 5:
        if (q == 0) {
            if (p == 3)
                push16(c, (uint16_t)(c->reg[A] << 8 | psw(c)));
            else
                push16(c, get_rp(c, p));
            return 12;
        }
        if (p == 0) {
            w = fetch16(c);
            push16(c, c->pc);
            c->pc = w;
            return 18;
        }
        illegal(c, start);
        return 4;
    case 6:
        alu(c, y, fetch(c));
        return 7;
    default:
        push16(c, c->pc);
        c->pc = (uint16_t)(y * 8);
        return 12;
    }
}

int cpu_step(cpu8085 *c)
{
    int used = 0;

    if (c->ei_delay > 0 && --c->ei_delay == 0)
        c->ie = true;
    if (c->ie && !c->ei_delay)
        used = accept_interrupt(c);
    if (!used)
        used = c->halted ? 4 : execute(c);
    c->cycles += (uint64_t)used;
    return used;
}
