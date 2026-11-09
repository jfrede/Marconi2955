/*
 * Marconi 2955 emulator, command line front end.
 *
 * Usage: emu2955 --rom FILE [--eeprom FILE] [--settings FILE] [--trace FILE]
 *                [--script FILE | --cycles N]
 *
 * Script commands, one per line:
 *   run N          run N CPU T-states
 *   key ROW COL    press a key (0-7, 0-7) for a default hold time
 *   knob +1|-1     one VARIABLE knob step
 *   screen         print the screen
 *   stat           print CPU state and I/O statistics
 *   settings       print EC00-EC3F in the layout of the GPIB SV command
 *   adc CH VALUE   set ADC input CH (0 peak, 1 trough, 2 RMS, 3 horizontal shift)
 *   vin V [VN]     apply V volts to the voltmeter, VN volts behind the notch filter
 *   status VALUE   set the static bits (6-0) of the EE00 status input
 *   peek ADDR      print one memory byte (hex address)
 *   runto ADDR N   run until the program counter reaches ADDR, at most N T-states
 *   watch ADDR     count reads of ADDR per program counter
 *   watchlist      print the counts collected by watch
 *   timer N VALUE  set the hold register value of Am9513 counter N (1-5)
 *   rf HZ|off      apply a signal of HZ hertz to the RF counter, or remove it
 *   remote 0|1     leave or enter the GPIB remote state
 *   gpib TEXT      send TEXT and LF to the instrument over GPIB
 *   gpibread N     read lines from the instrument until it has no more data;
 *                  N is the T-state limit per line
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "machine.h"

#define KEY_HOLD_CYCLES 200000u

static void print_stat(const machine *m)
{
    const cpu8085 *c = &m->cpu;
    printf("cycles=%" PRIu64 " pc=%04X sp=%04X ie=%d mask=%X halted=%d rom_writes=%" PRIu64
           " eeprom_writes=%" PRIu64 " leds=%02X\n",
           c->cycles, c->pc, c->sp, c->ie, c->mask, c->halted, m->rom_writes, m->eeprom_writes, m->leds);
    printf("irq 5.5=%u 6.5=%u 7.5=%u sod=%d\n", c->irq_count[0], c->irq_count[1], c->irq_count[2], c->sod);
    if (c->bad_opcode)
        printf("undocumented opcode at %04X\n", c->bad_pc);
    printf("io reads:");
    for (unsigned a = 0; a < IO_SIZE; a++)
        if (m->io_reads[a])
            printf(" %04X:%u", IO_BASE + a, m->io_reads[a]);
    printf("\nio writes:");
    for (unsigned a = 0; a < IO_SIZE; a++)
        if (m->io_writes[a])
            printf(" %04X:%u", IO_BASE + a, m->io_writes[a]);
    printf("\n");
}

/* Prints EC00-EC3F in the same layout as the GPIB SV command. */
static void print_settings(const machine *m)
{
    for (unsigned a = 0xEC00; a < 0xEC40; a += 16) {
        printf("DU%u", a);
        for (unsigned i = 0; i < 16; i++)
            printf(",%u", m->io[a + i - IO_BASE]);
        printf("\n");
    }
}

static int run_script(machine *m, const char *path)
{
    char line[128], cmd[16];
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return 1;
    }
    while (fgets(line, sizeof line, f)) {
        unsigned long long n;
        int a, b;
        if (sscanf(line, "%15s", cmd) != 1 || cmd[0] == '#')
            continue;
        if (!strcmp(cmd, "run") && sscanf(line, "%*s %llu", &n) == 1) {
            machine_run(m, n);
        } else if (!strcmp(cmd, "key") && sscanf(line, "%*s %d %d", &a, &b) == 2) {
            machine_press_key(m, a, b, KEY_HOLD_CYCLES);
        } else if (!strcmp(cmd, "knob") && sscanf(line, "%*s %d", &a) == 1) {
            machine_knob(m, a);
        } else if (!strcmp(cmd, "screen")) {
            machine_print_screen(m, stdout);
        } else if (!strcmp(cmd, "rawscreen") && sscanf(line, "%*s %d %d", &a, &b) == 2) {
            /* Prints the character codes of rows a to b, in hexadecimal. */
            for (int row = a; row <= b && row < SCREEN_ROWS; row++) {
                printf("%2d|", row);
                for (int col = 0; col < SCREEN_COLS; col++)
                    printf("%02X", m->screen[row][col]);
                printf("\n");
            }
        } else if (!strcmp(cmd, "stat")) {
            print_stat(m);
        } else if (!strcmp(cmd, "settings")) {
            print_settings(m);
        } else if (!strcmp(cmd, "adc") && sscanf(line, "%*s %d %d", &a, &b) == 2 && a >= 0 && a < 4) {
            m->adc[a] = (uint8_t)b;
        } else if (!strcmp(cmd, "vin")) {
            double volts = 0, notch = 0;
            if (sscanf(line, "%*s %lf %lf", &volts, &notch) >= 1) {
                m->af_model = true;
                m->af_volts = volts;
                m->af_notch_volts = notch;
            }
        } else if (!strcmp(cmd, "status") && sscanf(line, "%*s %i", &a) == 1) {
            m->status_in = (uint8_t)a;
        } else if (!strcmp(cmd, "watch") && sscanf(line, "%*s %x", (unsigned *)&a) == 1) {
            m->watch_addr = (uint16_t)a;
        } else if (!strcmp(cmd, "watchlist")) {
            for (unsigned pc = 0; pc < 0x10000; pc++)
                if (m->watch_hits[pc])
                    printf("watch %04X read near %04X: %u\n", m->watch_addr, pc, m->watch_hits[pc]);
        } else if (!strcmp(cmd, "runto") && sscanf(line, "%*s %x %llu", (unsigned *)&a, &n) == 2) {
            if (!machine_run_to(m, (uint16_t)a, n))
                printf("runto %04X: timeout\n", (unsigned)a);
        } else if (!strcmp(cmd, "peek") && sscanf(line, "%*s %x", (unsigned *)&a) == 1) {
            printf("%04X=%02X\n", (unsigned)a, m->cpu.read(m, (uint16_t)a));
        } else if (!strcmp(cmd, "timer") && sscanf(line, "%*s %d %d", &a, &b) == 2 && a >= 1 && a <= 5) {
            m->timer_hold[a] = (uint16_t)b;
        } else if (!strcmp(cmd, "rf")) {
            bool present = sscanf(line, "%*s %llu", &n) == 1;
            machine_set_rf(m, present ? n : 0, present);
        } else if (!strcmp(cmd, "remote") && sscanf(line, "%*s %d", &a) == 1) {
            machine_gpib_set_remote(m, a != 0);
        } else if (!strcmp(cmd, "lockout") && sscanf(line, "%*s %d", &a) == 1) {
            machine_gpib_set_lockout(m, a != 0);
        } else if (!strcmp(cmd, "spoll")) {
            printf("SPOLL=%02X\n", machine_gpib_serial_poll(m));
        } else if (!strcmp(cmd, "gpib")) {
            char *text = strstr(line, "gpib") + 4;
            text += strspn(text, " \t");
            text[strcspn(text, "\r\n")] = 0;
            machine_gpib_send(m, text);
        } else if (!strcmp(cmd, "gpibread") && sscanf(line, "%*s %llu", &n) == 1) {
            int empty = 0;
            while (empty < 2) {
                size_t got = machine_gpib_read_line(m, n);
                fwrite(m->gpib_out, 1, got, stdout);
                empty = got <= 1 ? empty + 1 : 0;
            }
        } else {
            fprintf(stderr, "bad script line: %s", line);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    static machine m;
    const char *rom = NULL, *eeprom = NULL, *settings = NULL, *script = NULL, *trace = NULL;
    unsigned long long cycles = 20000000;

    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--rom"))
            rom = argv[i + 1];
        else if (!strcmp(argv[i], "--eeprom"))
            eeprom = argv[i + 1];
        else if (!strcmp(argv[i], "--settings"))
            settings = argv[i + 1];
        else if (!strcmp(argv[i], "--script"))
            script = argv[i + 1];
        else if (!strcmp(argv[i], "--trace"))
            trace = argv[i + 1];
        else if (!strcmp(argv[i], "--cycles"))
            cycles = strtoull(argv[i + 1], NULL, 0);
        else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 1;
        }
    }
    if (!rom) {
        fprintf(stderr, "usage: %s --rom FILE [--eeprom FILE] [--settings FILE] [--trace FILE]"
                        " [--script FILE | --cycles N]\n", argv[0]);
        return 1;
    }

    machine_init(&m);
    if (!machine_load(rom, m.rom, ROM_SIZE, true)) {
        fprintf(stderr, "cannot load ROM image: %s\n", rom);
        return 1;
    }
    if (eeprom && !machine_load(eeprom, m.eeprom, sizeof m.eeprom, true)) {
        fprintf(stderr, "cannot load EEPROM image: %s\n", eeprom);
        return 1;
    }
    if (settings && !machine_load_settings(&m, settings)) {
        fprintf(stderr, "cannot load settings: %s\n", settings);
        return 1;
    }
    if (trace && !(m.trace = fopen(trace, "w"))) {
        perror(trace);
        return 1;
    }

    int status = 0;
    if (script) {
        status = run_script(&m, script);
    } else {
        machine_run(&m, cycles);
        print_stat(&m);
        machine_print_screen(&m, stdout);
    }
    if (m.trace)
        fclose(m.trace);
    return status;
}
