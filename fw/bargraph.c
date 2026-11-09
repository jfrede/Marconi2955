#include "bargraph.h"

#include <math.h>

#include "screen.h"

#define SCALE_COUNT 16u
#define PERCENT_FULL 100u
#define SCALE_DOWN_EVEN 25u /* a reading below this percentage selects the next lower scale */
#define SCALE_DOWN_ODD 27u

/* Character cells of the bar chart. */
#define ROW_LABEL 19u
#define ROW_TOP_CAP 18u
#define ROW_BAR_TOP 19u
#define ROW_BAR_BOTTOM 31u
#define CHAR_TOP_CAP 0xEEu
#define CHAR_EMPTY 0xE0u
#define CHAR_FULL 0xE8u
#define CHAR_PARTIAL_BASE 0xE1u   /* seven partly filled cells, one to seven pixels */
#define CHAR_BOTTOM_BASE 0xE9u    /* bottom cell, zero to four pixels */
#define CHAR_BOTTOM_FULL 0xEDu
#define CHAR_TICK_MAJOR 0xFFu
#define CHAR_TICK_MINOR 0xEFu
#define CHAR_TICK_THREE 0xFEu
#define BOTTOM_CELL_PIXELS 5u
#define CELL_PIXELS 8u
#define MIRRORED_OFFSET 0x10u

/* Full-scale values. */
static const double scale_values[SCALE_COUNT] = {
    0.1, 0.3, 1.0, 3.0, 10.0, 30.0, 100.0, 300.0,
    1000.0, 3000.0, 10000.0, 30000.0, 100000.0, 300000.0, 18.0, 50.0,
};

/* Keeps exact percentages from falling just short in binary floating point. */
#define PERCENT_GUARD 1e-9

static const char *const scale_labels[SCALE_COUNT] = {
    "100", "300", "  1", "  3", " 10", " 30", "100", "300",
    "  1", "  3", " 10", " 30", "100", "300", " 18", " 50",
};

/* Decade scales (1, 10, 100...) have other tick marks than the 3-times scales. */
static const uint8_t decade_scale[SCALE_COUNT] = {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1,
};

static uint8_t percent_of_scale(double value, uint8_t scale)
{
    double percent = floor(value * 100.0 / scale_values[scale] + PERCENT_GUARD);
    if (percent < 0.0)
        return 0;
    return percent >= PERCENT_FULL ? (uint8_t)PERCENT_FULL : (uint8_t)percent;
}

static uint8_t down_threshold(uint8_t scale)
{
    return (scale & 1u) ? SCALE_DOWN_ODD : SCALE_DOWN_EVEN;
}

static uint8_t select_scale(const bargraph *bar, double value)
{
    uint8_t scale = bar->scale;

    if (scale > bar->max_scale)
        scale = bar->max_scale;
    else if (scale <= bar->min_scale)
        scale = bar->min_scale;

    while (scale < bar->max_scale && percent_of_scale(value, scale) == PERCENT_FULL)
        scale++;
    while (scale > bar->min_scale && percent_of_scale(value, scale) < down_threshold(scale))
        scale--;
    return scale;
}

static void draw_frame(uint8_t col, uint8_t offset)
{
    screen_put(col, ROW_TOP_CAP, CHAR_TOP_CAP);
    for (uint8_t row = ROW_BAR_TOP; row < ROW_BAR_BOTTOM; row++)
        screen_put(col, row, (uint8_t)(CHAR_EMPTY + offset));
    screen_put(col, ROW_BAR_BOTTOM, (uint8_t)(CHAR_BOTTOM_BASE + offset));
}

static void draw_ticks(uint8_t col, uint8_t decade)
{
    screen_put(col, ROW_TOP_CAP, CHAR_TOP_CAP);
    screen_put(col, ROW_BAR_BOTTOM, CHAR_TICK_MINOR);
    if (decade) {
        screen_put(col, 29, CHAR_TICK_MAJOR);
        screen_put(col, 24, CHAR_TICK_MAJOR);
        screen_put(col, 26, CHAR_TICK_MINOR);
        screen_put(col, 21, CHAR_TICK_MINOR);
        screen_put(col, 27, ' ');
        screen_put(col, 23, ' ');
    } else {
        screen_put(col, 27, CHAR_TICK_MINOR);
        screen_put(col, 23, CHAR_TICK_THREE);
        screen_put(col, 29, ' ');
        screen_put(col, 26, ' ');
        screen_put(col, 24, ' ');
        screen_put(col, 21, ' ');
    }
}

/* Draws the bar for 0 to 100 pixels and returns its new top row. */
static uint8_t draw_bar(uint8_t col, uint8_t pixels, uint8_t old_top, uint8_t offset)
{
    uint8_t cell, top;

    if (pixels < BOTTOM_CELL_PIXELS) {
        cell = (uint8_t)(CHAR_BOTTOM_BASE + offset + pixels);
        top = ROW_BAR_BOTTOM;
    } else {
        uint8_t above_bottom = (uint8_t)(pixels - BOTTOM_CELL_PIXELS);
        cell = (uint8_t)(CHAR_PARTIAL_BASE + offset + (above_bottom & (CELL_PIXELS - 1u)));
        top = (uint8_t)(ROW_BAR_BOTTOM - ((pixels + 3u) >> 3));
        if (old_top == ROW_BAR_BOTTOM) {
            screen_put(col, ROW_BAR_BOTTOM, (uint8_t)(CHAR_BOTTOM_FULL + offset));
            old_top = ROW_BAR_BOTTOM - 1u;
        }
    }

    if (top < old_top) {
        for (uint8_t row = old_top; row > top; row--)
            screen_put(col, row, (uint8_t)(CHAR_FULL + offset));
    } else {
        for (uint8_t row = old_top; row < top; row++)
            screen_put(col, row, (uint8_t)(CHAR_EMPTY + offset));
    }
    screen_put(col, top, cell);
    return top;
}

void bargraph_init(bargraph *bar, uint8_t bar_col, uint8_t tick_col, uint8_t scale_col,
                   uint8_t unit_col, uint8_t min_scale, uint8_t max_scale, uint8_t initial_scale)
{
    bar->bar_col = bar_col;
    bar->tick_col = tick_col;
    bar->scale_col = scale_col;
    bar->unit_col = unit_col;
    bar->min_scale = min_scale;
    bar->max_scale = max_scale;
    bar->scale = initial_scale;
    bar->top_row = ROW_BAR_TOP;

    bar->mirrored = 0;
    draw_frame(bar_col, 0);
    draw_ticks(tick_col, 1);
    bar->top_row = draw_bar(bar_col, 0, bar->top_row, 0);
}

static uint8_t bars_hidden;

void bargraph_set_hidden(uint8_t hidden)
{
    bars_hidden = hidden;
}

void bargraph_update(bargraph *bar, double value, const char *unit)
{
    if (bars_hidden)
        return;
    uint8_t scale = select_scale(bar, value);

    bar->top_row = draw_bar(bar->bar_col, percent_of_scale(value, scale), bar->top_row, 0);
    if (scale != bar->scale) {
        bar->scale = scale;
        draw_ticks(bar->tick_col, decade_scale[scale]);
        screen_print(bar->unit_col, ROW_LABEL, unit);
        screen_print(bar->scale_col, ROW_LABEL, scale_labels[scale]);
    }
}

void bargraph_empty(bargraph *bar)
{
    if (bars_hidden)
        return;
    bar->top_row = draw_bar(bar->bar_col, 0, bar->top_row, 0);
}

void bargraph_init_mirrored(bargraph *bar, uint8_t bar_col)
{
    bar->bar_col = bar_col;
    bar->top_row = ROW_BAR_TOP;
    bar->mirrored = 1;
    draw_frame(bar_col, MIRRORED_OFFSET);
    bar->top_row = draw_bar(bar_col, 0, bar->top_row, MIRRORED_OFFSET);
}

void bargraph_update_mirrored(bargraph *bar, const bargraph *main_bar, double value)
{
    if (bars_hidden)
        return;
    bar->top_row = draw_bar(bar->bar_col, percent_of_scale(value, main_bar->scale), bar->top_row, MIRRORED_OFFSET);
}
