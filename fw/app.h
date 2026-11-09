/* Instrument application: settings, keyboard entry and screens. */
#ifndef APP_H
#define APP_H

#include <stdint.h>

#include "settings.h"

void app_init(void);
void app_redraw(void);

/* At power-on: keeps the settings of the last session for RECALL 00, then starts afresh. */
void app_power_on(void);

/* Restores the screen of the current mode, after HELP or the overload warning. */
void app_restore_screen(void);

/* Which input the overload warning most likely means. */
typedef enum { OVERLOAD_INPUT_N, OVERLOAD_INPUT_BNC, OVERLOAD_INPUT_EITHER } overload_input;
overload_input app_overload_input(void);

/* The tones screens drive the modulation source through these. */
void app_tone_output(uint32_t tenths_hz);   /* 0 is a silent pause */
void app_tone_output_end(void);
void app_format_tone_mod(char *out);
uint8_t app_two_tone(void);
void app_two_tone_toggle(void);
uint8_t app_sat_on(void);
uint8_t app_sat_is_am(void);
uint32_t app_sat_freq(void);
uint32_t app_sat_level(void);
void app_sat_set_freq(uint32_t tenths);
void app_sat_set_level(uint32_t value);
void app_sat_toggle(void);

/* The self test borrows the generator and the measuring circuits from the current mode. */
void app_selftest_begin(void);
void app_selftest_end(void);

/* Marks an external attenuator on the given socket, or clears the mark if there is none. */
void app_draw_external_attenuator(uint8_t col, uint8_t row, uint8_t bnc);

/* Runs the measurements and refreshes the readings; call it from the main loop. */
void app_poll(void);

/* Handles one key press; code = row * 8 + column + 1 as returned by keys_scan(). */
void app_key(uint8_t code);

/* Handles one step of the VARIABLE control; direction is +1 or -1. */
void app_knob(int8_t direction);

/* State queries for the GPIB commands that act only if something has to change. */
/* A key from the GPIB interface, which works in the remote state too. */
void app_command_key(uint8_t code);

/* The complete settings, for the SV command. */
void app_get_settings(instrument_settings *settings);
void app_set_settings(const instrument_settings *settings);

/* Call after the PD/EMF setting has changed. */
void app_level_type_changed(void);

uint8_t app_is_transmit(void);
uint8_t app_tx_manual(void);
uint8_t app_ac_coupled(void);
uint8_t app_filter(void);
uint8_t app_held(void);
uint8_t app_mod_off(void);
uint8_t app_sinad_mode(void);      /* 0 off, 1 SINAD, 2 S/N */
uint8_t app_distortion_on(void);
uint8_t app_input_select(void);    /* 0 BNC, 1 N type */

#endif
