/* RF synthesizer control and modulation tracking. */
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

/* Programs the synthesizer for an RF frequency given in 10 Hz units. */
void synth_set_frequency(int32_t freq_10hz);

/* Sets bit 7 of the synthesizer data register, which drives an attenuator pad. */
void synth_set_aux(uint8_t high);

/* Selects AM or FM tracking and sets the modulation bits of the routing register. */
void synth_set_modulation(uint8_t am, uint8_t routing_high_bit);

/* Recalculates the tracking DAC after a level or modulation change. */
void synth_update_tracking(void);

/* Routing while the instrument measures a transmitter. */
void synth_set_transmit(uint8_t transmit);

/* Range bits for the AF generator level. */
void synth_set_af_range(uint8_t bits);

#endif
