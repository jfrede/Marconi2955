#include "format.h"

#include "screen.h"

char *put_fixed(char *out, uint32_t value, uint8_t decimals)
{
    char digits[11];
    uint8_t count = 0;

    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value || count <= decimals);

    while (count--) {
        *out++ = digits[count];
        if (count == decimals && decimals)
            *out++ = '.';
    }
    *out = 0;
    return out;
}

char *put_text(char *out, const char *text)
{
    while (*text)
        *out++ = *text++;
    *out = 0;
    return out;
}

void print_field(uint8_t col, uint8_t row, const char *text, uint8_t width)
{
    uint8_t used = 0;
    const char *p = text;

    while (*p++)
        used++;
    screen_print(col, row, text);
    while (used < width)
        screen_put((uint8_t)(col + used++), row, ' ');
}

uint8_t parse_entry(const char *entry, uint8_t length, int32_t scale, int32_t *result)
{
    int32_t mantissa = 0;
    int32_t divisor = 1;
    uint8_t in_fraction = 0;
    uint8_t negative = 0;
    uint8_t digits = 0;

    for (uint8_t i = 0; i < length; i++) {
        char c = entry[i];
        if (c == '-' && i == 0) {
            negative = 1;
        } else if (c == '.' && !in_fraction) {
            in_fraction = 1;
        } else if (c >= '0' && c <= '9') {
            mantissa = mantissa * 10 + (c - '0');
            if (in_fraction)
                divisor *= 10;
            digits++;
        } else {
            return 0;
        }
    }
    if (!digits)
        return 0;
    *result = mantissa * (scale / divisor);
    if (negative)
        *result = -*result;
    return 1;
}

uint32_t round_scaled(double value, double scale)
{
    return (uint32_t)(value * scale + 0.5);
}
