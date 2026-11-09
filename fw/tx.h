/* Transmitter test screen. */
#ifndef TX_H
#define TX_H

#include <stdint.h>

#include "settings.h"

void tx_init(void);

/* Switches the hardware to transmitter measurement and draws the screen. */
void tx_enter(int16_t rf_level);

/* Restores what tx_enter changed; the caller redraws its own screen. */
void tx_leave(void);

/* Redraws the transmitter test after another page has been on the screen. */
/* AUDIO TEST: the AF generator on the audio test screen. */
void tx_audio_begin(void);
void tx_audio_end(void);
void tx_audio_draw(void);
uint8_t tx_audio_key(uint8_t code);

void tx_format_af_freq_inc(char *out);
void tx_format_af_level_inc(char *out);

uint8_t tx_two_tone(void);
void tx_set_two_tone(uint8_t on);
void tx_resume(void);

/* After the screen was rebuilt during HOLD: shows the frozen readings again. */
void tx_draw_held_readings(void);

void tx_poll(uint8_t update_display);

/* Redraws the fields, for example after a prompt has covered them. */
void tx_refresh(void);

/* Handles one step of the VARIABLE control. */
void tx_knob(int8_t direction);

/* Returns 1 if the key was used. */
uint8_t tx_key(uint8_t code);

/* Makes the AF generator settings readable over GPIB while the receiver test is shown. */
void tx_publish_settings(void);

uint8_t tx_manual(void);
uint8_t tx_filter(void);
uint8_t tx_distortion_on(void);
uint8_t tx_input_select(void);

/* Copies the transmitter test settings to or from a stored settings record. */
void tx_get_settings(instrument_settings *s);
void tx_set_settings(const instrument_settings *s);

/* The self test measures with the DUPLEX cycle, without its screen. */
void tx_selftest_begin(void);
void tx_selftest_end(void);
void tx_selftest_demod(uint8_t kind);
double tx_selftest_power_mw(void);
double tx_selftest_mod_level(void);

/* DUPLEX: the transmitter half of the screen and the shared measurement cycle. */
void tx_duplex_begin(uint8_t mod_kind);
void tx_duplex_end(void);
void tx_duplex_set_one_port(uint8_t one_port);
/* Returns 1 when new receiver readings are ready to be drawn. */
uint8_t tx_duplex_poll(uint8_t update_display);
void tx_duplex_draw_held_readings(void);
void tx_duplex_draw(uint8_t one_port, uint8_t filter, uint8_t af_selected, uint8_t freq_selected, uint8_t field,
                    const char *entry, uint8_t entry_len);
uint8_t tx_duplex_set_frequency(const char *entry, uint8_t entry_len, int32_t scale, const char *unit);
uint8_t tx_duplex_set_af_level(const char *entry, uint8_t entry_len, uint8_t unit_key);
void tx_duplex_set_modulation(uint8_t am);
void tx_duplex_set_af_level_value(int32_t tenths_mv);
int32_t tx_duplex_af_level(void);
uint8_t tx_duplex_knob_af_level(int8_t direction);
double tx_measured_hz(void);

#endif
