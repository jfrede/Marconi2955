/* Audio frequency counter: reciprocal period measurement with the Am9513. */
#ifndef AFCOUNTER_H
#define AFCOUNTER_H

#include <stdint.h>

void afcounter_init(void);

/* Call it regularly from the main loop. Returns 1 when a new reading is available. */
uint8_t afcounter_poll(void);

/* Last reading in Hz; 0 when no signal is present. */
double afcounter_hz(void);

/* One period at a time, without averaging, for short tones. Returns 0 if nothing is new;
 * hz is 0 when no edges arrive. */
uint8_t afcounter_sample(double *hz);

#endif
