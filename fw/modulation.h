/* Modulation source and level control. */
#ifndef MODULATION_H
#define MODULATION_H

#include <stdint.h>

typedef struct {
    uint32_t freq;        /* modulation frequency in 0.1 Hz */
    uint32_t deviation;   /* FM deviation in 0.1 Hz */
    uint8_t am_percent;   /* AM depth in percent */
    uint8_t am;           /* non-zero for AM, zero for FM */
    uint8_t off;          /* non-zero when the modulation is switched off */
    uint8_t sat_on;       /* the sub-audible tone is mixed in, from the variable oscillator */
    uint32_t sat_freq;    /* 0.1 Hz */
    uint32_t sat_level;   /* 0.1 Hz of deviation, or percent AM */
} modulation_state;

void modulation_apply(const modulation_state *state);

/* Selects the AF path setting used while a transmitter is measured. */
void modulation_set_transmit(uint8_t transmit);

/* Low nibble of REG_AF_SWITCHING while transmitting, set by the AF generator level range. */
void modulation_set_transmit_nibble(uint8_t nibble);

#endif
