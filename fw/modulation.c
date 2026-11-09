#include "modulation.h"

#include "hal.h"
#include "level.h"
#include "regs.h"
#include "synth.h"

#define ONE_KHZ 10000u           /* in 0.1 Hz: the fixed 1 kHz source is used */
#define COUNT_TENTHS_LIMIT 32768u /* below this the AF counter runs in 0.1 Hz units */
#define FM_LOW_RANGE_LIMIT 63750u /* 6.375 kHz in 0.1 Hz */
#define AM_LOW_RANGE_LIMIT 25u    /* percent */

#define ROUTING_LOW_RANGE 0x80u   /* REG_ROUTING bit 7 */
#define AF_LOW_NIBBLE_LOW_RANGE 3u
#define AF_LOW_NIBBLE_DEFAULT 5u
#define AF_LOW_NIBBLE_SAT 7u     /* the finest range, for the sub-audible tone */
#define SAT_FM_COUNT_TENTHS 100u /* 10 Hz per DAC count */
#define AF_TRANSMIT_NIBBLE_DEFAULT 4u

#define OUTPUT_FM 4u              /* low nibble of REG_LEVEL_DAC_LOW */
#define OUTPUT_AM 3u
#define OUTPUT_OFF 7u

/* AF filter band selection for each frequency range, upper limit in 0.1 Hz. */
static const struct {
    uint32_t limit;
    uint8_t bits;
} af_ranges[] = {
    {300u, 0x00},
    {1000u, 0x70},
    {3000u, 0xB0},
    {10000u, 0xD0},
    {30000u, 0xE0},
    {0xFFFFFFFFu, 0xF0},
};

static uint8_t transmit_mode;
static uint8_t transmit_nibble = AF_TRANSMIT_NIBBLE_DEFAULT;

void modulation_set_transmit(uint8_t transmit)
{
    transmit_mode = transmit;
    transmit_nibble = AF_TRANSMIT_NIBBLE_DEFAULT;
}

void modulation_set_transmit_nibble(uint8_t nibble)
{
    transmit_nibble = nibble;
}

/* The DAC count of an AM depth, on the range that suits it. */
static uint8_t am_dac(uint8_t percent, uint8_t *low_range)
{
    /*
     * (percent + 1/8) / 39.0625 times 400 or 100, as in the original, but rounded.
     * Deliberate difference: the original truncates and leaves a count of 1 at 0 %.
     */
    uint32_t numerator = (percent * 8u + 1u) * (percent < AM_LOW_RANGE_LIMIT ? 800u : 200u);

    *low_range = percent < AM_LOW_RANGE_LIMIT;
    return percent == 0 ? 0 : (uint8_t)((numerator + 312u) / 625u);
}

static uint8_t level_dac(const modulation_state *s, uint8_t *low_range)
{
    if (s->am) {
        return am_dac(s->am_percent, low_range);
    }
    *low_range = s->deviation < FM_LOW_RANGE_LIMIT;
    return (uint8_t)(s->deviation / (*low_range ? 250u : 1000u));
}

void modulation_apply(const modulation_state *s)
{
    uint8_t low_range;
    uint8_t dac = level_dac(s, &low_range);
    uint8_t fixed_source = s->freq == ONE_KHZ;
    uint8_t sat = s->sat_on && fixed_source && !transmit_mode;
    uint32_t oscillator = sat ? s->sat_freq : s->freq;
    uint16_t count = (uint16_t)(oscillator < COUNT_TENTHS_LIMIT ? oscillator : (oscillator + 5u) / 10u);
    uint8_t i = 0;
    uint8_t sat_low_range = 0;
    uint8_t sat_count = 0;

    if (sat && s->am) {
        sat_count = am_dac((uint8_t)s->sat_level, &sat_low_range);
    } else if (sat) {
        uint32_t counts = (s->sat_level + SAT_FM_COUNT_TENTHS / 2u) / SAT_FM_COUNT_TENTHS;

        sat_count = (uint8_t)(counts > 255u ? 255u : counts);
    }

    while (oscillator >= af_ranges[i].limit)
        i++;

    hal_write(REG_AF_COUNT_LOW, (uint8_t)count);
    hal_write(REG_AF_COUNT_HIGH, (uint8_t)(count >> 8));
    if (!transmit_mode) {
        hal_write(REG_AF_LEVEL, sat ? sat_count : (fixed_source ? 0 : dac));
        hal_write(REG_MOD_LEVEL, fixed_source ? dac : 0);
    }
    hal_write(REG_AF_SWITCHING, (uint8_t)(af_ranges[i].bits
                                          | (transmit_mode ? transmit_nibble
                                                           : (sat ? (s->am ? (sat_low_range ? AF_LOW_NIBBLE_LOW_RANGE : AF_LOW_NIBBLE_DEFAULT)
                                                                         : AF_LOW_NIBBLE_SAT)
                                                                  : (!fixed_source && low_range ? AF_LOW_NIBBLE_LOW_RANGE
                                                                                                : AF_LOW_NIBBLE_DEFAULT)))));
    level_set_output_mode(s->off ? OUTPUT_OFF : (s->am ? OUTPUT_AM : OUTPUT_FM));
    synth_set_modulation(s->am, transmit_mode || (fixed_source && low_range) ? ROUTING_LOW_RANGE : 0);
}
