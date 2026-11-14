/* Parts of the RP2350 hardware layer that only this target uses. */
#ifndef HAL_RP2350_H
#define HAL_RP2350_H

#include <stdbool.h>
#include <stdint.h>

void bus_init(void);
void bus_flush(void);

/* Writes without the EEPROM and NOVRAM protection; for the bus test only. */
void bus_write(uint16_t addr, uint8_t value);

/*
 * While bringing up a new board, writes to the EEPROM (F000-F7FF) and the NOVRAM (EC00-EC3F)
 * are refused and logged. Build with HAL_PROTECT_EEPROM=0 to allow them from the start.
 */
void hal_allow_writes(bool eeprom, bool novram);
bool hal_eeprom_writable(void);
bool hal_novram_writable(void);
uint32_t hal_refused_writes(void);

#endif
