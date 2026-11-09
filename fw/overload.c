#include "overload.h"

#include "app.h"
#include "hal.h"
#include "help.h"
#include "meter.h"
#include "regs.h"
#include "tones.h"
#include "screen.h"

#define STATUS_OVERLOAD 0x01u
#define TONE_SWITCH_BIT 0x08u   /* a filter switch; toggling it at audio rate makes the tone */

#define WAIT_STEP_MS 10u
#define WAIT_STEPS 230u         /* about 2.3 s of warning screen before the tone starts */
#define TONE_LENGTH_US 9000u    /* each pitch of the sweep */
#define SWEEP_PAUSE_MS 28u

#define BLOCK_CHAR 0xDBu
#define GLYPH_COLS 5u
#define GLYPH_ROWS 7u
#define GLYPH_PITCH 7u          /* column distance between letters */
#define HINT_ROW 30u

typedef enum { STATE_CLEAR, STATE_WAITING, STATE_SOUNDING } overload_state;

typedef struct {
    char letter;
    uint8_t rows[GLYPH_ROWS];   /* five bits each, bit 4 is the left column */
} glyph;

static const glyph glyphs[] = {
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'M', {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
};

/* Half periods of one rising sweep, in microseconds. */
static const uint8_t half_period_us[] = {145, 125, 110, 95, 80, 70};

static overload_state state;
static uint16_t waited;
static uint8_t saved_filter;

static uint8_t text_length(const char *text)
{
    uint8_t length = 0;

    while (text[length])
        length++;
    return length;
}

static void draw_letter(uint8_t col, uint8_t row, char letter)
{
    for (uint8_t i = 0; i < sizeof glyphs / sizeof glyphs[0]; i++) {
        if (glyphs[i].letter != letter)
            continue;
        for (uint8_t r = 0; r < GLYPH_ROWS; r++)
            for (uint8_t c = 0; c < GLYPH_COLS; c++)
                if (glyphs[i].rows[r] & (0x10u >> c))
                    screen_put((uint8_t)(col + c), (uint8_t)(row + r), BLOCK_CHAR);
        return;
    }
}

static void draw_word(uint8_t row, const char *word)
{
    uint8_t width = (uint8_t)(text_length(word) * GLYPH_PITCH - (GLYPH_PITCH - GLYPH_COLS));
    uint8_t col = (uint8_t)((SCREEN_COLS - width) / 2u);

    for (; *word; word++, col = (uint8_t)(col + GLYPH_PITCH))
        draw_letter(col, row, *word);
}

/* The status line does not say which socket it was, but the active socket nearly always does. */
static const char *hint_text(void)
{
    switch (app_overload_input()) {
    case OVERLOAD_INPUT_N:
        return "N SOCKET PAD TOO HOT";
    case OVERLOAD_INPUT_BNC:
        return "BNC INPUT OVERLOAD";
    default:
        return "BNC OVERLOAD OR N PAD TOO HOT";
    }
}

static void show_warning(void)
{
    const char *hint = hint_text();

    screen_clear();
    draw_word(3, "REMOVE");
    draw_word(12, "RF");
    draw_word(21, "INPUT");
    screen_print((uint8_t)((SCREEN_COLS - text_length(hint)) / 2u), HINT_ROW, hint);
}

static void sound_tone(uint8_t half_period)
{
    uint8_t value = meter_filter_register();

    for (uint16_t elapsed = 0; elapsed < TONE_LENGTH_US; elapsed = (uint16_t)(elapsed + half_period)) {
        value ^= TONE_SWITCH_BIT;
        hal_write(REG_AF_FILTER, value);
        hal_delay_us(half_period);
    }
    hal_write(REG_AF_FILTER, meter_filter_register());
}

static void end_warning(void)
{
    if (state == STATE_SOUNDING)
        meter_set_filter_extra(METER_FILTER_MASK, saved_filter);
    state = STATE_CLEAR;
    if (help_active())
        help_redraw();
    else if (tones_active())
        tones_redraw();
    else
        app_restore_screen();
}

uint8_t overload_poll(void)
{
    if (!(hal_read(REG_STATUS) & STATUS_OVERLOAD)) {
        if (state != STATE_CLEAR)
            end_warning();
        return 0;
    }

    switch (state) {
    case STATE_CLEAR:
        state = STATE_WAITING;
        waited = 0;
        show_warning();
        break;
    case STATE_WAITING:
        hal_delay_ms(WAIT_STEP_MS);
        if (++waited >= WAIT_STEPS) {
            state = STATE_SOUNDING;
            saved_filter = meter_filter_register();
            meter_set_filter(AF_FILTER_BAND_PASS);
        }
        break;
    case STATE_SOUNDING:
        for (uint8_t i = 0; i < sizeof half_period_us; i++)
            sound_tone(half_period_us[i]);
        hal_delay_ms(SWEEP_PAUSE_MS);
        break;
    }
    return 1;
}

uint8_t overload_active(void)
{
    return state != STATE_CLEAR;
}
