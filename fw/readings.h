/* Latest text of every reading and setting, for the GPIB RDn command. */
#ifndef READINGS_H
#define READINGS_H

#include <stdint.h>

/* Index n of RDn, and two entries that have no RD number. */
#define READING_COUNT 42u
#define READING_RF_COUNTER 1u
#define READING_POWER 2u
#define READING_MOD_FREQ 3u
#define READING_MOD_LEVEL 4u
#define READING_AF_FREQ 5u
#define READING_AF_LEVEL 6u
#define READING_RX_SECOND 7u
#define READING_TX_DISTORTION 8u
#define READING_PEAK_POS 9u
#define READING_PEAK_NEG 10u
#define READING_RECEIVED_TONE_FIRST 14u /* 12 readings: the tones the decoder received */
#define READING_RECEIVED_TONE_LAST 25u
#define READING_TONE_STANDARD 26u
#define READING_GEN_FREQ 27u
#define READING_GEN_LEVEL 28u
#define READING_AF_GEN_FREQ 29u
#define READING_AF_GEN_LEVEL 30u
#define READING_MOD_SET_FREQ 31u
#define READING_MOD_SET_LEVEL 32u
#define READING_RF_FREQ_INC 33u
#define READING_RF_LEVEL_INC 34u
#define READING_AF_FREQ_INC 35u
#define READING_AF_LEVEL_INC 36u
#define READING_MOD_FREQ_INC 37u
#define READING_MOD_LEVEL_INC 38u
#define READING_FILTER 40u
#define READING_RESERVED 41u

void readings_set(uint8_t index, const char *text);
const char *readings_get(uint8_t index);
void readings_clear(void);

#endif
