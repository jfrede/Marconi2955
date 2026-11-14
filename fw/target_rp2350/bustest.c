/*
 * Bus test and monitor over the USB serial port, for bringing up the board.
 * It only touches the bus when told to. Commands (numbers in hex):
 *   r ADDR            read one byte
 *   d ADDR COUNT      dump bytes
 *   w ADDR VALUE      write one byte (refused in the EEPROM and NOVRAM)
 *   s                 show the interrupt and reset inputs
 *   t                 repeat a fixed pattern of cycles for the logic analyzer, until a key
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

#include "../hal.h"
#include "board.h"
#include "hal_rp2350.h"

#define LINE_SIZE 64
#define DUMP_PER_LINE 16u

static void read_line(char *line, size_t size)
{
    size_t n = 0;

    for (;;) {
        int c = getchar();
        if (c == '\r' || c == '\n') {
            putchar('\n');
            break;
        }
        if ((c == 8 || c == 127) && n > 0) {
            n--;
            printf("\b \b");
        } else if (c >= ' ' && n + 1 < size) {
            line[n++] = (char)c;
            putchar(c);
        }
    }
    line[n] = '\0';
}

static void dump(uint16_t addr, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (i % DUMP_PER_LINE == 0)
            printf("%s%04X:", i ? "\n" : "", (unsigned)(uint16_t)(addr + i));
        printf(" %02X", hal_read((uint16_t)(addr + i)));
    }
    printf("\n");
}

/* Writes go to the screen position registers, which are harmless; reads hit RAM, timer, EEPROM. */
static void pattern(void)
{
    printf("running, press a key to stop\n");
    while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
        bus_write(0xE026, 0x55);
        bus_write(0xE027, 0xAA);
        (void)hal_read(0xF800);
        (void)hal_read(0xEA01);
        (void)hal_read(0xF000);
        busy_wait_us(20); /* a marker for the trigger */
    }
}

static void show_inputs(void)
{
    static const char *const names[] = {"RST5.5", "RST6.5", "RST7.5", "SID", "RESETIN", "5MHz"};
    uint32_t in = board_inputs();

    for (unsigned i = 0; i < 6; i++)
        printf("%s=%u ", names[i], (unsigned)(in >> i & 1u));
    printf("\n");
}

int main(void)
{
    char line[LINE_SIZE];

    stdio_init_all();
    board_start();

    for (;;) {
        printf("> ");
        read_line(line, sizeof line);

        char *end;
        char cmd = line[0];
        unsigned long a = strtoul(line + 1, &end, 16);
        unsigned long b = strtoul(end, NULL, 16);

        switch (cmd) {
        case 'r': printf("%04lX=%02X\n", a, hal_read((uint16_t)a)); break;
        case 'd': dump((uint16_t)a, b ? b : DUMP_PER_LINE); break;
        case 'w': hal_poke((uint16_t)a, (uint8_t)b); break;
        case 's': show_inputs(); break;
        case 't': pattern(); break;
        case '\0': break;
        default: printf("r ADDR | d ADDR COUNT | w ADDR VALUE | s | t\n"); break;
        }
    }
}
