/* The part of the hardware layer that all targets share: a copy of what was written. */
#include "hal.h"

#define SHADOW_BASE 0xE000u
#define SHADOW_SIZE 0x100u

static uint8_t shadow[SHADOW_SIZE];
static uint8_t written[SHADOW_SIZE];

void hal_write(uint16_t addr, uint8_t value)
{
    uint16_t index = (uint16_t)(addr - SHADOW_BASE);

    if (index < SHADOW_SIZE) {
        shadow[index] = value;
        written[index] = 1;
    }
    hal_poke(addr, value);
}

uint8_t hal_last_written(uint16_t addr, uint8_t *value)
{
    uint16_t index = (uint16_t)(addr - SHADOW_BASE);

    if (index >= SHADOW_SIZE || !written[index])
        return 0;
    *value = shadow[index];
    return 1;
}
