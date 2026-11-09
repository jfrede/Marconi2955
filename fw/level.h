/* RF output level control. */
#ifndef LEVEL_H
#define LEVEL_H

#include <stdint.h>

/*
 * Sets the attenuators and the fine level DAC from the EEPROM calibration.
 * tenths_dbm is the level in 0.1 dBm; bnc is non-zero for the BNC socket.
 */
/* The loss in 0.1 dB between the instrument's own circuits and the connected equipment:
 * the internal pad of the socket plus an external attenuator, if one is set.
 * Both the RF generator level and the power meter use this value. */
int16_t level_socket_loss(uint8_t bnc);

/* An external attenuator is 3 to 40 dB, or 0 for none. It stays in the EEPROM. */
#define LEVEL_EXTERNAL_MIN 30
#define LEVEL_EXTERNAL_MAX 400
int16_t level_external_attenuator(uint8_t bnc);
uint8_t level_external_valid(int16_t tenths_db);
void level_store_external_attenuator(uint8_t bnc, int16_t tenths_db);
void level_load_external_attenuators(void);

/* For settings that do not leave through an external attenuator. */
void level_set_without_external(int16_t tenths_dbm, uint8_t bnc);

void level_set(int16_t tenths_dbm, uint8_t bnc);

/* The self test: attenuation in 0.1 dB, with the output routed to the measuring circuits. */
void level_set_self_test(int16_t attenuation);

/* DUPLEX, one port: the generator output through the path to the shared N socket. */
void level_set_one_port(int16_t tenths_dbm);

/* Position within the current 10 dB step, in 0.1 dB, for the AM tracking table. */
uint8_t level_step_position(void);

/* Sets the low nibble of the shared level DAC register (modulation output mode). */
void level_set_output_mode(uint8_t mode);

/* While the transmitter power is measured its range drives bit 3 of the attenuator register. */
void level_set_power_range_bit(uint8_t on);

#endif
