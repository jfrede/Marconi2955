/* Text formatting and keypad entry helpers shared by the instrument screens. */
#ifndef FORMAT_H
#define FORMAT_H

#include <stdint.h>

/* Writes value with the given number of decimals; returns a pointer to the terminator. */
char *put_fixed(char *out, uint32_t value, uint8_t decimals);
char *put_text(char *out, const char *text);

/* Prints text and blanks the rest of the field. */
void print_field(uint8_t col, uint8_t row, const char *text, uint8_t width);

/* Converts a keypad entry to a value scaled by scale; returns 0 for malformed input. */
uint8_t parse_entry(const char *entry, uint8_t length, int32_t scale, int32_t *result);

uint32_t round_scaled(double value, double scale);

#endif
