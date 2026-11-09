#include "app.h"

#include <math.h>
#include <string.h>

#include "afcounter.h"
#include "bargraph.h"
#include "control.h"
#include "format.h"
#include "gpib.h"
#include "help.h"
#include "hal.h"
#include "keys.h"
#include "params.h"
#include "level.h"
#include "meter.h"
#include "modulation.h"
#include "overload.h"
#include "scope.h"
#include "settings.h"
#include "screen.h"
#include "selftest.h"
#include "readings.h"
#include "rfcounter.h"
#include "synth.h"
#include "tones.h"
#include "tx.h"

#define VALUE_COL 11
#define VALUE_WIDTH 13
#define INC_COL 24
#define INC_VALUE_COL 28
#define INC_VALUE_WIDTH 12
#define OFF_COL 20
#define AF_VALUE_WIDTH 12
#define SINAD_VALUE_COL 11
#define SINAD_VALUE_WIDTH 9
#define AF_FREQ_LABEL_COL 23
#define AF_FREQ_VALUE_COL 29
#define AF_FREQ_VALUE_WIDTH 11
#define AF_FREQ_HZ_LIMIT 10000u /* tenths of a hertz: from here the reading is shown in kHz */
#define FILTER_LABEL_COL 21
#define FILTER_VALUE_COL 28
#define FILTER_VALUE_WIDTH 10
#define COUPLING_COL 29
#define COUPLING_ROW 15
#define SECOND_READING_ROW 13
#define SECOND_LABEL_COL 4
#define SECOND_LABEL_LAST_COL 9
#define SECOND_BAR_FIRST_COL 3
#define SECOND_BAR_LAST_COL 10
#define BAR_FIRST_ROW 17
#define BAR_LAST_ROW 31
#define LOWER_AREA_FIRST_ROW 15
#define LOWER_AREA_LAST_ROW 31
#define HOLD_TEXT_COL 36
#define HOLD_TEXT_ROW 22
#define HOLD_OFF_ROW 23
#define HOLD_BOX_FIRST_ROW 24
#define HOLD_BOX_LAST_ROW 26
#define PROMPT_COL 0
#define PROMPT_ROW 0
#define PROMPT_DIGITS_COL_STORE 5
#define PROMPT_DIGITS_COL_RECALL 6
#define PROMPT_CLEAR_WIDTH 10
#define ENTRY_SIZE 13
#define UNIT_SIZE 4

#define RF_FREQ_MIN 40000L       /* 0.4 MHz, in 10 Hz units */
#define RF_FREQ_MAX 100000000L   /* 1000 MHz */
#define RF_FREQ_FINE_LIMIT 53000000L /* 530 MHz: the knob step doubles from here */

/* Level limits in 0.1 dB units, depending on the selected RF socket. */
#define DUPLEX_LEVEL_MIN_TWO_PORT (-1200)
#define DUPLEX_LEVEL_MAX_TWO_PORT (-400)
#define DUPLEX_LEVEL_MIN_ONE_PORT (-1400)
#define DUPLEX_LEVEL_MAX_ONE_PORT (-800)
#define AUDIO_VALUE_COL 13u
#define AUDIO_SECOND_LABEL_COL 6u
#define AUDIO_TITLE_COL 15u
#define AUDIO_VOLTS_LABEL_COL 3u
#define AUDIO_AF_FREQ_LABEL_COL 24u
#define AUDIO_FILTER_ROW 8u
#define SAT_FREQ_DEFAULT 1500u  /* 150 Hz in 0.1 Hz */
#define SAT_LEVEL_DEFAULT 5000u /* 500 Hz of deviation in 0.1 Hz */
#define MOD_MARK_COL 20u
#define MOD_MARK_ROW 6u
#define DUPLEX_MOD_MARK_COL 16u
#define DUPLEX_MOD_MARK_ROW 7u
#define MOD_MARK_WIDTH 3u
#define EXT_MARK_COL 0u
#define EXT_MARK_ROW 4u
#define EXT_MARK_WIDTH 3u
#define DUPLEX_EXT_MARK_COL_RX 1u
#define DUPLEX_EXT_MARK_COL_TX 24u
#define DUPLEX_EXT_MARK_ROW 16u
#define RF_ATTENUATION_MAX 1200 /* 0.1 dB: the generator's own range below 0 dBm */
#define RF_LEVEL_INC_MAX 1200
#define VOLTS_AT_0DBM 0.2236068 /* potential difference at 0 dBm into 50 ohms */
#define EMF_DB 6.0206           /* an EMF is twice the potential difference */
#define VOLTS_MV_LIMIT 0.3 /* below this the reading is shown in mV */
#define LEVEL_TYPE_COL 20
#define DUPLEX_INPUT_TWO_PORT 1u
#define DUPLEX_INPUT_ONE_PORT 2u
#define DUPLEX_VALUE_COL 7u
#define DUPLEX_COLON_COL 5u
#define DUPLEX_HEADING_LAST_UNDERLINE_COL 4u
#define DUPLEX_VALUE_WIDTH 12u
#define DUPLEX_ROW_FREQ 3u
#define DUPLEX_ROW_LEVEL 5u
#define DUPLEX_ROW_MOD_FREQ 7u
#define DUPLEX_ROW_MOD_LEVEL 9u
#define DUPLEX_ROW_AUDIO 11u
#define DUPLEX_OFF_COL 16u
#define DUPLEX_AC_COL 13u
#define DUPLEX_AC_ROW 15u
#define DUPLEX_FIXED_MOD_FREQ 10000u
#define PRINT_BOX_POSITION 7u
#define PRINT_BOX_WIDTH 3u
#define PRINT_BOX_FIRST_COL 34u
#define PRINT_TEXT_COL 35u
#define PRINT_TEXT_ROW 29u
#define NOVRAM_START 0xEC00u          /* the settings survive here while the power is off */
#define CONTROL_NOVRAM_RECALL 0x04u   /* a low pulse copies the NOVRAM's stored copy into its RAM */
#define SLOT_LAST_SESSION 0u
#define VOLTAGE_DIGITS_LIMIT 10000u /* four significant digits */
#define RF_INC_MHZ_FROM 100000L /* 1 MHz, in 10 Hz units */

/* Modulation limits and steps, frequencies and deviation in 0.1 Hz. */
#define MOD_FREQ_MIN 200L
#define MOD_FREQ_MAX 200000L
#define MOD_FREQ_FINE_LIMIT 32768L /* the knob step is 1 Hz from here */
#define DEV_MAX_EXCLUSIVE 255000L  /* 25.5 kHz */
#define DEV_COARSE_ABOVE 63750L    /* 6.375 kHz */
#define DEV_STEP_FINE 250L         /* 25 Hz */
#define DEV_STEP_COARSE 1000L      /* 100 Hz */
#define DEV_MAX_VALUE 254000L      /* the largest value accepted after rounding */
#define AM_MAX_EXCLUSIVE 995L      /* 99.5 %, in 0.1 % */
#define AM_MAX_PERCENT 99

/* Bar charts of the receiver test screen. */
#define BAR_TITLE_ROW 17
#define SINAD_BAR_TITLE_COL 4
#define SINAD_BAR_COL 6
#define SINAD_BAR_SCALE_COL 3
#define SINAD_BAR_UNIT_COL 8
#define VOLTS_BAR_TITLE_COL 26
#define VOLTS_BAR_COL 29
#define VOLTS_BAR_SCALE_COL 26
#define VOLTS_BAR_UNIT_COL 31
typedef struct {
    uint8_t value_col, second_label_col, second_bar_first_col, second_bar_last_col;
    uint8_t sinad_title_col, sinad_bar_col, sinad_scale_col, sinad_unit_col;
    uint8_t volts_title_col, volts_bar_col, volts_scale_col, volts_unit_col;
} rx_layout;

static const rx_layout receiver_layout = {
    VALUE_COL, SECOND_LABEL_COL, SECOND_BAR_FIRST_COL, SECOND_BAR_LAST_COL,
    SINAD_BAR_TITLE_COL, SINAD_BAR_COL, SINAD_BAR_SCALE_COL, SINAD_BAR_UNIT_COL,
    VOLTS_BAR_TITLE_COL, VOLTS_BAR_COL, VOLTS_BAR_SCALE_COL, VOLTS_BAR_UNIT_COL,
};
static const rx_layout duplex_layout = {7, 0, 1, 8, 2, 4, 1, 6, 10, 13, 10, 15};
static const rx_layout audio_layout = {
    AUDIO_VALUE_COL, AUDIO_SECOND_LABEL_COL, SECOND_BAR_FIRST_COL, SECOND_BAR_LAST_COL,
    SINAD_BAR_TITLE_COL, SINAD_BAR_COL, SINAD_BAR_SCALE_COL, SINAD_BAR_UNIT_COL,
    VOLTS_BAR_TITLE_COL, VOLTS_BAR_COL, VOLTS_BAR_SCALE_COL, VOLTS_BAR_UNIT_COL,
};
static const rx_layout *layout = &receiver_layout;

#define VOLTS_BAR_INITIAL_SCALE 10 /* above the highest scale, so the first reading draws the labels */

typedef enum { FIELD_NONE, FIELD_FREQ, FIELD_LEVEL } field_t;
typedef enum { GROUP_NONE, GROUP_RF, GROUP_MOD, GROUP_AF, GROUP_TX } group_t;

typedef struct {
    int32_t rf_freq;      /* 10 Hz units */
    int16_t rf_level;     /* 0.1 dBm */
    int32_t rf_freq_inc;  /* 10 Hz units, 0 = not set */
    char rf_freq_inc_text[ENTRY_SIZE + UNIT_SIZE];
    int16_t rf_level_inc; /* 0.1 dB, 0 = not set */
    uint8_t level_unit;   /* LEVEL_UNIT_DBM, or the voltage unit it was entered in */
    double level_volts;   /* voltage as entered */
    int16_t level_offset; /* 0.1 dB, from the knob and the step keys, on top of level_volts */
    int32_t mod_freq_inc; /* 0.1 Hz, 0 = not set */
    char mod_freq_inc_text[ENTRY_SIZE + UNIT_SIZE];
    int32_t mod_level_inc; /* 0.1 Hz of deviation or percent, 0 = not set */
    uint8_t mod_level_inc_am;
    uint8_t settings_changed;
    uint8_t duplex;
    uint8_t one_port;
    uint8_t bnc_selected;

    modulation_state mod;
    char mod_freq_text[ENTRY_SIZE + UNIT_SIZE]; /* as typed, while the field stays selected */

    group_t group;
    field_t field;
    uint8_t entering_inc;
    char entry[ENTRY_SIZE];
    uint8_t entry_len;

    af_filter filter;
    uint8_t ac_coupled;
    uint8_t scope_shown;
    uint8_t held;
    uint8_t audio;
    measure_mode mode_before_audio;
    uint8_t transmit;

    uint8_t prompt;       /* 0 none, otherwise the key that asked for a slot number */
    uint8_t prompt_digits;
    uint8_t prompt_cursor, prompt_cursor_col;
    uint8_t prompt_value;
} app_state;

static app_state app;
/* DUPLEX keeps settings of its own, and the receiver test finds its own again afterwards. */
static app_state duplex_state, receiver_state;

/* What was in the NOVRAM at power-on, for RECALL 00; survives a GPIB device clear. */
static uint8_t last_session[SETTINGS_IMAGE_SIZE];

static bargraph sinad_bar, volts_bar;

static void entry_clear(void)
{
    app.entry_len = 0;
    app.entry[0] = 0;
}

static void gate_modulation(uint8_t enable);
static void output_rf_level(void);
static uint8_t entry_shown_in(group_t group, field_t field);
static void draw_second_label(void);
static void redraw_duplex(void);
static void draw_af_frequency(void);

static void clear_area(uint8_t first_col, uint8_t last_col, uint8_t first_row, uint8_t last_row)
{
    for (uint8_t row = first_row; row <= last_row; row++)
        for (uint8_t col = first_col; col <= last_col; col++)
            screen_put(col, row, ' ');
}

static void init_sinad_bar(const char *title, uint8_t first_scale, uint8_t last_scale)
{
    screen_print(layout->sinad_title_col, BAR_TITLE_ROW, title);
    bargraph_init(&sinad_bar, layout->sinad_bar_col, (uint8_t)(layout->sinad_bar_col + 1u), layout->sinad_scale_col,
                  layout->sinad_unit_col, first_scale, last_scale, 0);
}

static void draw_second_bar(void)
{
    clear_area(layout->second_bar_first_col, layout->second_bar_last_col, BAR_FIRST_ROW, BAR_LAST_ROW);

    switch (meter_mode()) {
    case MEASURE_SINAD:
        init_sinad_bar("SINAD", BAR_SCALE_18, BAR_SCALE_50);
        break;
    case MEASURE_SN:
        init_sinad_bar("  S/N", BAR_SCALE_30, BAR_SCALE_100);
        break;
    case MEASURE_DISTORTION:
        init_sinad_bar("DISTN", BAR_SCALE_10, BAR_SCALE_30);
        break;
    case MEASURE_DBV:
        break;
    }
}

static void draw_bar_charts(void)
{
    draw_second_bar();
    screen_print(layout->volts_title_col, BAR_TITLE_ROW, "AF VOLTS");
    bargraph_init(&volts_bar, layout->volts_bar_col, (uint8_t)(layout->volts_bar_col + 1u), layout->volts_scale_col,
                  layout->volts_unit_col, BAR_SCALE_100M, BAR_SCALE_100, VOLTS_BAR_INITIAL_SCALE);
}

void app_init(void)
{
    app.rf_freq = 30000000L;
    app.rf_level = -1000;
    app.rf_freq_inc = 0;
    app.rf_freq_inc_text[0] = 0;
    app.rf_level_inc = 0;
    app.level_unit = LEVEL_UNIT_DBM;
    app.level_volts = 0.0;
    app.level_offset = 0;
    app.mod_freq_inc = 0;
    app.mod_freq_inc_text[0] = 0;
    app.mod_level_inc = 0;
    app.mod_level_inc_am = 0;
    app.settings_changed = 1;
    app.duplex = 0;
    app.one_port = 0;
    layout = &receiver_layout;
    level_load_external_attenuators();
    tones_init();
    app.mod.sat_on = 0;
    app.mod.sat_freq = SAT_FREQ_DEFAULT;
    app.mod.sat_level = SAT_LEVEL_DEFAULT;
    app.bnc_selected = 0;
    app.mod.freq = 10000;
    app.mod.deviation = 15000;
    app.mod.am_percent = 0;
    app.mod.am = 0;
    app.mod.off = 0;
    app.mod_freq_text[0] = 0;
    app.group = GROUP_NONE;
    app.field = FIELD_NONE;
    app.entering_inc = 0;
    app.filter = AF_FILTER_BAND_PASS;
    app.ac_coupled = 1;
    app.scope_shown = 0;
    app.held = 0;
    app.transmit = 0;
    app.prompt = 0;
    entry_clear();
    screen_clear();
    readings_clear();
    output_rf_level();
    synth_set_frequency(app.rf_freq);
    modulation_apply(&app.mod);
    meter_init();
    meter_set_mode((param_get(PARAM_SN_DEFAULT) & 1u) ? MEASURE_SN : MEASURE_SINAD);
    meter_set_mod_gate(gate_modulation);
    rfcounter_init();
    afcounter_init();
    tx_init();
    scope_init();
    draw_bar_charts();
    app_redraw();
    draw_af_frequency();
    tx_publish_settings();
    gpib_init();
    duplex_state = app;
    duplex_state.bnc_selected = 1;
}

static void draw_readings(void);

/* Switches the modulation for the S/N measurement without changing the user's setting. */
static void gate_modulation(uint8_t enable)
{
    modulation_state state = app.mod;

    state.off = (uint8_t)(app.mod.off || !enable);
    modulation_apply(&state);
}
static void draw_af_frequency(void);
static void draw_mod_mark(void);

static void capture_settings(instrument_settings *s);

/* The NOVRAM keeps a copy over a power failure without the firmware's help. */
static void save_live_settings(void)
{
    instrument_settings settings;
    uint8_t image[SETTINGS_IMAGE_SIZE];

    capture_settings(&settings);
    settings_to_image(&settings, image);
    for (uint8_t i = 0; i < SETTINGS_IMAGE_SIZE; i++)
        hal_write((uint16_t)(NOVRAM_START + i), image[i]);
    app.settings_changed = 0;
}

void app_poll(void)
{
    if (overload_poll())
        return;
    gpib_poll();
    tones_poll();
    if (help_active()) {
        selftest_poll();
        return;
    }
    if (tones_active())
        return;
    if (app.settings_changed)
        save_live_settings();
    if (app.duplex) {
        if (tx_duplex_poll(!app.held) && !app.held)
            draw_readings();
        return;
    }
    if (app.transmit) {
        tx_poll(!app.held);
        return;
    }

    uint8_t new_readings = meter_poll();
    uint8_t new_frequency = afcounter_poll();

    if (app.held)
        return;
    if (new_readings)
        draw_readings();
    if (new_frequency)
        draw_af_frequency();
}

static void format_rf_value(char *out, int32_t value)
{
    if (value >= RF_FREQ_MAX) {
        out = put_fixed(out, (uint32_t)(value / 10), 4);
    } else if (value < 100000L) {
        *out++ = '.';
        out = put_fixed(out, (uint32_t)value, 0);
    } else {
        out = put_fixed(out, (uint32_t)value, 5);
    }
    put_text(out, "MHz");
}

static void format_rf_freq(char *out)
{
    format_rf_value(out, app.rf_freq);
}

static int16_t level_min(void)
{
    int16_t external = level_external_attenuator(app.bnc_selected);

    if (app.duplex)
        return (int16_t)((app.one_port ? DUPLEX_LEVEL_MIN_ONE_PORT : DUPLEX_LEVEL_MIN_TWO_PORT) - external);
    return (int16_t)(-RF_ATTENUATION_MAX - level_socket_loss(app.bnc_selected));
}

static int16_t level_max(void)
{
    int16_t external = level_external_attenuator(app.bnc_selected);

    if (app.duplex)
        return (int16_t)((app.one_port ? DUPLEX_LEVEL_MAX_ONE_PORT : DUPLEX_LEVEL_MAX_TWO_PORT) - external);
    return (int16_t)-level_socket_loss(app.bnc_selected);
}

static void clear_freq_incs(void);
static void clear_level_incs(void);

static int32_t tenths_from_volts(double volts, int16_t offset)
{
    double dbm = 20.0 * log10(volts / VOLTS_AT_0DBM);

    if (hal_read(PARAM_LEVEL_EMF))
        dbm -= EMF_DB;
    return (int32_t)floor(dbm * 10.0 + 0.5) + offset;
}

static uint8_t level_is_voltage(void)
{
    return app.level_unit != LEVEL_UNIT_DBM;
}

static uint8_t level_in_range(int32_t tenths)
{
    return tenths >= level_min() && tenths <= level_max();
}

static void report_level_rejected(int32_t tenths)
{
    gpib_report_entry_error(tenths > level_max() ? ENTRY_ERROR_LEVEL_HIGH : ENTRY_ERROR_LEVEL_LOW);
}

static void apply_level(int32_t tenths)
{
    app.rf_level = (int16_t)tenths;
    output_rf_level();
}

/* Four significant digits. The choices are made on the rounded value, so 999.96 uV shows as 1.000mV. */
static void format_voltage(char *out, double volts)
{
    const char *unit = "uV";
    double value = volts * 1000000.0;

    if (round_scaled(value, 10.0) >= VOLTAGE_DIGITS_LIMIT) {
        unit = "mV";
        value = volts * 1000.0;
        if (round_scaled(value, 10.0) >= VOLTAGE_DIGITS_LIMIT) {
            unit = "V";
            value = volts;
        }
    }
    if (round_scaled(value, 1000.0) < VOLTAGE_DIGITS_LIMIT)
        out = put_fixed(out, round_scaled(value, 1000.0), 3);
    else if (round_scaled(value, 100.0) < VOLTAGE_DIGITS_LIMIT)
        out = put_fixed(out, round_scaled(value, 100.0), 2);
    else
        out = put_fixed(out, round_scaled(value, 10.0), 1);
    put_text(out, unit);
}

static void format_rf_level(char *out)
{
    if (level_is_voltage()) {
        format_voltage(out, app.level_volts * pow(10.0, app.level_offset / 200.0));
        return;
    }
    /* Levels in dBm are never positive. */
    if (app.rf_level < 0)
        *out++ = '-';
    out = put_fixed(out, (uint32_t)(-app.rf_level), 1);
    put_text(out, "dBm");
}

static void format_mod_freq(char *out)
{
    uint32_t f = app.mod.freq;

    if (f < 10000u)
        put_text(put_fixed(out, f, 1), "Hz");
    else if (f < (uint32_t)MOD_FREQ_FINE_LIMIT)
        put_text(put_fixed(out, f, 4), "kHz");
    else
        put_text(put_fixed(out, (f + 5u) / 10u, 3), "kHz");
}

static void format_deviation(char *out, uint32_t deviation)
{
    if (deviation < 10000u)
        put_text(put_fixed(out, deviation / 10u, 0), "Hz");
    else if (deviation < 100000u)
        put_text(put_fixed(out, deviation / 10u, 3), "kHz");
    else
        put_text(put_fixed(out, deviation / 100u, 2), "kHz");
}

static void format_mod_inc(char *out)
{
    if (app.mod_level_inc_am)
        put_text(put_fixed(out, (uint32_t)app.mod_level_inc, 0), "%");
    else
        format_deviation(out, (uint32_t)app.mod_level_inc);
}

static void format_mod_level(char *out)
{
    if (app.mod.am) {
        put_text(put_fixed(out, app.mod.am_percent, 0), "%");
    } else {
        format_deviation(out, app.mod.deviation);
    }
}

static void clear_range(uint8_t row, uint8_t first, uint8_t last)
{
    for (uint8_t col = first; col <= last; col++)
        screen_put(col, row, ' ');
}

static void print_word(uint8_t col, uint8_t row, const char *word, uint8_t reverse)
{
    if (reverse)
        screen_print_reverse(col, row, word);
    else
        screen_print(col, row, word);
}

static void draw_labels(void)
{
    uint8_t rf = app.group == GROUP_RF;
    uint8_t mod = app.group == GROUP_MOD;
    field_t field = app.prompt ? FIELD_NONE : app.field;

    /* The rows above the labels hold the top edge of reverse-field text. */
    clear_range(1, 1, 8);
    clear_range(3, 4, 8);
    clear_range(5, 1, 8);
    clear_range(7, 4, 8);
    clear_range(7, OFF_COL, OFF_COL + 2);

    print_word(1, 2, "GEN", rf);
    print_word(5, 2, "FREQ", rf && field == FIELD_FREQ);
    screen_print(9, 2, ":");
    print_word(4, 4, "LEVEL", rf && field == FIELD_LEVEL);
    screen_print(9, 4, ":");

    print_word(1, 6, "MOD", mod);
    print_word(5, 6, "FREQ", mod && field == FIELD_FREQ);
    screen_print(9, 6, ":");
    print_word(4, 8, "LEVEL", mod && field == FIELD_LEVEL);
    screen_print(9, 8, ":");
}

static uint8_t inc_entry_in(group_t group, field_t field)
{
    return app.entering_inc && !app.prompt && app.group == group && app.field == field;
}

/* The label is shown in reverse field while its increment is being entered. */
static void draw_inc_label(uint8_t row, group_t group, field_t field)
{
    clear_range((uint8_t)(row - 1u), INC_COL, INC_COL + 2);
    print_word(INC_COL, row, "INC", inc_entry_in(group, field));
    screen_print(INC_COL + 3, row, ":");
}

static uint8_t volts_in_millivolts(double volts)
{
    return volts < VOLTS_MV_LIMIT;
}

static void format_volts(char *out, double volts)
{
    if (volts_in_millivolts(volts))
        put_text(put_fixed(out, round_scaled(volts, 1000.0), 0), "mV");
    else if (volts < 10.0)
        put_text(put_fixed(out, round_scaled(volts, 1000.0), 3), "V");
    else if (volts < 100.0)
        put_text(put_fixed(out, round_scaled(volts, 100.0), 2), "V");
    else
        put_text(put_fixed(out, round_scaled(volts, 10.0), 1), "V");
}

static void draw_readings(void)
{
    char text[20];

    format_volts(text, meter_volts());
    print_field(layout->value_col, 11, text, app.audio ? (uint8_t)(AF_VALUE_WIDTH - 1u) : AF_VALUE_WIDTH);
    readings_set(READING_AF_LEVEL, text);
    if (!app.scope_shown)
        bargraph_update(&volts_bar, meter_volts(), volts_in_millivolts(meter_volts()) ? "mV " : "V  ");
    if (meter_second_valid()) {
        double value = meter_second_value();
        measure_mode mode = meter_mode();
        char *end = text;

        if (mode == MEASURE_DISTORTION) {
            put_text(put_fixed(text, round_scaled(value, 10.0), 1), "%");
        } else {
            uint32_t tenths = round_scaled(value < 0.0 ? -value : value, 10.0);

            if (value < 0.0 && tenths)
                *end++ = '-';
            put_text(put_fixed(end, tenths, 1),
                     mode != MEASURE_DBV ? "dB" : (meter_db_relative() ? "dBR" : (param_get(PARAM_AF_600_OHM) ? "dBm" : "dBV")));
        }
        print_field(layout->value_col, 13, text, SINAD_VALUE_WIDTH);
        readings_set(READING_RX_SECOND, mode == MEASURE_DBV ? "" : text);
        if (mode != MEASURE_DBV && !app.scope_shown)
            bargraph_update(&sinad_bar, value, mode == MEASURE_DISTORTION ? "%  " : "dB ");
    } else {
        print_field(layout->value_col, 13, "", SINAD_VALUE_WIDTH);
        readings_set(READING_RX_SECOND, "");
        if (meter_mode() != MEASURE_DBV && !app.scope_shown)
            bargraph_empty(&sinad_bar);
    }
}

/* The left half of the duplex screen; the transmitter side belongs to the transmitter test. */
static void redraw_duplex(void)
{
    char text[20];
    field_t field = app.prompt ? FIELD_NONE : app.field;
    uint8_t rf = app.group == GROUP_RF, mod = app.group == GROUP_MOD;
    static uint8_t cursor_col, cursor_row, cursor_shown;

    if (cursor_shown)
        screen_put(cursor_col, cursor_row, ' ');
    cursor_shown = 0;

    clear_range(DUPLEX_ROW_FREQ - 1, 1, 4);
    clear_range(DUPLEX_ROW_LEVEL - 1, 0, 4);
    clear_range(DUPLEX_ROW_MOD_LEVEL - 1, 0, 4);
    screen_put(DUPLEX_HEADING_LAST_UNDERLINE_COL, DUPLEX_ROW_FREQ - 1, SCREEN_UNDERLINE);
    print_word(1, DUPLEX_ROW_FREQ, "FREQ", rf && field == FIELD_FREQ);
    print_word(0, DUPLEX_ROW_LEVEL, "LEVEL", rf && field == FIELD_LEVEL);
    print_word(1, DUPLEX_ROW_MOD_FREQ, "FREQ", 0);
    print_word(0, DUPLEX_ROW_MOD_LEVEL, "LEVEL", mod && field == FIELD_LEVEL);
    for (uint8_t row = DUPLEX_ROW_FREQ; row <= DUPLEX_ROW_MOD_LEVEL; row = (uint8_t)(row + 2u))
        screen_put(DUPLEX_COLON_COL, row, ':');

    if (entry_shown_in(GROUP_RF, FIELD_FREQ)) {
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_FREQ, app.entry, DUPLEX_VALUE_WIDTH);
        cursor_row = DUPLEX_ROW_FREQ + 1;
        cursor_shown = 1;
    } else {
        format_rf_value(text, app.rf_freq);
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_FREQ, text, DUPLEX_VALUE_WIDTH);
        readings_set(READING_GEN_FREQ, text);
    }
    if (entry_shown_in(GROUP_RF, FIELD_LEVEL)) {
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_LEVEL, app.entry, DUPLEX_VALUE_WIDTH);
        cursor_row = DUPLEX_ROW_LEVEL + 1;
        cursor_shown = 1;
    } else {
        format_rf_level(text);
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_LEVEL, text, DUPLEX_VALUE_WIDTH);
        readings_set(READING_GEN_LEVEL, text);
    }
    format_mod_freq(text);
    print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_MOD_FREQ, text, DUPLEX_VALUE_WIDTH);
    readings_set(READING_MOD_SET_FREQ, text);
    if (entry_shown_in(GROUP_MOD, FIELD_LEVEL)) {
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_MOD_LEVEL, app.entry, DUPLEX_VALUE_WIDTH);
        cursor_row = DUPLEX_ROW_MOD_LEVEL + 1;
        cursor_shown = 1;
    } else {
        format_mod_level(text);
        print_field(DUPLEX_VALUE_COL, DUPLEX_ROW_MOD_LEVEL, text, DUPLEX_VALUE_WIDTH);
        readings_set(READING_MOD_SET_LEVEL, text);
    }
    clear_range(DUPLEX_ROW_MOD_LEVEL - 1, DUPLEX_OFF_COL, DUPLEX_OFF_COL + 2);
    if (app.mod.off)
        screen_print_reverse(DUPLEX_OFF_COL, DUPLEX_ROW_MOD_LEVEL, "OFF");
    else
        clear_range(DUPLEX_ROW_MOD_LEVEL, DUPLEX_OFF_COL, DUPLEX_OFF_COL + 2);

    screen_print(0, DUPLEX_ROW_AUDIO, "AUDIO:");
    draw_second_label();
    screen_print(DUPLEX_AC_COL, DUPLEX_AC_ROW, app.ac_coupled ? "AC" : "DC");

    tx_duplex_draw(app.one_port, app.filter, app.group == GROUP_AF, app.group == GROUP_TX,
                   app.group >= GROUP_AF ? (uint8_t)app.field : 0u, app.entry, app.entry_len);

    if (cursor_shown) {
        cursor_col = (uint8_t)(DUPLEX_VALUE_COL + app.entry_len);
        screen_put(cursor_col, cursor_row, SCREEN_CURSOR);
    }
    draw_mod_mark();
    app_draw_external_attenuator(DUPLEX_EXT_MARK_COL_RX, DUPLEX_EXT_MARK_ROW, app.bnc_selected);
    app_draw_external_attenuator(DUPLEX_EXT_MARK_COL_TX, DUPLEX_EXT_MARK_ROW, 0);
}

static void draw_af_frequency(void)
{
    char text[20];
    uint32_t tenths = round_scaled(afcounter_hz(), 10.0);

    if (tenths < AF_FREQ_HZ_LIMIT)
        put_text(put_fixed(text, tenths, 1), "Hz");
    else
        put_text(put_fixed(text, round_scaled(afcounter_hz(), 1.0), 3), "kHz");
    print_field(AF_FREQ_VALUE_COL, 11, text, AF_FREQ_VALUE_WIDTH);
    readings_set(READING_AF_FREQ, text);
    readings_set(READING_MOD_FREQ, text);
}

/* The 300 in the filter name uses the character set's small-size digits. */
static const uint8_t filter_300_text[] = {6, 3, 3};

static uint8_t filter_row(void)
{
    return app.audio ? AUDIO_FILTER_ROW : 13u;
}

static void draw_filter(void)
{
    if (app.filter == AF_FILTER_BAND_PASS) {
        print_field(FILTER_VALUE_COL, filter_row(), "0.3-3.4kHz", FILTER_VALUE_WIDTH);
        readings_set(READING_FILTER, "0.3-3.4kHz");
    } else if (app.filter == AF_FILTER_LOW_PASS_50K) {
        print_field(FILTER_VALUE_COL, filter_row(), "50kHz LP", FILTER_VALUE_WIDTH);
        readings_set(READING_FILTER, "50kHz LP");
    } else {
        print_field(FILTER_VALUE_COL, filter_row(), "   Hz LP", FILTER_VALUE_WIDTH);
        readings_set(READING_FILTER, "300Hz LP");
        for (uint8_t i = 0; i < sizeof filter_300_text; i++)
            screen_put((uint8_t)(FILTER_VALUE_COL + i), filter_row(), filter_300_text[i]);
    }
}

static void select_filter(af_filter filter)
{
    app.filter = filter;
    meter_set_filter(filter);
}

/* Selects the second reading; everything except dBV needs AC coupling. */
static void select_measure(measure_mode mode)
{
    meter_set_mode(mode);
    if (mode != MEASURE_DBV && !app.ac_coupled) {
        app.ac_coupled = 1;
        meter_set_ac_coupled(1);
    }
    if (mode == MEASURE_SINAD || mode == MEASURE_DISTORTION)
        select_filter(AF_FILTER_BAND_PASS);
    if (!app.scope_shown)
        draw_second_bar();
}

static void show_scope(void)
{
    app.scope_shown = 1;
    scope_show(SCOPE_RX, app.ac_coupled);
}

static void show_bar_charts(void)
{
    app.scope_shown = 0;
    meter_clear_scope_limits();
    clear_area(0, SCREEN_COLS - 1, LOWER_AREA_FIRST_ROW, LOWER_AREA_LAST_ROW);
    draw_bar_charts();
}

static void toggle_hold(void)
{
    app.held = !app.held;
    if (app.held) {
        screen_print(HOLD_TEXT_COL, HOLD_TEXT_ROW, "HOLD");
        screen_print(HOLD_TEXT_COL, HOLD_OFF_ROW, "OFF");
        /* A box around the soft key's symbol. */
        screen_put(HOLD_TEXT_COL, HOLD_BOX_FIRST_ROW, 0x3C);
        screen_put(HOLD_TEXT_COL + 1, HOLD_BOX_FIRST_ROW, 0x40);
        screen_put(HOLD_TEXT_COL + 2, HOLD_BOX_FIRST_ROW, 0x5B);
        screen_put(HOLD_TEXT_COL, HOLD_BOX_FIRST_ROW + 1, 0x3F);
        screen_put(HOLD_TEXT_COL + 3, HOLD_BOX_FIRST_ROW + 1, 0x5D);
        screen_put(HOLD_TEXT_COL, HOLD_BOX_LAST_ROW, 0x3E);
        screen_put(HOLD_TEXT_COL + 1, HOLD_BOX_LAST_ROW, 0x40);
        screen_put(HOLD_TEXT_COL + 2, HOLD_BOX_LAST_ROW, 0x5C);
        if (gpib_talk_only()) {
            screen_softkey_box(PRINT_BOX_POSITION, PRINT_BOX_WIDTH);
            screen_print(PRINT_TEXT_COL, PRINT_TEXT_ROW, "PRT");
        }
    } else {
        clear_area(HOLD_TEXT_COL, SCREEN_COLS - 1, HOLD_TEXT_ROW, HOLD_BOX_LAST_ROW);
        if (gpib_talk_only())
            clear_area(PRINT_BOX_FIRST_COL, SCREEN_COLS - 1, PRINT_TEXT_ROW - 1, PRINT_TEXT_ROW + 1);
    }
}

static uint8_t entry_shown_in(group_t group, field_t field)
{
    return app.group == group && app.field == field && app.entry_len && !app.entering_inc;
}

static void draw_second_label(void)
{
    uint8_t col = layout->second_label_col;

    clear_range(SECOND_READING_ROW, col, (uint8_t)(col + SECOND_LABEL_LAST_COL - SECOND_LABEL_COL));
    switch (meter_mode()) {
    case MEASURE_SINAD:
        screen_print(col, SECOND_READING_ROW, "SINAD:");
        break;
    case MEASURE_SN:
        screen_print((uint8_t)(col + 2u), SECOND_READING_ROW, "S/N:");
        break;
    case MEASURE_DISTORTION:
        screen_print(col, SECOND_READING_ROW, "DISTN:");
        break;
    case MEASURE_DBV:
        break;
    }
}

/* Shows where the next digit of a keypad entry goes. */
static void draw_cursor(void)
{
    static uint8_t cursor_col, cursor_row, cursor_shown;

    if (cursor_shown)
        screen_put(cursor_col, cursor_row, ' ');
    cursor_shown = 0;
    if (!app.entry_len || app.duplex)
        return;
    if (app.entering_inc) {
        cursor_col = INC_VALUE_COL;
        cursor_row = app.group == GROUP_RF ? (app.field == FIELD_LEVEL ? 5 : 3) : (app.field == FIELD_LEVEL ? 9 : 7);
    } else if (app.field == FIELD_FREQ) {
        cursor_col = VALUE_COL;
        cursor_row = app.group == GROUP_RF ? 3 : 7;
    } else {
        cursor_col = VALUE_COL;
        cursor_row = app.group == GROUP_RF ? 5 : 9;
    }
    cursor_col = (uint8_t)(cursor_col + app.entry_len);
    screen_put(cursor_col, cursor_row, SCREEN_CURSOR);
    cursor_shown = 1;
}


static void redraw_audio(void)
{
    screen_print(AUDIO_TITLE_COL, 0, "AUDIO TEST");
    for (uint8_t col = AUDIO_TITLE_COL; col < AUDIO_TITLE_COL + 10u; col++)
        screen_put(col, 1, SCREEN_UNDERLINE);
    tx_audio_draw();
    screen_print(AUDIO_VOLTS_LABEL_COL, 11, "AF VOLTS:");
    draw_second_label();
    screen_print(AUDIO_AF_FREQ_LABEL_COL, 11, "FREQ:");
    screen_print(FILTER_LABEL_COL, AUDIO_FILTER_ROW, "FILTER:");
    draw_filter();
    if (!app.scope_shown)
        screen_print(COUPLING_COL, COUPLING_ROW, app.ac_coupled ? "AC" : "DC");
}

void app_redraw(void)
{
    char text[20];

    if (app.audio) {
        redraw_audio();
        return;
    }
    if (app.duplex) {
        redraw_duplex();
        return;
    }

    screen_print(10, 0, "RECEIVER TEST");
    for (uint8_t col = 10; col < 23; col++)
        screen_put(col, 1, SCREEN_UNDERLINE);
    draw_labels();
    app_draw_external_attenuator(EXT_MARK_COL, EXT_MARK_ROW, app.bnc_selected);

    if (entry_shown_in(GROUP_RF, FIELD_FREQ)) {
        print_field(VALUE_COL, 2, app.entry, VALUE_WIDTH);
    } else {
        format_rf_freq(text);
        print_field(VALUE_COL, 2, text, VALUE_WIDTH);
        readings_set(READING_GEN_FREQ, text);
    }
    draw_inc_label(2, GROUP_RF, FIELD_FREQ);
    print_field(INC_VALUE_COL, 2, inc_entry_in(GROUP_RF, FIELD_FREQ) ? app.entry : app.rf_freq_inc_text, INC_VALUE_WIDTH);

    if (entry_shown_in(GROUP_RF, FIELD_LEVEL)) {
        print_field(VALUE_COL, 4, app.entry, VALUE_WIDTH);
    } else {
        format_rf_level(text);
        print_field(VALUE_COL, 4, text, VALUE_WIDTH);
        readings_set(READING_GEN_LEVEL, text);
        if (level_is_voltage())
            screen_print(LEVEL_TYPE_COL, 4, hal_read(PARAM_LEVEL_EMF) ? "EMF" : "PD");
    }
    draw_inc_label(4, GROUP_RF, FIELD_LEVEL);
    if (inc_entry_in(GROUP_RF, FIELD_LEVEL)) {
        print_field(INC_VALUE_COL, 4, app.entry, INC_VALUE_WIDTH);
    } else if (app.rf_level_inc) {
        put_text(put_fixed(text, (uint32_t)app.rf_level_inc, 1), "dB");
        print_field(INC_VALUE_COL, 4, text, INC_VALUE_WIDTH);
    } else {
        print_field(INC_VALUE_COL, 4, "", INC_VALUE_WIDTH);
    }

    if (entry_shown_in(GROUP_MOD, FIELD_FREQ)) {
        print_field(VALUE_COL, 6, app.entry, VALUE_WIDTH);
    } else if (app.mod_freq_text[0] && app.group == GROUP_MOD && app.field == FIELD_FREQ) {
        print_field(VALUE_COL, 6, app.mod_freq_text, VALUE_WIDTH);
        format_mod_freq(text);
        readings_set(READING_MOD_SET_FREQ, text);
    } else {
        format_mod_freq(text);
        print_field(VALUE_COL, 6, text, VALUE_WIDTH);
        readings_set(READING_MOD_SET_FREQ, text);
    }
    draw_inc_label(6, GROUP_MOD, FIELD_FREQ);
    print_field(INC_VALUE_COL, 6, inc_entry_in(GROUP_MOD, FIELD_FREQ) ? app.entry : app.mod_freq_inc_text, INC_VALUE_WIDTH);

    if (entry_shown_in(GROUP_MOD, FIELD_LEVEL)) {
        print_field(VALUE_COL, 8, app.entry, VALUE_WIDTH);
    } else {
        format_mod_level(text);
        print_field(VALUE_COL, 8, text, VALUE_WIDTH);
        readings_set(READING_MOD_SET_LEVEL, text);
    }
    if (app.mod.off)
        screen_print_reverse(OFF_COL, 8, "OFF");
    draw_inc_label(8, GROUP_MOD, FIELD_LEVEL);
    if (inc_entry_in(GROUP_MOD, FIELD_LEVEL)) {
        print_field(INC_VALUE_COL, 8, app.entry, INC_VALUE_WIDTH);
    } else if (app.mod_level_inc) {
        format_mod_inc(text);
        print_field(INC_VALUE_COL, 8, text, INC_VALUE_WIDTH);
    } else {
        print_field(INC_VALUE_COL, 8, "", INC_VALUE_WIDTH);
    }

    screen_print(1, 11, "AF VOLTS:");
    draw_second_label();
    draw_cursor();
    screen_print(AF_FREQ_LABEL_COL, 11, "FREQ:");
    screen_print(FILTER_LABEL_COL, 13, "FILTER:");
    draw_filter();
    if (!app.scope_shown)
        screen_print(COUPLING_COL, COUPLING_ROW, app.ac_coupled ? "AC" : "DC");
    draw_mod_mark();
}

static void entry_add(char c)
{
    if (app.field == FIELD_NONE || app.entry_len >= ENTRY_SIZE - 1)
        return;
    app.entry[app.entry_len++] = c;
    app.entry[app.entry_len] = 0;
}

static uint8_t entry_value(int32_t scale, int32_t *result)
{
    return parse_entry(app.entry, app.entry_len, scale, result);
}

static void finish_rf_freq(int32_t scale, const char *unit)
{
    int32_t value;

    if (!entry_value(scale, &value))
        return;
    if (app.entering_inc) {
        if (value > RF_FREQ_MAX || value <= 0) {
            gpib_report_entry_error(value > 0 ? ENTRY_ERROR_RF_FREQ_RANGE : ENTRY_ERROR_INC_TOO_SMALL);
            return;
        }
        clear_freq_incs();
        put_text(put_text(app.rf_freq_inc_text, app.entry), unit);
        app.rf_freq_inc = value;
        app.entering_inc = 0;
        entry_clear();
    } else if (value >= RF_FREQ_MIN && value <= RF_FREQ_MAX) {
        app.rf_freq = value;
        synth_set_frequency(app.rf_freq);
        entry_clear();
    } else {
        /* An out-of-range entry stays on the screen, as in the original. */
        gpib_report_entry_error(ENTRY_ERROR_RF_FREQ_RANGE);
    }
}

static void finish_rf_level(void)
{
    int32_t value;

    if (!entry_value(10, &value))
        return;
    if (level_in_range(value)) {
        app.level_unit = LEVEL_UNIT_DBM;
        app.level_offset = 0;
        apply_level(value);
        entry_clear();
    } else {
        report_level_rejected(value);
    }
}

static void finish_rf_level_voltage(uint8_t unit, double volts_per_unit)
{
    int32_t thousandths;

    if (!entry_value(1000, &thousandths) || thousandths <= 0)
        return;
    double volts = thousandths / 1000.0 * volts_per_unit;
    int32_t tenths = tenths_from_volts(volts, 0);

    if (level_in_range(tenths)) {
        app.level_unit = unit;
        app.level_volts = volts;
        app.level_offset = 0;
        apply_level(tenths);
        entry_clear();
    } else {
        report_level_rejected(tenths);
    }
}

/* Moves the level by a number of 0.1 dB steps; a voltage keeps its unit and collects an offset. */
static void step_rf_level(int16_t delta)
{
    int32_t tenths = (int32_t)app.rf_level + delta;

    if (!level_in_range(tenths))
        return;
    if (level_is_voltage())
        app.level_offset = (int16_t)(app.level_offset + delta);
    apply_level(tenths);
}

void app_level_type_changed(void)
{
    if (!app.transmit && level_is_voltage())
        apply_level(tenths_from_volts(app.level_volts, app.level_offset));
    if (!app.transmit)
        app_redraw();
}

/* A stepping increment belongs to one target: setting a new one replaces the old. */
static void clear_freq_incs(void)
{
    app.rf_freq_inc = 0;
    app.rf_freq_inc_text[0] = 0;
    app.mod_freq_inc = 0;
    app.mod_freq_inc_text[0] = 0;
}

static void clear_level_incs(void)
{
    app.rf_level_inc = 0;
    app.mod_level_inc = 0;
}

static void finish_mod_freq(int32_t scale, const char *unit)
{
    int32_t value;

    if (app.entering_inc) {
        if (!entry_value(scale, &value))
            return;
        if (value > MOD_FREQ_MAX) {
            gpib_report_entry_error(ENTRY_ERROR_MOD_FREQ_HIGH);
        } else if (value > 0) {
            clear_freq_incs();
            put_text(put_text(app.mod_freq_inc_text, app.entry), unit);
            app.mod_freq_inc = value;
            app.entering_inc = 0;
            entry_clear();
        }
        return;
    }
    if (!entry_value(scale, &value))
        return;
    if (value < MOD_FREQ_MIN || value > MOD_FREQ_MAX) {
        gpib_report_entry_error(value > MOD_FREQ_MAX ? ENTRY_ERROR_MOD_FREQ_HIGH : ENTRY_ERROR_MOD_FREQ_LOW);
        return;
    }
    put_text(put_text(app.mod_freq_text, app.entry), unit);
    app.mod.freq = (uint32_t)value;
    app.mod.sat_on = 0;
    entry_clear();
    modulation_apply(&app.mod);
    draw_mod_mark();
}

static uint32_t round_deviation(int32_t raw)
{
    if (raw > DEV_COARSE_ABOVE)
        return (uint32_t)((raw + DEV_STEP_COARSE / 2) / DEV_STEP_COARSE * DEV_STEP_COARSE);
    return (uint32_t)((raw + DEV_STEP_FINE / 2) / DEV_STEP_FINE * DEV_STEP_FINE);
}

static void finish_mod_deviation(int32_t scale)
{
    int32_t value;

    if (!entry_value(scale, &value) || value < 0)
        return;
    if (value >= DEV_MAX_EXCLUSIVE) {
        gpib_report_entry_error(ENTRY_ERROR_MOD_LEVEL_HIGH);
        return;
    }
    if (app.entering_inc) {
        clear_level_incs();
        app.mod_level_inc = value;
        app.mod_level_inc_am = 0;
        app.entering_inc = 0;
        entry_clear();
        return;
    }
    app.mod.deviation = round_deviation(value);
    app.mod.am = 0;
    entry_clear();
    modulation_apply(&app.mod);
}

static void finish_mod_am(void)
{
    int32_t value;

    if (!entry_value(10, &value) || value < 0)
        return;
    if (value >= AM_MAX_EXCLUSIVE) {
        gpib_report_entry_error(ENTRY_ERROR_MOD_LEVEL_HIGH);
        return;
    }
    if (app.entering_inc) {
        clear_level_incs();
        app.mod_level_inc = (value + 5) / 10;
        app.mod_level_inc_am = 1;
        app.entering_inc = 0;
        entry_clear();
        return;
    }
    app.mod.am_percent = (uint8_t)((value + 5) / 10);
    app.mod.am = 1;
    entry_clear();
    modulation_apply(&app.mod);
}

/* Switches the RF socket; a level outside the new range moves by the attenuator value. */
static void toggle_socket(void)
{
    app.bnc_selected = !app.bnc_selected;
    int16_t loss_difference = (int16_t)(level_socket_loss(0) - level_socket_loss(1));
    int16_t magnitude = loss_difference < 0 ? (int16_t)-loss_difference : loss_difference;
    int16_t shift = app.rf_level < level_min() ? magnitude : (app.rf_level > level_max() ? (int16_t)-magnitude : 0);

    if (level_is_voltage())
        app.level_offset = (int16_t)(app.level_offset + shift);
    apply_level((int32_t)app.rf_level + shift);
    if (!app.duplex)
        app_draw_external_attenuator(EXT_MARK_COL, EXT_MARK_ROW, app.bnc_selected);
}

static void select_group(group_t group)
{
    app.group = group;
    app.field = FIELD_NONE;
    app.entering_inc = 0;
    app.mod_freq_text[0] = 0;
    entry_clear();
}

static void select_field(field_t field)
{
    if (app.group == GROUP_NONE)
        return;
    app.field = field;
    app.entering_inc = 0;
    app.mod_freq_text[0] = 0;
    entry_clear();
}

static void step_rf_freq(int32_t delta)
{
    int32_t value = app.rf_freq + delta;

    if (value < RF_FREQ_MIN)
        value = RF_FREQ_MIN;
    if (value > RF_FREQ_MAX)
        value = RF_FREQ_MAX;
    app.rf_freq = value;
    synth_set_frequency(app.rf_freq);
}

/* Handles a frequency unit key; scale is for RF frequency in 10 Hz and modulation in 0.1 Hz. */
static void freq_unit_key(int32_t rf_scale, int32_t mod_scale, const char *unit)
{
    if (app.field != FIELD_FREQ || !app.entry_len)
        return;
    if (app.group == GROUP_RF && rf_scale)
        finish_rf_freq(rf_scale, unit);
    else if (app.group == GROUP_MOD && mod_scale)
        finish_mod_freq(mod_scale, unit);
}

uint8_t app_is_transmit(void) { return app.transmit; }
uint8_t app_tx_manual(void) { return tx_manual(); }
uint8_t app_ac_coupled(void) { return app.ac_coupled; }
uint8_t app_filter(void) { return app.transmit ? tx_filter() : app.filter; }
uint8_t app_held(void) { return app.held; }
uint8_t app_mod_off(void) { return app.mod.off; }
uint8_t app_input_select(void) { return app.transmit ? tx_input_select() : !app.bnc_selected; }

uint8_t app_sinad_mode(void)
{
    if (app.transmit)
        return 0;
    return meter_mode() == MEASURE_SINAD ? 1u : (meter_mode() == MEASURE_SN ? 2u : 0u);
}

uint8_t app_distortion_on(void)
{
    return app.transmit ? tx_distortion_on() : meter_mode() == MEASURE_DISTORTION;
}

static void key_action(uint8_t code);

/* A recalled increment has no typed text: whole hertz, or kilohertz without trailing zeros. */
static void format_inc_freq(char *out, int32_t tenths_hz)
{
    uint32_t hz = (uint32_t)(tenths_hz + 5) / 10u;

    if (hz < 1000u) {
        put_text(put_fixed(out, hz, 0), "Hz");
    } else {
        uint8_t decimals = 3;

        while (decimals && hz % 10u == 0u) {
            hz /= 10u;
            decimals--;
        }
        put_text(put_fixed(out, hz, decimals), "kHz");
    }
}

static void capture_settings(instrument_settings *s)
{
    uint8_t vertical[SCOPE_PM + 1], single;

    *s = (instrument_settings){0};
    s->transmit = app.transmit;
    s->duplex = app.duplex;
    s->level_tenths = app.rf_level;
    s->level_unit = app.level_unit;
    s->level_volts = app.level_volts;
    s->level_offset = app.level_offset;
    s->mod_freq_inc = app.mod_freq_inc;
    s->mod_level_inc = app.mod_level_inc;
    s->mod_level_inc_am = app.mod_level_inc_am;
    s->deviation = app.mod.deviation;
    s->am_percent = app.mod.am_percent;
    s->sat_level = (int32_t)app.mod.sat_level;
    s->mod_off = app.mod.off;
    s->ac_coupled = app.ac_coupled;
    s->sinad = app_sinad_mode();
    s->distortion = app_distortion_on();
    s->scope_shown = app.scope_shown;
    scope_get_settings(&s->scope_timebase, vertical, &single);
    s->scope_single = single;
    s->scope_rx_am = app.transmit ? vertical[SCOPE_AM] : vertical[SCOPE_RX];
    s->scope_fm = app.transmit ? vertical[SCOPE_FM] : 0u;
    s->scope_pm = app.transmit ? vertical[SCOPE_PM] : 0u;

    s->audio = app.audio;
    if (app.transmit || app.audio) {
        tx_get_settings(s);
        if (app.audio)
            s->mod_kind = 0; /* this mode has no modulation */
    } else {
        s->rf_hz = (uint32_t)app.rf_freq * 10u;
        s->rf_inc_10hz = app.rf_freq_inc;
        s->rf_level_inc = app.rf_level_inc;
        s->af_freq = app.mod.sat_on ? (int32_t)app.mod.sat_freq : (int32_t)app.mod.freq;
        s->sat_on = app.mod.sat_on;
        s->af_level = app.duplex ? tx_duplex_af_level() : 1000;
        s->mod_kind = app.mod.am ? 1u : 2u;
        s->level_is_am = app.mod.am;
        s->input_select = app.duplex ? (app.one_port ? DUPLEX_INPUT_ONE_PORT : DUPLEX_INPUT_TWO_PORT) : !app.bnc_selected;
        s->filter = app.filter;
        s->group = app.group == GROUP_RF ? 2u : (app.group == GROUP_MOD ? 1u : (app.group == GROUP_AF ? 3u : 0u));
        if (app.duplex)
            s->scope_rx_am = 0;
        s->field = app.field == FIELD_FREQ ? 4u : (app.field == FIELD_LEVEL ? 2u : 0u);
    }
}

static void show_audio_screen(void)
{
    screen_clear();
    readings_clear();
    app.scope_shown = 0;
    draw_bar_charts();
    app_redraw();
    draw_af_frequency();
    tx_publish_settings();
}

static void enter_audio(uint8_t distortion)
{
    entry_clear();
    draw_cursor();
    app.audio = 1;
    app.group = GROUP_NONE;
    app.field = FIELD_NONE;
    app.entering_inc = 0;
    layout = &audio_layout;
    app.mode_before_audio = meter_mode();
    meter_set_mode(distortion ? MEASURE_DISTORTION : MEASURE_DBV);
    meter_clear_scope_limits();
    tx_audio_begin();
    show_audio_screen();
}

static void leave_audio_if_active(void)
{
    if (!app.audio)
        return;
    app.audio = 0;
    layout = &receiver_layout;
    meter_set_mode(app.mode_before_audio);
    tx_audio_end();
}

static void show_receiver_screen(void)
{
    screen_clear();
    readings_clear();
    output_rf_level();
    synth_set_frequency(app.rf_freq);
    modulation_apply(&app.mod);
    app.scope_shown = 0;
    draw_bar_charts();
    app_redraw();
    draw_af_frequency();
    tx_publish_settings();
}

/* A level can fall outside the range when the external attenuator changes. */
static void limit_rf_level(void)
{
    int16_t limited = app.rf_level;

    if (limited > level_max())
        limited = level_max();
    else if (limited < level_min())
        limited = level_min();
    if (limited == app.rf_level)
        return;
    if (level_is_voltage())
        app.level_offset = (int16_t)(app.level_offset + limited - app.rf_level);
    app.rf_level = limited;
}

static void output_rf_level(void)
{
    limit_rf_level();
    if (app.duplex && app.one_port)
        level_set_one_port(app.rf_level);
    else
        level_set(app.rf_level, app.bnc_selected);
}

static void show_duplex_screen(void)
{
    screen_clear();
    readings_clear();
    output_rf_level();
    synth_set_frequency(app.rf_freq);
    modulation_apply(&app.mod);
    tx_duplex_begin(app.mod.am ? 1u : 2u);
    tx_duplex_set_one_port(app.one_port);
    draw_bar_charts();
    app_redraw();
    tx_publish_settings();
}

static void set_one_port(uint8_t one_port)
{
    app.one_port = one_port;
    app.bnc_selected = !one_port;
    if (app.rf_level > level_max())
        app.rf_level = level_max();
    tx_duplex_set_one_port(one_port);
    output_rf_level();
}

static void enter_duplex(void)
{
    if (app.transmit) {
        tx_leave();
        app.transmit = 0;
    }
    if (app.duplex)
        return;
    receiver_state = app;
    app = duplex_state;
    /* The increments are those of the receiver test. */
    app.rf_freq_inc = receiver_state.rf_freq_inc;
    memcpy(app.rf_freq_inc_text, receiver_state.rf_freq_inc_text, sizeof app.rf_freq_inc_text);
    app.rf_level_inc = receiver_state.rf_level_inc;
    app.duplex = 1;
    app.held = 0;
    app.prompt = 0;
    app.scope_shown = 0;
    app.mod.freq = DUPLEX_FIXED_MOD_FREQ;
    entry_clear();
    layout = &duplex_layout;
    meter_set_filter(app.filter);
    meter_clear_scope_limits();
}

/* Back to the receiver settings; the caller shows the next screen. */
static void leave_duplex(void)
{
    if (!app.duplex)
        return;
    duplex_state = app;
    duplex_state.duplex = 0;
    if (duplex_state.group == GROUP_TX) {
        duplex_state.group = GROUP_NONE;
        duplex_state.field = FIELD_NONE;
    }
    app = receiver_state;
    app.duplex = 0;
    app.held = 0;
    layout = &receiver_layout;
    meter_prepare_transmit(0);
    meter_set_filter(app.filter);
    tx_duplex_end();
}

static void leave_transmit_if_active(void)
{
    if (app.transmit) {
        app.transmit = 0;
        tx_leave();
    }
}

static void apply_settings(const instrument_settings *s)
{
    /* An out-of-range value leaves that setting as it was. */
    uint8_t vertical[SCOPE_PM + 1] = {s->transmit ? 0xFFu : s->scope_rx_am, s->transmit ? s->scope_rx_am : 0xFFu,
                                      s->scope_fm, s->scope_pm};

    scope_set_settings(s->scope_timebase, vertical, s->scope_single);
    leave_audio_if_active();
    if (app.duplex && !s->duplex)
        leave_duplex();
    if (s->transmit) {
        leave_transmit_if_active();
        tx_set_settings(s);
        entry_clear();
        app.transmit = 1;
        app.scope_shown = 0;
        meter_clear_scope_limits();
        readings_clear();
        tx_enter(app.rf_level);
        return;
    }

    leave_transmit_if_active();
    if (s->audio) {
        tx_set_settings(s);
        enter_audio(s->distortion);
        return;
    }
    if (s->duplex) {
        enter_duplex();
        app.one_port = s->input_select == DUPLEX_INPUT_ONE_PORT;
        app.bnc_selected = !app.one_port;
    }
    if (s->rf_hz >= RF_FREQ_MIN * 10L && s->rf_hz <= (uint32_t)RF_FREQ_MAX * 10u)
        app.rf_freq = (int32_t)(s->rf_hz / 10u);
    if (!s->duplex)
        app.bnc_selected = s->input_select == 0;
    {
        int32_t tenths = s->level_unit == LEVEL_UNIT_DBM ? s->level_tenths
                                                         : tenths_from_volts(s->level_volts, s->level_offset);

        if (level_in_range(tenths)) {
            app.rf_level = (int16_t)tenths;
            app.level_unit = s->level_unit;
            app.level_volts = s->level_volts;
            app.level_offset = s->level_offset;
        }
    }
    app.rf_freq_inc = s->rf_inc_10hz;
    app.rf_freq_inc_text[0] = 0;
    if (app.rf_freq_inc >= RF_INC_MHZ_FROM)
        format_rf_value(app.rf_freq_inc_text, app.rf_freq_inc);
    else if (app.rf_freq_inc)
        put_text(put_fixed(app.rf_freq_inc_text, (uint32_t)app.rf_freq_inc, 2), "kHz");
    app.rf_level_inc = s->rf_level_inc;
    app.mod_freq_inc = s->mod_freq_inc;
    app.mod_freq_inc_text[0] = 0;
    if (app.mod_freq_inc)
        format_inc_freq(app.mod_freq_inc_text, app.mod_freq_inc);
    app.mod_level_inc = s->mod_level_inc;
    app.mod_level_inc_am = s->mod_level_inc_am;
    app.mod.freq = (uint32_t)(s->af_freq > 0 ? s->af_freq : 10000);
    app.mod.deviation = s->deviation;
    app.mod.am_percent = s->am_percent;
    app.mod.sat_on = s->sat_on;
    if (s->sat_on) {
        app.mod.sat_freq = s->sat_freq;
        app.mod.sat_level = s->sat_level;
    }
    app.mod.am = s->mod_kind == 1u;
    app.mod.off = s->mod_off;
    app.mod_freq_text[0] = 0;
    app.group = s->group == 2u ? GROUP_RF : (s->group == 1u ? GROUP_MOD : (s->group == 3u && s->duplex ? GROUP_AF : GROUP_NONE));
    app.field = s->field == 4u ? FIELD_FREQ : (s->field == 2u ? FIELD_LEVEL : FIELD_NONE);
    app.entering_inc = 0;
    entry_clear();

    meter_set_mod_off(app.mod.off);
    meter_set_mode(s->distortion ? MEASURE_DISTORTION
                                 : (s->sinad == 1u ? MEASURE_SINAD : (s->sinad == 2u ? MEASURE_SN : MEASURE_DBV)));
    app.ac_coupled = s->ac_coupled;
    meter_set_ac_coupled(app.ac_coupled);
    app.filter = s->filter <= AF_FILTER_LOW_PASS_300 ? (af_filter)s->filter : AF_FILTER_BAND_PASS;
    meter_set_filter(app.filter);

    if (s->duplex) {
        app.mod.freq = DUPLEX_FIXED_MOD_FREQ;
        tx_duplex_set_af_level_value(s->af_level);
        show_duplex_screen();
        return;
    }
    show_receiver_screen();
    if (s->scope_shown)
        show_scope();
}

static uint8_t rf_level_entry_active(void)
{
    return app.group == GROUP_RF && app.field == FIELD_LEVEL && app.entry_len && !app.entering_inc;
}

static uint8_t rf_level_inc_entry_active(void)
{
    return app.group == GROUP_RF && app.field == FIELD_LEVEL && app.entry_len && app.entering_inc;
}

/* The arrow keys act on whichever target the increment was set for. */
static void step_freq_key(int8_t direction)
{
    if (app.rf_freq_inc) {
        step_rf_freq(direction * app.rf_freq_inc);
    } else if (app.mod_freq_inc) {
        int32_t freq = (int32_t)app.mod.freq + direction * app.mod_freq_inc;

        if (freq >= MOD_FREQ_MIN && freq <= MOD_FREQ_MAX) {
            app.mod.freq = (uint32_t)freq;
            app.mod_freq_text[0] = 0;
            modulation_apply(&app.mod);
        }
    }
}

static void step_level_key(int8_t direction)
{
    if (app.rf_level_inc) {
        step_rf_level((int16_t)(direction * app.rf_level_inc));
    } else if (app.mod_level_inc && app.mod_level_inc_am == app.mod.am) {
        int32_t step = direction * app.mod_level_inc;

        if (app.mod.am) {
            int32_t percent = (int32_t)app.mod.am_percent + step;

            if (percent < 0 || percent > AM_MAX_PERCENT)
                return;
            app.mod.am_percent = (uint8_t)percent;
        } else {
            int32_t deviation = (int32_t)app.mod.deviation + step;

            if (deviation < 0 || deviation > DEV_MAX_VALUE)
                return;
            app.mod.deviation = (uint32_t)deviation;
        }
        modulation_apply(&app.mod);
    }
}

static uint8_t prompt_col(void)
{
    return app.prompt == KEY_STORE ? PROMPT_DIGITS_COL_STORE : PROMPT_DIGITS_COL_RECALL;
}

static void prompt_erase_cursor(void)
{
    if (app.prompt_cursor)
        screen_put((uint8_t)(prompt_col() + app.prompt_cursor_col), PROMPT_ROW + 1, ' ');
    app.prompt_cursor = 0;
}

/* The field stays selected but is not highlighted while a slot number is entered. */
static void redraw_after_prompt_change(void)
{
    if (app.transmit)
        tx_refresh();
    else
        app_redraw();
}

static void prompt_clear(void)
{
    prompt_erase_cursor();
    app.prompt = 0;
    print_field(PROMPT_COL, PROMPT_ROW, "", PROMPT_CLEAR_WIDTH);
    redraw_after_prompt_change();
}

static void prompt_start(uint8_t key)
{
    app.prompt = key;
    app.prompt_digits = 0;
    app.prompt_value = 0;
    app.prompt_cursor = 0;
    redraw_after_prompt_change();
    print_field(PROMPT_COL, PROMPT_ROW, key == KEY_STORE ? "STORE " : "RECALL", PROMPT_CLEAR_WIDTH);
}

static void prompt_show_cursor(void)
{
    app.prompt_cursor = 1;
    app.prompt_cursor_col = app.prompt_digits;
    screen_put((uint8_t)(prompt_col() + app.prompt_digits), PROMPT_ROW + 1, SCREEN_CURSOR);
}

static int8_t digit_of_key(uint8_t code)
{
    static const uint8_t digit_keys[10] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};

    for (int8_t digit = 0; digit < 10; digit++)
        if (digit_keys[digit] == code)
            return digit;
    return -1;
}

/* Collects the two digits of a slot number; returns 1 if the key was used up. */
static uint8_t prompt_key(uint8_t code)
{
    int8_t digit = digit_of_key(code);

    if (code == KEY_DELETE) {
        prompt_erase_cursor();
        if (app.prompt_digits) {
            app.prompt_digits--;
            app.prompt_value /= 10u;
            screen_put((uint8_t)(prompt_col() + app.prompt_digits), PROMPT_ROW, ' ');
        }
        prompt_show_cursor();
        return 1;
    }
    if (code == KEY_TX || code == KEY_RX) {
        prompt_clear();
        return 0;
    }
    if (digit < 0)
        return 0; /* other keys keep working and the prompt stays */

    prompt_erase_cursor();
    screen_put((uint8_t)(prompt_col() + app.prompt_digits), PROMPT_ROW, (uint8_t)('0' + digit));
    app.prompt_value = (uint8_t)(app.prompt_value * 10u + (uint8_t)digit);
    if (++app.prompt_digits == 1u) {
        prompt_show_cursor();
        return 1;
    }

    if (app.prompt_value > SETTINGS_SLOT_LAST) {
        /* Not a valid slot: the digits go away and the prompt stays. */
        prompt_erase_cursor();
        for (uint8_t i = 0; i < 2u; i++)
            screen_put((uint8_t)(prompt_col() + i), PROMPT_ROW, ' ');
        app.prompt_digits = 0;
        app.prompt_value = 0;
        return 1;
    }

    uint8_t key = app.prompt, slot = app.prompt_value;
    prompt_clear();
    if (key == KEY_STORE) {
        instrument_settings settings;

        capture_settings(&settings);
        settings_store(slot, &settings);
    } else {
        instrument_settings settings = {0};

        if (slot == SLOT_LAST_SESSION) {
            settings_from_image(last_session, &settings);
            apply_settings(&settings);
        } else if (settings_recall(slot, &settings)) {
            apply_settings(&settings);
        }
    }
    return 1;
}

void app_power_on(void)
{
    control_set(CONTROL_NOVRAM_RECALL, 0);
    control_set(CONTROL_NOVRAM_RECALL, CONTROL_NOVRAM_RECALL);
    for (uint8_t i = 0; i < SETTINGS_IMAGE_SIZE; i++)
        last_session[i] = hal_read((uint16_t)(NOVRAM_START + i));
    app_init();
}

void app_restore_screen(void)
{
    uint8_t scope = app.scope_shown;

    if (app.audio) {
        show_audio_screen();
        if (scope)
            show_scope();
        if (app.held) {
            draw_readings();
            draw_af_frequency();
        }
    } else if (app.duplex) {
        show_duplex_screen();
        if (app.held) {
            tx_duplex_draw_held_readings();
            draw_readings();
        }
    } else if (app.transmit) {
        tx_resume();
        if (app.held)
            tx_draw_held_readings();
    } else {
        show_receiver_screen();
        if (scope)
            show_scope();
        if (app.held) {
            draw_readings();
            draw_af_frequency();
        }
    }
    gpib_redraw_indicators();
}

void app_draw_external_attenuator(uint8_t col, uint8_t row, uint8_t bnc)
{
    for (uint8_t i = 0; i < EXT_MARK_WIDTH; i++) {
        screen_put((uint8_t)(col + i), (uint8_t)(row - 1u), ' '); /* the top edge of the mark */
        screen_put((uint8_t)(col + i), row, ' ');
    }
    if (level_external_attenuator(bnc))
        screen_print_reverse(col, row, "EXT");
}

/* Marks a running sub-audible tone or repeating calling tones beside the modulation frequency. */
static void draw_mod_mark(void)
{
    uint8_t col = app.duplex ? DUPLEX_MOD_MARK_COL : MOD_MARK_COL;
    uint8_t row = app.duplex ? DUPLEX_MOD_MARK_ROW : MOD_MARK_ROW;

    for (uint8_t i = 0; i < MOD_MARK_WIDTH; i++) {
        screen_put((uint8_t)(col + i), (uint8_t)(row - 1u), ' ');
        screen_put((uint8_t)(col + i), row, ' ');
    }
    if (app.mod.sat_on)
        screen_print_reverse(col, row, "SAT");
    else if (tones_continuous())
        screen_print_reverse(col, row, "TS");
}

void app_tone_output(uint32_t tenths_hz)
{
    modulation_state state = app.mod;

    state.sat_on = 0;
    if (tenths_hz) {
        state.freq = tenths_hz;
    } else {
        state.deviation = 0;
        state.am_percent = 0;
    }
    modulation_apply(&state);
}

void app_tone_output_end(void)
{
    modulation_apply(&app.mod);
}

void app_format_tone_mod(char *out)
{
    if (app.transmit) {
        put_text(out, app.mod.am ? "MOD: AM" : "MOD: FM");
        return;
    }
    char *p = put_text(out, "MOD:");
    format_mod_level(p);
    while (*p)
        p++;
    put_text(p, app.mod.am ? " AM" : " FM");
}

uint8_t app_two_tone(void) { return tx_two_tone(); }

void app_two_tone_toggle(void)
{
    tx_set_two_tone(!tx_two_tone());
}

uint8_t app_sat_on(void) { return app.mod.sat_on; }
uint8_t app_sat_is_am(void) { return app.mod.am; }
uint32_t app_sat_freq(void) { return app.mod.sat_freq; }
uint32_t app_sat_level(void) { return app.mod.sat_level; }

static void apply_sat(void)
{
    if (app.mod.sat_on) {
        app.mod.freq = DUPLEX_FIXED_MOD_FREQ;
        app.mod_freq_text[0] = 0;
    }
    modulation_apply(&app.mod);
}

void app_sat_set_freq(uint32_t tenths)
{
    app.mod.sat_freq = tenths;
    apply_sat();
}

void app_sat_set_level(uint32_t value)
{
    app.mod.sat_level = value;
    apply_sat();
}

void app_sat_toggle(void)
{
    app.mod.sat_on = !app.mod.sat_on;
    app.settings_changed = 1;
    apply_sat();
}

void app_selftest_begin(void)
{
    if (app.transmit)
        tx_leave();
    else if (app.audio)
        tx_audio_end();
    else if (app.duplex)
        tx_duplex_end();
    tx_selftest_begin();
}

void app_selftest_end(void)
{
    tx_selftest_end();
    meter_prepare_transmit(0);
    meter_set_filter(app.filter);
    meter_set_filter_extra(0xC3u, 0u); /* the power range and demodulator switches of the loop */
    if (app.audio)
        tx_audio_begin();
    else {
        output_rf_level();
        synth_set_frequency(app.rf_freq);
        modulation_apply(&app.mod);
    }
}

overload_input app_overload_input(void)
{
    if (app.duplex)
        return app.one_port ? OVERLOAD_INPUT_N : OVERLOAD_INPUT_EITHER;
    return app_input_select() ? OVERLOAD_INPUT_N : OVERLOAD_INPUT_BNC;
}

void app_get_settings(instrument_settings *settings)
{
    capture_settings(settings);
}

void app_set_settings(const instrument_settings *settings)
{
    apply_settings(settings);
}

void app_key(uint8_t code)
{
    if (!overload_active() && gpib_front_panel_key(code))
        app_command_key(code);
}

void app_command_key(uint8_t code)
{
    if (help_active()) {
        help_key(code);
        return;
    }
    if (tones_active()) {
        tones_key(code);
        return;
    }
    if (code == KEY_TONES && !app.held) {
        if (app.audio)
            tones_open_two_tone();
        else
            tones_open(app.transmit);
        return;
    }
    if (code == KEY_HELP) {
        if (app.held)
            gpib_print_page();
        else
            help_open();
        return;
    }
    app.settings_changed = 1;
    key_action(code);
    if (app.duplex)
        tx_duplex_set_modulation(app.mod.am);
    if (app.prompt && app.prompt_cursor)
        screen_put((uint8_t)(prompt_col() + app.prompt_cursor_col), PROMPT_ROW + 1, SCREEN_CURSOR);
    gpib_redraw_indicators();
}

/* What differs from the receiver test; returns 1 if the key has been dealt with. */
static uint8_t duplex_key(uint8_t code)
{
    switch (code) {
    case KEY_RX:
        leave_duplex();
        show_receiver_screen();
        return 1;
    case KEY_TX:
        leave_duplex();
        return 0;
    case KEY_DUPLEX:
    case KEY_SCOPE:
    case KEY_BAR_CHART:
    case KEY_DELTA_INCR:
        return 1;
    case KEY_SELECT:
        set_one_port(!app.one_port);
        break;
    case KEY_AF_GEN:
        app.group = GROUP_AF;
        app.field = FIELD_NONE;
        entry_clear();
        break;
    case KEY_FREQ:
        if (app.group == GROUP_RF)
            return 0;
        if (app.group == GROUP_NONE || app.group == GROUP_TX) {
            app.group = GROUP_TX;
            app.field = FIELD_FREQ;
            entry_clear();
        }
        break;
    case KEY_LEVEL:
        if (app.group == GROUP_RF || app.group == GROUP_MOD)
            return 0;
        if (app.group == GROUP_AF) {
            app.field = FIELD_LEVEL;
            entry_clear();
        }
        break;
    case KEY_RX_EQUALS_TX:
        if (tx_measured_hz() > 0.0) {
            app.rf_freq = (int32_t)floor(tx_measured_hz() / 10.0 + 0.5);
            if (app.rf_freq >= RF_FREQ_MIN && app.rf_freq <= RF_FREQ_MAX)
                synth_set_frequency(app.rf_freq);
            else
                app.rf_freq = duplex_state.rf_freq;
        }
        break;
    case KEY_MHZ:
    case KEY_KHZ:
    case KEY_HZ:
    case KEY_DBM:
        if (app.group == GROUP_TX && app.entry_len) {
            if (code != KEY_DBM && tx_duplex_set_frequency(app.entry, app.entry_len, code == KEY_MHZ ? 100000L
                                                           : (code == KEY_KHZ ? 100L : 0L),
                                                           code == KEY_MHZ ? "MHz" : (code == KEY_KHZ ? "kHz" : "")))
                entry_clear();
            break;
        }
        if (app.group == GROUP_AF && app.entry_len) {
            if (tx_duplex_set_af_level(app.entry, app.entry_len, code))
                entry_clear();
            break;
        }
        return 0;
    default:
        return 0;
    }
    app_redraw();
    return 1;
}

/* The receiver keys that also work in AUDIO TEST. */
static uint8_t audio_passes_key(uint8_t code)
{
    switch (code) {
    case KEY_SCOPE:
    case KEY_BAR_CHART:
    case KEY_SCOPE_VERT_EXPAND:
    case KEY_SCOPE_VERT_COMPRESS:
    case KEY_SCOPE_HORIZ_EXPAND:
    case KEY_SCOPE_HORIZ_COMPRESS:
    case KEY_SCOPE_REPEAT:
    case KEY_SCOPE_SINGLE:
    case KEY_DB:
    case KEY_AF_BAND_PASS:
    case KEY_AF_LOW_PASS:
    case KEY_AF_LOW_PASS_300:
    case KEY_AC_DC:
    case KEY_DISTORTION:
        return 1;
    default:
        return 0;
    }
}

static void key_action(uint8_t code)
{
    if (code == KEY_HOLD_DISPLAY) {
        toggle_hold();
        return;
    }
    if (app.held)
        return; /* every other key is inoperable while the display is held */

    if (app.prompt && prompt_key(code))
        return;
    if (code == KEY_STORE || code == KEY_RECALL) {
        if (code == KEY_RECALL || !hal_read(PARAM_STORE_DISABLE))
            prompt_start(code);
        return;
    }

    if (code == KEY_DUPLEX && !app.duplex) {
        leave_audio_if_active();
        enter_duplex();
        show_duplex_screen();
        return;
    }
    if (app.transmit) {
        if (code == KEY_RX) {
            leave_transmit_if_active();
            show_receiver_screen();
        } else {
            tx_key(code);
        }
        return;
    }
    if (app.duplex && duplex_key(code))
        return;
    if (app.audio) {
        if (code == KEY_RX) {
            leave_audio_if_active();
            show_receiver_screen();
            return;
        }
        if (code == KEY_TX) {
            leave_audio_if_active();
        } else if (tx_audio_key(code)) {
            return;
        } else if (!audio_passes_key(code)) {
            return;
        }
    } else if (code == KEY_AF_GEN && !app.duplex) {
        enter_audio(0);
        return;
    }

    switch (code) {
    case KEY_TX:
        entry_clear();
        draw_cursor();
        app.transmit = 1;
        app.scope_shown = 0;
        meter_clear_scope_limits();
        readings_clear();
        tx_enter(app.rf_level);
        return;
    case KEY_SCOPE:
        if (!app.scope_shown)
            show_scope();
        break;
    case KEY_BAR_CHART:
        if (app.scope_shown)
            show_bar_charts();
        break;
    case KEY_SCOPE_VERT_EXPAND:
    case KEY_SCOPE_VERT_COMPRESS:
        if (app.scope_shown)
            scope_step_vertical(code == KEY_SCOPE_VERT_COMPRESS ? 1 : -1);
        break;
    case KEY_SCOPE_HORIZ_EXPAND:
    case KEY_SCOPE_HORIZ_COMPRESS:
        if (app.scope_shown)
            scope_step_timebase(code == KEY_SCOPE_HORIZ_COMPRESS ? 1 : -1);
        break;
    case KEY_SCOPE_REPEAT:
    case KEY_SCOPE_SINGLE:
        if (app.scope_shown)
            scope_set_single_sweep(code == KEY_SCOPE_SINGLE);
        break;
    case KEY_DB:
        if (app.entering_inc && app.field == FIELD_LEVEL && app.entry_len) {
            int32_t value;

            if (!entry_value(10, &value))
                break;
            if (value > RF_LEVEL_INC_MAX) {
                gpib_report_entry_error(ENTRY_ERROR_LEVEL_HIGH);
            } else if (value > 0) {
                clear_level_incs();
                app.rf_level_inc = (int16_t)value;
                app.entering_inc = 0;
                entry_clear();
            }
        } else if (meter_mode() == MEASURE_DBV) {
            meter_toggle_db_relative();
        }
        break;
    case KEY_SELECT:
        toggle_socket();
        break;
    case KEY_RF_GEN:
        select_group(GROUP_RF);
        break;
    case KEY_SET_MOD:
        select_group(GROUP_MOD);
        break;
    case KEY_FREQ:
        if (!(app.duplex && app.group == GROUP_MOD))
            select_field(FIELD_FREQ);
        break;
    case KEY_LEVEL:
        select_field(FIELD_LEVEL);
        break;
    case KEY_DELTA_INCR:
        if (app.group != GROUP_NONE && app.field != FIELD_NONE) {
            app.entering_inc = 1;
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
        if (app.entry_len)
            app.entry[--app.entry_len] = 0;
        break;
    case KEY_MHZ:
        if (rf_level_inc_entry_active())
            gpib_report_entry_error(ENTRY_ERROR_LEVEL_INC_UNIT);
        else if (rf_level_entry_active())
            finish_rf_level_voltage(LEVEL_UNIT_V, 1.0);
        else
            freq_unit_key(100000L, 0, "MHz");
        break;
    case KEY_HZ:
        if (rf_level_inc_entry_active())
            gpib_report_entry_error(ENTRY_ERROR_LEVEL_INC_UNIT);
        else if (rf_level_entry_active())
            finish_rf_level_voltage(LEVEL_UNIT_UV, 0.000001);
        else if (app.group == GROUP_MOD && app.field == FIELD_LEVEL && app.entry_len)
            finish_mod_deviation(10);
        else
            freq_unit_key(0, 10L, "Hz");
        break;
    case KEY_KHZ:
        if (rf_level_inc_entry_active())
            gpib_report_entry_error(ENTRY_ERROR_LEVEL_INC_UNIT);
        else if (rf_level_entry_active())
            finish_rf_level_voltage(LEVEL_UNIT_MV, 0.001);
        else if (app.group == GROUP_MOD && app.field == FIELD_LEVEL && app.entry_len)
            finish_mod_deviation(10000);
        else
            freq_unit_key(100L, 10000L, "kHz");
        break;
    case KEY_AM_PERCENT:
        if (app.group == GROUP_MOD && app.field == FIELD_LEVEL && app.entry_len)
            finish_mod_am();
        break;
    case KEY_DBM:
        if (rf_level_inc_entry_active())
            gpib_report_entry_error(ENTRY_ERROR_LEVEL_INC_UNIT);
        else if (app.group == GROUP_RF && app.field == FIELD_LEVEL && app.entry_len)
            finish_rf_level();
        else if (app.field == FIELD_FREQ && app.entry_len)
            gpib_report_entry_error(app.group == GROUP_RF ? ENTRY_ERROR_RF_FREQ_UNIT : ENTRY_ERROR_MOD_FREQ_UNIT);
        break;
    case KEY_MOD_ON_OFF:
        app.mod.off = !app.mod.off;
        modulation_apply(&app.mod);
        meter_set_mod_off(app.mod.off);
        break;
    case KEY_SINAD_SN:
        if (meter_mode() == MEASURE_SINAD)
            select_measure(MEASURE_SN);
        else if (meter_mode() == MEASURE_SN)
            select_measure(MEASURE_SINAD);
        else
            select_measure((hal_read(PARAM_SN_DEFAULT) & 1u) ? MEASURE_SN : MEASURE_SINAD);
        break;
    case KEY_DISTORTION:
        select_measure(meter_mode() == MEASURE_DISTORTION ? MEASURE_DBV : MEASURE_DISTORTION);
        break;
    case KEY_AC_DC:
        if (meter_mode() != MEASURE_DBV)
            break;
        app.ac_coupled = !app.ac_coupled;
        meter_set_ac_coupled(app.ac_coupled);
        if (!app.ac_coupled && app.filter == AF_FILTER_BAND_PASS)
            select_filter(AF_FILTER_LOW_PASS_50K); /* DC needs the wide filter */
        break;
    case KEY_AF_BAND_PASS:
        select_filter(AF_FILTER_BAND_PASS);
        break;
    case KEY_AF_LOW_PASS_300:
        select_filter(AF_FILTER_LOW_PASS_300);
        break;
    case KEY_FM:
        if (app.group == GROUP_MOD && app.field == FIELD_LEVEL && app.entry_len)
            finish_mod_deviation(10000); /* an extension: the original needs kHz here */
        break;
    case KEY_AF_LOW_PASS:
        select_filter(app.filter == AF_FILTER_LOW_PASS_50K ? AF_FILTER_LOW_PASS_300 : AF_FILTER_LOW_PASS_50K);
        break;
    case KEY_LEVEL_UP:
        step_level_key(1);
        break;
    case KEY_LEVEL_DOWN:
        step_level_key(-1);
        break;
    case KEY_FREQ_UP:
        step_freq_key(1);
        break;
    case KEY_FREQ_DOWN:
        step_freq_key(-1);
        break;
    default:
        return;
    }
    app_redraw();
}

static void knob_rf(int8_t direction)
{
    if (app.field == FIELD_FREQ) {
        int32_t step = app.rf_freq < RF_FREQ_FINE_LIMIT ? 5 : 10;
        step_rf_freq(direction > 0 ? step : -step);
    } else if (app.field == FIELD_LEVEL) {
        step_rf_level(direction);
    }
}

static void knob_mod(int8_t direction)
{
    if (app.field == FIELD_FREQ) {
        int32_t freq = (int32_t)app.mod.freq;
        int32_t step = freq < MOD_FREQ_FINE_LIMIT ? 1 : 10;
        freq += direction > 0 ? step : -step;
        if (freq < MOD_FREQ_MIN || freq > MOD_FREQ_MAX)
            return;
        app.mod.freq = (uint32_t)freq;
        app.mod_freq_text[0] = 0;
    } else if (app.field == FIELD_LEVEL && app.mod.am) {
        int16_t percent = (int16_t)(app.mod.am_percent + direction);
        if (percent < 0 || percent > AM_MAX_PERCENT)
            return;
        app.mod.am_percent = (uint8_t)percent;
    } else if (app.field == FIELD_LEVEL) {
        int32_t deviation = (int32_t)app.mod.deviation;
        int32_t step = deviation < DEV_COARSE_ABOVE ? DEV_STEP_FINE : DEV_STEP_COARSE;
        deviation += direction > 0 ? step : -step;
        if (deviation < 0 || deviation > DEV_MAX_VALUE)
            return;
        app.mod.deviation = (uint32_t)deviation;
    } else {
        return;
    }
    modulation_apply(&app.mod);
}

void app_knob(int8_t direction)
{
    if (app.held || overload_active() || help_active() || tones_active() || !gpib_variable_enabled())
        return;
    app.settings_changed = 1;
    if (app.duplex && app.group == GROUP_AF) {
        if (app.field == FIELD_LEVEL && tx_duplex_knob_af_level(direction))
            app_redraw();
        return;
    }
    if (app.transmit || app.audio) {
        tx_knob(direction);
        return;
    }
    if (app.field == FIELD_NONE)
        return;
    if (app.group == GROUP_RF)
        knob_rf(direction);
    else if (app.group == GROUP_MOD)
        knob_mod(direction);
    app_redraw();
}
