/* Hardware access layer. Each target (host test, 8085, RP2040) provides these functions. */
#ifndef HAL_H
#define HAL_H

#include <stdint.h>

#ifdef HAL_DIRECT_MEMORY_MAP
/* Targets with memory-mapped registers avoid a function call per access. */
#define hal_read(addr) (*(volatile uint8_t *)(addr))
#define hal_write(addr, value) (*(volatile uint8_t *)(addr) = (value))
#else
uint8_t hal_read(uint16_t addr);
void hal_write(uint16_t addr, uint8_t value); /* in hal_common.c; every target provides hal_poke */
void hal_poke(uint16_t addr, uint8_t value);

/* The last value written to a register that cannot be read back; returns 0 if there was none. */
uint8_t hal_last_written(uint16_t addr, uint8_t *value);
#endif

/* Waits until the screen memory may be written. */
void hal_screen_wait(void);
void hal_delay_ms(uint16_t ms);
void hal_delay_us(uint16_t us);

/* Milliseconds since start; wraps around. Times the calling tones. */
uint32_t hal_millis(void);

#endif
