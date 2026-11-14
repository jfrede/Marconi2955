/* Hardware access for the original 8085 board: all registers are memory mapped.
 * hal_screen_wait is in ../hal_screen.c. */
#include "../hal.h"

#ifndef HAL_DIRECT_MEMORY_MAP
uint8_t hal_read(uint16_t addr)
{
    return *(volatile uint8_t *)addr;
}

void hal_poke(uint16_t addr, uint8_t value)
{
    *(volatile uint8_t *)addr = value;
}
#endif

uint32_t hal_millis(void)
{
    return 0; /* size measurements only: no time base on this target */
}

void hal_delay_us(uint16_t us)
{
    /* Rough; to be measured on real hardware. */
    for (volatile uint16_t i = 0; i < us / 16u; i++)
        ;
}

void hal_delay_ms(uint16_t ms)
{
    /* About 5 ms per thousand iterations at the 5 MHz CPU clock; refine on real hardware. */
    for (volatile uint16_t i = 0; i < ms; i++)
        for (volatile uint16_t j = 0; j < 200u; j++)
            ;
}
