#include "readings.h"

#define READING_SIZE 16u

static char values[READING_COUNT][READING_SIZE];

void readings_set(uint8_t index, const char *text)
{
    uint8_t i = 0;

    while (text[i] && i < READING_SIZE - 1u) {
        values[index][i] = text[i];
        i++;
    }
    values[index][i] = 0;
}

const char *readings_get(uint8_t index)
{
    return values[index];
}

void readings_clear(void)
{
    for (uint8_t i = 0; i < READING_COUNT; i++)
        values[i][0] = 0;
}
