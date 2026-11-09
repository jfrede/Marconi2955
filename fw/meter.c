#include "meter.h"

#include <math.h>

#include "hal.h"
#include "params.h"
#include "regs.h"

#define ADC_RMS 0xE802u
#define ADC_HORIZONTAL 0xE803u
#define STATUS_SETTLED 0x04u
#define DUPLEX_E3E_TX_KEEP 0xC6u
#define DUPLEX_E3E_TX_SET 0x10u
#define DUPLEX_E3E_RX_KEEP 0xC7u
#define DUPLEX_E3E_RX_SET 0x21u
#define DUPLEX_E3B_CLEAR 0x30u
#define ADC_AUX_CHANNEL 0xE803u
#define DB_ABSOLUTE_REFERENCE_VOLTS 1.0
#define DB_600_OHM_REFERENCE_VOLTS 0.7746

#define SWITCH_E3E_DEFAULT 0xAFu
#define SWITCH_E3D_DEFAULT 0x7Cu
#define SWITCH_E3E_AC 0x04u
#define SWITCH_E3E_PEAK 0x01u
#define SWITCH_E3E_TX_KEEP 0xCEu
#define SWITCH_E3E_TX_SET 0x10u
#define SWITCH_E3D_MODE_KEEP 0x3Fu
#define SWITCH_E3D_TX 0x80u
#define PATH_MASK 0x03u
#define PATH_VOLTS 0x00u
#define PATH_NOTCH 0x01u
#define RANGE_MASK_E3E 0x82u
#define RANGE_MASK_E3D 0x0Cu

#define RANGE_COUNT 10u
#define SCOPE_RANGE_OFFSET_ENTRIES 6u /* switch patterns used while a millivolt scope scale is shown */
#define RANGE_TOP 9u
#define SCOPE_SWITCH_OFFSET 10u
#define SCOPE_MV_RANGE_TOP 5u
#define SCOPE_V_RANGE_BOTTOM 3u
#define SCOPE_GAIN_COUNT 13u
#define SCOPE_GAIN_RANGE_OFFSET 3u /* gain index for voltmeter range 0 */
#define SCOPE_GAIN_RANGE_TOP 7u    /* higher voltmeter ranges keep this gain */
#define SCOPE_GAIN_NONE 0xFFu
#define SCOPE_GAIN_E3D_MASK 0x30u
#define SCOPE_GAIN_E3E_MASK 0x4Au
#define ADC_FULL_SCALE 255u
#define SINAD_OFFSET_DB 20.0
#define NOTCH_RATIO_MAX 10.0
#define DISTORTION_FACTOR 10.0  /* the notch path has 20 dB gain */
#define DISTORTION_MAX_PERCENT 100.0
#define DBV_FLOOR (-99.9)
#define NOISE_CONVERSIONS 3u     /* the first conversions after the switch still settle */

/* Volts per ADC count for each range. */
static const double volts_per_count[RANGE_COUNT] = {
    0.0005, 0.001, 0.0025, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5,
};

/* Range switch positions; bits 7 and 1 go to E03E, bits 3 and 2 to E03D. */
static const uint8_t range_switches[RANGE_COUNT + SCOPE_RANGE_OFFSET_ENTRIES] = {
    0x8E, 0x8A, 0x86, 0x8C, 0x88, 0x84, 0x0C, 0x08, 0x04, 0x00,
    0x8E, 0x8A, 0x86, 0x0E, 0x0A, 0x06,
};

/* Scope and trigger path sensitivity; bits 5-4 go to E03D, bits 6, 3 and 1 to E03E. */
static const uint8_t scope_gain_switches[SCOPE_GAIN_COUNT] = {
    0x7A, 0x6A, 0x5A, 0x3A, 0x2A, 0x1A, 0x38, 0x28, 0x18, 0x30, 0x20, 0x10, 0x00,
};

/* A reading below this moves one range down; full scale moves one range up. */
static const uint8_t range_down_threshold[RANGE_COUNT] = {
    0x00, 0x76, 0x5C, 0x76, 0x76, 0x5C, 0x76, 0x76, 0x5C, 0x76,
};

typedef enum { STATE_VOLTS_READ, STATE_NOTCH_READ, STATE_NOISE_READ, STATE_HORIZONTAL_READ } meter_state;

/* Two cascaded first-order filters, each with a time constant of four samples. */
typedef struct {
    double sum1, out1, sum2, out2;
    uint8_t primed;
} smoother;

static struct {
    meter_state state;
    uint8_t switching_done;
    uint8_t e3e, e3d, e3b;
    uint8_t volts_adc, volts_range;
    uint8_t notch_adc, notch_range;
    uint8_t conversion_range;   /* range of the conversion now running, without the notch step */
    uint8_t selected_range;     /* range just switched to, without the notch step */
    uint8_t noise_adc, noise_range, noise_conversions;
    measure_mode mode;
    uint8_t mod_off, mod_gated;
    void (*mod_gate)(uint8_t enable);
    uint8_t new_results;
    uint8_t gain_following;
    uint8_t range_bottom, range_top, switch_offset;
    uint8_t scope_active, gain_index;
    uint8_t db_relative;
    double db_reference;
    double volts, second_value;
    smoother second_filter;
} meter;

static uint8_t settled(void)
{
    return (hal_read(REG_STATUS) & STATUS_SETTLED) != 0;
}

/* Returns the previous conversion and starts the next one on the given input. */
static uint8_t read_and_start(uint16_t next_channel)
{
    uint8_t value = hal_read(ADC_RMS);
    hal_write(next_channel, 0);
    return value;
}

static uint8_t adjust_range(uint8_t adc, uint8_t range)
{
    if (adc < range_down_threshold[range])
        range = (uint8_t)(range - 1u);
    else if (adc == ADC_FULL_SCALE && range < RANGE_TOP)
        range = (uint8_t)(range + 1u);
    if (range > meter.range_top)
        return meter.range_top;
    return range < meter.range_bottom ? meter.range_bottom : range;
}

static void select_path(uint8_t path, uint8_t range)
{
    uint8_t switches = range_switches[range + meter.switch_offset];

    meter.e3e = (uint8_t)((meter.e3e & (uint8_t)~RANGE_MASK_E3E) | (switches & RANGE_MASK_E3E));
    meter.e3d = (uint8_t)((meter.e3d & (uint8_t)~(RANGE_MASK_E3D | PATH_MASK))
                          | (switches & RANGE_MASK_E3D) | path);
    hal_write(REG_RANGE_E3E, meter.e3e);
    hal_write(REG_RANGE_E3D, meter.e3d);
}

/* dB relative to the voltage at the time of the dB key, or to 1 V (0 dBm into 600 ohms with the accessory). */
static double current_db_reference(void)
{
    if (meter.db_relative && meter.db_reference > 0.0)
        return meter.db_reference;
    return param_get(PARAM_AF_600_OHM) ? DB_600_OHM_REFERENCE_VOLTS : DB_ABSOLUTE_REFERENCE_VOLTS;
}

static double smooth(smoother *f, double input)
{
    if (!f->primed) {
        f->sum1 = f->sum2 = input * 4.0;
        f->out1 = f->out2 = input;
        f->primed = 1;
        return input;
    }
    f->sum1 += input - f->out1;
    f->out1 = f->sum1 / 4.0;
    f->sum2 += f->out1 - f->out2;
    f->out2 = f->sum2 / 4.0;
    return f->out2;
}

static uint8_t second_measurement_active(void)
{
    return meter.mode != MEASURE_DBV && !meter.mod_off;
}

static void gate_modulation(uint8_t enable)
{
    meter.mod_gated = !enable;
    if (meter.mod_gate)
        meter.mod_gate(enable);
}

static double adc_volts(uint8_t adc, uint8_t range)
{
    return (adc ? adc : 1u) * volts_per_count[range];
}

static void update_results(void)
{
    meter.new_results = 1;
    meter.volts = meter.volts_adc * volts_per_count[meter.volts_range];

    if (!meter.scope_active) {
        uint8_t range = meter.volts_range > SCOPE_GAIN_RANGE_TOP ? SCOPE_GAIN_RANGE_TOP : meter.volts_range;

        if (meter.gain_following && (uint8_t)(range + SCOPE_GAIN_RANGE_OFFSET) != meter.gain_index)
            meter_set_scope_gain((uint8_t)(range + SCOPE_GAIN_RANGE_OFFSET));
    }

    switch (meter.mode) {
    case MEASURE_DBV:
        meter.second_value = smooth(&meter.second_filter,
                                    meter.volts > 0.0 ? 20.0 * log10(meter.volts / current_db_reference()) : DBV_FLOOR);
        break;
    case MEASURE_SINAD:
        if (!meter.mod_off && meter.volts > 0.0) {
            double ratio = adc_volts(meter.notch_adc, meter.notch_range) / meter.volts;
            if (ratio > NOTCH_RATIO_MAX)
                ratio = NOTCH_RATIO_MAX;
            meter.second_value = smooth(&meter.second_filter, SINAD_OFFSET_DB - 20.0 * log10(ratio));
        }
        break;
    case MEASURE_SN:
        if (!meter.mod_off && meter.volts > 0.0) {
            meter.second_value = smooth(&meter.second_filter,
                                        20.0 * log10(meter.volts / adc_volts(meter.noise_adc, meter.noise_range)));
        }
        break;
    case MEASURE_DISTORTION:
        if (!meter.mod_off && meter.volts > 0.0) {
            double percent = adc_volts(meter.notch_adc, meter.notch_range) / meter.volts * DISTORTION_FACTOR;
            meter.second_value = percent > DISTORTION_MAX_PERCENT ? DISTORTION_MAX_PERCENT : percent;
        }
        break;
    }
}

static void restart_measurement(void)
{
    if (meter.mod_gated)
        gate_modulation(1);
    meter.second_filter.primed = 0;
    meter.second_value = 0.0;
    meter.switching_done = 0;
    meter.conversion_range = meter.selected_range = meter.volts_range;
    meter.state = STATE_VOLTS_READ;
}

/* REG_AF_FILTER value for each af_filter position. */
static const uint8_t filter_bits[] = {0x04, 0x00, 0x08};


double meter_volts_per_count(uint8_t range)
{
    return volts_per_count[range];
}

uint8_t meter_adjust_range(uint8_t adc, uint8_t range, uint8_t bottom, uint8_t top)
{
    if (adc < range_down_threshold[range])
        range = (uint8_t)(range - 1u);
    else if (adc == ADC_FULL_SCALE && range < RANGE_TOP)
        range = (uint8_t)(range + 1u);
    if (range > top)
        return top;
    return range < bottom ? bottom : range;
}

void meter_set_filter(af_filter filter)
{
    meter.e3b = (uint8_t)((meter.e3b & (uint8_t)~METER_FILTER_MASK) | filter_bits[filter]);
    hal_write(REG_AF_FILTER, meter.e3b);
}

uint8_t meter_filter_register(void)
{
    return meter.e3b;
}

void meter_set_filter_extra(uint8_t mask, uint8_t value)
{
    meter.e3b = (uint8_t)((meter.e3b & (uint8_t)~mask) | (value & mask));
    hal_write(REG_AF_FILTER, meter.e3b);
}

/* Range switches without changing the measurement path. */
void meter_select_range(uint8_t range)
{
    uint8_t switches = range_switches[range];

    meter.e3e = (uint8_t)((meter.e3e & (uint8_t)~RANGE_MASK_E3E) | (switches & RANGE_MASK_E3E));
    meter.e3d = (uint8_t)((meter.e3d & (uint8_t)~RANGE_MASK_E3D) | (switches & RANGE_MASK_E3D));
    hal_write(REG_RANGE_E3E, meter.e3e);
    hal_write(REG_RANGE_E3D, meter.e3d);
}

void meter_select_path(uint8_t path)
{
    meter.e3d = (uint8_t)((meter.e3d & (uint8_t)~PATH_MASK) | path);
    hal_write(REG_RANGE_E3D, meter.e3d);
}

void meter_set_peak_hold(uint8_t on)
{
    meter.e3e = (uint8_t)((meter.e3e & (uint8_t)~SWITCH_E3E_PEAK) | (on ? SWITCH_E3E_PEAK : 0u));
    hal_write(REG_RANGE_E3E, meter.e3e);
}

void meter_set_scope_gain_following(uint8_t following)
{
    meter.gain_following = following;
}

void meter_duplex_measure_transmitter(uint8_t range)
{
    meter.e3e = (uint8_t)((meter.e3e & DUPLEX_E3E_TX_KEEP) | DUPLEX_E3E_TX_SET);
    hal_write(REG_RANGE_E3E, meter.e3e);
    meter_select_range(range);
}

void meter_duplex_measure_receiver(void)
{
    meter.e3e = (uint8_t)((meter.e3e & DUPLEX_E3E_RX_KEEP) | DUPLEX_E3E_RX_SET);
    hal_write(REG_RANGE_E3E, meter.e3e);
    meter.e3b &= (uint8_t)~DUPLEX_E3B_CLEAR;
    hal_write(REG_AF_FILTER, meter.e3b);
    meter.switching_done = 0;
    meter.state = STATE_HORIZONTAL_READ;
    select_path(PATH_VOLTS, meter.volts_range);
    hal_write(ADC_AUX_CHANNEL, 0);
}

void meter_prepare_transmit(uint8_t transmit)
{
    if (transmit) {
        meter.e3e = (uint8_t)((meter.e3e & SWITCH_E3E_TX_KEEP) | SWITCH_E3E_TX_SET);
        meter.e3d = (uint8_t)((meter.e3d & SWITCH_E3D_MODE_KEEP) | SWITCH_E3D_TX);
    } else {
        meter.e3e = SWITCH_E3E_DEFAULT;
        meter.e3d = SWITCH_E3D_DEFAULT;
        meter.e3b &= (uint8_t)~0x03u; /* the power range bits stay as the last measurement left them */
        meter_set_filter(AF_FILTER_BAND_PASS);
        restart_measurement();
    }
    hal_write(REG_RANGE_E3E, meter.e3e);
    hal_write(REG_RANGE_E3D, meter.e3d);
}

void meter_set_ac_coupled(uint8_t ac)
{
    meter.e3e = (uint8_t)(ac ? meter.e3e | SWITCH_E3E_AC : meter.e3e & (uint8_t)~SWITCH_E3E_AC);
    hal_write(REG_RANGE_E3E, meter.e3e);
}

void meter_set_scope_gain(uint8_t index)
{
    uint8_t switches = scope_gain_switches[index];

    meter.gain_index = index;
    meter.e3d = (uint8_t)((meter.e3d & (uint8_t)~SCOPE_GAIN_E3D_MASK) | (switches & SCOPE_GAIN_E3D_MASK));
    meter.e3e = (uint8_t)((meter.e3e & (uint8_t)~SCOPE_GAIN_E3E_MASK) | (switches & SCOPE_GAIN_E3E_MASK));
    hal_write(REG_RANGE_E3D, meter.e3d);
    hal_write(REG_RANGE_E3E, meter.e3e);
}

void meter_set_scope_limits(uint8_t millivolt_scale)
{
    meter.scope_active = 1;
    meter.range_bottom = millivolt_scale ? 0u : SCOPE_V_RANGE_BOTTOM;
    meter.range_top = millivolt_scale ? SCOPE_MV_RANGE_TOP : RANGE_TOP;
    meter.switch_offset = millivolt_scale ? SCOPE_SWITCH_OFFSET : 0u;
}

void meter_clear_scope_limits(void)
{
    meter.scope_active = 0;
    meter.gain_index = SCOPE_GAIN_NONE;
    meter.range_bottom = 0;
    meter.range_top = RANGE_TOP;
    meter.switch_offset = 0;
}

void meter_toggle_db_relative(void)
{
    meter.db_relative = !meter.db_relative;
    meter.db_reference = meter.volts;
    meter.second_filter.primed = 0;
}

uint8_t meter_db_relative(void)
{
    return meter.db_relative;
}

void meter_set_mode(measure_mode mode)
{
    meter.mode = mode;
    restart_measurement();
}

measure_mode meter_mode(void)
{
    return meter.mode;
}

void meter_set_mod_off(uint8_t off)
{
    meter.mod_off = off;
    restart_measurement();
}

void meter_set_mod_gate(void (*gate)(uint8_t enable))
{
    meter.mod_gate = gate;
}

void meter_init(void)
{
    meter.gain_following = 1;
    meter.state = STATE_HORIZONTAL_READ;
    meter.switching_done = 0;
    meter.e3e = SWITCH_E3E_DEFAULT;
    meter.e3d = SWITCH_E3D_DEFAULT;
    meter.volts_adc = meter.notch_adc = 0;
    meter.volts_range = meter.notch_range = meter.conversion_range = 0;
    meter.noise_adc = meter.noise_range = meter.noise_conversions = 0;
    meter.mode = MEASURE_SINAD;
    meter.mod_off = meter.mod_gated = 0;
    meter.db_relative = 0;
    meter.db_reference = DB_ABSOLUTE_REFERENCE_VOLTS;
    meter_clear_scope_limits();
    meter.volts = meter.second_value = 0.0;
    meter.second_filter.primed = 0;
    hal_write(REG_RANGE_E3E, meter.e3e);
    hal_write(REG_RANGE_E3D, meter.e3d);
    meter_set_filter(AF_FILTER_BAND_PASS);
    hal_write(ADC_RMS, 0);
}

void meter_debug(uint8_t values[4])
{
    values[0] = meter.volts_adc;
    values[1] = meter.volts_range;
    values[2] = meter.notch_adc;
    values[3] = meter.notch_range;
}

double meter_volts(void) { return meter.volts; }
double meter_second_value(void) { return meter.second_value; }
uint8_t meter_second_valid(void) { return meter.mode == MEASURE_DBV || !meter.mod_off; }

uint8_t meter_poll(void)
{
    meter.new_results = 0;
    switch (meter.state) {
    case STATE_VOLTS_READ:
        if (!second_measurement_active()) {
            /* Without the notch step: switch first, then read the conversion started before. */
            if (!meter.switching_done) {
                meter.selected_range = adjust_range(meter.volts_adc, meter.volts_range);
                select_path(PATH_VOLTS, meter.selected_range);
                meter.switching_done = 1;
                break;
            }
            if (!settled())
                break;
            meter.volts_adc = read_and_start(ADC_RMS);
            meter.volts_range = meter.conversion_range; /* the reading came from this range */
            meter.conversion_range = meter.selected_range;
            update_results();
            meter.switching_done = 0;
            break;
        }
        if (meter.mode == MEASURE_SN) {
            /* The modulation goes off so that the same path measures the noise alone. */
            if (!meter.switching_done) {
                gate_modulation(0);
                meter.noise_range = adjust_range(meter.noise_adc, meter.noise_range);
                select_path(PATH_VOLTS, meter.noise_range);
                meter.switching_done = 1;
                break;
            }
            if (!settled())
                break;
            meter.volts_adc = read_and_start(ADC_RMS);
            meter.noise_conversions = 0;
            meter.switching_done = 0;
            meter.state = STATE_NOISE_READ;
            break;
        }
        if (!meter.switching_done) {
            meter.notch_range = adjust_range(meter.notch_adc, meter.notch_range);
            if (meter.notch_range < meter.volts_range)
                meter.notch_range = meter.volts_range; /* never more sensitive than the volts path */
            select_path(PATH_NOTCH, meter.notch_range);
            meter.switching_done = 1;
            break;
        }
        if (!settled())
            break;
        meter.volts_adc = read_and_start(ADC_RMS); /* this conversion ran on the volts path */
        meter.switching_done = 0;
        meter.state = STATE_NOTCH_READ;
        break;
    case STATE_NOTCH_READ:
        meter.notch_adc = read_and_start(ADC_HORIZONTAL);
        update_results();
        meter.state = STATE_HORIZONTAL_READ;
        break;
    case STATE_NOISE_READ:
        meter.noise_adc = read_and_start(ADC_RMS);
        if (++meter.noise_conversions < NOISE_CONVERSIONS)
            break;
        update_results();
        gate_modulation(1);
        meter.state = STATE_HORIZONTAL_READ;
        break;
    case STATE_HORIZONTAL_READ:
        if (!meter.switching_done) {
            meter.volts_range = adjust_range(meter.volts_adc, meter.volts_range);
            select_path(PATH_VOLTS, meter.volts_range);
            meter.switching_done = 1;
            break;
        }
        if (!settled())
            break;
        (void)read_and_start(ADC_RMS);
        meter.switching_done = 0;
        meter.state = STATE_VOLTS_READ;
        break;
    }
    return meter.new_results;
}
