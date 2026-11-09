/* 40 x 32 character screen. */
#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

#define SCREEN_COLS 40
#define SCREEN_ROWS 32
#define SCREEN_UNDERLINE 35u
#define SCREEN_CURSOR 0xDCu /* marks the next position under a keypad entry */
#define SCREEN_MILLI 39u /* character code of the small m */
#define SCREEN_REVERSE_TOP 59u /* top edge of reverse-field text, in the row above it */

/* While off, screen_put writes nothing; for the GPIB command that stops the display updating. */
void screen_set_output(uint8_t on);
uint8_t screen_output(void);

void screen_clear(void);
void screen_put(uint8_t col, uint8_t row, uint8_t code);

/* Prints ASCII text, translated to the instrument's character codes. */
void screen_print(uint8_t col, uint8_t row, const char *text);

/* Prints upper-case text in reverse field; row must be at least 1. */
void screen_print_reverse(uint8_t col, uint8_t row, const char *text);

/* The arrow-shaped box of a mode key beside the screen; position 0 is the top key. */
void screen_softkey_box(uint8_t position, uint8_t width);

#endif
