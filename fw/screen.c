#include "screen.h"

#include "hal.h"
#include "regs.h"

#define REVERSE_POINT 1u

/* The character generator has lower case only for a few unit symbols. */
static uint8_t to_screen_code(char c)
{
    switch (c) {
    case 'u': return 36;
    case 'z': return 34;
    case 'm': return 39;
    case 'd': return 40;
    case 'k': return 41;
    default: return (uint8_t)c;
    }
}

void screen_put(uint8_t col, uint8_t row, uint8_t code)
{
    hal_screen_wait();
    hal_write(REG_SCREEN_ROW, row);
    hal_write(REG_SCREEN_COL, col);
    hal_write(REG_SCREEN_CHAR, code);
}

void screen_clear(void)
{
    for (uint8_t row = 0; row < SCREEN_ROWS; row++)
        for (uint8_t col = 0; col < SCREEN_COLS; col++)
            screen_put(col, row, ' ');
}

/* Reverse-field letters follow the alphabet; the digits are scattered in the character set. */
#define REVERSE_MINUS 0u

static uint8_t reverse_code(char c)
{
    static const uint8_t digits[10] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    if (c >= 'A' && c <= 'Z')
        return (uint8_t)(c + 32);
    if (c >= '0' && c <= '9')
        return digits[c - '0'];
    if (c == '-')
        return REVERSE_MINUS;
    return c == '.' ? REVERSE_POINT : (uint8_t)c;
}

#define SOFTKEY_ROWS_PER_KEY 4u
#define SOFTKEY_TIP_COL 38u
#define BOX_TOP_LEFT 0x3Cu
#define BOX_SIDE 0x3Fu
#define BOX_BOTTOM_LEFT 0x3Eu
#define BOX_HORIZONTAL 0x40u
#define BOX_TOP_RIGHT 0x5Bu
#define BOX_TIP 0x5Du
#define BOX_BOTTOM_RIGHT 0x5Cu

void screen_print_reverse(uint8_t col, uint8_t row, const char *text)
{
    for (; *text; text++, col++) {
        char c = *text;
        /* Character codes 97-122 are the reverse-field letters. */
        screen_put(col, row, reverse_code(c));
        screen_put(col, (uint8_t)(row - 1), SCREEN_REVERSE_TOP);
    }
}

void screen_print(uint8_t col, uint8_t row, const char *text)
{
    for (; *text; text++) {
        screen_put(col, row, to_screen_code(*text));
        if (++col == SCREEN_COLS) {
            col = 0;
            row++;
        }
    }
}

void screen_softkey_box(uint8_t position, uint8_t width)
{
    uint8_t top = (uint8_t)(position * SOFTKEY_ROWS_PER_KEY);
    uint8_t left = (uint8_t)(SOFTKEY_TIP_COL - width - 1u);

    screen_put(left, top, BOX_TOP_LEFT);
    screen_put(left, (uint8_t)(top + 1u), BOX_SIDE);
    screen_put(left, (uint8_t)(top + 2u), BOX_BOTTOM_LEFT);
    for (uint8_t col = (uint8_t)(left + 1u); col < SOFTKEY_TIP_COL; col++) {
        screen_put(col, top, BOX_HORIZONTAL);
        screen_put(col, (uint8_t)(top + 2u), BOX_HORIZONTAL);
    }
    screen_put(SOFTKEY_TIP_COL, top, BOX_TOP_RIGHT);
    screen_put(SOFTKEY_TIP_COL + 1u, (uint8_t)(top + 1u), BOX_TIP);
    screen_put(SOFTKEY_TIP_COL, (uint8_t)(top + 2u), BOX_BOTTOM_RIGHT);
}
