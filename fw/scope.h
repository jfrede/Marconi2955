/* Oscilloscope screen: graticule, timebase and vertical sensitivity. */
#ifndef SCOPE_H
#define SCOPE_H

#include <stdint.h>

void scope_init(void);

/* What the vertical axis of the scope displays. */
typedef enum { SCOPE_RX, SCOPE_AM, SCOPE_FM, SCOPE_PM } scope_kind;

/* Draws the oscilloscope screen and applies its settings. */
void scope_show(scope_kind kind, uint8_t ac_coupled);

/* direction: +1 for the next higher value in the table, -1 for the next lower. */
void scope_step_vertical(int8_t direction);
void scope_step_timebase(int8_t direction);

void scope_set_single_sweep(uint8_t single);

/* Settings kept with the stored instrument settings. */
void scope_get_settings(uint8_t *timebase, uint8_t vertical[SCOPE_PM + 1], uint8_t *single);
void scope_set_settings(uint8_t timebase, const uint8_t vertical[SCOPE_PM + 1], uint8_t single);

#endif
