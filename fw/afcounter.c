#include "afcounter.h"

#include "hal.h"
#include "regs.h"

#define CMD_POINT_COUNTER3_HOLD 0x13u
#define CMD_SAVE_COUNTER3 0xA4u
#define COUNT_NO_EDGES 2u /* the counter holds this value while no input edges arrive */

/* EE00 bits 4-3 report the prescaler the hardware selected for the period measurement. */
#define STATUS_RANGE_MASK 0x18u
#define STATUS_RANGE_LOW 0x00u
#define STATUS_RANGE_HIGH 0x18u

#define CLOCK_RANGE_LOW 500000.0
#define CLOCK_RANGE_MID 5000000.0
#define CLOCK_RANGE_HIGH 50000000.0

#define POLLS_PER_SAMPLE 11u
#define SAMPLES_PER_READING 16u
#define NO_SIGNAL_SAMPLES 20u

static struct {
    uint8_t poll_count, samples, no_signal_left, reported_none;
    uint16_t last_count;
    double sum, hz;
} counter;

static uint16_t read_hold(void)
{
    hal_write(REG_TIMER_CONTROL, CMD_POINT_COUNTER3_HOLD);
    uint8_t low = hal_read(REG_TIMER_DATA);
    uint8_t high = hal_read(REG_TIMER_DATA);
    return (uint16_t)(low | ((uint16_t)high << 8));
}

static double clock_for_range(uint8_t range)
{
    if (range == STATUS_RANGE_LOW)
        return CLOCK_RANGE_LOW;
    return range == STATUS_RANGE_HIGH ? CLOCK_RANGE_HIGH : CLOCK_RANGE_MID;
}

void afcounter_init(void)
{
    counter.poll_count = counter.samples = 0;
    counter.reported_none = 0;
    counter.no_signal_left = NO_SIGNAL_SAMPLES;
    counter.last_count = COUNT_NO_EDGES;
    counter.sum = counter.hz = 0.0;
}

double afcounter_hz(void)
{
    return counter.hz;
}

uint8_t afcounter_sample(double *hz)
{
    uint16_t period = read_hold();
    uint8_t range = hal_read(REG_STATUS) & STATUS_RANGE_MASK;
    hal_write(REG_TIMER_CONTROL, CMD_SAVE_COUNTER3);
    uint16_t count = read_hold();

    if (count == COUNT_NO_EDGES) {
        counter.last_count = COUNT_NO_EDGES;
        *hz = 0.0;
        return 1;
    }
    uint8_t stale = period == counter.last_count;
    counter.last_count = count;
    if (stale || period == 0)
        return 0;
    *hz = clock_for_range(range) / period;
    return 1;
}

uint8_t afcounter_poll(void)
{
    if (++counter.poll_count < POLLS_PER_SAMPLE)
        return 0;
    counter.poll_count = 0;

    uint16_t period = read_hold();
    uint8_t range = hal_read(REG_STATUS) & STATUS_RANGE_MASK;
    hal_write(REG_TIMER_CONTROL, CMD_SAVE_COUNTER3);
    uint16_t count = read_hold();

    if (count == COUNT_NO_EDGES) {
        if (counter.reported_none || --counter.no_signal_left)
            return 0;
        counter.reported_none = 1;
        counter.last_count = COUNT_NO_EDGES;
        counter.samples = 0;
        counter.sum = counter.hz = 0.0;
        return 1;
    }

    uint8_t stale = period == counter.last_count;
    counter.last_count = count;
    if (stale || period == 0)
        return 0;

    counter.reported_none = 0;
    counter.no_signal_left = NO_SIGNAL_SAMPLES;
    counter.sum += clock_for_range(range) / period;
    if (++counter.samples < SAMPLES_PER_READING)
        return 0;
    counter.hz = counter.sum / SAMPLES_PER_READING;
    counter.sum = 0.0;
    counter.samples = 0;
    return 1;
}
