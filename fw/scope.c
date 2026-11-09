#include "scope.h"

#include "control.h"
#include "hal.h"
#include "meter.h"
#include "regs.h"
#include "screen.h"

#define VERTICAL_COUNT 11u
#define VERTICAL_LAST_TX 12u
#define VERTICAL_FIRST_AM 8u
#define VERTICAL_FIRST_PM 9u
#define VERTICAL_LAST_AM 10u
#define ROW_SCALE_BOTTOM 31u
#define COL_SCALE 3u
#define ROW_TX_TITLE 22u
#define COL_TX_TITLE 2u
#define ROW_TX_UNIT 26u
#define ROW_ZERO 24u
#define COL_ZERO 5u
#define CHAR_ARROW_UP 0x7Fu
#define CHAR_ARROW_DOWN 0x7Eu
#define VERTICAL_MILLIVOLT_LIMIT 6u /* below this index the scale is in mV */
#define TIMEBASE_COUNT 15u

#define ROW_TIMEBASE 15u
#define COL_TIMEBASE 19u
#define COL_PER_DIV 23u
#define ROW_COUPLING 18u
#define COL_COUPLING 2u
#define ROW_VERTICAL 22u
#define COL_VERTICAL 2u
#define ROW_VERTICAL_UNIT 24u
#define COL_VERTICAL_PREFIX 0u
#define COL_VERTICAL_UNIT 1u
#define COL_SWEEP 30u
#define ROW_SWEEP_TOP 14u
#define ROW_SWEEP 15u

/* Timebase control: EE00 bit 1 stops the timebase while its counter is reprogrammed. */
#define TIMEBASE_ENABLE 0x02u
#define CMD_POINT_COUNTER5_LOAD 0x05u
#define MODE_FASTEST_LOW 0x20u
#define MODE_SLOW_LOW 0x25u
#define MODE_SLOW_HIGH 0x15u

/* EE00 bits 4-3: 01 repetitive sweep, 10 single sweep, followed by 11. */
#define SWEEP_MASK 0xE7u
#define SWEEP_REPEAT 0x08u
#define SWEEP_SINGLE 0x10u
#define SWEEP_IDLE 0x18u

static const char *const vertical_labels[VERTICAL_COUNT] = {
    "10 ", "20 ", "50 ", "100", "200", "500", "1  ", "2  ", "5  ", "10 ", "20 ",
};

/* Counter 5 load value for each timebase step; the first runs with the counter's fixed clock. */
static const uint16_t timebase_counts[TIMEBASE_COUNT] = {
    0, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000,
};

/* Character codes: the micro sign is 0x24 and the milli sign 0x27 in this character set. */
static const char *const timebase_labels[TIMEBASE_COUNT] = {
    "100\x24", "200\x24", "500\x24", "  1'", "  2'", "  5'", " 10'", " 20'", " 50'",
    "100'", "200'", "500'", "   1", "   2", "   5",
};

/* Graticule: the character set holds pieces of grid line for each position in a cell. */
#define GRATICULE_FIRST_COL 6u
#define GRATICULE_WIDTH 29u
#define GRATICULE_FIRST_ROW 16u
#define GRATICULE_ROWS 16u
#define GRATICULE_JUNCTIONS 11u

typedef struct {
    uint8_t fill_a, fill_b;                 /* the two alternating pieces between vertical lines */
    uint8_t junctions[GRATICULE_JUNCTIONS]; /* the crossings with the vertical lines, left to right */
} graticule_row;

/* Across a row: '+' is a crossing, 'a' and 'b' are the fill pieces. */
static const char graticule_columns[GRATICULE_WIDTH + 1u] = "+ab+b+ba+ab+ba+ab+ba+ab+b+ba+";

enum { ROW_TOP, ROW_DOTS_UPPER, ROW_PLAIN, ROW_MINOR, ROW_LINE, ROW_DOTS_LOWER, ROW_BOTTOM };

static const graticule_row graticule_types[] = {
    [ROW_TOP] = {0x82, 0x82, {0x81, 0x88, 0x9E, 0x8D, 0x8A, 0xA0, 0xA1, 0xA2, 0x88, 0x9E, 0xA3}},
    [ROW_DOTS_UPPER] = {0xAF, 0xB0, {0xAE, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xAE, 0xB1, 0xB2, 0xB3}},
    [ROW_PLAIN] = {0x80, 0x80, {0x83, 0x84, 0x85, 0x86, 0x87, 0x89, 0x8B, 0x83, 0x84, 0x85, 0x86}},
    [ROW_MINOR] = {0x9F, 0x9F, {0x8C, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x8E, 0x8F, 0x9D}},
    [ROW_LINE] = {0x82, 0x82, {0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x96, 0x97, 0xAD}},
    [ROW_DOTS_LOWER] = {0xB8, 0xB9, {0xB7, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xB7, 0xBA, 0xBB, 0xBC}},
    [ROW_BOTTOM] = {0x82, 0x82, {0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xA5, 0xA6, 0xAC}},
};

static const uint8_t graticule_layout[GRATICULE_ROWS] = {
    ROW_TOP, ROW_DOTS_UPPER, ROW_PLAIN, ROW_MINOR, ROW_PLAIN, ROW_LINE, ROW_PLAIN, ROW_PLAIN,
    ROW_MINOR, ROW_PLAIN, ROW_LINE, ROW_PLAIN, ROW_PLAIN, ROW_MINOR, ROW_DOTS_LOWER, ROW_BOTTOM,
};

static struct {
    uint8_t vertical[SCOPE_PM + 1];
    uint8_t timebase, single_sweep;
    scope_kind kind;
} scope;

void scope_init(void)
{
    scope.vertical[SCOPE_RX] = 10;
    scope.vertical[SCOPE_AM] = 10;
    scope.vertical[SCOPE_FM] = 12;
    scope.vertical[SCOPE_PM] = 12;
    scope.kind = SCOPE_RX;
    scope.timebase = 0;
    scope.single_sweep = 0;
}

static void print_codes(uint8_t col, uint8_t row, const char *codes)
{
    while (*codes)
        screen_put(col++, row, (uint8_t)*codes++);
}

static uint8_t graticule_code(const graticule_row *type, uint8_t col, uint8_t *junction)
{
    switch (graticule_columns[col]) {
    case '+': return type->junctions[(*junction)++];
    case 'a': return type->fill_a;
    default: return type->fill_b;
    }
}

static void draw_graticule(void)
{
    for (uint8_t col = 0; col < SCREEN_COLS; col++)
        screen_put(col, ROW_TIMEBASE, ' ');

    for (uint8_t i = 0; i < GRATICULE_ROWS; i++) {
        const graticule_row *type = &graticule_types[graticule_layout[i]];
        uint8_t row = (uint8_t)(GRATICULE_FIRST_ROW + i);
        uint8_t junction = 0;

        for (uint8_t col = 0; col < SCREEN_COLS; col++) {
            uint8_t inside = col >= GRATICULE_FIRST_COL && col < GRATICULE_FIRST_COL + GRATICULE_WIDTH;

            screen_put(col, row, inside ? graticule_code(type, (uint8_t)(col - GRATICULE_FIRST_COL), &junction) : ' ');
        }
    }
}

static void apply_timebase(void)
{
    uint8_t step = scope.timebase;

    control_set(TIMEBASE_ENABLE, 0);
    hal_write(REG_TIMER_CONTROL, CMD_POINT_COUNTER5_LOAD);
    if (step == 0) {
        hal_write(REG_TIMER_DATA, MODE_FASTEST_LOW);
    } else {
        hal_write(REG_TIMER_DATA, MODE_SLOW_LOW);
        hal_write(REG_TIMER_DATA, MODE_SLOW_HIGH);
        hal_write(REG_TIMER_DATA, (uint8_t)timebase_counts[step]);
        hal_write(REG_TIMER_DATA, (uint8_t)(timebase_counts[step] >> 8));
    }
    print_codes(COL_TIMEBASE, ROW_TIMEBASE, timebase_labels[step]);
    control_set(TIMEBASE_ENABLE, TIMEBASE_ENABLE);
}

static void draw_peak_scale(const char *label, uint8_t sign_col)
{
    for (uint8_t col = 0; col < GRATICULE_FIRST_COL; col++) {
        screen_put(col, GRATICULE_FIRST_ROW, ' ');
        screen_put(col, ROW_SCALE_BOTTOM, ' ');
    }
    print_codes(COL_SCALE, GRATICULE_FIRST_ROW, label);
    print_codes(COL_SCALE, ROW_SCALE_BOTTOM, label);
    screen_put(sign_col, GRATICULE_FIRST_ROW, '+');
    screen_put(sign_col, ROW_SCALE_BOTTOM, '-');
}

static void apply_vertical(void)
{
    /* Peak scales from index 8: label and the column of its sign. */
    static const char *const fm_labels[5] = {"1.5", "  3", "  6", " 15", " 30"};
    static const uint8_t fm_sign_cols[5] = {2, 4, 4, 3, 3};
    static const char *const pm_labels[4] = {"1.5", "  3", "7.5", " 15"};
    static const uint8_t pm_sign_cols[4] = {2, 4, 2, 3};
    static const char *const am_labels[3] = {" 5", "10", "20"};
    uint8_t vertical = scope.vertical[scope.kind];

    meter_set_scope_gain(vertical);
    switch (scope.kind) {
    case SCOPE_RX:
        meter_set_scope_limits(vertical < VERTICAL_MILLIVOLT_LIMIT);
        print_codes(COL_VERTICAL, ROW_VERTICAL, vertical_labels[vertical]);
        screen_put(COL_VERTICAL_PREFIX, ROW_VERTICAL_UNIT,
                   vertical < VERTICAL_MILLIVOLT_LIMIT ? SCREEN_MILLI : ' ');
        break;
    case SCOPE_AM:
        print_codes(COL_VERTICAL, ROW_VERTICAL_UNIT, am_labels[vertical - VERTICAL_FIRST_AM]);
        break;
    case SCOPE_FM:
        draw_peak_scale(fm_labels[vertical - VERTICAL_FIRST_AM], fm_sign_cols[vertical - VERTICAL_FIRST_AM]);
        break;
    case SCOPE_PM:
        draw_peak_scale(pm_labels[vertical - VERTICAL_FIRST_PM], pm_sign_cols[vertical - VERTICAL_FIRST_PM]);
        break;
    }
}

static void apply_sweep(void)
{
    uint8_t mode = (uint8_t)((control_get() & SWEEP_MASK) | (scope.single_sweep ? SWEEP_SINGLE : SWEEP_REPEAT));

    if (scope.single_sweep) {
        print_codes(COL_SWEEP, ROW_SWEEP_TOP, ";;;;;");
        print_codes(COL_SWEEP, ROW_SWEEP, "s\x01swp");
    } else {
        print_codes(COL_SWEEP, ROW_SWEEP_TOP, "      ");
        print_codes(COL_SWEEP, ROW_SWEEP, "      ");
    }
    hal_write(REG_STATUS, mode);
    hal_write(REG_STATUS, (uint8_t)(mode | SWEEP_IDLE));
}

static void draw_kind_labels(uint8_t ac_coupled)
{
    switch (scope.kind) {
    case SCOPE_RX:
        screen_print(COL_VERTICAL_UNIT, ROW_VERTICAL_UNIT, "V/DIV");
        screen_print(COL_COUPLING, ROW_COUPLING, ac_coupled ? "AC" : "DC");
        break;
    case SCOPE_AM:
        screen_print(2, GRATICULE_FIRST_ROW, "PEAK");
        screen_put(3, ROW_COUPLING, CHAR_ARROW_UP);
        screen_print(COL_TX_TITLE, ROW_TX_TITLE, "AM");
        screen_print(1, ROW_TX_UNIT, "%/DIV");
        screen_put(3, 29, CHAR_ARROW_DOWN);
        screen_print(0, ROW_SCALE_BOTTOM, "TROUGH");
        break;
    case SCOPE_FM:
        screen_print(COL_TX_TITLE, ROW_TX_TITLE, "FM");
        screen_put(COL_ZERO, ROW_ZERO, '0');
        screen_print(COL_TX_TITLE, ROW_TX_UNIT, "kHz");
        break;
    case SCOPE_PM:
        screen_print(COL_TX_TITLE, ROW_TX_TITLE, "*M");
        screen_put(COL_ZERO, ROW_ZERO, '0');
        screen_print(1, ROW_TX_UNIT, "RADS");
        break;
    }
}

void scope_show(scope_kind kind, uint8_t ac_coupled)
{
    scope.kind = kind;
    draw_graticule();
    apply_timebase();
    screen_print(COL_PER_DIV, ROW_TIMEBASE, "!/DIV");
    apply_sweep();
    apply_vertical();
    draw_kind_labels(ac_coupled);
}

void scope_step_vertical(int8_t direction)
{
    static const uint8_t first[SCOPE_PM + 1] = {0, VERTICAL_FIRST_AM, VERTICAL_FIRST_AM, VERTICAL_FIRST_PM};
    static const uint8_t last[SCOPE_PM + 1] = {VERTICAL_COUNT - 1u, VERTICAL_LAST_AM, VERTICAL_LAST_TX, VERTICAL_LAST_TX};
    uint8_t *vertical = &scope.vertical[scope.kind];

    if ((direction > 0 && *vertical >= last[scope.kind]) || (direction < 0 && *vertical <= first[scope.kind]))
        return;
    *vertical = (uint8_t)(*vertical + direction);
    apply_vertical();
}

void scope_step_timebase(int8_t direction)
{
    if ((direction > 0 && scope.timebase + 1u >= TIMEBASE_COUNT) || (direction < 0 && scope.timebase == 0))
        return;
    scope.timebase = (uint8_t)(scope.timebase + direction);
    apply_timebase();
}

void scope_set_single_sweep(uint8_t single)
{
    scope.single_sweep = single;
    apply_sweep();
}

void scope_get_settings(uint8_t *timebase, uint8_t vertical[SCOPE_PM + 1], uint8_t *single)
{
    *timebase = scope.timebase;
    *single = scope.single_sweep;
    for (uint8_t i = 0; i <= SCOPE_PM; i++)
        vertical[i] = scope.vertical[i];
}

void scope_set_settings(uint8_t timebase, const uint8_t vertical[SCOPE_PM + 1], uint8_t single)
{
    static const uint8_t first[SCOPE_PM + 1] = {0, VERTICAL_FIRST_AM, VERTICAL_FIRST_AM, VERTICAL_FIRST_PM};
    static const uint8_t last[SCOPE_PM + 1] = {VERTICAL_COUNT - 1u, VERTICAL_LAST_AM, VERTICAL_LAST_TX, VERTICAL_LAST_TX};

    scope.timebase = timebase < TIMEBASE_COUNT ? timebase : 0u;
    scope.single_sweep = single;
    for (uint8_t i = 0; i <= SCOPE_PM; i++)
        if (vertical[i] >= first[i] && vertical[i] <= last[i])
            scope.vertical[i] = vertical[i];
}
