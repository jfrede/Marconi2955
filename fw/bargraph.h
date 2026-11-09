/* Vertical bar charts for the measured values, with automatic scale selection. */
#ifndef BARGRAPH_H
#define BARGRAPH_H

#include <stdint.h>

/* Scale indices, as used by bargraph_config. */
#define BAR_SCALE_100M 0u
#define BAR_SCALE_10 4u
#define BAR_SCALE_30 5u
#define BAR_SCALE_100 6u
#define BAR_SCALE_18 14u
#define BAR_SCALE_50 15u

typedef struct {
    uint8_t bar_col, tick_col;       /* screen columns of the bar and its tick marks */
    uint8_t scale_col, unit_col;     /* screen columns of the full-scale value and unit */
    uint8_t min_scale, max_scale;
    uint8_t scale;                   /* currently selected scale */
    uint8_t top_row;                 /* row of the top of the bar */
    uint8_t mirrored;                /* extends the bar the other way, sharing the tick marks */
} bargraph;

/* Draws the empty bar. initial_scale may lie outside the limits; it forces a first label. */
void bargraph_init(bargraph *bar, uint8_t bar_col, uint8_t tick_col, uint8_t scale_col,
                   uint8_t unit_col, uint8_t min_scale, uint8_t max_scale, uint8_t initial_scale);

/* Selects the scale for value, redraws the labels if it changed, and draws the bar. */
void bargraph_update(bargraph *bar, double value, const char *unit);

/* Empties the bar without changing its scale. */
void bargraph_empty(bargraph *bar);

/* The second bar of a pair: same scale as main, drawn on the other side of the ticks. */
void bargraph_init_mirrored(bargraph *bar, uint8_t bar_col);
void bargraph_update_mirrored(bargraph *bar, const bargraph *main_bar, double value);

#endif
