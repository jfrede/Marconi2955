#include "tx.h"

#include <math.h>
#include <string.h>

#include "afcounter.h"
#include "app.h"
#include "bargraph.h"
#include "format.h"
#include "gpib.h"
#include "hal.h"
#include "keys.h"
#include "params.h"
#include "level.h"
#include "meter.h"
#include "modulation.h"
#include "readings.h"
#include "regs.h"
#include "rfcounter.h"
#include "scope.h"
#include "screen.h"
#include "synth.h"

#define ENTRY_SIZE 13
#define UNIT_SIZE 4

#define VALUE_COL 11
#define VALUE_WIDTH 13
#define OFFSET_LABEL_COL 25
#define OFFSET_VALUE_COL 32
#define OFFSET_ROW 3
#define OFFSET_VALUE_WIDTH 8
#define FILTER_LABEL_COL 21
#define FILTER_VALUE_COL 28
#define FILTER_VALUE_WIDTH 10
#define FILTER_ROW 10
#define AF_LABEL_COL 3
#define AF_FREQ_ROW 12
#define AF_LEVEL_ROW 14
#define AF_FREQ_WIDTH 9
#define AF_INC_COL 20
#define AF_INC_FREQ_COL 24
#define AF_INC_LEVEL_COL 33
#define AF_INC_FREQ_WIDTH 9
#define AF_INC_LEVEL_WIDTH 7
#define AF_LEVEL_WIDTH 12

/* Bar charts. */
#define POWER_BAR_COL 4
#define POWER_SCALE_COL 1
#define AUDIO_RF_PARKED (-1200) /* the generator is kept low while the audio path is in use */
#define AUDIO_FREQ_ROW 3u
#define AUDIO_LEVEL_ROW 5u
#define AUDIO_FREQ_LABEL_COL 7u
#define AUDIO_LEVEL_LABEL_COL 6u
#define AUDIO_COLON_COL 11u
#define AUDIO_VALUE_COL 13u
#define AUDIO_VALUE_WIDTH 10u
#define AUDIO_INC_COL 25u
#define AUDIO_INC_VALUE_COL 29u
#define AUDIO_INC_WIDTH 9u
#define AUDIO_ACCESSORY_COL 20u
#define AUDIO_TWO_TONE_COL 9u
#define AUDIO_TWO_TONE_ROW 7u
#define TWO_TONE_COL 5u
#define TWO_TONE_ROW 14u
#define EXT_MARK_COL 0u
#define EXT_MARK_ROW 4u
#define POWER_UNIT_COL 6
#define POWER_TITLE_COL 2
#define MOD_BAR_COL 16
#define MOD_NEG_BAR_COL 18
#define MOD_SCALE_COL 13
#define MOD_UNIT_COL 19
#define MOD_TITLE_COL 16
#define MOD_TITLE_PHASE "*M" /* the character set has a phi at the position of the asterisk */
#define DIST_BAR_COL 31
#define DIST_SCALE_COL 28
#define DIST_UNIT_COL 33
#define DIST_TITLE_COL 29
#define DIST_AREA_FIRST_COL 28u
#define DIST_AREA_LAST_COL 35u
#define DIST_LABEL_COL 4
#define DIST_LABEL_ROW 10
#define DIST_VALUE_WIDTH 9
#define BAR_TITLE_ROW 17
#define BAR_SIGN_ROW 23
#define BAR_ZERO_ROW 25
#define BAR_PLUS_COL 14
#define BAR_MINUS_COL 20
#define MOD_PEAK_LEFT_COL 12u
#define MOD_PEAK_LEFT_LAST_COL 15u
#define MOD_PEAK_RIGHT_COL 19
#define MOD_PEAK_WIDTH 5

/* The box around the AUTO TUNE soft key. */
#define AUTO_BOX_COL 29u
#define AUTO_BOX_WIDTH 10u
#define BOX_TOP_LEFT 0x3Cu
#define BOX_TOP_RIGHT 0x5Bu
#define BOX_BOTTOM_LEFT 0x3Eu
#define BOX_BOTTOM_RIGHT 0x5Cu
#define BOX_HORIZONTAL 0x40u
#define BOX_LEFT 0x3Fu
#define BOX_RIGHT 0x5Du

/* Frequency limits for manual tuning and automatic search. */
#define TUNE_MIN_HZ 1390000.0
#define TUNE_MAX_HZ 1200000000.0
#define RETUNE_HZ 5000.0
#define RETUNE_READINGS 3u
#define STARTUP_TUNE_HZ 300000000.0

/* Receiver tuning, from the local oscillator and IF plan. */
#define TUNE_OFFSET_HZ 110000.0
#define TUNE_STEP_HIGH_HZ 13800000.0
#define TUNE_STEP_LOW_HZ 3100000.0
#define TUNE_REFERENCE_HZ 10000000.0
#define TUNE_SCALE 0.00376
#define TUNE_LOW_LIMIT 5640.0
#define BAND_HIGH_FLAG 0x80u
#define RF_CONTROL_STROBE 0x01u
#define RF_CONTROL_RUN 0x10u
#define RF_CONTROL_LOW_BAND 0x02u
#define RF_CONTROL_POWER_RANGE 0x08u

/* Measurement. */
#define ADC_PEAK_POS 0xE800u
#define ADC_PEAK_NEG 0xE801u
#define ADC_POWER 0xE802u
#define ADC_AUX 0xE803u
#define STATUS_SETTLED 0x04u
#define ONE_PORT_POWER_FACTOR 4.0 /* the shared socket path halves the power reading twice */
#define ATTENUATOR_FACTOR 10
#define VOLTS_AT_0DBM_600_OHM 0.7746
#define AF_ACCESSORY_COL 9u
#define PATH_PEAK 0x00u
#define PATH_POWER 0x02u
#define PATH_NOTCH 0x01u
#define DIST_NOTCH_PASSES 3u
#define DIST_FACTOR 10.0
#define DIST_MAX_PERCENT 100.0
#define POWER_VOLTS_FACTOR 6.052
#define MW_PER_VOLT_SQUARED 20.0  /* power in mW into 50 ohms */
#define POWER_RANGE_BOTTOM 1u
#define POWER_RANGE_DEFAULT 6u
#define POWER_RANGE_TOP_N 8u
#define POWER_RANGE_TOP_BNC 7u
#define POWER_RANGE_LOW 4u       /* calibration tables have an extra set below this range */
#define POWER_RANGE_EXTRA 7u     /* this range has an additional correction */
#define MOD_RANGE_TOP 9u
#define MOD_RANGE_BOTTOM_AM 9u
#define MOD_RANGE_BOTTOM_ANGLE 6u
#define SCOPE_GAIN_FM_AM 9u
#define SCOPE_GAIN_PHASE 10u
#define POWER_CALIBRATED_LOSS 200 /* 0.1 dB: the power calculation assumes the 20 dB pad of the N socket */

/* EEPROM calibration tables for the power meter. */
#define CAL_POWER_HIGH 0xF7AAu
#define CAL_POWER_LOW 0xF69Cu
#define CAL_POWER_EXTRA 0xF6BFu
#define CAL_LOW_RANGE_OFFSET_HIGH 0x2Bu
#define CAL_LOW_RANGE_OFFSET_LOW 0x0Eu

#define AF_FREQ_MIN 200L
#define AF_FREQ_MAX 200000L
#define AF_FREQ_FINE_LIMIT 32768L
#define AF_LEVEL_MAX_MV 2550L
#define AF_FREQ_FIXED 10000L
#define AF_TRANSMIT_NIBBLE_OFF 4u
#define AF_LEVEL_DEFAULT_TENTH_MV 1000L

typedef enum { MOD_AM = 1, MOD_FM = 2, MOD_PM = 3 } mod_kind;
typedef enum { STEP_PEAK_POS, STEP_PEAK_NEG, STEP_DIST, STEP_POWER, STEP_AUX } tx_step;
typedef enum { GROUP_NONE, GROUP_RF, GROUP_AF } tx_group;
typedef enum { FIELD_NONE, FIELD_FREQ, FIELD_LEVEL } tx_field;

typedef struct {
    double sum1, out1, sum2, out2;
    uint8_t primed;
} smoother;

static struct {
    tx_step step;
    uint8_t switching_done;
    uint8_t peak_pos, peak_neg, peak_range, pending_peak_range;
    uint8_t power_adc, power_range, pending_power_range;
    uint8_t aux;
    double power_mw;
    double mod_level, mod_peak_pos, mod_peak_neg;
    smoother mod_filter;
    uint8_t new_readings;

    mod_kind mod;
    uint8_t sel;
    uint8_t filter;

    uint8_t manual, two_tone, audio, selftest;
    double tuned_hz;
    uint8_t band;
    uint8_t retune_left;
    char manual_text[ENTRY_SIZE + UNIT_SIZE];
    double measured_hz;
    uint8_t have_measurement;

    uint8_t scope_shown;
    int16_t rf_level;
    uint8_t manual_recalled, recall_scope;
    uint8_t duplex_one_port;
    tx_group recall_group;
    tx_field recall_field;

    uint8_t dist_on, dist_passes;
    uint8_t dist_volts_adc, dist_volts_range, dist_pending_volts;
    uint8_t dist_notch_adc, dist_notch_range, dist_pending_notch;
    double dist_percent;

    int32_t af_freq;      /* 0.1 Hz */
    int32_t af_level;     /* 0.1 mV */
    int32_t af_freq_inc, af_level_inc;
    char af_freq_inc_text[ENTRY_SIZE + UNIT_SIZE];
    uint8_t entering_inc;
    uint8_t cursor_col, cursor_row, cursor_shown;

    tx_group group;
    tx_field field;
    char entry[ENTRY_SIZE];
    uint8_t entry_len;
} tx;

static bargraph power_bar, mod_bar, mod_neg_bar, dist_bar;

/* ---- helpers ------------------------------------------------------------------------- */

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

static uint8_t read_and_start(uint16_t channel, uint16_t next)
{
    uint8_t value = hal_read(channel);
    hal_write(next, 0);
    return value;
}

static void entry_clear(void)
{
    tx.entry_len = 0;
    tx.entry[0] = 0;
}

/* Linear interpolation in a calibration table of bytes, rounded. */
static uint8_t table_value(uint16_t table, double position)
{
    if (position < 0.0)
        position = 0.0;
    double whole = floor(position);
    double t = position - whole;
    uint16_t index = (uint16_t)whole;
    double first = hal_read((uint16_t)(table + index));
    double second = hal_read((uint16_t)(table + index + 1u));
    return (uint8_t)((1.0 - t) * first + t * second + 0.5);
}

static double high_band_correction(double mhz, uint8_t range)
{
    uint16_t table = CAL_POWER_HIGH;
    double start, step;

    if (mhz > 1020.0)
        mhz = 1020.0;
    if (mhz < 650.0) {
        start = 0.1;
        step = 50.0;
    } else if (mhz < 810.0) {
        start = 650.0;
        step = 20.0;
        table += 0x0Du;
    } else {
        start = 810.0;
        step = 10.0;
        table += 0x15u;
    }
    if (range < POWER_RANGE_LOW)
        table += CAL_LOW_RANGE_OFFSET_HIGH;
    return (table_value(table, (mhz - start) / step) + 100.0) / 200.0;
}

static double low_band_correction(double mhz, uint8_t band, uint8_t range)
{
    uint16_t table = CAL_POWER_LOW;
    double start, step;

    if (band == 1) {
        start = 1.3;
        step = 0.85;
    } else if (band == 2) {
        start = 2.9;
        step = 1.6;
        table += 3u;
    } else if (band == 3) {
        start = 6.0;
        step = 3.2;
        table += 6u;
    } else if (band >= 5) {
        start = 12.2;
        step = 45.3;
        table += 9u;
    } else {
        start = 9.1;
        step = 3.2;
        table += 8u;
    }
    if (range < POWER_RANGE_LOW)
        table += CAL_LOW_RANGE_OFFSET_LOW;
    return (table_value(table, (mhz - start) / step) + 250.0) / 400.0;
}

static double power_correction(uint8_t range)
{
    double mhz = tx.tuned_hz / 1000000.0;
    double factor = (tx.band & BAND_HIGH_FLAG) ? high_band_correction(mhz, range)
                                               : low_band_correction(mhz, tx.band, range);

    if (range == POWER_RANGE_EXTRA || tx.duplex_one_port) {
        double limited = mhz > 1020.0 ? 1020.0 : mhz;
        factor *= (table_value(CAL_POWER_EXTRA, (limited - 0.1) / 50.0) + 810.0) / 1000.0;
    }
    return factor;
}

/* ---- tuning -------------------------------------------------------------------------- */

static void strobe_tuning_word(uint8_t word)
{
    hal_write(REG_RF_SELECT, word);
    rfcounter_set_control(RF_CONTROL_STROBE | RF_CONTROL_RUN, RF_CONTROL_STROBE | RF_CONTROL_RUN);
    rfcounter_set_control(RF_CONTROL_STROBE, 0);
}

static void tune_receiver(double hz)
{
    double offset = hz + TUNE_OFFSET_HZ;
    double n = floor(offset / TUNE_STEP_HIGH_HZ + 1.0);
    double value = (offset - n * TUNE_REFERENCE_HZ) * TUNE_SCALE / n;
    uint8_t band = (uint8_t)((uint8_t)n | BAND_HIGH_FLAG);

    if (value < TUNE_LOW_LIMIT) {
        rfcounter_set_control(RF_CONTROL_LOW_BAND, RF_CONTROL_LOW_BAND);
        n = floor(offset / TUNE_STEP_LOW_HZ + 1.0);
        band = (uint8_t)n;
        value = offset * TUNE_SCALE / n;
    } else {
        rfcounter_set_control(RF_CONTROL_LOW_BAND, 0);
    }

    uint16_t word = (uint16_t)(value + 0.5);
    tx.band = band;
    tx.tuned_hz = hz;
    for (uint8_t nibble = 0; nibble < 4; nibble++)
        strobe_tuning_word((uint8_t)(((word >> (nibble * 4u)) & 0x0Fu) | (nibble << 4)));
}

/* ---- measurement --------------------------------------------------------------------- */

static uint8_t settled(void)
{
    return (hal_read(REG_STATUS) & STATUS_SETTLED) != 0;
}

static void apply_power_range(uint8_t range)
{
    static const uint8_t power_range_switches[8] = {0x00, 0x8C, 0x4C, 0x0C, 0x84, 0x44, 0x04, 0x00};
    uint8_t switches = power_range_switches[range > 7 ? 7 : range];

    meter_set_filter_extra(0xC0u, switches);
    rfcounter_set_control(RF_CONTROL_POWER_RANGE, switches);
    level_set_power_range_bit((switches & 0x04u) != 0);
    meter_select_path(PATH_POWER);
}

static uint8_t power_range_top(void)
{
    return tx.sel == 1 ? POWER_RANGE_TOP_BNC : POWER_RANGE_TOP_N;
}

static void update_modulation(void)
{
    double factor = meter_volts_per_count(tx.peak_range);
    double scale = tx.mod == MOD_FM ? 0.2 : (tx.mod == MOD_PM ? 0.1 : 1.0);
    double average = (tx.peak_pos + tx.peak_neg) / 2.0;

    tx.mod_level = smooth(&tx.mod_filter, average * factor * scale);
    tx.mod_peak_pos = tx.peak_pos * factor * scale;
    tx.mod_peak_neg = tx.peak_neg * factor * scale;
}

static void update_power(void)
{
    double volts = tx.power_adc * POWER_VOLTS_FACTOR * meter_volts_per_count(tx.power_range);

    volts *= power_correction(tx.power_range);
    tx.power_mw = volts * volts * MW_PER_VOLT_SQUARED;
    if (tx.selftest)
        tx.power_mw *= ONE_PORT_POWER_FACTOR;
    else if (tx.duplex_one_port)
        tx.power_mw *= ONE_PORT_POWER_FACTOR * pow(10.0, level_external_attenuator(0) / 100.0);
    else
        tx.power_mw *= pow(10.0, (level_socket_loss(tx.sel == 0) - POWER_CALIBRATED_LOSS) / 100.0);
    tx.new_readings = 1;
}

static uint8_t mod_range_bottom(void)
{
    return tx.mod == MOD_AM ? MOD_RANGE_BOTTOM_AM : MOD_RANGE_BOTTOM_ANGLE;
}

static void select_power_range(void)
{
    tx.pending_power_range = meter_adjust_range(tx.power_adc, tx.power_range, POWER_RANGE_BOTTOM, power_range_top());
    apply_power_range(tx.pending_power_range);
}

static void select_notch(void)
{
    tx.dist_pending_notch = meter_adjust_range(tx.dist_notch_adc, tx.dist_notch_range, tx.dist_volts_range, MOD_RANGE_TOP);
    meter_select_path(PATH_NOTCH);
    meter_select_range(tx.dist_pending_notch);
}

static void update_distortion(void)
{
    double volts = tx.dist_volts_adc * meter_volts_per_count(tx.dist_volts_range);
    double notch = tx.dist_notch_adc * meter_volts_per_count(tx.dist_notch_range);
    double percent = volts > 0.0 ? notch / volts * DIST_FACTOR : DIST_MAX_PERCENT;

    tx.dist_percent = percent > DIST_MAX_PERCENT ? DIST_MAX_PERCENT : percent;
}

static void measurement_step(void)
{
    switch (tx.step) {
    case STEP_PEAK_POS:
        tx.peak_pos = read_and_start(ADC_PEAK_POS, ADC_PEAK_NEG);
        tx.peak_range = tx.pending_peak_range;
        update_modulation();
        tx.switching_done = 0;
        tx.step = STEP_PEAK_NEG;
        break;
    case STEP_PEAK_NEG:
        if (!tx.switching_done) {
            meter_set_peak_hold(1);
            if (!tx.dist_on) {
                select_power_range();
            } else if (tx.dist_passes == 0) {
                tx.dist_pending_volts = meter_adjust_range(tx.dist_volts_adc, tx.dist_volts_range,
                                                           mod_range_bottom(), MOD_RANGE_TOP);
                meter_select_range(tx.dist_pending_volts);
            } else {
                select_notch();
            }
            tx.switching_done = 1;
            break;
        }
        if (!settled())
            break;
        tx.peak_neg = read_and_start(ADC_PEAK_NEG, ADC_POWER);
        tx.switching_done = 0;
        tx.step = tx.dist_on ? STEP_DIST : STEP_POWER;
        break;
    case STEP_DIST:
        if (tx.dist_passes == 0) {
            /* First the signal itself, then the same path through the notch. */
            if (!tx.switching_done) {
                select_notch();
                tx.switching_done = 1;
                break;
            }
            if (!settled())
                break;
            tx.dist_volts_adc = read_and_start(ADC_POWER, ADC_POWER);
            tx.dist_volts_range = tx.dist_pending_volts;
            update_distortion();
            tx.dist_passes = DIST_NOTCH_PASSES;
            tx.switching_done = 0;
            break;
        }
        if (!tx.switching_done) {
            select_power_range();
            tx.switching_done = 1;
            break;
        }
        if (!settled())
            break;
        tx.dist_notch_adc = read_and_start(ADC_POWER, ADC_POWER);
        tx.dist_notch_range = tx.dist_pending_notch;
        update_distortion();
        tx.dist_passes--;
        tx.switching_done = 0;
        tx.step = STEP_POWER;
        break;
    case STEP_POWER:
        meter_select_path(PATH_PEAK);
        meter_set_peak_hold(0);
        tx.pending_peak_range = meter_adjust_range(tx.peak_pos, tx.peak_range, mod_range_bottom(), MOD_RANGE_TOP);
        meter_select_range(tx.pending_peak_range);
        tx.power_adc = read_and_start(ADC_POWER, ADC_AUX);
        tx.power_range = tx.pending_power_range;
        update_power();
        tx.step = STEP_AUX;
        break;
    case STEP_AUX:
        if (!settled())
            break;
        tx.aux = read_and_start(ADC_AUX, ADC_PEAK_POS);
        tx.step = STEP_PEAK_POS;
        break;
    }
}

/* ---- display ------------------------------------------------------------------------- */

static void format_rf(char *out, double hz)
{
    uint32_t value = (uint32_t)(hz + 0.5);

    if (value >= 1000000UL)
        put_text(put_fixed(out, value, 6), "MHz");
    else if (value >= 1000UL)
        put_text(put_fixed(out, value, 3), "kHz");
    else
        put_text(put_fixed(out, value, 0), "Hz");
}

static const char *format_power(char *out, double mw)
{
    if (mw < 0.3 && tx.sel == 0) {
        put_text(put_fixed(out, round_scaled(mw, 1000.0), 0), "$W");
        return "$W ";
    }
    if (mw < 3.0) {
        put_text(put_fixed(out, round_scaled(mw, 100.0), 2), "mW");
        return "mW ";
    }
    if (mw < 30.0) {
        put_text(put_fixed(out, round_scaled(mw, 10.0), 1), "mW");
        return "mW ";
    }
    if (mw < 300.0) {
        put_text(put_fixed(out, round_scaled(mw, 1.0), 0), "mW");
        return "mW ";
    }
    if (mw < 3000.0) {
        put_text(put_fixed(out, round_scaled(mw / 1000.0, 100.0), 2), "W");
        return "W  ";
    }
    if (mw < 30000.0) {
        put_text(put_fixed(out, round_scaled(mw / 1000.0, 10.0), 1), "W");
        return "W  ";
    }
    put_text(put_fixed(out, round_scaled(mw / 1000.0, 1.0), 0), "W");
    return "W  ";
}

static const char *mod_unit(void)
{
    return tx.mod == MOD_FM ? "kHz" : (tx.mod == MOD_AM ? "%" : "RAD");
}

static char *format_mod_value(char *out, double value)
{
    if (tx.mod == MOD_AM)
        return put_fixed(out, round_scaled(value, 1.0), 0);
    if (round_scaled(value, 100.0) == 0)
        return put_fixed(out, 0, 0);
    if (value < 10.0)
        return put_fixed(out, round_scaled(value, 100.0), 2);
    return put_fixed(out, round_scaled(value, 10.0), 1);
}

static void format_mod_level(char *out)
{
    put_text(format_mod_value(out, tx.mod_level), mod_unit());
}

/* The peak readings beside the bars: the positive one ends at the bar, the negative starts there. */
static void draw_peak_values_at(uint8_t left_first, uint8_t left_last, uint8_t right)
{
    char text[12];
    uint8_t length = 0;

    format_mod_value(text, tx.mod_peak_pos);
    while (text[length])
        length++;
    for (uint8_t col = left_first; col <= left_last; col++) {
        uint8_t from_end = (uint8_t)(left_last - col);
        screen_put(col, BAR_ZERO_ROW, from_end < length ? (uint8_t)text[length - 1u - from_end] : ' ');
    }
    format_mod_value(text, tx.mod_peak_neg);
    print_field(right, BAR_ZERO_ROW, text, MOD_PEAK_WIDTH);
}

static void draw_peak_values(void)
{
    draw_peak_values_at(MOD_PEAK_LEFT_COL, MOD_PEAK_LEFT_LAST_COL, MOD_PEAK_RIGHT_COL);
}

static void format_af_freq(char *out)
{
    uint32_t f = (uint32_t)tx.af_freq;

    if (f < 10000u)
        put_text(put_fixed(out, f, 1), "Hz");
    else if (f < 32768u)
        put_text(put_fixed(out, f, 4), "kHz");
    else
        put_text(put_fixed(out, (f + 5u) / 10u, 3), "kHz");
}

static void format_af_level(char *out, int32_t tenths_mv)
{
    uint32_t level = (uint32_t)tenths_mv;

    if (level < 10000u)
        put_text(put_fixed(out, level, 1), "mV");
    else
        put_text(put_fixed(out, level / 10u, 3), "V");
}

/* The level as the operator sees it, after the optional accessories. */
static void format_shown_af_level(char *out, int32_t tenths_mv)
{
    int32_t shown = param_get(PARAM_AF_ATTENUATOR) ? tenths_mv / ATTENUATOR_FACTOR : tenths_mv;

    if (param_get(PARAM_AF_600_OHM) && shown > 0) {
        double dbm = 20.0 * log10(shown / 10000.0 / VOLTS_AT_0DBM_600_OHM);
        uint32_t tenths = round_scaled(dbm < 0.0 ? -dbm : dbm, 10.0);

        if (dbm < 0.0 && tenths)
            *out++ = '-';
        put_text(put_fixed(out, tenths, 1), "dBm");
    } else {
        format_af_level(out, shown);
    }
}

static void draw_box(uint8_t show)
{
    for (uint8_t col = AUTO_BOX_COL; col < AUTO_BOX_COL + AUTO_BOX_WIDTH; col++) {
        uint8_t edge = col == AUTO_BOX_COL || col == AUTO_BOX_COL + AUTO_BOX_WIDTH - 1u;
        uint8_t first = col == AUTO_BOX_COL;

        screen_put(col, 0, show ? (edge ? (first ? BOX_TOP_LEFT : BOX_TOP_RIGHT) : BOX_HORIZONTAL) : ' ');
        screen_put(col, 2, show ? (edge ? (first ? BOX_BOTTOM_LEFT : BOX_BOTTOM_RIGHT) : BOX_HORIZONTAL) : ' ');
    }
    if (show) {
        screen_put(AUTO_BOX_COL, 1, BOX_LEFT);
        screen_print(AUTO_BOX_COL + 1, 1, "AUTO TUNE");
        screen_put(AUTO_BOX_COL + AUTO_BOX_WIDTH, 1, BOX_RIGHT);
    } else {
        for (uint8_t col = AUTO_BOX_COL; col <= AUTO_BOX_COL + AUTO_BOX_WIDTH; col++)
            screen_put(col, 1, ' ');
    }
}

/* Three significant digits, as the field is narrow. */
static void format_offset(char *out, double hz)
{
    if (hz < 999.5)
        put_text(put_fixed(out, (uint32_t)(hz + 0.5), 0), "Hz");
    else if (hz < 9995.0)
        put_text(put_fixed(out, round_scaled(hz, 0.1), 2), "kHz");
    else if (hz < 99950.0)
        put_text(put_fixed(out, round_scaled(hz, 0.01), 1), "kHz");
    else if (hz < 999500.0)
        put_text(put_fixed(out, round_scaled(hz, 0.001), 0), "kHz");
    else if (hz < 9995000.0)
        put_text(put_fixed(out, round_scaled(hz, 0.0001), 2), "MHz");
    else if (hz < 99950000.0)
        put_text(put_fixed(out, round_scaled(hz, 0.00001), 1), "MHz");
    else
        put_text(put_fixed(out, round_scaled(hz, 0.000001), 0), "MHz");
}

static void draw_offset(void)
{
    char text[16];
    char *out = text;

    if (!tx.manual) {
        print_field(OFFSET_LABEL_COL, OFFSET_ROW, "", 15);
        return;
    }
    screen_print(OFFSET_LABEL_COL, OFFSET_ROW, "OFFSET:");
    if (!tx.have_measurement) {
        print_field(OFFSET_VALUE_COL, OFFSET_ROW, "", OFFSET_VALUE_WIDTH);
        return;
    }
    double offset = tx.measured_hz - tx.tuned_hz;
    double magnitude = offset < 0.0 ? -offset : offset;

    *out++ = offset < 0.0 ? '-' : '+';
    format_offset(out, magnitude);
    print_field(OFFSET_VALUE_COL, OFFSET_ROW, text, OFFSET_VALUE_WIDTH);
    readings_set(READING_RF_COUNTER, text);
}

static uint8_t rf_entry_active(void)
{
    return tx.group != GROUP_AF && tx.field == FIELD_FREQ && tx.entry_len;
}

static void draw_rf_label(void)
{
    for (uint8_t col = 1; col <= 7; col++)
        screen_put(col, 1, ' ');
    if (tx.group == GROUP_RF || rf_entry_active()) {
        screen_print_reverse(1, 2, "TX");
        screen_print_reverse(4, 2, "FREQ");
    } else {
        screen_print(1, 2, "TX FREQ");
    }
}

static void draw_frequency(void)
{
    char text[20];

    if (rf_entry_active()) {
        print_field(VALUE_COL, 2, tx.entry, VALUE_WIDTH);
    } else if (tx.manual) {
        print_field(VALUE_COL, 2, tx.manual_text, VALUE_WIDTH);
    } else if (tx.have_measurement) {
        format_rf(text, tx.measured_hz);
        print_field(VALUE_COL, 2, text, VALUE_WIDTH);
        readings_set(READING_RF_COUNTER, text);
    } else {
        print_field(VALUE_COL, 2, "", VALUE_WIDTH);
    }
}

static void draw_filter(void)
{
    static const uint8_t code_300[] = {6, 3, 3};

    if (tx.filter == AF_FILTER_BAND_PASS) {
        print_field(FILTER_VALUE_COL, FILTER_ROW, "0.3-3.4kHz", FILTER_VALUE_WIDTH);
        readings_set(READING_FILTER, "0.3-3.4kHz");
    } else if (tx.filter == AF_FILTER_LOW_PASS_50K) {
        print_field(FILTER_VALUE_COL, FILTER_ROW, "15kHz LP", FILTER_VALUE_WIDTH);
        readings_set(READING_FILTER, "15kHz LP");
    } else {
        print_field(FILTER_VALUE_COL, FILTER_ROW, "   Hz LP", FILTER_VALUE_WIDTH);
        readings_set(READING_FILTER, "300Hz LP");
        for (uint8_t i = 0; i < sizeof code_300; i++)
            screen_put((uint8_t)(FILTER_VALUE_COL + i), FILTER_ROW, code_300[i]);
    }
}

/* A selected value is shown in reverse field, without its unit. */
static void print_value(uint8_t row, const char *text, uint8_t width, uint8_t selected)
{
    uint8_t digits = 0;

    for (uint8_t col = VALUE_COL; col < VALUE_COL + width; col++)
        screen_put(col, (uint8_t)(row - 1u), ' ');
    if (!selected) {
        print_field(VALUE_COL, row, text, width);
        return;
    }
    while ((text[digits] >= '0' && text[digits] <= '9') || text[digits] == '.' || text[digits] == '-')
        digits++;
    print_field(VALUE_COL, row, "", width);
    for (uint8_t i = 0; i < digits; i++) {
        char single[2] = {text[i], 0};
        screen_print_reverse((uint8_t)(VALUE_COL + i), row, single);
    }
    screen_print((uint8_t)(VALUE_COL + digits), row, text + digits);
}

static void draw_af(void)
{
    char text[20];
    uint8_t af = tx.group == GROUP_AF;
    uint8_t typing_value = af && !tx.entering_inc && tx.entry_len;

    for (uint8_t col = AF_LABEL_COL; col < VALUE_COL; col++)
        screen_put(col, AF_FREQ_ROW - 1u, ' ');
    if (af) {
        screen_print_reverse(AF_LABEL_COL, AF_FREQ_ROW, "AF");
        screen_print_reverse(AF_LABEL_COL + 3, AF_FREQ_ROW, "GEN");
    } else {
        screen_print(AF_LABEL_COL, AF_FREQ_ROW, "AF GEN");
    }

    for (uint8_t col = TWO_TONE_COL; col < TWO_TONE_COL + 2u; col++) {
        screen_put(col, TWO_TONE_ROW - 1u, ' ');
        screen_put(col, TWO_TONE_ROW, ' ');
    }
    if (tx.two_tone)
        screen_print_reverse(TWO_TONE_COL, TWO_TONE_ROW, "2T");

    if (typing_value && tx.field == FIELD_FREQ) {
        print_value(AF_FREQ_ROW, tx.entry, AF_FREQ_WIDTH, 0);
    } else {
        format_af_freq(text);
        print_value(AF_FREQ_ROW, text, AF_FREQ_WIDTH, af && tx.field == FIELD_FREQ && !tx.entering_inc);
        readings_set(READING_AF_GEN_FREQ, text);
    }
    if (typing_value && tx.field == FIELD_LEVEL) {
        print_value(AF_LEVEL_ROW, tx.entry, AF_LEVEL_WIDTH, 0);
    } else {
        format_shown_af_level(text, tx.af_level);
        print_value(AF_LEVEL_ROW, text, AF_LEVEL_WIDTH, af && tx.field == FIELD_LEVEL && !tx.entering_inc);
        if (param_get(PARAM_AF_ATTENUATOR))
            screen_print_reverse(AF_ACCESSORY_COL, AF_LEVEL_ROW, "A");
        readings_set(READING_AF_GEN_LEVEL, text);
    }

    for (uint8_t col = AF_INC_COL; col < AF_INC_COL + 3u; col++)
        screen_put(col, AF_FREQ_ROW - 1u, ' ');
    if (tx.entering_inc)
        screen_print_reverse(AF_INC_COL, AF_FREQ_ROW, "INC");
    else
        screen_print(AF_INC_COL, AF_FREQ_ROW, "INC");
    screen_print(AF_INC_COL + 3, AF_FREQ_ROW, ":");

    if (tx.entering_inc && tx.field == FIELD_FREQ)
        print_field(AF_INC_FREQ_COL, AF_FREQ_ROW, tx.entry, AF_INC_FREQ_WIDTH);
    else
        print_field(AF_INC_FREQ_COL, AF_FREQ_ROW, tx.af_freq_inc_text, AF_INC_FREQ_WIDTH);
    if (tx.entering_inc && tx.field == FIELD_LEVEL) {
        print_field(AF_INC_LEVEL_COL, AF_FREQ_ROW, tx.entry, AF_INC_LEVEL_WIDTH);
    } else if (tx.af_level_inc) {
        format_af_level(text, tx.af_level_inc);
        print_field(AF_INC_LEVEL_COL, AF_FREQ_ROW, text, AF_INC_LEVEL_WIDTH);
    } else {
        print_field(AF_INC_LEVEL_COL, AF_FREQ_ROW, "", AF_INC_LEVEL_WIDTH);
    }
}

/* Shows where the next digit of a keypad entry goes. */
static void draw_cursor(void)
{
    if (tx.cursor_shown)
        screen_put(tx.cursor_col, tx.cursor_row, ' ');
    tx.cursor_shown = 0;
    if (!tx.entry_len)
        return;
    if (tx.entering_inc) {
        tx.cursor_col = tx.field == FIELD_FREQ ? AF_INC_FREQ_COL : AF_INC_LEVEL_COL;
        tx.cursor_row = AF_FREQ_ROW + 1;
    } else {
        tx.cursor_col = VALUE_COL;
        tx.cursor_row = tx.group != GROUP_AF ? 3 : (tx.field == FIELD_FREQ ? AF_FREQ_ROW + 1 : AF_LEVEL_ROW + 1);
    }
    tx.cursor_col = (uint8_t)(tx.cursor_col + tx.entry_len);
    screen_put(tx.cursor_col, tx.cursor_row, SCREEN_CURSOR);
    tx.cursor_shown = 1;
}

static void draw_mod_bars(void)
{
    static const uint8_t ranges[4][2] = {{0, 0}, {BAR_SCALE_10, BAR_SCALE_100}, {2, BAR_SCALE_30}, {2, BAR_SCALE_10}};

    screen_print(MOD_TITLE_COL, BAR_TITLE_ROW, tx.mod == MOD_FM ? "FM" : (tx.mod == MOD_AM ? "AM" : MOD_TITLE_PHASE));
    bargraph_init(&mod_bar, MOD_BAR_COL, MOD_BAR_COL + 1, MOD_SCALE_COL, MOD_UNIT_COL,
                  ranges[tx.mod][0], ranges[tx.mod][1], 0);
    bargraph_init_mirrored(&mod_neg_bar, MOD_NEG_BAR_COL);
}

static void draw_power_bar(void)
{
    uint8_t low = tx.sel == 0 && !level_external_attenuator(1);

    screen_print(POWER_TITLE_COL, BAR_TITLE_ROW, "POWER");
    bargraph_init(&power_bar, POWER_BAR_COL, POWER_BAR_COL + 1, POWER_SCALE_COL, POWER_UNIT_COL,
                  low ? 1u : BAR_SCALE_30, low ? 8u : 13u, 0);
}

static void draw_dist_label(void)
{
    if (tx.dist_on)
        screen_print(DIST_LABEL_COL, DIST_LABEL_ROW, "DISTN:");
    else
        print_field(DIST_LABEL_COL, DIST_LABEL_ROW, "", VALUE_WIDTH);
}

static void clear_dist_bar(void)
{
    for (uint8_t row = BAR_TITLE_ROW; row <= 31; row++)
        for (uint8_t col = DIST_AREA_FIRST_COL; col <= DIST_AREA_LAST_COL; col++)
            screen_put(col, row, ' ');
}

static void draw_dist_bar(void)
{
    screen_print(DIST_TITLE_COL, BAR_TITLE_ROW, "DISTN");
    bargraph_init(&dist_bar, DIST_BAR_COL, DIST_BAR_COL + 1, DIST_SCALE_COL, DIST_UNIT_COL,
                  BAR_SCALE_10, BAR_SCALE_30, 0);
}

static void draw_bars_area(void)
{
    screen_put(BAR_PLUS_COL, BAR_SIGN_ROW, '+');
    screen_put(BAR_MINUS_COL, BAR_SIGN_ROW, '-');
    draw_peak_values();
    draw_power_bar();
    draw_mod_bars();
    if (tx.dist_on)
        draw_dist_bar();
}

static void draw_static(void)
{
    screen_clear();
    screen_print(10, 0, "TRANSMITTER TEST");
    for (uint8_t col = 10; col < 26; col++)
        screen_put(col, 1, SCREEN_UNDERLINE);
    screen_print(8, 2, " :");
    screen_print(4, 4, "POWER:");
    screen_print(0, 6, "MOD FREQ :");
    screen_print(4, 8, "LEVEL:");
    screen_print(FILTER_LABEL_COL, FILTER_ROW, "FILTER:");
    screen_print(AF_LABEL_COL + 6, AF_FREQ_ROW, ":");
    draw_dist_label();
    draw_bars_area();
}

static void draw_external_attenuator(void)
{
    app_draw_external_attenuator(EXT_MARK_COL, EXT_MARK_ROW, tx.sel == 0);
}

static void draw_all(void)
{
    draw_box(tx.manual);
    draw_rf_label();
    draw_external_attenuator();
    draw_frequency();
    draw_offset();
    draw_filter();
    draw_af();
    draw_cursor();
}

static void draw_readings(void)
{
    char text[20];
    const char *unit = format_power(text, tx.power_mw);

    print_field(VALUE_COL, 4, text, VALUE_WIDTH);
    readings_set(READING_POWER, text);
    format_mod_level(text);
    print_field(VALUE_COL, 8, text, VALUE_WIDTH);
    readings_set(READING_MOD_LEVEL, text);
    put_text(format_mod_value(text, tx.mod_peak_pos), mod_unit());
    readings_set(READING_PEAK_POS, text);
    put_text(format_mod_value(text, tx.mod_peak_neg), mod_unit());
    readings_set(READING_PEAK_NEG, text);
    if (tx.dist_on) {
        put_text(put_fixed(text, round_scaled(tx.dist_percent, 10.0), 1), "%");
        print_field(VALUE_COL, DIST_LABEL_ROW, text, DIST_VALUE_WIDTH);
        readings_set(READING_TX_DISTORTION, text);
    }
    if (tx.scope_shown)
        return;

    bargraph_update(&power_bar, tx.power_mw, unit);
    bargraph_update(&mod_bar, tx.mod_peak_pos, tx.mod == MOD_FM ? "kHz" : (tx.mod == MOD_AM ? "%  " : "RAD"));
    bargraph_update_mirrored(&mod_neg_bar, &mod_bar, tx.mod_peak_neg);
    draw_peak_values();
    if (tx.dist_on)
        bargraph_update(&dist_bar, tx.dist_percent, "%  ");
}

static void draw_af_counter(void)
{
    char text[20];
    uint32_t tenths = round_scaled(afcounter_hz(), 10.0);

    if (tenths < 10000u)
        put_text(put_fixed(text, tenths, 1), "Hz");
    else
        put_text(put_fixed(text, round_scaled(afcounter_hz(), 1.0), 3), "kHz");
    print_field(VALUE_COL, 6, text, VALUE_WIDTH);
    readings_set(READING_MOD_FREQ, text);
    readings_set(READING_AF_FREQ, text);
}

void tx_get_settings(instrument_settings *s)
{
    s->deviation = 15000u; /* the transmitter test has no modulation setting of its own */
    s->rf_hz = tx.manual ? (uint32_t)(tx.tuned_hz + 0.5) : 0u;
    s->manual = tx.manual;
    s->two_tone = tx.two_tone;
    s->af_freq = tx.af_freq;
    s->af_level = tx.af_level;
    s->mod_kind = (uint8_t)tx.mod;
    s->input_select = tx.sel;
    s->filter = tx.filter;
    s->distortion = tx.dist_on;
    s->scope_shown = tx.scope_shown;
    s->af_freq_inc = tx.af_freq_inc;
    s->af_level_inc = tx.af_level_inc;
    s->group = tx.group == GROUP_AF ? 3u : 0u;
    s->field = tx.field == FIELD_FREQ ? 4u : (tx.field == FIELD_LEVEL ? 2u : 0u);
}

/* Takes effect at the next tx_enter. */
void tx_set_settings(const instrument_settings *s)
{
    tx.manual = s->manual && s->rf_hz;
    tx.two_tone = s->two_tone;
    tx.tuned_hz = (double)s->rf_hz;
    tx.manual_recalled = tx.manual;
    tx.af_freq = s->af_freq;
    tx.af_level = s->af_level;
    tx.mod = s->mod_kind >= MOD_AM && s->mod_kind <= MOD_PM ? (mod_kind)s->mod_kind : MOD_FM;
    tx.sel = s->input_select & 1u;
    tx.filter = s->filter <= AF_FILTER_LOW_PASS_300 ? s->filter : AF_FILTER_BAND_PASS;
    tx.dist_on = s->distortion;
    tx.recall_scope = s->scope_shown;
    tx.af_freq_inc = s->af_freq_inc;
    tx.af_level_inc = s->af_level_inc;
    tx.af_freq_inc_text[0] = 0;
    if (tx.af_freq_inc) {
        char *out = tx.af_freq_inc_text;
        uint32_t inc = (uint32_t)tx.af_freq_inc;

        if (inc < 10000u)
            put_text(put_fixed(out, inc / 10u, 0), "Hz");
        else
            put_text(put_fixed(out, inc / 10u, 3), "kHz");
    }
    tx.recall_group = s->group == 3u ? GROUP_AF : GROUP_NONE;
    tx.recall_field = s->field == 4u ? FIELD_FREQ : (s->field == 2u ? FIELD_LEVEL : FIELD_NONE);
}

void tx_publish_settings(void)
{
    char text[20];

    format_af_freq(text);
    readings_set(READING_AF_GEN_FREQ, text);
    format_shown_af_level(text, tx.af_level);
    readings_set(READING_AF_GEN_LEVEL, text);
}

void tx_refresh(void)
{
    draw_all();
}

uint8_t tx_manual(void) { return tx.manual; }
uint8_t tx_filter(void) { return tx.filter; }
uint8_t tx_distortion_on(void) { return tx.dist_on; }
uint8_t tx_input_select(void) { return tx.sel; }

/* ---- public interface ---------------------------------------------------------------- */

/* The tuner chip needs these three words before the first frequency. */
static const uint8_t tuner_setup[] = {0x4E, 0x52, 0x69};

void tx_init(void)
{
    tx.mod = MOD_FM;
    tx.sel = 1;
    tx.filter = AF_FILTER_BAND_PASS;
    tx.manual = 0;
    tx.retune_left = RETUNE_READINGS;
    tx.af_freq = 10000;
    tx.af_level = AF_LEVEL_DEFAULT_TENTH_MV;
    tx.af_freq_inc = 0;
    tx.af_level_inc = 0;
    tx.af_freq_inc_text[0] = 0;
    tx.dist_on = 0;
    tx.scope_shown = 0;
    tx.manual_recalled = 0;
    tx.recall_scope = 0;
    tx.recall_group = GROUP_NONE;
    tx.recall_field = FIELD_NONE;
    for (uint8_t i = 0; i < sizeof tuner_setup; i++)
        strobe_tuning_word(tuner_setup[i]);
    tune_receiver(STARTUP_TUNE_HZ);
}

static void apply_scope_gain(void);
static scope_kind current_scope_kind(void);

static void apply_af_freq(void);

static void apply_af_level(void)
{
    static const uint8_t range_nibbles[3] = {4u, 2u, 6u};
    static const uint8_t range_bits[3] = {0x10u, 0x20u, 0x30u};
    double volts = tx.af_level / 10000.0;
    uint8_t range = volts > 0.6375 ? 0u : (volts > 0.255 ? 1u : 2u);
    double multiplier = range == 0 ? 100.0 : (range == 1 ? 400.0 : 1000.0);
    uint8_t dac = (uint8_t)(volts * multiplier + 0.5);

    /* The level goes to the fixed 1 kHz source or to the variable oscillator, or both. */
    if (tx.two_tone) {
        hal_write(REG_AF_LEVEL, dac);
        hal_write(REG_AF_RANGE_DAC, dac);
        modulation_set_transmit_nibble(range_nibbles[range]);
        synth_set_af_range(range_bits[range]);
    } else if (tx.af_freq == AF_FREQ_FIXED) {
        hal_write(REG_AF_LEVEL, 0);
        hal_write(REG_AF_RANGE_DAC, dac);
        modulation_set_transmit_nibble(AF_TRANSMIT_NIBBLE_OFF);
        synth_set_af_range(range_bits[range]);
    } else {
        hal_write(REG_AF_LEVEL, dac);
        hal_write(REG_AF_RANGE_DAC, 0);
        modulation_set_transmit_nibble(range_nibbles[range]);
        synth_set_af_range(range_bits[0]);
    }
    apply_af_freq();
}

static void apply_af_freq(void)
{
    modulation_state state = {0};

    state.freq = (uint32_t)tx.af_freq;
    state.deviation = 15000u;
    modulation_apply(&state);
}

uint8_t tx_two_tone(void)
{
    return tx.two_tone;
}

void tx_set_two_tone(uint8_t on)
{
    tx.two_tone = on;
    apply_af_level();
}

void tx_resume(void)
{
    tx.recall_group = tx.group;
    tx.recall_field = tx.field;
    tx.recall_scope = tx.scope_shown;
    tx_enter(tx.rf_level);
}

void tx_draw_held_readings(void)
{
    draw_readings();
    draw_af_counter();
}

void tx_enter(int16_t rf_level)
{
    tx.step = STEP_PEAK_POS;
    tx.switching_done = 0;
    tx.peak_pos = tx.peak_neg = 0;
    tx.peak_range = tx.pending_peak_range = MOD_RANGE_TOP;
    tx.power_adc = 0;
    tx.power_range = tx.pending_power_range = POWER_RANGE_DEFAULT;
    tx.mod_filter.primed = 0;
    tx.new_readings = 0;
    tx.have_measurement = 0;
    tx.group = tx.recall_group;
    tx.field = tx.recall_field;
    tx.recall_group = GROUP_NONE;
    tx.recall_field = FIELD_NONE;
    tx.entering_inc = 0;
    tx.cursor_shown = 0;
    tx.scope_shown = 0;
    tx.rf_level = rf_level;
    tx.dist_passes = 0;
    tx.dist_volts_range = tx.dist_pending_volts = MOD_RANGE_TOP;
    tx.dist_notch_range = tx.dist_pending_notch = MOD_RANGE_TOP;
    entry_clear();

    modulation_set_transmit(1);
    synth_set_transmit(1);
    level_set_without_external(rf_level, tx.sel);
    meter_prepare_transmit(1);
    meter_select_range(MOD_RANGE_TOP);
    meter_set_filter_extra(0x03u, 0x02u);
    apply_af_level();
    apply_power_range(POWER_RANGE_DEFAULT);
    apply_scope_gain();

    meter_set_filter((af_filter)tx.filter);
    meter_set_filter_extra(0x03u, tx.mod == MOD_AM ? 0x00u : (tx.mod == MOD_FM ? 0x02u : 0x01u));
    if (tx.manual_recalled) {
        double hz = tx.tuned_hz;
        char *out = put_fixed(tx.manual_text, (uint32_t)(hz / 10.0 + 0.5), 5);
        put_text(out, "MHz");
        tune_receiver(hz);
        tx.manual_recalled = 0;
    }

    draw_static();
    draw_all();
    draw_af_counter();
    if (tx.recall_scope) {
        tx.recall_scope = 0;
        tx.scope_shown = 1;
        scope_show(current_scope_kind(), 1);
    }
}

void tx_leave(void)
{
    modulation_set_transmit(0);
    synth_set_transmit(0);
    synth_set_af_range(0x30u);
    level_set_power_range_bit(1);
    meter_prepare_transmit(0);
}

void tx_poll(uint8_t update_display)
{
    double hz;

    measurement_step();
    if (afcounter_poll() && update_display)
        draw_af_counter();

    if (rfcounter_poll(&hz)) {
        tx.measured_hz = hz;
        tx.have_measurement = 1;
        if (!tx.manual) {
            if (hz > TUNE_MIN_HZ) {
                double error = hz - tx.tuned_hz;
                if ((error < 0 ? -error : error) > RETUNE_HZ) {
                    if (--tx.retune_left == 0) {
                        tune_receiver(hz);
                        tx.retune_left = RETUNE_READINGS;
                    }
                } else {
                    tx.retune_left = RETUNE_READINGS;
                }
            } else {
                tx.retune_left = RETUNE_READINGS;
            }
        }
        if (update_display) {
            draw_frequency();
            draw_offset();
        }
    }

    if (tx.new_readings && update_display)
        draw_readings();
    tx.new_readings = 0;
}

static void entry_add(char c)
{
    if (tx.field == FIELD_NONE || tx.entry_len >= ENTRY_SIZE - 1)
        return;
    tx.entry[tx.entry_len++] = c;
    tx.entry[tx.entry_len] = 0;
}

static void finish_rf_freq(int32_t scale_tenths, const char *unit)
{
    int32_t value;

    if (!parse_entry(tx.entry, tx.entry_len, scale_tenths, &value))
        return;
    double hz = value * 10.0;
    if (hz < TUNE_MIN_HZ || hz > TUNE_MAX_HZ)
        return;
    put_text(put_text(tx.manual_text, tx.entry), unit);
    tx.manual = 1;
    tune_receiver(hz);
    entry_clear();
    if (tx.group == GROUP_NONE)
        tx.field = FIELD_NONE;
}

static void finish_af_freq(int32_t scale, const char *unit)
{
    int32_t value;

    if (!parse_entry(tx.entry, tx.entry_len, scale, &value))
        return;
    if (tx.entering_inc) {
        if (value > AF_FREQ_MAX) {
            gpib_report_entry_error(ENTRY_ERROR_MOD_FREQ_HIGH);
            return;
        }
        put_text(put_text(tx.af_freq_inc_text, tx.entry), unit);
        tx.af_freq_inc = value;
        tx.entering_inc = 0;
        entry_clear();
        return;
    }
    if (value < AF_FREQ_MIN || value > AF_FREQ_MAX) {
        gpib_report_entry_error(value > AF_FREQ_MAX ? ENTRY_ERROR_MOD_FREQ_HIGH : ENTRY_ERROR_MOD_FREQ_LOW);
        return;
    }
    tx.af_freq = value;
    entry_clear();
    apply_af_level();
}

/* Stores a level as entered: the attenuator accessory needs ten times the voltage. */
static void store_af_level(int32_t value)
{
    if (param_get(PARAM_AF_ATTENUATOR))
        value *= ATTENUATOR_FACTOR;
    if (value < 0)
        return;
    if (value > AF_LEVEL_MAX_MV * 10L) {
        gpib_report_entry_error(ENTRY_ERROR_MOD_LEVEL_HIGH);
        return;
    }
    if (tx.entering_inc) {
        tx.af_level_inc = value;
        tx.entering_inc = 0;
    } else {
        tx.af_level = value;
        apply_af_level();
    }
    entry_clear();
}

static void finish_af_level(int32_t scale)
{
    int32_t value;

    if (parse_entry(tx.entry, tx.entry_len, scale, &value))
        store_af_level(value);
}

/* Only the 600 ohm accessory makes dBm a level unit for the AF generator. */
static void finish_af_level_dbm(void)
{
    int32_t tenths_db;

    if (!param_get(PARAM_AF_600_OHM)) {
        gpib_report_entry_error(ENTRY_ERROR_AF_LEVEL_UNIT);
    } else if (parse_entry(tx.entry, tx.entry_len, 10, &tenths_db)) {
        double volts = VOLTS_AT_0DBM_600_OHM * pow(10.0, tenths_db / 200.0);

        store_af_level((int32_t)floor(volts * 10000.0 + 0.5));
    }
}

static void step_af_freq(int32_t delta)
{
    int32_t value = tx.af_freq + delta;

    if (!delta || value < AF_FREQ_MIN || value > AF_FREQ_MAX)
        return;
    tx.af_freq = value;
    apply_af_level();
}

static void step_af_level(int32_t delta)
{
    int32_t value = tx.af_level + delta;

    if (!delta || value < 0 || value > AF_LEVEL_MAX_MV * 10L)
        return;
    tx.af_level = value;
    apply_af_level();
}

/* One step of the level DAC in the present range; the result lands on a DAC step. */
static uint8_t step_af_level_dac(int8_t direction)
{
    int32_t step = tx.af_level > 6375L ? 100L : (tx.af_level > 2550L ? 25L : 10L);
    int32_t value = tx.af_level + direction * step;

    if (value < 0 || value > AF_LEVEL_MAX_MV * 10L)
        return 0;
    int32_t divisor = value > 6375L ? 100L : (value > 2550L ? 25L : 10L);

    tx.af_level = (value + divisor / 2) / divisor * divisor;
    return 1;
}

static void refresh_af_screen(void)
{
    if (tx.audio)
        tx_audio_draw();
    else
        draw_all();
}

void tx_knob(int8_t direction)
{
    if (tx.group != GROUP_AF || tx.entering_inc)
        return;
    if (tx.field == FIELD_FREQ) {
        step_af_freq(direction * (tx.af_freq < AF_FREQ_FINE_LIMIT ? 1L : 10L));
    } else if (tx.field == FIELD_LEVEL) {
        if (!step_af_level_dac(direction))
            return;
        apply_af_level();
    } else {
        return;
    }
    refresh_af_screen();
}

static scope_kind current_scope_kind(void)
{
    return tx.mod == MOD_FM ? SCOPE_FM : (tx.mod == MOD_AM ? SCOPE_AM : SCOPE_PM);
}

static void show_bars(void)
{
    tx.scope_shown = 0;
    for (uint8_t row = 15; row <= 31; row++)
        for (uint8_t col = 0; col < SCREEN_COLS; col++)
            screen_put(col, row, ' ');
    apply_scope_gain();
    draw_bars_area();
}

static void toggle_distortion(void)
{
    tx.dist_on = !tx.dist_on;
    tx.dist_passes = 0;
    tx.switching_done = 0;
    if (tx.step == STEP_DIST)
        tx.step = STEP_POWER;
    draw_dist_label();
    if (tx.dist_on) {
        tx.dist_volts_range = tx.dist_pending_volts = MOD_RANGE_TOP;
        tx.dist_notch_range = tx.dist_pending_notch = MOD_RANGE_TOP;
        tx.dist_volts_adc = tx.dist_notch_adc = 0;
        tx.dist_percent = 0.0;
        tx.filter = AF_FILTER_BAND_PASS;
        meter_set_filter(AF_FILTER_BAND_PASS);
        if (!tx.scope_shown)
            draw_dist_bar();
    } else if (!tx.scope_shown) {
        clear_dist_bar();
    }
}

static void apply_scope_gain(void)
{
    meter_set_scope_gain(tx.mod == MOD_PM ? SCOPE_GAIN_PHASE : SCOPE_GAIN_FM_AM);
}

static void select_mod(mod_kind mod)
{
    tx.mod = mod;
    if (!tx.scope_shown)
        apply_scope_gain();
    tx.mod_filter.primed = 0;
    tx.pending_peak_range = MOD_RANGE_TOP;
    meter_set_filter_extra(0x03u, mod == MOD_AM ? 0x00u : (mod == MOD_FM ? 0x02u : 0x01u));
    if (tx.scope_shown) {
        scope_show(current_scope_kind(), 1);
        return;
    }
    screen_print(MOD_TITLE_COL, BAR_TITLE_ROW, "  ");
    for (uint8_t row = 18; row <= 31; row++)
        for (uint8_t col = MOD_SCALE_COL; col <= MOD_UNIT_COL + 2; col++)
            if (col != BAR_PLUS_COL && col != BAR_MINUS_COL)
                screen_put(col, row, ' ');
    draw_mod_bars();
}

uint8_t tx_key(uint8_t code)
{
    switch (code) {
    case KEY_TX:
        if (tx.manual) {
            tx.manual = 0;
            tx.retune_left = RETUNE_READINGS;
        }
        break;
    case KEY_RF_GEN:
        tx.group = GROUP_RF;
        tx.field = FIELD_NONE;
        tx.entering_inc = 0;
        entry_clear();
        break;
    case KEY_AF_GEN:
        tx.group = GROUP_AF;
        tx.field = FIELD_NONE;
        tx.entering_inc = 0;
        entry_clear();
        break;
    case KEY_FREQ:
        tx.field = FIELD_FREQ;
        tx.entering_inc = 0;
        entry_clear();
        break;
    case KEY_DELTA_INCR:
        if (tx.group == GROUP_AF && tx.field != FIELD_NONE) {
            tx.entering_inc = 1;
            entry_clear();
        }
        break;
    case KEY_FREQ_UP:
        if (tx.group == GROUP_AF)
            step_af_freq(tx.af_freq_inc);
        break;
    case KEY_FREQ_DOWN:
        if (tx.group == GROUP_AF)
            step_af_freq(-tx.af_freq_inc);
        break;
    case KEY_LEVEL_UP:
        if (tx.group == GROUP_AF)
            step_af_level(tx.af_level_inc);
        break;
    case KEY_LEVEL_DOWN:
        if (tx.group == GROUP_AF)
            step_af_level(-tx.af_level_inc);
        break;
    case KEY_DISTORTION:
        toggle_distortion();
        break;
    case KEY_SELECT:
        tx.sel = !tx.sel;
        level_set_without_external(tx.rf_level, tx.sel);
        draw_external_attenuator();
        tx.power_range = tx.pending_power_range = POWER_RANGE_DEFAULT;
        if (!tx.scope_shown) {
            for (uint8_t row = 18; row <= 31; row++)
                for (uint8_t col = POWER_SCALE_COL; col <= POWER_UNIT_COL + 2u; col++)
                    screen_put(col, row, ' ');
            draw_power_bar();
        }
        break;
    case KEY_AF_LOW_PASS_300:
        tx.filter = AF_FILTER_LOW_PASS_300;
        meter_set_filter(AF_FILTER_LOW_PASS_300);
        break;
    case KEY_SCOPE:
        if (!tx.scope_shown) {
            tx.scope_shown = 1;
            scope_show(current_scope_kind(), 1);
        }
        break;
    case KEY_BAR_CHART:
        if (tx.scope_shown)
            show_bars();
        break;
    case KEY_SCOPE_VERT_EXPAND:
    case KEY_SCOPE_VERT_COMPRESS:
        if (tx.scope_shown)
            scope_step_vertical(code == KEY_SCOPE_VERT_COMPRESS ? 1 : -1);
        break;
    case KEY_SCOPE_HORIZ_EXPAND:
    case KEY_SCOPE_HORIZ_COMPRESS:
        if (tx.scope_shown)
            scope_step_timebase(code == KEY_SCOPE_HORIZ_COMPRESS ? 1 : -1);
        break;
    case KEY_SCOPE_REPEAT:
    case KEY_SCOPE_SINGLE:
        if (tx.scope_shown)
            scope_set_single_sweep(code == KEY_SCOPE_SINGLE);
        break;
    case KEY_LEVEL:
        if (tx.group == GROUP_AF) {
            tx.field = FIELD_LEVEL;
            tx.entering_inc = 0;
            entry_clear();
        }
        break;
    case KEY_0: entry_add('0'); break;
    case KEY_1: entry_add('1'); break;
    case KEY_2: entry_add('2'); break;
    case KEY_3: entry_add('3'); break;
    case KEY_4: entry_add('4'); break;
    case KEY_5: entry_add('5'); break;
    case KEY_6: entry_add('6'); break;
    case KEY_7: entry_add('7'); break;
    case KEY_8: entry_add('8'); break;
    case KEY_9: entry_add('9'); break;
    case KEY_POINT: entry_add('.'); break;
    case KEY_MINUS: entry_add('-'); break;
    case KEY_DELETE:
        if (tx.entry_len)
            tx.entry[--tx.entry_len] = 0;
        break;
    case KEY_MHZ:
        if (tx.field == FIELD_FREQ && tx.group != GROUP_AF && tx.entry_len)
            finish_rf_freq(100000L, "MHz");
        else if (tx.field == FIELD_LEVEL && tx.entry_len)
            finish_af_level(10000L);
        break;
    case KEY_KHZ:
        if (tx.field == FIELD_FREQ && tx.group != GROUP_AF && tx.entry_len)
            finish_rf_freq(100L, "kHz");
        else if (tx.field == FIELD_FREQ && tx.entry_len)
            finish_af_freq(10000L, "kHz");
        else if (tx.field == FIELD_LEVEL && tx.entry_len)
            finish_af_level(10L);
        break;
    case KEY_HZ:
        if (tx.field == FIELD_FREQ && tx.group == GROUP_AF && tx.entry_len)
            finish_af_freq(10L, "Hz");
        break;
    case KEY_DBM:
        if (tx.field == FIELD_LEVEL && tx.group == GROUP_AF && tx.entry_len)
            finish_af_level_dbm();
        break;
    case KEY_FM:
        select_mod(MOD_FM);
        break;
    case KEY_AM_PERCENT:
        select_mod(MOD_AM);
        break;
    case KEY_PM:
        select_mod(MOD_PM);
        break;
    case KEY_AF_BAND_PASS:
        tx.filter = AF_FILTER_BAND_PASS;
        meter_set_filter(AF_FILTER_BAND_PASS);
        break;
    case KEY_AF_LOW_PASS:
        tx.filter = tx.filter == AF_FILTER_LOW_PASS_50K ? AF_FILTER_LOW_PASS_300 : AF_FILTER_LOW_PASS_50K;
        meter_set_filter((af_filter)tx.filter);
        break;
    default:
        return 0;
    }
    refresh_af_screen();
    return 1;
}

/* ---- audio test ---------------------------------------------------------------------- */

void tx_audio_begin(void)
{
    tx.audio = 1;
    tx.group = GROUP_AF;
    tx.field = tx.recall_field; /* set when the settings come from a store */
    tx.recall_group = GROUP_NONE;
    tx.recall_field = FIELD_NONE;
    tx.entering_inc = 0;
    tx.cursor_shown = 0;
    entry_clear();
    modulation_set_transmit(1);
    synth_set_transmit(1);
    level_set_without_external(AUDIO_RF_PARKED, 0);
    apply_af_level();
}

void tx_audio_end(void)
{
    tx.audio = 0;
    tx_leave();
}

uint8_t tx_audio_key(uint8_t code)
{
    switch (code) {
    case KEY_FREQ:
    case KEY_LEVEL:
    case KEY_DELTA_INCR:
    case KEY_FREQ_UP:
    case KEY_FREQ_DOWN:
    case KEY_LEVEL_UP:
    case KEY_LEVEL_DOWN:
    case KEY_0:
    case KEY_1:
    case KEY_2:
    case KEY_3:
    case KEY_4:
    case KEY_5:
    case KEY_6:
    case KEY_7:
    case KEY_8:
    case KEY_9:
    case KEY_POINT:
    case KEY_MINUS:
    case KEY_DELETE:
    case KEY_MHZ:
    case KEY_KHZ:
    case KEY_HZ:
    case KEY_DBM:
        tx_key(code);
        return 1;
    default:
        return 0;
    }
}

void tx_audio_draw(void)
{
    char text[20];
    uint8_t freq_field = tx.field == FIELD_FREQ;
    uint8_t level_field = tx.field == FIELD_LEVEL;

    screen_print_reverse(0, AUDIO_FREQ_ROW, "AF");
    screen_print_reverse(3, AUDIO_FREQ_ROW, "GEN");
    for (uint8_t col = AUDIO_FREQ_LABEL_COL; col < AUDIO_FREQ_LABEL_COL + 4u; col++)
        screen_put(col, AUDIO_FREQ_ROW - 1u, ' ');
    if (freq_field)
        screen_print_reverse(AUDIO_FREQ_LABEL_COL, AUDIO_FREQ_ROW, "FREQ");
    else
        screen_print(AUDIO_FREQ_LABEL_COL, AUDIO_FREQ_ROW, "FREQ");
    screen_print(AUDIO_COLON_COL, AUDIO_FREQ_ROW, ":");

    for (uint8_t col = AUDIO_LEVEL_LABEL_COL; col < AUDIO_LEVEL_LABEL_COL + 5u; col++)
        screen_put(col, AUDIO_LEVEL_ROW - 1u, ' ');
    if (level_field)
        screen_print_reverse(AUDIO_LEVEL_LABEL_COL, AUDIO_LEVEL_ROW, "LEVEL");
    else
        screen_print(AUDIO_LEVEL_LABEL_COL, AUDIO_LEVEL_ROW, "LEVEL");
    screen_print(AUDIO_COLON_COL, AUDIO_LEVEL_ROW, ":");

    if (tx.entry_len && freq_field && !tx.entering_inc) {
        print_field(AUDIO_VALUE_COL, AUDIO_FREQ_ROW, tx.entry, AUDIO_VALUE_WIDTH);
    } else {
        format_af_freq(text);
        print_field(AUDIO_VALUE_COL, AUDIO_FREQ_ROW, text, AUDIO_VALUE_WIDTH);
        readings_set(READING_AF_GEN_FREQ, text);
    }
    if (tx.entry_len && level_field && !tx.entering_inc) {
        print_field(AUDIO_VALUE_COL, AUDIO_LEVEL_ROW, tx.entry, AUDIO_VALUE_WIDTH);
    } else {
        format_shown_af_level(text, tx.af_level);
        print_field(AUDIO_VALUE_COL, AUDIO_LEVEL_ROW, text, AUDIO_VALUE_WIDTH);
        if (param_get(PARAM_AF_ATTENUATOR))
            screen_print_reverse(AUDIO_ACCESSORY_COL, AUDIO_LEVEL_ROW, "A");
        readings_set(READING_AF_GEN_LEVEL, text);
    }

    for (uint8_t i = 0; i < 2u; i++) {
        uint8_t row = i ? AUDIO_LEVEL_ROW : AUDIO_FREQ_ROW;
        uint8_t this_field = i ? FIELD_LEVEL : FIELD_FREQ;

        for (uint8_t col = AUDIO_INC_COL; col < AUDIO_INC_COL + 3u; col++)
            screen_put(col, (uint8_t)(row - 1u), ' ');
        if (tx.entering_inc && tx.field == this_field)
            screen_print_reverse(AUDIO_INC_COL, row, "INC");
        else
            screen_print(AUDIO_INC_COL, row, "INC");
        screen_print(AUDIO_INC_COL + 3u, row, ":");
    }
    if (tx.entering_inc && tx.field == FIELD_FREQ)
        print_field(AUDIO_INC_VALUE_COL, AUDIO_FREQ_ROW, tx.entry, AUDIO_INC_WIDTH);
    else
        print_field(AUDIO_INC_VALUE_COL, AUDIO_FREQ_ROW, tx.af_freq_inc_text, AUDIO_INC_WIDTH);
    if (tx.entering_inc && tx.field == FIELD_LEVEL) {
        print_field(AUDIO_INC_VALUE_COL, AUDIO_LEVEL_ROW, tx.entry, AUDIO_INC_WIDTH);
    } else if (tx.af_level_inc) {
        format_af_level(text, tx.af_level_inc);
        print_field(AUDIO_INC_VALUE_COL, AUDIO_LEVEL_ROW, text, AUDIO_INC_WIDTH);
    } else {
        print_field(AUDIO_INC_VALUE_COL, AUDIO_LEVEL_ROW, "", AUDIO_INC_WIDTH);
    }

    for (uint8_t col = AUDIO_TWO_TONE_COL; col < AUDIO_TWO_TONE_COL + 2u; col++) {
        screen_put(col, AUDIO_TWO_TONE_ROW - 1u, ' ');
        screen_put(col, AUDIO_TWO_TONE_ROW, ' ');
    }
    if (tx.two_tone)
        screen_print_reverse(AUDIO_TWO_TONE_COL, AUDIO_TWO_TONE_ROW, "2T");

    if (tx.cursor_shown)
        screen_put(tx.cursor_col, tx.cursor_row, ' ');
    tx.cursor_shown = 0;
    if (tx.entry_len) {
        tx.cursor_col = (uint8_t)((tx.entering_inc ? AUDIO_INC_VALUE_COL : AUDIO_VALUE_COL) + tx.entry_len);
        tx.cursor_row = (uint8_t)((tx.field == FIELD_FREQ ? AUDIO_FREQ_ROW : AUDIO_LEVEL_ROW) + 1u);
        screen_put(tx.cursor_col, tx.cursor_row, SCREEN_CURSOR);
        tx.cursor_shown = 1;
    }
}

/* ---- duplex -------------------------------------------------------------------------- */

#define DUPLEX_VALUE_COL 28u
#define DUPLEX_FREQ_COL 27u
#define DUPLEX_VALUE_WIDTH 12u
#define DUPLEX_ROW_FREQ 3u
#define DUPLEX_ROW_POWER 5u
#define DUPLEX_ROW_MOD_FREQ 7u
#define DUPLEX_ROW_MOD_LEVEL 9u
#define DUPLEX_ROW_FILTER 11u
#define DUPLEX_ROW_AF_FREQ 13u
#define DUPLEX_ROW_AF_LEVEL 15u
#define DUPLEX_LABEL_FREQ_COL 22u
#define DUPLEX_LABEL_AF_COL 20u
#define DUPLEX_PORT_COL 16u
#define DUPLEX_BNC_COL 13u
#define DUPLEX_N_COL 38u
#define DUPLEX_POWER_BAR_COL 21u
#define DUPLEX_POWER_SCALE_COL 18u
#define DUPLEX_POWER_UNIT_COL 23u
#define DUPLEX_POWER_TITLE_COL 19u
#define DUPLEX_MOD_BAR_COL 29u
#define DUPLEX_MOD_NEG_BAR_COL 31u
#define DUPLEX_MOD_SCALE_COL 26u
#define DUPLEX_MOD_UNIT_COL 32u
#define DUPLEX_MOD_TITLE_COL 29u
#define DUPLEX_PLUS_COL 27u
#define DUPLEX_MINUS_COL 33u
#define DUPLEX_PEAK_LEFT_FIRST_COL 25u
#define DUPLEX_PEAK_LEFT_LAST_COL 28u
#define DUPLEX_PEAK_RIGHT_COL 32u
#define DUPLEX_CYCLES_PER_SIDE 2u
#define DUPLEX_FILTER_FM 0x02u
#define DUPLEX_FILTER_AM 0x00u
#define DUPLEX_SEPARATOR 0x3Fu
#define DUPLEX_SEPARATOR_COL 19u
#define DUPLEX_LAST_FRAME_ROW 13u

static uint8_t duplex_on_transmitter, duplex_passes;

/* Manual tuning in DUPLEX must not disturb the transmitter test. */
static uint8_t duplex_running, saved_manual;
static double saved_tuned_hz;
static char saved_manual_text[sizeof tx.manual_text];

/* Only 10 Hz resolution fits beside the receiver column. */
static void duplex_format_counter(char *out, double hz)
{
    uint32_t tens = (uint32_t)(hz / 10.0 + 0.5);

    if (tens >= 100000UL)
        put_text(put_fixed(out, tens, 5), "MHz");
    else
        format_rf(out, hz);
}

static void duplex_draw_mod_bars(void)
{
    static const uint8_t ranges[4][2] = {{0, 0}, {BAR_SCALE_10, BAR_SCALE_100}, {2, BAR_SCALE_30}, {2, BAR_SCALE_10}};

    screen_print(DUPLEX_MOD_TITLE_COL, BAR_TITLE_ROW, tx.mod == MOD_FM ? "FM" : (tx.mod == MOD_AM ? "AM" : MOD_TITLE_PHASE));
    bargraph_init(&mod_bar, DUPLEX_MOD_BAR_COL, DUPLEX_MOD_BAR_COL + 1, DUPLEX_MOD_SCALE_COL, DUPLEX_MOD_UNIT_COL,
                  ranges[tx.mod][0], ranges[tx.mod][1], 0);
    bargraph_init_mirrored(&mod_neg_bar, DUPLEX_MOD_NEG_BAR_COL);
}

static void duplex_draw_static(void)
{
    screen_print(17, 0, "DUPLEX");
    for (uint8_t col = 17; col < 23; col++)
        screen_put(col, 1, SCREEN_UNDERLINE);
    screen_print(4, 1, "RECEIVER");
    screen_print(26, 1, "TRANSMITTER");
    for (uint8_t col = 4; col < 12; col++)
        screen_put(col, 2, SCREEN_UNDERLINE);
    for (uint8_t col = 26; col < 37; col++)
        screen_put(col, 2, SCREEN_UNDERLINE);
    for (uint8_t row = 2; row <= DUPLEX_LAST_FRAME_ROW; row++)
        screen_put(DUPLEX_SEPARATOR_COL, row, DUPLEX_SEPARATOR);
    screen_print(DUPLEX_LABEL_FREQ_COL + 4u, DUPLEX_ROW_FREQ, ":");
    screen_print(21, DUPLEX_ROW_POWER, "POWER:");
    screen_print(DUPLEX_LABEL_FREQ_COL, DUPLEX_ROW_MOD_FREQ, "FREQ:");
    screen_print(21, DUPLEX_ROW_MOD_LEVEL, "LEVEL:");
    screen_print(20, DUPLEX_ROW_FILTER, "FILTER:");
    screen_print(DUPLEX_LABEL_AF_COL + 6u, DUPLEX_ROW_AF_FREQ, ":");

    screen_put(DUPLEX_PLUS_COL, BAR_SIGN_ROW, '+');
    screen_put(DUPLEX_MINUS_COL, BAR_SIGN_ROW, '-');
    screen_print(DUPLEX_POWER_TITLE_COL, BAR_TITLE_ROW, "POWER");
    bargraph_init(&power_bar, DUPLEX_POWER_BAR_COL, DUPLEX_POWER_BAR_COL + 1, DUPLEX_POWER_SCALE_COL,
                  DUPLEX_POWER_UNIT_COL, BAR_SCALE_30, 13u, 0);
    duplex_draw_mod_bars();
    draw_peak_values_at(DUPLEX_PEAK_LEFT_FIRST_COL, DUPLEX_PEAK_LEFT_LAST_COL, DUPLEX_PEAK_RIGHT_COL);
}

static void duplex_draw_counter(void)
{
    char text[20];
    uint32_t tenths = round_scaled(afcounter_hz(), 10.0);

    if (tenths < 10000u)
        put_text(put_fixed(text, tenths, 1), "Hz");
    else
        put_text(put_fixed(text, round_scaled(afcounter_hz(), 1.0), 3), "kHz");
    print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_MOD_FREQ, text, DUPLEX_VALUE_WIDTH);
    readings_set(READING_MOD_FREQ, text);
}

void tx_duplex_set_modulation(uint8_t am)
{
    mod_kind kind = am ? MOD_AM : MOD_FM;

    if (kind == tx.mod)
        return;
    tx.mod = kind;
    meter_set_filter_extra(0x03u, kind == MOD_FM ? DUPLEX_FILTER_FM : DUPLEX_FILTER_AM);
    duplex_draw_mod_bars();
}

static void duplex_apply_af(void)
{
    static const uint8_t range_bits[3] = {0x10u, 0x20u, 0x30u};
    double volts = tx.af_level / 10000.0;
    uint8_t range = volts > 0.6375 ? 0u : (volts > 0.255 ? 1u : 2u);
    double multiplier = range == 0 ? 100.0 : (range == 1 ? 400.0 : 1000.0);

    hal_write(REG_AF_LEVEL, 0);
    hal_write(REG_AF_RANGE_DAC, (uint8_t)(volts * multiplier + 0.5));
    synth_set_af_range(range_bits[range]);
}

static void duplex_go_transmitter(void)
{
    duplex_on_transmitter = 1;
    duplex_passes = 0;
    meter_duplex_measure_transmitter(tx.pending_peak_range);
    tx.step = STEP_AUX;
    hal_write(ADC_AUX, 0);
}

static void duplex_go_receiver(void)
{
    duplex_on_transmitter = 0;
    duplex_passes = 0;
    meter_duplex_measure_receiver();
}

void tx_duplex_begin(uint8_t mod_kind_value)
{
    if (!duplex_running) {
        duplex_running = 1;
        saved_manual = tx.manual;
        saved_tuned_hz = tx.tuned_hz;
        memcpy(saved_manual_text, tx.manual_text, sizeof saved_manual_text);
        tx.manual = 0;
    }
    tx.mod = mod_kind_value == 1u ? MOD_AM : MOD_FM;
    tx.dist_on = 0;
    tx.scope_shown = 0;
    tx.sel = 1;
    tx.peak_pos = tx.peak_neg = 0;
    tx.peak_range = tx.pending_peak_range = MOD_RANGE_TOP;
    tx.power_adc = 0;
    tx.power_range = tx.pending_power_range = POWER_RANGE_DEFAULT;
    tx.mod_filter.primed = 0;
    tx.new_readings = 0;
    tx.have_measurement = 0;
    tx.switching_done = 0;
    meter_prepare_transmit(1);
    meter_set_scope_gain_following(0);
    meter_set_filter_extra(0x03u, tx.mod == MOD_FM ? DUPLEX_FILTER_FM : DUPLEX_FILTER_AM);
    duplex_apply_af();
    duplex_draw_static();
    duplex_draw_counter();
    duplex_go_receiver();
}

void tx_selftest_begin(void)
{
    tx.selftest = 1;
    tx_duplex_begin(2u);
    tx_duplex_set_one_port(1);
}

void tx_selftest_end(void)
{
    tx.selftest = 0;
    tx_duplex_end();
}

void tx_selftest_demod(uint8_t kind)
{
    static const mod_kind kinds[] = {MOD_FM, MOD_AM, MOD_PM};
    static const uint8_t filters[] = {0x02u, 0x00u, 0x01u};

    tx.mod = kinds[kind];
    tx.mod_filter.primed = 0;
    meter_set_filter_extra(0x03u, filters[kind]);
}

double tx_selftest_power_mw(void)
{
    return tx.power_mw;
}

double tx_selftest_mod_level(void)
{
    return tx.mod_level;
}

void tx_duplex_set_one_port(uint8_t one_port)
{
    tx.duplex_one_port = one_port;
}

void tx_duplex_end(void)
{
    tx.duplex_one_port = 0;
    duplex_running = 0;
    tx.manual = saved_manual;
    memcpy(tx.manual_text, saved_manual_text, sizeof saved_manual_text);
    tx.tuned_hz = saved_tuned_hz;
    if (tx.manual)
        tune_receiver(tx.tuned_hz);
    meter_set_scope_gain_following(1);
}

static void duplex_draw_readings(void)
{
    char text[20];
    const char *unit = format_power(text, tx.power_mw);

    print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_POWER, text, DUPLEX_VALUE_WIDTH);
    readings_set(READING_POWER, text);
    format_mod_level(text);
    print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_MOD_LEVEL, text, DUPLEX_VALUE_WIDTH);
    readings_set(READING_MOD_LEVEL, text);
    bargraph_update(&power_bar, tx.power_mw, unit);
    bargraph_update(&mod_bar, tx.mod_peak_pos, tx.mod == MOD_FM ? "kHz" : (tx.mod == MOD_AM ? "%  " : "RAD"));
    bargraph_update_mirrored(&mod_neg_bar, &mod_bar, tx.mod_peak_neg);
    draw_peak_values_at(DUPLEX_PEAK_LEFT_FIRST_COL, DUPLEX_PEAK_LEFT_LAST_COL, DUPLEX_PEAK_RIGHT_COL);
}

void tx_duplex_draw_held_readings(void)
{
    duplex_draw_readings();
    duplex_draw_counter();
}

uint8_t tx_duplex_poll(uint8_t update_display)
{
    uint8_t receiver_results = 0;
    double hz;

    if (afcounter_poll() && update_display)
        duplex_draw_counter();
    if (rfcounter_poll(&hz)) {
        tx.measured_hz = hz;
        tx.have_measurement = 1;
        if (!tx.manual) {
            double error = hz - tx.tuned_hz;

            if (hz > TUNE_MIN_HZ && (error < 0 ? -error : error) > RETUNE_HZ) {
                if (--tx.retune_left == 0) {
                    tune_receiver(hz);
                    tx.retune_left = RETUNE_READINGS;
                }
            } else {
                tx.retune_left = RETUNE_READINGS;
            }
            if (update_display) {
                char text[20];

                duplex_format_counter(text, hz);
                print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_FREQ, text, DUPLEX_VALUE_WIDTH);
                readings_set(READING_RF_COUNTER, text);
            }
        }
    }

    if (duplex_on_transmitter) {
        uint8_t before = tx.step;

        measurement_step();
        if (tx.new_readings && update_display)
            duplex_draw_readings();
        tx.new_readings = 0;
        if (before != STEP_PEAK_POS && tx.step == STEP_PEAK_POS && ++duplex_passes == DUPLEX_CYCLES_PER_SIDE)
            duplex_go_receiver();
    } else {
        receiver_results = meter_poll();
        if (receiver_results && ++duplex_passes == DUPLEX_CYCLES_PER_SIDE)
            duplex_go_transmitter();
    }
    return receiver_results;
}

void tx_duplex_draw(uint8_t one_port, uint8_t filter, uint8_t af_selected, uint8_t freq_selected, uint8_t field,
                    const char *entry, uint8_t entry_len)
{
    static const uint8_t code_300[] = {6, 3, 3};
    char text[20];
    uint8_t typing_freq = freq_selected && entry_len;
    uint8_t level_selected = af_selected && field == FIELD_LEVEL;

    for (uint8_t col = DUPLEX_LABEL_FREQ_COL; col < DUPLEX_LABEL_FREQ_COL + 4u; col++)
        screen_put(col, DUPLEX_ROW_FREQ - 1u, ' ');
    if (freq_selected)
        screen_print_reverse(DUPLEX_LABEL_FREQ_COL, DUPLEX_ROW_FREQ, "FREQ");
    else
        screen_print(DUPLEX_LABEL_FREQ_COL, DUPLEX_ROW_FREQ, "FREQ");

    if (typing_freq) {
        print_field(DUPLEX_FREQ_COL, DUPLEX_ROW_FREQ, entry, DUPLEX_VALUE_WIDTH);
    } else if (tx.manual) {
        print_field(DUPLEX_FREQ_COL, DUPLEX_ROW_FREQ, tx.manual_text, DUPLEX_VALUE_WIDTH);
    } else if (tx.have_measurement) {
        duplex_format_counter(text, tx.measured_hz);
        print_field(DUPLEX_FREQ_COL, DUPLEX_ROW_FREQ, "", DUPLEX_VALUE_WIDTH);
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_FREQ, text, DUPLEX_VALUE_WIDTH);
    } else {
        print_field(DUPLEX_FREQ_COL, DUPLEX_ROW_FREQ, "", DUPLEX_VALUE_WIDTH);
    }

    if (filter == AF_FILTER_BAND_PASS) {
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_FILTER, "0.3-3.4kHz", DUPLEX_VALUE_WIDTH);
        readings_set(READING_FILTER, "0.3-3.4kHz");
    } else if (filter == AF_FILTER_LOW_PASS_50K) {
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_FILTER, "15kHz LP", DUPLEX_VALUE_WIDTH);
        readings_set(READING_FILTER, "15kHz LP");
    } else {
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_FILTER, "   Hz LP", DUPLEX_VALUE_WIDTH);
        readings_set(READING_FILTER, "300Hz LP");
        for (uint8_t i = 0; i < sizeof code_300; i++)
            screen_put((uint8_t)(DUPLEX_VALUE_COL + i), DUPLEX_ROW_FILTER, code_300[i]);
    }

    for (uint8_t col = DUPLEX_LABEL_AF_COL; col < DUPLEX_LABEL_AF_COL + 6u; col++)
        screen_put(col, DUPLEX_ROW_AF_FREQ - 1u, ' ');
    if (af_selected) {
        screen_print_reverse(DUPLEX_LABEL_AF_COL, DUPLEX_ROW_AF_FREQ, "AF");
        screen_print_reverse(DUPLEX_LABEL_AF_COL + 3u, DUPLEX_ROW_AF_FREQ, "GEN");
    } else {
        screen_print(DUPLEX_LABEL_AF_COL, DUPLEX_ROW_AF_FREQ, "AF GEN");
    }
    format_af_freq(text);
    print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_AF_FREQ, text, DUPLEX_VALUE_WIDTH);
    for (uint8_t col = DUPLEX_VALUE_COL; col < DUPLEX_VALUE_COL + 7u; col++)
        screen_put(col, DUPLEX_ROW_AF_LEVEL - 1u, ' ');
    if (level_selected && entry_len) {
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_AF_LEVEL, entry, DUPLEX_VALUE_WIDTH);
    } else {
        format_shown_af_level(text, tx.af_level);
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_AF_LEVEL, "", DUPLEX_VALUE_WIDTH);
        for (uint8_t i = 0; text[i]; i++) {
            char single[2] = {text[i], 0};

            if (level_selected && ((text[i] >= '0' && text[i] <= '9') || text[i] == '.' || text[i] == '-'))
                screen_print_reverse((uint8_t)(DUPLEX_VALUE_COL + i), DUPLEX_ROW_AF_LEVEL, single);
            else
                screen_print((uint8_t)(DUPLEX_VALUE_COL + i), DUPLEX_ROW_AF_LEVEL, single);
        }
        readings_set(READING_AF_GEN_LEVEL, text);
    }

    if (one_port) {
        screen_print(DUPLEX_BNC_COL, 1, "   ");
        screen_print(DUPLEX_BNC_COL, 0, "   ");
        screen_print(DUPLEX_N_COL, 1, " ");
        screen_print(DUPLEX_N_COL, 0, " ");
    } else {
        screen_print_reverse(DUPLEX_BNC_COL, 1, "BNC");
        screen_print_reverse(DUPLEX_N_COL, 1, "N");
    }
    screen_print_reverse(DUPLEX_PORT_COL, DUPLEX_ROW_AF_LEVEL, one_port ? "ONE PORT" : "TWO PORT");
    screen_put(DUPLEX_SEPARATOR_COL, DUPLEX_ROW_AF_LEVEL - 1u, DUPLEX_SEPARATOR);
}

uint8_t tx_duplex_set_frequency(const char *entry, uint8_t entry_len, int32_t scale, const char *unit)
{
    int32_t value;

    if (!scale || !parse_entry(entry, entry_len, scale, &value))
        return 0;
    double hz = value * 10.0;
    if (hz < TUNE_MIN_HZ || hz > TUNE_MAX_HZ)
        return 0;
    put_text(put_text(tx.manual_text, entry), unit);
    tx.manual = 1;
    tune_receiver(hz);
    return 1;
}

uint8_t tx_duplex_set_af_level(const char *entry, uint8_t entry_len, uint8_t unit_key)
{
    int32_t value;
    int32_t scale = unit_key == KEY_MHZ ? 10000L : (unit_key == KEY_KHZ ? 10L : 0L);

    if (!scale || !parse_entry(entry, entry_len, scale, &value))
        return 0;
    if (param_get(PARAM_AF_ATTENUATOR))
        value *= ATTENUATOR_FACTOR;
    if (value < 0 || value > AF_LEVEL_MAX_MV * 10L)
        return 0;
    tx.af_level = value;
    duplex_apply_af();
    return 1;
}

void tx_duplex_set_af_level_value(int32_t tenths_mv)
{
    if (tenths_mv >= 0 && tenths_mv <= AF_LEVEL_MAX_MV * 10L)
        tx.af_level = tenths_mv;
}

int32_t tx_duplex_af_level(void)
{
    return tx.af_level;
}

uint8_t tx_duplex_knob_af_level(int8_t direction)
{
    if (!step_af_level_dac(direction))
        return 0;
    duplex_apply_af();
    return 1;
}

double tx_measured_hz(void)
{
    return tx.have_measurement ? tx.measured_hz : 0.0;
}
