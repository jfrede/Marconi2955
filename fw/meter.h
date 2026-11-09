/* AF voltmeter and SINAD measurement. */
#ifndef METER_H
#define METER_H

#include <stdint.h>

void meter_init(void);

/* Advances the measurement; call it regularly from the main loop.
 * Returns 1 when a new set of results is available. */
uint8_t meter_poll(void);

/* AF filter positions. */
typedef enum { AF_FILTER_BAND_PASS, AF_FILTER_LOW_PASS_50K, AF_FILTER_LOW_PASS_300 } af_filter;

#define METER_FILTER_MASK 0x0Cu /* the filter selection bits of the filter register */

void meter_set_filter(af_filter filter);

/*
 * Sensitivity of the scope and trigger path, whose switches sit beside the voltmeter's.
 * index 0 is the most sensitive setting. While the bar charts are shown the path follows
 * the voltmeter range, because the AF counter takes its input from the trigger.
 */
void meter_set_scope_gain(uint8_t index);

/* While the scope is shown its gain limits the ranges the voltmeter may use. */
void meter_set_scope_limits(uint8_t millivolt_scale);
void meter_clear_scope_limits(void);

/* Hardware access for the transmitter measurement cycle. */
double meter_volts_per_count(uint8_t range);
uint8_t meter_adjust_range(uint8_t adc, uint8_t range, uint8_t bottom, uint8_t top);
void meter_select_range(uint8_t range);
void meter_select_path(uint8_t path);
void meter_set_peak_hold(uint8_t on);
void meter_prepare_transmit(uint8_t transmit);
void meter_set_filter_extra(uint8_t mask, uint8_t value);

/* The latest value written to the filter register. */
uint8_t meter_filter_register(void);
uint8_t meter_range_register(void);

/* In dBV mode: toggles the reading relative to the present level. */
void meter_toggle_db_relative(void);
uint8_t meter_db_relative(void);
void meter_set_ac_coupled(uint8_t ac);

/* What the second reading shows. */
typedef enum { MEASURE_SINAD, MEASURE_SN, MEASURE_DISTORTION, MEASURE_DBV } measure_mode;

void meter_set_mode(measure_mode mode);
measure_mode meter_mode(void);

/* Tells the meter that the user switched the modulation off. */
void meter_set_mod_off(uint8_t off);

/* Called during S/N measurements to switch the modulation off and back on. */
void meter_set_mod_gate(void (*gate)(uint8_t enable));

double meter_volts(void);

/* Second reading: dB for SINAD, S/N and dBV, percent for distortion. */
double meter_second_value(void);
uint8_t meter_second_valid(void);

/* Test support: volts ADC, volts range, notch ADC, notch range. */
void meter_debug(uint8_t values[4]);

/* DUPLEX shares the meter between the two halves of the screen. */
/* The scope is not used in DUPLEX, so the trigger gain stays where the transmitter path needs it. */
void meter_set_scope_gain_following(uint8_t following);
void meter_duplex_measure_transmitter(uint8_t range);
void meter_duplex_measure_receiver(void);

#endif
