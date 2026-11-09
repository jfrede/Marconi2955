/* Marconi 2955 memory map and peripherals, as far as they are modelled so far. */
#ifndef MACHINE_H
#define MACHINE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cpu8085.h"

#define ROM_SIZE 0xE000u
#define SCREEN_COLS 40
#define SCREEN_ROWS 32
#define IO_BASE 0xE000u
#define IO_SIZE 0x1000u
#define GPIB_BUFFER 8192u
#define RF_DIGITS 9

typedef struct {
    cpu8085 cpu;
    uint8_t rom[ROM_SIZE];
    uint8_t eeprom[0x800];
    uint8_t ram[0x800];
    uint8_t io[IO_SIZE];              /* last value written, FFh until then */
    uint32_t io_reads[IO_SIZE];
    uint32_t io_writes[IO_SIZE];
    uint8_t screen[SCREEN_ROWS][SCREEN_COLS];
    uint8_t screen_col, screen_row;
    uint8_t columns;                  /* keyboard column drive */
    uint8_t leds;
    int key_row, key_col;             /* -1 when no key is held */
    uint64_t key_release;
    bool key_irq;                     /* key flip-flop, drives RST 6.5 */
    uint8_t status_in;                /* static bits of the EE00 status input */
    uint64_t rom_writes, eeprom_writes;
    FILE *trace;

    /* NEC uPD7210 GPIB controller */
    uint8_t gpib_isr1, gpib_isr2, gpib_imr1, gpib_imr2, gpib_addr_status;
    uint8_t gpib_switches;
    uint8_t gpib_in[GPIB_BUFFER];
    size_t gpib_in_len, gpib_in_pos;
    uint8_t gpib_out[GPIB_BUFFER];
    size_t gpib_out_len;
    uint64_t gpib_di_at, gpib_do_at; /* 0 = nothing scheduled */
    bool gpib_line_done;             /* the talker has sent LF */
    bool gpib_remote;                /* instrument is in the GPIB remote state */
    bool gpib_lockout;               /* local lockout: HELP may not return to local */

    uint8_t adc[4];                  /* ADC0809 inputs: peak, trough, RMS, horizontal shift */
    uint8_t adc_output;              /* result of the last conversion started by a write */
    bool af_model;                   /* the RMS input is derived from the voltages below */
    double af_volts, af_notch_volts; /* voltmeter input, without and with the notch filter */

    uint16_t watch_addr;             /* reads of this address are counted per program counter */
    uint32_t watch_hits[0x10000];

    /* Am9513: only reading hold registers is modelled, values come from the script */
    uint16_t timer_hold[6];          /* index 1-5 is the counter number */
    uint8_t timer_group;             /* counter selected by the data pointer */
    bool timer_high_byte;

    /* RF counter: nine BCD digits, selected through AA15 and read from AA14 */
    uint64_t rf_hz;                  /* applied frequency */
    bool rf_present;                 /* a signal is applied, measurements complete */
    bool rf_gate_done;               /* Am9513 counter 2 output, status bit 2 */
    uint64_t rf_gate_at;             /* 0 = no measurement running */
} machine;

void machine_init(machine *m);
bool machine_load(const char *path, uint8_t *dst, size_t size, bool exact);
bool machine_load_settings(machine *m, const char *path);
void machine_run(machine *m, uint64_t cycles);
bool machine_run_to(machine *m, uint16_t pc, uint64_t max_cycles); /* false on timeout */
void machine_press_key(machine *m, int row, int col, uint64_t hold);
void machine_knob(machine *m, int direction);
void machine_tick(machine *m);                         /* applies time-dependent events */
void machine_gpib_send(machine *m, const char *text);  /* addressed to listen, text + LF */
void machine_gpib_set_remote(machine *m, bool remote);
void machine_gpib_set_lockout(machine *m, bool lockout);
/* A serial poll: returns the status byte and withdraws the service request. */
uint8_t machine_gpib_serial_poll(machine *m);
void machine_set_rf(machine *m, uint64_t hz, bool present);
/* Addresses the instrument to talk until it ends a line; returns the line length. */
size_t machine_gpib_read_line(machine *m, uint64_t max_cycles);
void machine_print_screen(const machine *m, FILE *out);

#endif
