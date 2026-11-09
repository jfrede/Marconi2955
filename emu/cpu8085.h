/* Intel 8085 CPU core. */
#ifndef CPU8085_H
#define CPU8085_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t (*cpu_read_fn)(void *ctx, uint16_t addr);
typedef void (*cpu_write_fn)(void *ctx, uint16_t addr, uint8_t value);

typedef struct {
    uint8_t reg[8]; /* B C D E H L (unused) A, indexed as in the opcode */
    uint16_t sp, pc;
    bool s, z, ac, p, cy;

    bool ie;        /* interrupts enabled */
    int ei_delay;   /* EI takes effect after the next instruction */
    bool halted;
    uint8_t mask;   /* SIM mask bits: 2=7.5, 1=6.5, 0=5.5 */
    bool latch75;   /* RST 7.5 edge latch */
    bool line55, line65; /* level-sensitive inputs, driven by the machine */
    bool sid, sod;

    uint64_t cycles;
    uint32_t irq_count[3]; /* accepted interrupts: RST 5.5, 6.5, 7.5 */
    bool bad_opcode; /* set when an undocumented opcode is reached */
    uint16_t bad_pc;

    cpu_read_fn read;
    cpu_write_fn write;
    void *ctx;
} cpu8085;

void cpu_reset(cpu8085 *cpu);

/* Executes one instruction or accepts one interrupt; returns T-states used. */
int cpu_step(cpu8085 *cpu);

/* Raises the RST 7.5 edge latch. */
void cpu_pulse_rst75(cpu8085 *cpu);

#endif
