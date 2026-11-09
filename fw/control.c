#include "control.h"

#include "hal.h"
#include "regs.h"

static uint8_t shadow = 0x3D;

void control_set(uint8_t mask, uint8_t value)
{
    shadow = (uint8_t)((shadow & (uint8_t)~mask) | (value & mask));
    hal_write(REG_STATUS, shadow);
}

uint8_t control_get(void)
{
    return shadow;
}
