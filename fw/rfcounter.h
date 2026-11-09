/* RF frequency counter. */
#ifndef RFCOUNTER_H
#define RFCOUNTER_H

#include <stdint.h>

/* Programs the Am9513 timer chip; the AF counter and timebase depend on this as well. */
void rfcounter_init(void);

/* Returns 1 and stores the frequency in Hz when a new measurement is complete. */
uint8_t rfcounter_poll(double *hz);

/* Changes bits of REG_RF_CONTROL (E006), which other functions share. */
void rfcounter_set_control(uint8_t mask, uint8_t value);

#endif
