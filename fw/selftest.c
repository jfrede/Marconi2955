#include "selftest.h"

#include <math.h>

#include "afcounter.h"
#include "app.h"
#include "format.h"
#include "hal.h"
#include "help.h"
#include "keys.h"
#include "level.h"
#include "modulation.h"
#include "params.h"
#include "screen.h"
#include "synth.h"
#include "tx.h"

#define GROUP_COUNT 3u
#define KEY_NEXT_TEST KEY_TX
#define KEY_RETURN KEY_HOLD_DISPLAY
#define RETURN_POSITION 6u
#define NEXT_POSITION 0u
#define BOX_WIDTH 9u
#define BOX_TEXT_COL 29u

#define COL_NUMBER 0u
#define COL_STATUS 4u
#define COL_CODE 11u
#define COL_MEASURED 14u
#define STATUS_WIDTH 6u
#define MEASURED_WIDTH 14u
#define MEASURED_SIZE 24u
#define LIMITS_ROW 30u
#define ROWS_PER_TEST 2u

#define MHZ_TO_10HZ 100000L
#define RF_TOLERANCE_HZ 50.0
#define POWER_TOLERANCE 1.585   /* 2 dB */
#define LEVEL_TOLERANCE 0.1
#define AF_TOLERANCE_HZ 1.0
#define RF_LOOP_MS 2000u
#define POWER_LOOP_MS 2000u
#define MOD_LOOP_MS 3000u

/* Output attenuation in 0.1 dB, from the generator's strongest setting. */
#define ATTENUATION_RF 0
#define ATTENUATION_POWER 20
#define ATTENUATION_POWER_LOW 50
#define ATTENUATION_MOD 50

#define DEMOD_FM 0u
#define DEMOD_AM 1u
#define DEMOD_PM 2u

#define FREQ_400_HZ 4000u  /* 0.1 Hz */
#define FREQ_1_KHZ 10000u
#define DEVIATION_5_KHZ 50000u /* 0.1 Hz */
#define AM_DEPTH 50u
#define MOD_EXPECTED_KHZ 5.0
#define MOD_EXPECTED_PERCENT 50.0
#define MOD_EXPECTED_RAD 5.0
#define POWER_EXPECTED_MW 79.0
#define POWER_EXPECTED_LOW_MW 40.0

#define ERROR_FLAG_NO_READING 0x0Bu

#define PARAM_LAST_ERROR 0xF6BCu /* one byte per test group: the last failure */

typedef enum { PHASE_OFF, PHASE_WAITING, PHASE_RUNNING, PHASE_FINISHED } test_phase;

static const uint8_t tests_in_group[GROUP_COUNT] = {5u, 4u, 4u};
static const uint8_t first_row[GROUP_COUNT] = {4u, 14u, 22u};

static const int16_t rf_counter_mhz[] = {20, 111, 218, 340, 480};
static const int16_t power_mhz[] = {300, 849, 20, 20};
#define MODULATION_MHZ 210

static struct {
    test_phase phase;
    uint8_t single_step;
    uint8_t group, index;
    uint32_t started;
    char measured[MEASURED_SIZE];
} st;

static uint32_t duration_ms(void)
{
    return st.group == 0u ? RF_LOOP_MS : (st.group == 1u ? POWER_LOOP_MS : MOD_LOOP_MS);
}

static uint8_t test_row(void)
{
    return (uint8_t)(first_row[st.group] + st.index * ROWS_PER_TEST);
}

static void draw_screen(void)
{
    screen_clear();
    screen_print(10, 0, "SELF TEST");
    for (uint8_t col = 10; col < 19; col++)
        screen_put(col, 1, SCREEN_UNDERLINE);
    screen_print(0, 2, "TEST");
    screen_print(COL_STATUS + 1u, 2, "RESULT");
    screen_print(COL_CODE, 2, "CODE");
    screen_print(COL_MEASURED + 2u, 2, "MEASURED");
    screen_print(0, LIMITS_ROW, "LIMITS  1.X 50Hz   2.X 2dB   3.X 10%");
    screen_softkey_box(RETURN_POSITION, BOX_WIDTH);
    screen_print(BOX_TEXT_COL, (uint8_t)(RETURN_POSITION * 4u + 1u), "RETURN");
    if (st.single_step) {
        screen_softkey_box(NEXT_POSITION, BOX_WIDTH);
        screen_print(BOX_TEXT_COL, 1, "NEXT TEST");
    }
}

static void draw_status(const char *text)
{
    print_field(COL_STATUS, test_row(), text, STATUS_WIDTH);
}

static void configure_generator(void)
{
    modulation_state mod = {0};
    int32_t mhz;
    int16_t attenuation;

    mod.freq = FREQ_1_KHZ;
    mod.deviation = DEVIATION_5_KHZ;
    mod.off = st.group != 2u;
    switch (st.group) {
    case 0:
        mhz = rf_counter_mhz[st.index];
        attenuation = ATTENUATION_RF;
        break;
    case 1:
        mhz = power_mhz[st.index];
        attenuation = st.index == 3u ? ATTENUATION_POWER_LOW : ATTENUATION_POWER;
        break;
    default:
        mhz = MODULATION_MHZ;
        attenuation = ATTENUATION_MOD;
        if (st.index == 0u)
            mod.freq = FREQ_400_HZ;
        if (st.index == 2u) {
            mod.am = 1u;
            mod.am_percent = AM_DEPTH;
        }
        tx_selftest_demod(st.index == 2u ? DEMOD_AM : (st.index == 3u ? DEMOD_PM : DEMOD_FM));
        break;
    }
    synth_set_frequency(mhz * MHZ_TO_10HZ);
    level_set_self_test(attenuation);
    modulation_apply(&mod);
}

static void begin_test(void)
{
    char text[6];

    text[0] = (char)('1' + st.group);
    text[1] = '.';
    text[2] = (char)('0' + st.index);
    text[3] = 0;
    screen_print(COL_NUMBER, test_row(), text);
    draw_status("ACTIVE");
    print_field(COL_CODE, test_row(), "", 2u);
    print_field(COL_MEASURED, test_row(), "", MEASURED_WIDTH);
    configure_generator();
    st.phase = PHASE_RUNNING;
    st.started = hal_millis();
}

/* Writes a sign and a magnitude; returns a pointer to the terminator. */
static char *put_signed(char *out, double value, uint8_t decimals)
{
    double size = value < 0.0 ? -value : value;
    double scale = decimals ? 10.0 : 1.0;

    *out++ = value < 0.0 ? '-' : '+';
    return put_fixed(out, round_scaled(size, scale), decimals);
}

/* The reading and how far it is from what was expected, in percent. */
static void describe_level(double value, double expected, uint8_t decimals, const char *unit)
{
    char *out = st.measured;

    out = put_fixed(out, round_scaled(value, decimals == 2u ? 100.0 : 10.0), decimals);
    out = put_text(out, unit);
    *out++ = ' ';
    put_text(put_signed(out, (value - expected) / expected * 100.0, 1), "%");
}

/* Each result is 0 for a pass, or the code of the failure in this group. */
static uint8_t check_rf_counter(void)
{
    double measured = tx_measured_hz();
    double difference = measured - rf_counter_mhz[st.index] * 1.0e6;

    if (measured <= 0.0) {
        put_text(st.measured, "NO READING");
        return ERROR_FLAG_NO_READING;
    }
    put_text(put_signed(st.measured, difference, 0), "Hz");
    if (difference > RF_TOLERANCE_HZ)
        return (uint8_t)(st.index * 2u + 1u);
    if (difference < -RF_TOLERANCE_HZ)
        return (uint8_t)(st.index * 2u + 2u);
    return 0;
}

static uint8_t check_power(void)
{
    double expected = st.index == 3u ? POWER_EXPECTED_LOW_MW : POWER_EXPECTED_MW;
    double power = tx_selftest_power_mw();
    char *out = st.measured;

    out = put_text(put_fixed(out, round_scaled(power, 10.0), 1), "mW ");
    if (power > 0.0)
        put_text(put_signed(out, 10.0 * log10(power / expected), 1), "dB");
    else
        put_text(out, "NONE");

    if (power < expected / POWER_TOLERANCE)
        return (uint8_t)(st.index * 2u + 1u);
    if (power > expected * POWER_TOLERANCE)
        return (uint8_t)(st.index * 2u + 2u);
    return 0;
}

static uint8_t check_modulation(void)
{
    double expected = st.index == 2u ? MOD_EXPECTED_PERCENT : (st.index == 3u ? MOD_EXPECTED_RAD : MOD_EXPECTED_KHZ);
    double level = tx_selftest_mod_level();

    if (st.index == 2u)
        describe_level(level, expected, 1u, "%");
    else
        describe_level(level, expected, 2u, st.index == 3u ? "RAD" : "kHz");

    if (level < expected * (1.0 - LEVEL_TOLERANCE))
        return (uint8_t)(st.index * 2u + 3u);
    if (level > expected * (1.0 + LEVEL_TOLERANCE))
        return (uint8_t)(st.index * 2u + 4u);
    if (st.index < 2u) {
        double expected_hz = st.index == 0u ? 400.0 : 1000.0;
        double difference = afcounter_hz() - expected_hz;

        if (difference > AF_TOLERANCE_HZ || difference < -AF_TOLERANCE_HZ) {
            char *out = put_fixed(st.measured, round_scaled(afcounter_hz(), 10.0), 1);

            put_text(out, "Hz");
            return (uint8_t)(st.index + 1u);
        }
    }
    return 0;
}

static uint8_t evaluate(void)
{
    if (st.group == 0u)
        return check_rf_counter();
    return st.group == 1u ? check_power() : check_modulation();
}

static void show_result(uint8_t code)
{
    st.measured[MEASURED_WIDTH] = 0; /* keeps clear of the key box beside it */
    print_field(COL_MEASURED, test_row(), st.measured, MEASURED_WIDTH);
    if (!code) {
        draw_status("PASSED");
        return;
    }
    char text[4];

    draw_status("FAILED");
    text[0] = (char)('1' + st.group);
    text[1] = (char)(code < 10u ? (char)('0' + code) : (char)('A' + code - 10u));
    text[2] = 0;
    screen_print(COL_CODE, test_row(), text);
    param_set((uint16_t)(PARAM_LAST_ERROR + st.group), code);
}

static void advance(void)
{
    if (++st.index < tests_in_group[st.group])
        return;
    st.index = 0;
    st.group++;
}

static void next_step(void)
{
    if (st.group >= GROUP_COUNT) {
        if (st.single_step) {
            st.group = st.index = 0;
            st.phase = PHASE_WAITING;
            return;
        }
        st.phase = PHASE_FINISHED;
        return;
    }
    if (st.single_step)
        st.phase = PHASE_WAITING;
    else
        begin_test();
}

void selftest_start(uint8_t single_step)
{
    st.single_step = single_step;
    st.group = st.index = 0;
    app_selftest_begin();
    draw_screen();
    if (single_step)
        st.phase = PHASE_WAITING;
    else
        begin_test();
}

uint8_t selftest_active(void)
{
    return st.phase != PHASE_OFF;
}

void selftest_poll(void)
{
    if (st.phase != PHASE_RUNNING)
        return;
    tx_duplex_poll(0);
    if ((uint32_t)(hal_millis() - st.started) < duration_ms())
        return;
    show_result(evaluate());
    advance();
    next_step();
}

void selftest_key(uint8_t code)
{
    if (code == KEY_RETURN) {
        st.phase = PHASE_OFF;
        app_selftest_end();
        help_selftest_closed(st.single_step);
    } else if (code == KEY_NEXT_TEST && st.phase == PHASE_WAITING) {
        if (st.group == 0u && st.index == 0u)
            draw_screen();
        begin_test();
    }
}

void selftest_redraw(void)
{
    draw_screen();
}
