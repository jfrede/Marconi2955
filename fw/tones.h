/* The TONES menu: sub-audible tone, two tones, sending and receiving selective calling tones. */
#ifndef TONES_H
#define TONES_H

#include <stdint.h>

void tones_init(void);

/* Opens the menu for the transmitter test (decoding, two tones) or the receiver test. */
void tones_open(uint8_t transmit);
void tones_open_two_tone(void);
uint8_t tones_active(void);
void tones_key(uint8_t code);

/* Runs the tone sequencer and, while its screen is open, the decoder. Call it from the main loop. */
void tones_poll(void);

/* Non-zero while tones repeat on their own, so the mode screen can show that. */
uint8_t tones_continuous(void);

/* Draws the open screen again, after it has been overwritten. */
void tones_redraw(void);

#endif
