#include "synth.h"

#include "hal.h"
#include "level.h"
#include "regs.h"

#define MHZ_10HZ 100000L

/*
 * Serial load on REG_SYNTH_DATA: bits 6-4 select the strobe, the low nibble is data.
 * Bit 7 belongs to the attenuator and is kept in aux_bit.
 */
#define SYNTH_DATA 0x70u
#define SYNTH_STROBE_COARSE 0x60u
#define SYNTH_STROBE_FINE 0x50u
#define SYNTH_LOAD_START 0x30u
#define SYNTH_LOAD_END 0x70u
#define SYNTH_AUX 0x80u

#define FINE_FILL_HIGH 0xBu /* constant digit of the fine block */
#define FINE_TOP_DIGIT 0xAu /* constant top digit of the fine block */
#define DIGITS_PER_BLOCK 7

#define ROUTING_FIXED_BITS 0x30u /* other functions sharing REG_ROUTING */
#define ROUTING_TRANSMIT 0x40u
#define ROUTING_FM 0x08u         /* plus the band bits */
#define ROUTING_AM 0x0Bu
#define IF_CORRECTION_STEP_HZ 5000000.0
#define AM_TRACK_TABLE 0xF717u   /* EEPROM: AM tracking against the level step position */
#define AM_TRACK_ENTRIES 12u
#define AM_TRACK_STEP 10.0

typedef struct {
    int32_t limit;        /* first frequency above this band, in 10 Hz units */
    uint8_t band;         /* value for REG_SYNTH_BAND */
    int16_t offset_mhz;
    uint8_t multiplier;
    uint8_t routing;      /* low bits of REG_ROUTING */
    uint8_t track_table;  /* index into track_tables */
} synth_band;

static const synth_band bands[] = {
    { 60 * MHZ_10HZ, 0x8C, 1400, 2, 2, 0},
    { 88 * MHZ_10HZ, 0x4C, 1400, 2, 2, 1},
    {130 * MHZ_10HZ, 0x84, 1000, 4, 3, 0},
    {165 * MHZ_10HZ, 0x44, 1000, 4, 3, 1},
    {260 * MHZ_10HZ, 0x80, 1000, 2, 2, 0},
    {400 * MHZ_10HZ, 0x40, 1000, 2, 2, 1},
    {530 * MHZ_10HZ, 0x00, 1000, 2, 2, 2},
    {630 * MHZ_10HZ, 0x70, 1000, 1, 1, 1},
    {800 * MHZ_10HZ, 0x60, 1000, 1, 1, 1},
    {0x7FFFFFFFL, 0x20, 1000, 1, 1, 2},
};

/* FM tracking calibration tables in EEPROM, indexed by synthesizer frequency. */
typedef struct {
    uint16_t address;
    uint8_t entries;
    double start_hz;
    double step_hz;
} track_table;

static const track_table track_tables[] = {
    {0xF723, 39, 330000000.0, 5000000.0},
    {0xF74A, 48, 520000000.0, 6000000.0},
    {0xF77A, 45, 800000000.0, 6000000.0},
};

static int16_t last_band = -1;
static uint8_t aux_bit = SYNTH_AUX;
static const synth_band *current_band = bands;
static double current_synth_hz;
static uint8_t am_selected;
static uint8_t routing_high_bit = 0x80u;
static uint8_t routing_transmit;
static uint8_t af_range_bits = ROUTING_FIXED_BITS;

static void write_routing(void)
{
    hal_write(REG_ROUTING, (uint8_t)(routing_high_bit | routing_transmit | af_range_bits
                                     | (am_selected ? ROUTING_AM : (ROUTING_FM | current_band->routing))));
}

void synth_set_aux(uint8_t high)
{
    aux_bit = high ? SYNTH_AUX : 0;
    hal_write(REG_SYNTH_DATA, (uint8_t)(SYNTH_DATA | aux_bit));
}

static void load_digit(uint8_t strobe, uint8_t digit)
{
    hal_write(REG_SYNTH_DATA, (uint8_t)(SYNTH_DATA | aux_bit | digit));
    hal_write(REG_SYNTH_DATA, (uint8_t)(strobe | aux_bit | digit));
}

/* Interpolates a tracking DAC value from an EEPROM table. */
static uint8_t interpolate(uint16_t address, uint8_t entries, double position)
{
    uint8_t low;
    double fraction;

    if (position < 0.0)
        position = 0.0;
    low = (uint8_t)position;
    if (low > entries - 2u)
        low = (uint8_t)(entries - 2u);
    fraction = position - low;

    double first = hal_read((uint16_t)(address + low));
    double second = hal_read((uint16_t)(address + low + 1u));
    return (uint8_t)(first * (1.0 - fraction) + second * fraction + 0.5);
}

void synth_update_tracking(void)
{
    if (am_selected) {
        double position = level_step_position() / AM_TRACK_STEP;
        hal_write(REG_MOD_TRACKING, interpolate(AM_TRACK_TABLE, AM_TRACK_ENTRIES, position));
    } else {
        const track_table *table = &track_tables[current_band->track_table];
        double position = (current_synth_hz - table->start_hz) / table->step_hz;
        hal_write(REG_MOD_TRACKING, interpolate(table->address, table->entries, position));
    }
}

void synth_set_af_range(uint8_t bits)
{
    af_range_bits = bits;
    write_routing();
}

void synth_set_transmit(uint8_t transmit)
{
    routing_transmit = transmit ? ROUTING_TRANSMIT : 0;
    write_routing();
}

void synth_set_modulation(uint8_t am, uint8_t high_bit)
{
    am_selected = am;
    routing_high_bit = high_bit;
    write_routing();
    synth_update_tracking();
}

void synth_set_frequency(int32_t freq_10hz)
{
    const synth_band *b = bands;
    uint8_t coarse[DIGITS_PER_BLOCK] = {0};
    uint8_t fine[DIGITS_PER_BLOCK] = {0, 0, FINE_FILL_HIGH, 0, 0, 0, FINE_TOP_DIGIT};
    uint8_t i;

    while (freq_10hz >= b->limit)
        b++;

    if (b->band != last_band) {
        hal_write(REG_SYNTH_BAND, b->band);
        last_band = b->band;
    }

    /* Synthesizer frequency above 1000 MHz, in 100 Hz units. */
    int32_t f = (b->offset_mhz - 1000) * 10000L + freq_10hz * b->multiplier / 10;
    double synth_hz = f * 100.0;

    current_band = b;
    current_synth_hz = synth_hz;

    fine[0] = (uint8_t)(f % 10);
    f /= 10;
    fine[1] = (uint8_t)(f % 10);
    f /= 10;
    for (i = 1; i < 5; i++) {
        coarse[i] = (uint8_t)(f % 10);
        f /= 10;
    }
    coarse[5] = (uint8_t)f; /* the hundreds digit counts up to 10 */
    coarse[6] = 1;

    for (i = 0; i < DIGITS_PER_BLOCK; i++)
        load_digit(SYNTH_STROBE_COARSE, coarse[i]);
    for (i = 0; i < DIGITS_PER_BLOCK; i++)
        load_digit(SYNTH_STROBE_FINE, fine[i]);
    hal_write(REG_SYNTH_DATA, (uint8_t)(SYNTH_LOAD_START | aux_bit));
    hal_write(REG_SYNTH_DATA, (uint8_t)(SYNTH_LOAD_END | aux_bit));

    if (!am_selected)
        hal_write(REG_IF_CORRECTION, (uint8_t)(synth_hz / IF_CORRECTION_STEP_HZ));
    write_routing();
    synth_update_tracking();
}
