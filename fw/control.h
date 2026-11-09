/* The write side of REG_STATUS (EE00), which several functions share. */
#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

/* Changes the bits in mask to value and writes the register. */
void control_set(uint8_t mask, uint8_t value);
uint8_t control_get(void);

#endif
