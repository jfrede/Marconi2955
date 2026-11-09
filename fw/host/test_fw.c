/*
 * Runs the firmware modules on the host against the emulator's register model.
 * Usage: test_fw [SCENARIO.scn REFERENCE.screen]
 * With a scenario, replays its key presses and knob steps and compares the
 * settings rows of the screen at every checkpoint with the stored reference.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../emu/machine.h"
#include "../app.h"
#include "../hal.h"
#include "../keys.h"
#include "../level.h"
#include "../meter.h"
#include "../regs.h"
#include "../synth.h"

#define LINE_SIZE 64

static machine m;
static int failures;

uint8_t hal_read(uint16_t addr) { return m.cpu.read(&m, addr); }
static int log_synth;
void hal_write(uint16_t addr, uint8_t value)
{
    if (log_synth && (addr == REG_SYNTH_BAND || addr == REG_SYNTH_DATA))
        printf("%04X=%02X ", addr, value);
    m.cpu.write(&m, addr, value);
}
void hal_screen_wait(void) {}
void hal_delay_ms(uint16_t ms) { (void)ms; }
void hal_delay_us(uint16_t us) { (void)us; }
uint32_t hal_millis(void) { return (uint32_t)(m.cpu.cycles / 5000u); }

/* The RF counter gate runs on the CPU clock, which the host build does not advance. */
static void poll_all(void)
{
    app_poll();
    if (m.rf_gate_at) {
        m.rf_gate_at = 0;
        m.rf_gate_done = m.rf_present;
    }
    /* Whatever the emulator would have timed since the last poll. */
    m.cpu.cycles += 2000;
    machine_tick(&m);
}

static void test_keys(void)
{
    if (keys_scan() != 0) {
        puts("FAIL: key reported with none held");
        failures++;
    }
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            m.key_row = row;
            m.key_col = col;
            uint8_t code = keys_scan();
            if (code != KEY_CODE(row, col)) {
                printf("FAIL: key %d,%d returned %u\n", row, col, code);
                failures++;
            }
        }
    }
    m.key_row = m.key_col = -1;
}

static void read_screen_lines(FILE *f, char lines[][LINE_SIZE], int count)
{
    for (int i = 0; i < count; i++) {
        if (!fgets(lines[i], LINE_SIZE, f))
            lines[i][0] = 0;
        lines[i][strcspn(lines[i], "\r\n")] = 0;
    }
}

static void compare_screen(FILE *reference, int checkpoint)
{
    static const int rows[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    char ours[SCREEN_ROWS][LINE_SIZE], theirs[SCREEN_ROWS][LINE_SIZE];

    FILE *tmp = tmpfile();
    machine_print_screen(&m, tmp);
    rewind(tmp);
    read_screen_lines(tmp, ours, SCREEN_ROWS);
    fclose(tmp);
    read_screen_lines(reference, theirs, SCREEN_ROWS);

    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        const char *a = ours[rows[i]], *b = theirs[rows[i]];
        if (strcmp(a, b)) {
            printf("FAIL: checkpoint %d row %d\n  reference: %s\n  firmware:  %s\n", checkpoint, rows[i], b, a);
            failures++;
        }
    }
}

static void skip_lines(FILE *f, int count)
{
    char line[LINE_SIZE * 4];
    while (count-- && fgets(line, sizeof line, f))
        ;
}

static void replay(const char *scenario, const char *reference)
{
    char line[128];
    int checkpoints = 0;

    FILE *script = fopen(scenario, "r");
    FILE *golden = fopen(reference, "r");

    if (!script || !golden) {
        puts("FAIL: cannot open scenario or reference");
        failures++;
        return;
    }
    while (fgets(line, sizeof line, script)) {
        int a, b;
        unsigned long long hz;
        if (sscanf(line, "key %d %d", &a, &b) == 2) {
            app_key(KEY_CODE(a, b));
        } else if (sscanf(line, "knob %d", &a) == 1) {
            app_knob((int8_t)a);
        } else if (sscanf(line, "rf %llu", &hz) == 1) {
            machine_set_rf(&m, hz, true);
        } else if (sscanf(line, "adc %d %d", &a, &b) == 2) {
            m.adc[a] = (uint8_t)b;
        } else if (sscanf(line, "run %llu", &hz) == 1) {
            for (unsigned long long i = 0; i < hz / 20000u; i++)
                poll_all();
        } else if (!strncmp(line, "settings", 8))
            skip_lines(golden, 4);
        else if (!strncmp(line, "screen", 6))
            compare_screen(golden, ++checkpoints);
    }
    fclose(script);
    fclose(golden);
    printf("%d checkpoints compared\n", checkpoints);
}

int main(int argc, char **argv)
{
    machine_init(&m);
    if (argc == 5 && !strcmp(argv[1], "level")) {
        if (!machine_load(argv[4], m.eeprom, sizeof m.eeprom, true)) {
            puts("cannot load EEPROM image");
            return 1;
        }
        level_set((int16_t)atoi(argv[2]), (uint8_t)atoi(argv[3]));
        printf("E017=%02X E01A=%02X E01B=%02X E001=%02X\n", m.io[REG_ATTEN - IO_BASE],
               m.io[REG_LEVEL_DAC_LOW - IO_BASE], m.io[REG_LEVEL_DAC_HIGH - IO_BASE],
               m.io[REG_SYNTH_DATA - IO_BASE]);
        return 0;
    }
    if (argc == 6 && !strcmp(argv[1], "meter")) {
        /* Applies a voltmeter input, runs the measurement and prints the reading rows. */
        char rows[SCREEN_ROWS][LINE_SIZE];
        if (!machine_load(argv[4], m.eeprom, sizeof m.eeprom, true)) {
            puts("cannot load EEPROM image");
            return 1;
        }
        m.af_model = true;
        m.af_volts = atof(argv[2]);
        m.af_notch_volts = atof(argv[3]);
        m.status_in = 0x04;
        app_init();
        if (atoi(argv[5]))
            app_key(KEY_MOD_ON_OFF);
        for (int i = 0; i < 2000; i++)
            app_poll();
        FILE *tmp = tmpfile();
        machine_print_screen(&m, tmp);
        rewind(tmp);
        read_screen_lines(tmp, rows, SCREEN_ROWS);
        fclose(tmp);
        uint8_t values[4];
        meter_debug(values);
        printf("%s\n%s\nDBG %02X %02X %02X %02X\n", rows[11], rows[13], values[0], values[1], values[2], values[3]);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "afreq")) {
        /* Feeds a period count, varied by one so that each sample differs, and prints the reading. */
        int status = atoi(argv[2]), period = atoi(argv[3]);
        char rows[SCREEN_ROWS][LINE_SIZE];
        if (!machine_load(argv[4], m.eeprom, sizeof m.eeprom, true)) {
            puts("cannot load EEPROM image");
            return 1;
        }
        m.status_in = (uint8_t)(0x04 | status);
        app_init();
        for (int i = 0; i < 20000; i++) {
            m.timer_hold[3] = (uint16_t)(period + (i / 11) % 2);
            app_poll();
        }
        FILE *tmp = tmpfile();
        machine_print_screen(&m, tmp);
        rewind(tmp);
        read_screen_lines(tmp, rows, SCREEN_ROWS);
        fclose(tmp);
        printf("%s\n", rows[11]);
        return 0;
    }
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "script")) {
        /* Runs an emulator script from stdin against the firmware, with the same output.
         * An optional file gives the settings the NOVRAM holds at power-on. */
        char line[128];
        if (!machine_load(argv[2], m.eeprom, sizeof m.eeprom, true)) {
            puts("cannot load EEPROM image");
            return 1;
        }
        if (argc == 4 && !machine_load_settings(&m, argv[3])) {
            puts("cannot load settings");
            return 1;
        }
        m.status_in = 0x04;
        app_power_on();
        while (fgets(line, sizeof line, stdin)) {
            int a, b;
            unsigned long long n;
            unsigned addr;
            if (sscanf(line, "key %d %d", &a, &b) == 2) {
                app_key(KEY_CODE(a, b));
            } else if (!strncmp(line, "gpibread", 8)) {
                int empty = 0;
                while (empty < 2) {
                    m.gpib_out_len = 0;
                    m.gpib_line_done = false;
                    m.gpib_addr_status = 0x02;
                    m.gpib_isr2 |= 0x01;
                    m.gpib_do_at = m.cpu.cycles + 100;
                    for (int i = 0; i < 60000 && !m.gpib_line_done; i++)
                        poll_all();
                    m.gpib_addr_status = 0;
                    m.gpib_isr2 |= 0x01;
                    m.gpib_do_at = 0;
                    for (int i = 0; i < 20; i++)
                        poll_all();
                    fwrite(m.gpib_out, 1, m.gpib_out_len, stdout);
                    empty = m.gpib_out_len <= 1 ? empty + 1 : 0;
                }
            } else if (!strncmp(line, "gpib ", 5)) {
                char *text = line + 5;
                text[strcspn(text, "\r\n")] = 0;
                machine_gpib_send(&m, text);
            } else if (!strncmp(line, "vin ", 4)) {
                double volts, notch;
                if (sscanf(line, "vin %lf %lf", &volts, &notch) == 2) {
                    m.af_model = true;
                    m.af_volts = volts;
                    m.af_notch_volts = notch;
                }
            } else if (sscanf(line, "status %i", &a) == 1) {
                m.status_in = (uint8_t)a;
            } else if (sscanf(line, "remote %d", &a) == 1) {
                machine_gpib_set_remote(&m, a != 0);
            } else if (sscanf(line, "lockout %d", &a) == 1) {
                machine_gpib_set_lockout(&m, a != 0);
            } else if (!strncmp(line, "spoll", 5)) {
                printf("SPOLL=%02X\n", machine_gpib_serial_poll(&m));
            } else if (sscanf(line, "knob %d", &a) == 1) {
                app_knob((int8_t)a);
            } else if (sscanf(line, "rf %llu", &n) == 1) {
                machine_set_rf(&m, n, true);
            } else if (sscanf(line, "adc %d %d", &a, &b) == 2) {
                m.adc[a] = (uint8_t)b;
            } else if (sscanf(line, "run %llu", &n) == 1) {
                for (unsigned long long i = 0; i < n / 20000u; i++)
                    poll_all();
            } else if (sscanf(line, "peek %x", &addr) == 1) {
                printf("%04X=%02X\n", addr, hal_read((uint16_t)addr));
            } else if (sscanf(line, "rawscreen %d %d", &a, &b) == 2) {
                for (int row = a; row <= b; row++) {
                    printf("%2d|", row);
                    for (int col = 0; col < SCREEN_COLS; col++)
                        printf("%02X", m.screen[row][col]);
                    printf("\n");
                }
            }
        }
        return 0;
    }
    if (argc >= 5 && !strcmp(argv[1], "modes")) {
        /* Applies an input, presses keys with measurements in between, prints the second reading. */
        char rows[SCREEN_ROWS][LINE_SIZE];
        if (!machine_load(argv[4], m.eeprom, sizeof m.eeprom, true)) {
            puts("cannot load EEPROM image");
            return 1;
        }
        m.af_model = true;
        m.af_volts = atof(argv[2]);
        m.af_notch_volts = atof(argv[3]);
        m.status_in = 0x04;
        app_init();
        for (int i = 0; i < 3000; i++)
            app_poll();
        for (int i = 5; i + 1 < argc; i += 2) {
            app_key(KEY_CODE(atoi(argv[i]), atoi(argv[i + 1])));
            for (int j = 0; j < 3000; j++)
                app_poll();
        }
        FILE *tmp = tmpfile();
        machine_print_screen(&m, tmp);
        rewind(tmp);
        read_screen_lines(tmp, rows, SCREEN_ROWS);
        fclose(tmp);
        printf("E03B=%02X E03E=%02X\n", m.io[0xE03B - IO_BASE], m.io[0xE03E - IO_BASE] & 0x04);
        for (int r = 11; r <= 19; r += 2)
            printf("%s\n", rows[r]);
        printf("E03D=%02X E03E=%02X\n", m.io[0xE03D - IO_BASE], m.io[0xE03E - IO_BASE]);
        for (int row = 13; row < SCREEN_ROWS; row++) {
            printf("%2d|", row);
            for (int col = 0; col < SCREEN_COLS; col++)
                printf("%02X", m.screen[row][col]);
            printf("\n");
        }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "keys")) {
        /* Presses the keys given as row and column pairs and prints the AF switching state. */
        char rows[SCREEN_ROWS][LINE_SIZE];
        if (!machine_load(argv[2], m.eeprom, sizeof m.eeprom, true)) {
            puts("cannot load EEPROM image");
            return 1;
        }
        app_init();
        for (int i = 3; i + 1 < argc; i += 2)
            app_key(KEY_CODE(atoi(argv[i]), atoi(argv[i + 1])));
        FILE *tmp = tmpfile();
        machine_print_screen(&m, tmp);
        rewind(tmp);
        read_screen_lines(tmp, rows, SCREEN_ROWS);
        fclose(tmp);
        printf("E03B=%02X E03E=%02X\n%s\n%s\n", m.io[0xE03B - IO_BASE], m.io[0xE03E - IO_BASE], rows[13], rows[15]);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "sweep")) {
        /* Reads "V VN" lines, measures each and prints the bar chart rows as character codes. */
        char input[64];
        double volts, notch;
        if (!machine_load(argv[2], m.eeprom, sizeof m.eeprom, true)) {
            puts("cannot load EEPROM image");
            return 1;
        }
        m.af_model = true;
        m.status_in = 0x04;
        m.af_volts = 1.0;
        m.af_notch_volts = 0.1;
        app_init();
        while (fgets(input, sizeof input, stdin) && sscanf(input, "%lf %lf", &volts, &notch) == 2) {
            m.af_volts = volts;
            m.af_notch_volts = notch;
            for (int i = 0; i < 2000; i++)
                app_poll();
            for (int row = 11; row < SCREEN_ROWS; row++) {
                printf("%2d|", row);
                for (int col = 0; col < SCREEN_COLS; col++)
                    printf("%02X", m.screen[row][col]);
                printf("\n");
            }
            uint8_t values[4];
            meter_debug(values);
            printf("DBG %02X %02X %02X %02X\n", values[0], values[1], values[2], values[3]);
        }
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "regs")) {
        /* Replays a scenario with the real EEPROM and prints the final control registers. */
        static const uint16_t list[] = {0xE010, 0xE011, 0xE012, 0xE013, 0xE015, 0xE016, 0xE018, 0xE019, 0xE01A};
        char line[128];
        FILE *script = fopen(argv[2], "r");
        if (!script || !machine_load(argv[3], m.eeprom, sizeof m.eeprom, true)) {
            puts("cannot open scenario or EEPROM image");
            return 1;
        }
        app_init();
        while (fgets(line, sizeof line, script)) {
            int a, b;
            if (sscanf(line, "key %d %d", &a, &b) == 2)
                app_key(KEY_CODE(a, b));
            else if (sscanf(line, "knob %d", &a) == 1)
                app_knob((int8_t)a);
        }
        fclose(script);
        for (size_t i = 0; i < sizeof list / sizeof list[0]; i++)
            printf("%04X=%02X ", list[i], m.io[list[i] - IO_BASE]);
        printf("\n");
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "track")) {
        if (!machine_load(argv[3], m.eeprom, sizeof m.eeprom, true)) {
            puts("cannot load EEPROM image");
            return 1;
        }
        synth_set_frequency(strtol(argv[2], NULL, 10));
        printf("E016=%02X E018=%02X E019=%02X\n", m.io[REG_ROUTING - IO_BASE],
               m.io[REG_MOD_TRACKING - IO_BASE], m.io[REG_IF_CORRECTION - IO_BASE]);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "synth")) {
        log_synth = 1;
        synth_set_frequency(strtol(argv[2], NULL, 10));
        printf("\n");
        return 0;
    }
    test_keys();
    m.status_in = 0x04;
    app_init();
    if (argc == 3)
        replay(argv[1], argv[2]);
    puts(failures ? "firmware tests FAILED" : "firmware tests passed");
    return failures != 0;
}
