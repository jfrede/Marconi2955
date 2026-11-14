/* Hardware access through the PIO bus master in place of the 8085. */
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "../hal.h"
#include "bus.pio.h"
#include "hal_rp2350.h"
#include "pins.h"

#define BUS_PIO pio0
#define BUS_SM 0u
#define READ_FLAG (1u << 16)

#define EEPROM_FIRST 0xF000u
#define EEPROM_LAST 0xF7FFu
#define NOVRAM_FIRST 0xEC00u
#define NOVRAM_LAST 0xEC3Fu

#ifndef HAL_PROTECT_EEPROM
#define HAL_PROTECT_EEPROM 1
#endif

static bool eeprom_writes_allowed = !HAL_PROTECT_EEPROM;
static bool novram_writes_allowed = !HAL_PROTECT_EEPROM;
static uint32_t refused_writes;

void bus_init(void)
{
    uint offset = pio_add_program(BUS_PIO, &bus8085_program);
    bus8085_program_init(BUS_PIO, BUS_SM, offset, PIN_AD0, PIN_ALE);
}

uint8_t hal_read(uint16_t addr)
{
    pio_sm_put_blocking(BUS_PIO, BUS_SM, (uint32_t)addr | READ_FLAG);
    return (uint8_t)pio_sm_get_blocking(BUS_PIO, BUS_SM);
}

static bool write_allowed(uint16_t addr)
{
    if (addr >= EEPROM_FIRST && addr <= EEPROM_LAST)
        return eeprom_writes_allowed;
    if (addr >= NOVRAM_FIRST && addr <= NOVRAM_LAST)
        return novram_writes_allowed;
    return true;
}

void bus_write(uint16_t addr, uint8_t value)
{
    pio_sm_put_blocking(BUS_PIO, BUS_SM, addr);
    pio_sm_put_blocking(BUS_PIO, BUS_SM, (uint32_t)value | (addr & 0xFF00u));
}

void hal_poke(uint16_t addr, uint8_t value)
{
    if (!write_allowed(addr)) {
        refused_writes++;
        printf("refused write %04X=%02X\n", addr, value);
        return;
    }
    bus_write(addr, value);
}

/* Waits until every queued cycle has run. */
void bus_flush(void)
{
    while (!pio_sm_is_tx_fifo_empty(BUS_PIO, BUS_SM))
        tight_loop_contents();
    busy_wait_us(5); /* the last cycle and its gap */
}

void hal_allow_writes(bool eeprom, bool novram)
{
    eeprom_writes_allowed = eeprom;
    novram_writes_allowed = novram;
}

bool hal_eeprom_writable(void) { return eeprom_writes_allowed; }
bool hal_novram_writable(void) { return novram_writes_allowed; }
uint32_t hal_refused_writes(void) { return refused_writes; }

void hal_delay_ms(uint16_t ms)
{
    bus_flush();
    sleep_ms(ms);
}

void hal_delay_us(uint16_t us)
{
    bus_flush();
    busy_wait_us(us);
}

uint32_t hal_millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}
