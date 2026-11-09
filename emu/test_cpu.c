/*
 * Self-test for the 8085 core. Results are computed here from the rules in the
 * 8085 data sheet, independently of the helpers in cpu8085.c.
 */
#include <stdio.h>
#include <string.h>

#include "cpu8085.h"

static uint8_t mem[65536];
static cpu8085 cpu;
static int failures;

static uint8_t rd(void *ctx, uint16_t a) { (void)ctx; return mem[a]; }
static void wr(void *ctx, uint16_t a, uint8_t v) { (void)ctx; mem[a] = v; }

static void fail(const char *what, int a, int b, int c)
{
    if (failures++ < 20)
        printf("FAIL %s a=%02X b=%02X c=%d: A=%02X S%d Z%d AC%d P%d CY%d\n", what, a, b, c, cpu.reg[7],
               cpu.s, cpu.z, cpu.ac, cpu.p, cpu.cy);
}

static void setup(uint8_t a, uint8_t b, bool carry)
{
    cpu_reset(&cpu);
    cpu.sp = 0xF000;
    cpu.reg[7] = a;
    cpu.reg[0] = b;
    cpu.cy = carry;
}

static int even_parity(unsigned v)
{
    int bits = 0;
    for (int i = 0; i < 8; i++)
        bits += v >> i & 1;
    return bits % 2 == 0;
}

static void check_szp(const char *what, unsigned r, int a, int b, int c)
{
    r &= 255;
    if (cpu.s != (r >= 128) || cpu.z != (r == 0) || cpu.p != even_parity(r))
        fail(what, a, b, c);
}

/* Executes one instruction located at address 0. */
static int run1(uint8_t b0, uint8_t b1, uint8_t b2)
{
    mem[0] = b0;
    mem[1] = b1;
    mem[2] = b2;
    return cpu_step(&cpu);
}

static void test_alu(void)
{
    for (int a = 0; a < 256; a++) {
        for (int b = 0; b < 256; b++) {
            for (int c = 0; c < 2; c++) {
                setup((uint8_t)a, (uint8_t)b, c);
                run1(0x88, 0, 0); /* ADC B */
                unsigned sum = (unsigned)(a + b + c);
                if (cpu.reg[7] != (sum & 255) || cpu.cy != (sum > 255)
                    || cpu.ac != ((a & 15) + (b & 15) + c > 15))
                    fail("ADC", a, b, c);
                check_szp("ADC szp", sum, a, b, c);

                setup((uint8_t)a, (uint8_t)b, c);
                run1(0x98, 0, 0); /* SBB B */
                unsigned diff = (unsigned)(a - b - c) & 255;
                if (cpu.reg[7] != diff || cpu.cy != (a < b + c)
                    || cpu.ac != ((a & 15) >= (b & 15) + c))
                    fail("SBB", a, b, c);
                check_szp("SBB szp", diff, a, b, c);

                setup((uint8_t)a, (uint8_t)b, c);
                run1(0xB8, 0, 0); /* CMP B */
                if (cpu.reg[7] != a || cpu.cy != (a < b) || cpu.z != (a == b))
                    fail("CMP", a, b, c);

                setup((uint8_t)a, (uint8_t)b, c);
                run1(0xA0, 0, 0); /* ANA B */
                if (cpu.reg[7] != (a & b) || cpu.cy || !cpu.ac)
                    fail("ANA", a, b, c);
                check_szp("ANA szp", (unsigned)(a & b), a, b, c);

                setup((uint8_t)a, (uint8_t)b, c);
                run1(0xB0, 0, 0); /* ORA B */
                if (cpu.reg[7] != (a | b) || cpu.cy || cpu.ac)
                    fail("ORA", a, b, c);

                setup((uint8_t)a, (uint8_t)b, c);
                run1(0xA8, 0, 0); /* XRA B */
                if (cpu.reg[7] != (a ^ b) || cpu.cy || cpu.ac)
                    fail("XRA", a, b, c);
            }
        }
    }
}

static void test_inr_dcr_rotate(void)
{
    for (int a = 0; a < 256; a++) {
        for (int c = 0; c < 2; c++) {
            setup((uint8_t)a, 0, c);
            run1(0x3C, 0, 0); /* INR A */
            if (cpu.reg[7] != ((a + 1) & 255) || cpu.cy != c || cpu.ac != ((a & 15) == 15))
                fail("INR", a, 0, c);
            setup((uint8_t)a, 0, c);
            run1(0x3D, 0, 0); /* DCR A */
            if (cpu.reg[7] != ((a - 1) & 255) || cpu.cy != c || cpu.ac != ((a & 15) != 0))
                fail("DCR", a, 0, c);
            setup((uint8_t)a, 0, c);
            run1(0x07, 0, 0); /* RLC */
            if (cpu.reg[7] != (((a << 1) | (a >> 7)) & 255) || cpu.cy != (a >> 7))
                fail("RLC", a, 0, c);
            setup((uint8_t)a, 0, c);
            run1(0x0F, 0, 0); /* RRC */
            if (cpu.reg[7] != (((a >> 1) | (a << 7)) & 255) || cpu.cy != (a & 1))
                fail("RRC", a, 0, c);
            setup((uint8_t)a, 0, c);
            run1(0x17, 0, 0); /* RAL */
            if (cpu.reg[7] != (((a << 1) | c) & 255) || cpu.cy != (a >> 7))
                fail("RAL", a, 0, c);
            setup((uint8_t)a, 0, c);
            run1(0x1F, 0, 0); /* RAR */
            if (cpu.reg[7] != ((a >> 1) | (c << 7)) || cpu.cy != (a & 1))
                fail("RAR", a, 0, c);
        }
    }
}

static int bcd(int v) { return (v / 10) << 4 | v % 10; }

static void test_daa(void)
{
    for (int x = 0; x < 100; x++) {
        for (int y = 0; y < 100; y++) {
            setup((uint8_t)bcd(x), (uint8_t)bcd(y), false);
            run1(0x80, 0, 0); /* ADD B */
            cpu.pc = 0;
            run1(0x27, 0, 0); /* DAA */
            if (cpu.reg[7] != bcd((x + y) % 100) || cpu.cy != (x + y > 99))
                fail("DAA", x, y, 0);
        }
    }
}

static void test_pairs_and_stack(void)
{
    setup(0x12, 0x34, true);
    cpu.reg[1] = 0x56; /* C */
    cpu.reg[4] = 0x80; /* H */
    cpu.reg[5] = 0x01; /* L */
    run1(0x09, 0, 0);  /* DAD B: 8001 + 3456 */
    if (cpu.reg[4] != 0xB4 || cpu.reg[5] != 0x57 || cpu.cy)
        fail("DAD", 0, 0, 0);

    setup(0xA5, 0, true);
    cpu.s = true;
    run1(0xF5, 0, 0);  /* PUSH PSW */
    cpu.reg[7] = 0;
    cpu.cy = cpu.s = false;
    cpu.pc = 0;
    run1(0xF1, 0, 0);  /* POP PSW */
    if (cpu.reg[7] != 0xA5 || !cpu.cy || !cpu.s || cpu.sp != 0xF000)
        fail("PUSH/POP PSW", 0, 0, 0);

    setup(0, 0, false);
    if (run1(0xCD, 0x34, 0x12) != 18 || cpu.pc != 0x1234 || cpu.sp != 0xEFFE
        || mem[0xEFFE] != 3 || mem[0xEFFF] != 0)
        fail("CALL", 0, 0, 0);
    setup(0, 0, false);
    cpu.z = false;
    if (run1(0xCA, 0x34, 0x12) != 7 || cpu.pc != 3) /* JZ not taken */
        fail("JZ not taken", 0, 0, 0);
    setup(0, 0, false);
    if (run1(0xC8, 0, 0) != 6 || cpu.pc != 1) /* RZ not taken */
        fail("RZ not taken", 0, 0, 0);
}

static void test_interrupts(void)
{
    /* EI takes effect only after the following instruction. */
    setup(0, 0, false);
    cpu.mask = 0;
    cpu.line65 = true;
    memset(mem, 0, 16);
    mem[0] = 0xFB; /* EI */
    cpu_step(&cpu);
    if (cpu.pc != 1)
        fail("EI", 0, 0, 0);
    cpu_step(&cpu); /* NOP runs with the interrupt still deferred */
    if (cpu.pc != 2)
        fail("EI delay", 0, 0, 0);
    cpu_step(&cpu);
    if (cpu.pc != 0x34 || cpu.ie)
        fail("RST 6.5 vector", 0, 0, 0);

    /* Priority: 7.5 before 6.5 before 5.5, mask respected. */
    setup(0, 0, false);
    cpu.ie = true;
    cpu.mask = 4;
    cpu.latch75 = true;
    cpu.line65 = cpu.line55 = true;
    cpu_step(&cpu);
    if (cpu.pc != 0x34 || !cpu.latch75)
        fail("masked 7.5", 0, 0, 0);

    /* RIM reports SID, pending inputs, IE and mask. */
    setup(0, 0, false);
    cpu.sid = true;
    cpu.line55 = true;
    cpu.mask = 5;
    run1(0x20, 0, 0);
    if (cpu.reg[7] != (0x80 | 0x10 | 5))
        fail("RIM", 0, 0, 0);

    /* SIM sets the mask only with bit 3 and SOD only with bit 6. */
    setup(0xC2, 0, false);
    run1(0x30, 0, 0);
    if (cpu.mask != 7 || !cpu.sod)
        fail("SIM", 0, 0, 0);
    setup(0x0A, 0, false);
    run1(0x30, 0, 0);
    if (cpu.mask != 2 || cpu.sod)
        fail("SIM mask", 0, 0, 0);
}

int main(void)
{
    cpu.read = rd;
    cpu.write = wr;
    test_alu();
    test_inr_dcr_rotate();
    test_daa();
    test_pairs_and_stack();
    test_interrupts();
    printf(failures ? "%d failures\n" : "all CPU tests passed\n", failures);
    return failures != 0;
}
