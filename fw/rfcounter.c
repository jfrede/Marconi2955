#include "rfcounter.h"

#include "control.h"
#include "hal.h"
#include "params.h"
#include "regs.h"

#define STATUS_GATE_DONE 0x04u
#define CMD_RESTART 0x22u
#define CMD_POINT_COUNTER2_LOAD 0x12u

#define CONTROL_COUNTERS_ENABLE 0x01u
#define RF_CONTROL_RUN 0x10u
#define RF_CONTROL_FINE 0x04u
#define RF_CONTROL_DEFAULT 0x10u

#define DIGITS 9u
#define SELECT_CLEAR 0xF0u

#define GATE_FINE_LONG 10000u
#define GATE_FINE_SHORT 1000u
#define GATE_COARSE 4000u

#define FINE_MAX_HZ 200000000.0
#define FINE_MIN_HZ 400000.0
#define STABLE_HZ 1000.0
#define UNSTABLE_READINGS 4u

/* Am9513 setup: pairs of register (0 data, 1 command) and value. */
static const uint8_t timer_setup[][2] = {
    {1, 0xFF}, {1, 0x5F}, {1, 0x01}, {1, 0xDF}, {1, 0x17}, {0, 0xC0}, {0, 0x8A}, {1, 0x01},
    {0, 0x21}, {0, 0x0D}, {0, 0x00}, {0, 0x01}, {0, 0x00}, {0, 0x00},
    {0, 0x42}, {0, 0x02}, {0, 0x02}, {0, 0x00}, {0, 0x90}, {0, 0x01},
    {0, 0xA9}, {0, 0xC3}, {0, 0x01}, {0, 0x00}, {0, 0x00}, {0, 0x00},
    {0, 0x21}, {0, 0xF4}, {0, 0x0F}, {0, 0x00}, {0, 0x00}, {0, 0x00},
    {0, 0x25}, {0, 0x15}, {0, 0x0F}, {0, 0x00}, {0, 0x00}, {0, 0x00},
    {1, 0x5F}, {1, 0xEA}, {1, 0x3F},
};

static struct {
    uint8_t control;
    uint8_t coarse;
    uint8_t unstable_left;
    double last_hz;
} rf;

void rfcounter_set_control(uint8_t mask, uint8_t value)
{
    rf.control = (uint8_t)((rf.control & (uint8_t)~mask) | (value & mask));
    hal_write(REG_RF_CONTROL, rf.control);
}

static void load_gate(uint16_t count, uint8_t fine)
{
    rf.coarse = !fine;
    hal_write(REG_TIMER_CONTROL, CMD_POINT_COUNTER2_LOAD);
    control_set(CONTROL_COUNTERS_ENABLE, 0);
    hal_write(REG_TIMER_DATA, (uint8_t)count);
    hal_write(REG_TIMER_DATA, (uint8_t)(count >> 8));
    control_set(CONTROL_COUNTERS_ENABLE, CONTROL_COUNTERS_ENABLE);
    rfcounter_set_control(RF_CONTROL_FINE, fine ? RF_CONTROL_FINE : 0);
}

static void select_coarse(void) { load_gate(GATE_COARSE, 0); }

static void select_fine(void)
{
    load_gate(hal_read(PARAM_COUNTER_FINE) ? GATE_FINE_LONG : GATE_FINE_SHORT, 1);
}

void rfcounter_init(void)
{
    rf.control = RF_CONTROL_DEFAULT;
    rf.unstable_left = UNSTABLE_READINGS;
    rf.last_hz = 0.0;
    hal_write(REG_RF_CONTROL, rf.control);
    for (uint8_t i = 0; i < sizeof timer_setup / sizeof timer_setup[0]; i++)
        hal_write(timer_setup[i][0] ? REG_TIMER_CONTROL : REG_TIMER_DATA, timer_setup[i][1]);
    control_set(CONTROL_COUNTERS_ENABLE, CONTROL_COUNTERS_ENABLE);
    select_coarse();
}

static void restart(void)
{
    hal_write(REG_RF_SELECT, SELECT_CLEAR);
    hal_write(REG_RF_SELECT, 0);
    rfcounter_set_control(RF_CONTROL_RUN, RF_CONTROL_RUN);
    control_set(CONTROL_COUNTERS_ENABLE, 0);
    hal_write(REG_TIMER_CONTROL, CMD_RESTART);
    control_set(CONTROL_COUNTERS_ENABLE, CONTROL_COUNTERS_ENABLE);
}

uint8_t rfcounter_poll(double *hz)
{
    if (!(hal_read(REG_TIMER_CONTROL) & STATUS_GATE_DONE))
        return 0;

    hal_write(REG_RF_SELECT, 0);
    rfcounter_set_control(RF_CONTROL_RUN, 0);

    uint32_t count = 0, weight = 1;
    for (uint8_t digit = 0; digit < DIGITS; digit++) {
        hal_write(REG_RF_SELECT, (uint8_t)(digit << 4));
        count += (uint32_t)(hal_read(REG_RF_DATA) & 0x0Fu) * weight;
        weight *= 10;
    }

    /* The long fine gate counts single hertz, the other gates tens of hertz. */
    uint8_t single_hertz = !rf.coarse && hal_read(PARAM_COUNTER_FINE);
    double value = count * (single_hertz ? 1.0 : 10.0);

    if (value > FINE_MAX_HZ || value < FINE_MIN_HZ) {
        /* Far out of the fine range: count faster so that the search does not stall. */
        if (!rf.coarse)
            select_coarse();
        rf.unstable_left = UNSTABLE_READINGS;
    } else {
        double change = value - rf.last_hz;
        if ((change < 0 ? -change : change) < STABLE_HZ) {
            rf.unstable_left = UNSTABLE_READINGS;
            if (rf.coarse)
                select_fine();
        } else if (--rf.unstable_left == 0) {
            rf.unstable_left = UNSTABLE_READINGS;
            if (!rf.coarse)
                select_coarse();
        }
        rf.last_hz = value;
    }

    restart();
    *hz = value;
    return 1;
}
