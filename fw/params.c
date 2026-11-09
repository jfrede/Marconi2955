#include "params.h"

#include "hal.h"

#define EEPROM_WRITE_MS 10u

uint8_t param_get(uint16_t address)
{
    return hal_read(address);
}

void param_set(uint16_t address, uint8_t value)
{
    hal_write(address, value);
    hal_delay_ms(EEPROM_WRITE_MS);
}

void param_toggle(uint16_t address)
{
    param_set(address, param_get(address) ? 0u : 1u);
}
