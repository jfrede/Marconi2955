#include "keys.h"

#include "control.h"
#include "hal.h"
#include "regs.h"

#define NO_ROW 0xFFu
#define STABLE_SCANS 96u        /* identical scans in a row before a key counts, as in the original */
#define CONTROL_KEY_LATCH 0x20u  /* EE00 bit 5: a low pulse clears the key-pressed latch */

uint8_t keys_scan(void)
{
    uint8_t code = 0;

    /* Driving all columns low pulls down the row of a held key. */
    hal_write(REG_KEY_COLS, 0x00);
    uint8_t rows = hal_read(REG_KEY_ROWS);
    if (rows == NO_ROW)
        return 0;

    uint8_t row = 0;
    while (rows & (1u << row))
        row++;

    /* Raising only the held key's column releases its row. */
    for (uint8_t col = 0; col < 8; col++) {
        hal_write(REG_KEY_COLS, (uint8_t)(1u << col));
        if (hal_read(REG_KEY_ROWS) == NO_ROW) {
            code = (uint8_t)(row * 8u + col + 1u);
            break;
        }
    }
    hal_write(REG_KEY_COLS, 0x00);
    return code;
}

uint8_t keys_read(void)
{
    uint8_t code;
    uint8_t same;

    do {
        code = keys_scan();
        for (same = 0; same < STABLE_SCANS; same++)
            if (keys_scan() != code)
                break;
    } while (same < STABLE_SCANS);
    return code;
}

void keys_rearm(void)
{
    control_set(CONTROL_KEY_LATCH, 0);
    control_set(CONTROL_KEY_LATCH, CONTROL_KEY_LATCH);
}
