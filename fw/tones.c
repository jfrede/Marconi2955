#include "tones.h"

#include "afcounter.h"
#include "app.h"
#include "format.h"
#include "hal.h"
#include "keys.h"
#include "params.h"
#include "screen.h"

/* The mode keys beside the screen, from the top. */
#define POS_0 0u
#define POS_1 1u
#define POS_2 2u
#define POS_3 3u
#define POS_4 4u
#define POS_5 5u
#define POS_6 6u
#define POS_7 7u
#define ROW_PER_KEY 4u

#define SK_0 KEY_TX
#define SK_1 KEY_RX
#define SK_2 KEY_DUPLEX
#define SK_3 KEY_TONES
#define SK_4 KEY_BAR_CHART
#define SK_5 KEY_SCOPE
#define SK_6 KEY_HOLD_DISPLAY
#define SK_7 KEY_HELP

#define KEY_MILLISECONDS KEY_BAR_CHART

#define STANDARD_COUNT 4u
#define USER_STANDARD STANDARD_COUNT
#define TONE_NUMBERS 15u
#define SENT_TONES_MAX 11u
#define RECEIVED_TONES_MAX 12u
#define TONE_EMPTY (-2)
#define TONE_NULL (-1)
#define TONE_ENTRY_SIZE 3u
#define ENTRY_SIZE 12u
#define EXTENDED_FROM_LAST 5u  /* the tone extended is the fifth from the last */
#define EXTEND_FACTOR 10u
#define EXTEND_LIMIT_MS 700u
#define GAP_TONES 2u           /* pause between repeats, in tone durations */
#define SHIFT_MAX 9

#define USER_FREQ_ADDRESS 0xF6E0u /* 15 words: 0.1 Hz, or bit 15 and Hz above 3276.7 Hz */
#define USER_DURATION_ADDRESS 0xF6FEu /* 0.1 ms */
#define USER_FREQ_HZ_FLAG 0x8000u
#define USER_FREQ_TENTHS_LIMIT 32768u
#define USER_MIN 200u          /* 20 Hz in 0.1 Hz */
#define USER_MAX 200000u       /* 20 kHz */
#define USER_DURATION_MIN_MS 10u
#define USER_DURATION_MAX_MS 999u

#define SAT_FREQ_MIN 200u
#define SAT_FREQ_MAX 200000u
#define SAT_LEVEL_MAX 25500u   /* 0.1 Hz of deviation */

/* Decoder limits, from the instrument's specification. */
#define DECODE_MIN_HZ 280.0
#define DECODE_MAX_HZ 3600.0
#define DECODE_TOLERANCE 0.025
#define DECODE_MIN_TONE_MS 20u
#define DECODE_MIN_PAUSE_MS 25u
#define DECODE_END_PAUSE_MS 800u
#define MATCH_LIMIT_PERCENT 5.0
#define ERROR_WARN_PERCENT 2.0

#define FREQ_TENTHS_KHZ_FROM 10000u   /* 1 kHz, in 0.1 Hz */
#define FREQ_TENTHS_3_DECIMALS 100000u

typedef enum {
    VIEW_CLOSED,
    VIEW_MENU,
    VIEW_SAT,
    VIEW_TWO_TONE,
    VIEW_SEND,
    VIEW_RECEIVE,
    VIEW_USER,
} tones_view;

typedef enum { ENTRY_NONE, ENTRY_SAT_FREQ, ENTRY_SAT_LEVEL, ENTRY_TONE_NUMBER, ENTRY_SHIFT, ENTRY_USER_FREQ, ENTRY_DURATION } entry_kind;

typedef enum { SEND_IDLE, SEND_BURST, SEND_CONTINUOUS, SEND_STEP } send_mode;

typedef enum { DECODE_READY, DECODE_ACTIVE, DECODE_DONE } decode_state;

typedef struct {
    const char *name;
    uint16_t duration_ms;
    uint16_t tones[TONE_NUMBERS]; /* 0.1 Hz */
} tone_standard;

/* The published tables of the calling tone standards. */
static const tone_standard standards[STANDARD_COUNT] = {
    {"CCIR", 100, {19810, 11246, 11970, 12750, 13580, 14460, 15400, 16400, 17470, 18600, 24000, 9300, 22470, 9910, 21100}},
    {"ZVEI", 70, {24000, 10606, 11600, 12700, 14000, 15300, 16700, 18300, 20000, 22000, 28000, 8100, 9700, 8850, 26000}},
    {"DZVEI", 70, {22000, 9700, 10606, 11600, 12700, 14000, 15300, 16700, 18300, 20000, 24000, 26000, 8850, 8250, 8100}},
    {"EEA", 40, {19810, 11240, 11970, 12750, 13580, 14460, 15400, 16400, 17470, 18600, 10550, 9300, 22470, 9910, 21100}},
};

typedef struct {
    double hz;
    uint16_t ms;
    uint8_t pause;
} received_tone;

static struct {
    tones_view view;
    uint8_t transmit;
    uint8_t standard;
    entry_kind entry_kind;
    char entry[ENTRY_SIZE];
    uint8_t entry_len;
    int8_t list[SENT_TONES_MAX];
    uint8_t count, cursor;
    int8_t shift;
    uint8_t extended;
    uint8_t user_cursor;
    /* sending */
    send_mode mode;
    int8_t position;
    uint32_t change_at;
    /* receiving */
    decode_state decode;
    received_tone received[RECEIVED_TONES_MAX];
    uint8_t received_count;
    double candidate_sum;
    uint16_t candidate_samples;
    uint32_t candidate_start, silent_since;
    uint8_t signal_present, candidate_valid;
} t;

/* ---- values ------------------------------------------------------------------------------ */

static uint16_t user_word(uint8_t index)
{
    uint16_t address = (uint16_t)(USER_FREQ_ADDRESS + index * 2u);

    return (uint16_t)(param_get(address) | ((uint16_t)param_get((uint16_t)(address + 1u)) << 8));
}

static uint32_t user_tenths(uint8_t index)
{
    uint16_t word = user_word(index);

    return (word & USER_FREQ_HZ_FLAG) ? (uint32_t)(word & ~USER_FREQ_HZ_FLAG) * 10u : word;
}

static void store_word(uint16_t address, uint16_t value)
{
    param_set(address, (uint8_t)value);
    param_set((uint16_t)(address + 1u), (uint8_t)(value >> 8));
}

static uint32_t standard_tenths(uint8_t number)
{
    return t.standard == USER_STANDARD ? user_tenths(number) : standards[t.standard].tones[number];
}

static uint32_t shifted_tenths(uint8_t number)
{
    int32_t base = (int32_t)standard_tenths(number);

    return (uint32_t)(base + (base * t.shift + (t.shift < 0 ? -50 : 50)) / 100);
}

static uint16_t base_duration_ms(void)
{
    if (t.standard == USER_STANDARD) {
        uint16_t tenths = (uint16_t)(param_get(USER_DURATION_ADDRESS)
                                     | ((uint16_t)param_get((uint16_t)(USER_DURATION_ADDRESS + 1u)) << 8));
        return (uint16_t)(tenths / 10u);
    }
    return standards[t.standard].duration_ms;
}

static uint16_t tone_duration_ms(uint8_t index)
{
    uint16_t ms = base_duration_ms();

    if (t.extended && index + EXTENDED_FROM_LAST == t.count) {
        uint32_t longer = (uint32_t)ms * EXTEND_FACTOR;

        if (t.standard != USER_STANDARD && longer > EXTEND_LIMIT_MS)
            longer = EXTEND_LIMIT_MS;
        return (uint16_t)longer;
    }
    return ms;
}

static const char *standard_name(void)
{
    return t.standard == USER_STANDARD ? "USER DEFINED" : standards[t.standard].name;
}

static void format_frequency(char *out, uint32_t tenths)
{
    if (tenths < FREQ_TENTHS_KHZ_FROM)
        put_text(put_fixed(out, tenths, 1), "Hz");
    else if (tenths < FREQ_TENTHS_3_DECIMALS)
        put_text(put_fixed(out, tenths, 4), "kHz");
    else
        put_text(put_fixed(out, tenths / 10u, 3), "kHz");
}

static void format_mod_level(char *out, uint32_t tenths)
{
    if (tenths < FREQ_TENTHS_KHZ_FROM)
        put_text(put_fixed(out, tenths / 10u, 0), "Hz");
    else
        put_text(put_fixed(out, tenths / 10u, 3), "kHz");
}

/* ---- screen helpers ----------------------------------------------------------------------- */

static void box(uint8_t position, uint8_t width, const char *text)
{
    screen_softkey_box(position, width);
    screen_print((uint8_t)(SCREEN_COLS - 2u - width), (uint8_t)(position * ROW_PER_KEY + 1u), text);
}

static void title(uint8_t col, const char *text)
{
    uint8_t length = 0;

    while (text[length])
        length++;
    screen_print(col, 0, text);
    for (uint8_t i = 0; i < length; i++)
        screen_put((uint8_t)(col + i), 1, SCREEN_UNDERLINE);
}

static void blank(uint8_t col, uint8_t row, uint8_t width)
{
    for (uint8_t i = 0; i < width; i++)
        screen_put((uint8_t)(col + i), row, ' ');
}

static void show_entry(uint8_t col, uint8_t row, uint8_t width)
{
    t.entry[t.entry_len] = 0;
    print_field(col, row, t.entry, width);
    blank(col, (uint8_t)(row + 1u), width);
    screen_put((uint8_t)(col + t.entry_len), (uint8_t)(row + 1u), SCREEN_CURSOR);
}

static void start_entry(entry_kind kind)
{
    t.entry_kind = kind;
    t.entry_len = 0;
    t.entry[0] = 0;
}

static int8_t entry_char(uint8_t code)
{
    static const uint8_t digit_keys[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};

    for (uint8_t i = 0; i < sizeof digit_keys; i++)
        if (digit_keys[i] == code)
            return (int8_t)('0' + i);
    if (code == KEY_POINT)
        return '.';
    return code == KEY_MINUS ? '-' : 0;
}

/* ---- the sequencer ------------------------------------------------------------------------ */

static void output_position(void)
{
    int8_t tone = t.list[t.position];

    app_tone_output(tone >= 0 ? shifted_tenths((uint8_t)tone) : 0u);
}

static void stop_sending(void)
{
    if (t.mode == SEND_IDLE)
        return;
    t.mode = SEND_IDLE;
    app_tone_output_end();
}

static void start_sending(send_mode mode)
{
    if (!t.count)
        return;
    t.mode = mode;
    t.position = 0;
    output_position();
    t.change_at = hal_millis() + tone_duration_ms(0);
}

static void step_tone(void)
{
    if (t.mode != SEND_STEP) {
        stop_sending();
        start_sending(SEND_STEP);
        return;
    }
    if ((uint8_t)(t.position + 1) >= t.count) {
        stop_sending();
        return;
    }
    t.position++;
    output_position();
}

static void sequencer_poll(void)
{
    if (t.mode != SEND_BURST && t.mode != SEND_CONTINUOUS)
        return;
    if ((int32_t)(hal_millis() - t.change_at) < 0)
        return;

    if (t.position < 0) {
        /* The gap before a repeat is over. */
        t.position = 0;
    } else if ((uint8_t)(t.position + 1) < t.count) {
        t.position++;
    } else if (t.mode == SEND_CONTINUOUS) {
        t.position = -1;
        app_tone_output(0);
        t.change_at += (uint32_t)base_duration_ms() * GAP_TONES;
        return;
    } else {
        stop_sending();
        return;
    }
    output_position();
    t.change_at += tone_duration_ms((uint8_t)t.position);
}

/* ---- the decoder -------------------------------------------------------------------------- */

static void add_received(double hz, uint16_t ms, uint8_t pause)
{
    if (t.received_count >= RECEIVED_TONES_MAX)
        return;
    t.received[t.received_count].hz = hz;
    t.received[t.received_count].ms = ms;
    t.received[t.received_count].pause = pause;
    t.received_count++;
}

static void close_candidate(uint32_t now)
{
    if (!t.candidate_valid)
        return;
    t.candidate_valid = 0;
    uint32_t ms = now - t.candidate_start;

    if (ms >= DECODE_MIN_TONE_MS && t.candidate_samples)
        add_received(t.candidate_sum / t.candidate_samples, (uint16_t)ms, 0);
}

static void start_candidate(double hz, uint32_t now)
{
    t.candidate_valid = 1;
    t.candidate_sum = hz;
    t.candidate_samples = 1;
    t.candidate_start = now;
}

static void draw_status(const char *text)
{
    print_field(32, 6, text, 7);
}

static void draw_received(void)
{
    char text[20];

    for (uint8_t i = 0; i < t.received_count; i++) {
        uint8_t row = (uint8_t)(8u + i * 2u);
        const received_tone *r = &t.received[i];

        blank(2, row, 26);
        if (r->pause) {
            screen_print(9, row, "NULL");
            continue;
        }
        put_text(text, "");
        double best_error = 100.0;
        int8_t best = -1;

        for (uint8_t n = 0; n < TONE_NUMBERS; n++) {
            double reference = standard_tenths(n) / 10.0;
            double error = (r->hz - reference) / reference * 100.0;
            double size = error < 0.0 ? -error : error;

            if (size < (best_error < 0.0 ? -best_error : best_error)) {
                best_error = error;
                best = (int8_t)n;
            }
        }
        format_frequency(text, round_scaled(r->hz, 10.0));
        screen_print(9, row, text);
        double size = best_error < 0.0 ? -best_error : best_error;

        if (best >= 0 && size <= MATCH_LIMIT_PERCENT) {
            put_fixed(text, (uint32_t)best, 0);
            screen_print(3, row, text);
            if (size > ERROR_WARN_PERCENT)
                screen_put(6, row, 'X');
            char *p = text;
            *p++ = best_error < 0.0 ? '-' : '+';
            put_text(put_fixed(p, round_scaled(size, 10.0), 1), "%");
            screen_print(20, row, text);
        }
    }
}

static void decode_reset(void)
{
    for (uint8_t i = 0; i < RECEIVED_TONES_MAX; i++)
        blank(2, (uint8_t)(8u + i * 2u), 26);
    t.received_count = 0;
    t.candidate_valid = 0;
    t.signal_present = 0;
    t.decode = DECODE_READY;
    draw_status("READY");
}

static void decode_finish(uint32_t now)
{
    close_candidate(now);
    t.decode = DECODE_DONE;
    draw_status("");
    draw_received();
}

static void decoder_poll(void)
{
    double hz;
    uint32_t now = hal_millis();

    if (t.decode == DECODE_DONE)
        return;
    if (!afcounter_sample(&hz)) {
        if (t.decode == DECODE_ACTIVE && !t.signal_present && now - t.silent_since >= DECODE_END_PAUSE_MS)
            decode_finish(now);
        return;
    }

    if (hz < DECODE_MIN_HZ || hz > DECODE_MAX_HZ) {
        if (t.signal_present) {
            close_candidate(now);
            t.signal_present = 0;
            t.silent_since = now;
        }
        if (t.decode == DECODE_ACTIVE && now - t.silent_since >= DECODE_END_PAUSE_MS)
            decode_finish(now);
        return;
    }

    if (!t.signal_present) {
        if (t.decode == DECODE_ACTIVE && now - t.silent_since >= DECODE_MIN_PAUSE_MS)
            add_received(0.0, (uint16_t)(now - t.silent_since), 1);
        t.signal_present = 1;
        start_candidate(hz, now);
        if (t.decode == DECODE_READY) {
            t.decode = DECODE_ACTIVE;
            draw_status("ACTIVE");
        }
    } else {
        double mean = t.candidate_sum / t.candidate_samples;
        double difference = hz > mean ? hz - mean : mean - hz;

        if (difference <= mean * DECODE_TOLERANCE) {
            t.candidate_sum += hz;
            t.candidate_samples++;
        } else {
            close_candidate(now);
            start_candidate(hz, now);
        }
    }
    if (t.received_count >= RECEIVED_TONES_MAX)
        decode_finish(now);
}

void tones_poll(void)
{
    sequencer_poll();
    if (t.view == VIEW_RECEIVE)
        decoder_poll();
}

uint8_t tones_continuous(void)
{
    return t.mode == SEND_CONTINUOUS;
}

/* ---- screens ------------------------------------------------------------------------------ */

static void draw_menu(void)
{
    screen_clear();
    title(4, "TONES STANDARD MENU");
    box(POS_0, 11, t.transmit ? "TWO TONES" : "SUB-AUDIBLE");
    box(POS_1, 11, "CCIR");
    box(POS_2, 11, "ZVEI");
    box(POS_3, 11, "DZVEI");
    box(POS_4, 11, "EEA");
    box(POS_5, 11, "USER DEFINE");
    box(POS_6, 11, "RETURN");
    t.view = VIEW_MENU;
}

static void draw_sat_values(void)
{
    char text[20];

    format_frequency(text, app_sat_freq());
    print_field(16, 4, text, 10);
    if (app_sat_is_am()) {
        put_text(put_fixed(text, app_sat_level(), 0), "%");
        print_field(16, 11, text, 10);
    } else {
        format_mod_level(text, app_sat_level());
        print_field(16, 11, text, 10);
    }
}

static void draw_sat(void)
{
    screen_clear();
    title(10, "RX SUB-AUDIBLE TONE");
    screen_print(5, 4, "FREQUENCY:");
    screen_print(5, 6, "USE DATA ENTRY KEYS");
    screen_print(5, 11, app_sat_is_am() ? "AM  LEVEL:" : "FM  LEVEL:");
    screen_print(5, 13, "USE DATA ENTRY KEYS");
    screen_print(5, 17, "AUDIBLE TONE MODULATION");
    screen_print(5, 19, "FREQUENCY NOW: 1kHz");
    box(POS_1, 9, "FREQUENCY");
    box(POS_3, 9, "MOD LEVEL");
    box(POS_5, 9, app_sat_on() ? "STOP TONE" : "SEND TONE");
    box(POS_6, 9, "RETURN");
    t.view = VIEW_SAT;
    start_entry(ENTRY_NONE);
    draw_sat_values();
}

static void draw_two_tone(void)
{
    screen_clear();
    title(16, "TWO TONE");
    screen_print(2, 4, "SECOND AUDIO TONE OF 1KHz");
    box(POS_5, 9, app_two_tone() ? "STOP TONE" : "SEND TONE");
    box(POS_6, 9, "RETURN");
    t.view = VIEW_TWO_TONE;
}

static uint8_t list_row(uint8_t index)
{
    return (uint8_t)(10u + index * 2u);
}

static void draw_list_entry(uint8_t index)
{
    char text[20];
    uint8_t row = list_row(index);
    int8_t tone = t.list[index];

    blank(2, row, 20);
    blank(2, (uint8_t)(row + 1u), 20);
    if (tone == TONE_EMPTY)
        return;
    if (tone == TONE_NULL) {
        screen_print(8, row, "NULL");
        return;
    }
    put_fixed(text, (uint32_t)tone, 0);
    screen_print(3, row, text);
    format_frequency(text, shifted_tenths((uint8_t)tone));
    screen_print(8, row, text);
    if (t.extended && index + EXTENDED_FROM_LAST == t.count)
        screen_put(5, row, 'E');
}

static void draw_list_cursor(void)
{
    if (t.cursor < SENT_TONES_MAX)
        screen_put((uint8_t)(3u + t.entry_len), (uint8_t)(list_row(t.cursor) + 1u), SCREEN_CURSOR);
}

static void draw_tone_list(void)
{
    for (uint8_t i = 0; i < SENT_TONES_MAX; i++)
        draw_list_entry(i);
    draw_list_cursor();
}

static void draw_shift(void)
{
    char text[8];
    char *p = text;

    blank(19, 21, 5);
    blank(20, 23, 6);
    if (t.entry_kind == ENTRY_SHIFT || t.shift)
        screen_print(19, 21, "SHIFT");
    if (!t.shift)
        return;
    *p++ = t.shift < 0 ? '-' : '+';
    put_text(put_fixed(p, (uint32_t)(t.shift < 0 ? -t.shift : t.shift), 0), "%");
    screen_print(20, 23, text);
}

static void draw_send_frame(void)
{
    screen_put(24, 9, 0xEEu);
    screen_put(24, 17, 0xEEu);
    for (uint8_t row = 10; row <= 16; row++)
        screen_put(24, row, 0xE0u);
    for (uint8_t col = 25; col <= 26; col++) {
        screen_put(col, 9, 0xEEu);
        screen_put(col, 13, 0xEEu);
        screen_put(col, 17, 0xEEu);
    }
}

static void draw_mod_summary(uint8_t col)
{
    char text[20];

    app_format_tone_mod(text);
    screen_print(col, 2, text);
}

static void draw_send(void)
{
    screen_clear();
    title(11, "RX SEQUENTIAL TONE");
    screen_print(0, 2, "STANDARD:");
    screen_print(10, 2, standard_name());
    draw_mod_summary(25);
    screen_print(9, 4, "ENTER TONE NUMBER");
    screen_print(9, 6, "USING DATA KEYS");
    screen_print(2, 8, "TONE   FREQ   TRANSMIT");
    for (uint8_t col = 2; col < 6; col++)
        screen_put(col, 9, SCREEN_UNDERLINE);
    for (uint8_t col = 9; col < 13; col++)
        screen_put(col, 9, SCREEN_UNDERLINE);
    for (uint8_t col = 16; col < 24; col++)
        screen_put(col, 9, SCREEN_UNDERLINE);
    draw_send_frame();
    box(POS_1, 10, "NEXT TONE");
    box(POS_2, 10, "TONE BURST");
    box(POS_3, 10, t.mode == SEND_CONTINUOUS ? "STOP TONES" : "CONTINUOUS");
    box(POS_4, 10, "TONE STEP");
    box(POS_5, 10, "FREQ SHIFT");
    box(POS_6, 10, "RETURN");
    box(POS_7, 10, t.extended ? "CANCEL E" : "EXTENDED");
    t.view = VIEW_SEND;
    start_entry(ENTRY_TONE_NUMBER);
    draw_tone_list();
    draw_shift();
}

static void draw_receive(void)
{
    screen_clear();
    title(11, "TX SEQUENTIAL TONE");
    screen_print(0, 2, "STANDARD:");
    screen_print(10, 2, standard_name());
    draw_mod_summary(25);
    screen_print(2, 5, "TONE    FREQ      ERROR");
    for (uint8_t col = 2; col < 6; col++)
        screen_put(col, 6, SCREEN_UNDERLINE);
    for (uint8_t col = 10; col < 14; col++)
        screen_put(col, 6, SCREEN_UNDERLINE);
    for (uint8_t col = 20; col < 25; col++)
        screen_put(col, 6, SCREEN_UNDERLINE);
    box(POS_2, 6, "RESET");
    box(POS_6, 6, "RETURN");
    t.view = VIEW_RECEIVE;
    decode_reset();
}

static void draw_user_duration(void)
{
    char text[12];

    put_text(put_fixed(text, base_duration_ms(), 0), "\x27!");
    print_field(27, 17, text, 7);
}

static void draw_user_row(uint8_t index)
{
    char text[20];
    uint8_t row = (uint8_t)(2u + index * 2u);

    put_fixed(text, index, 0);
    print_field(1, row, "", 4);
    screen_print(index < 10u ? 2u : 1u, row, text);
    format_frequency(text, user_tenths(index));
    print_field(6, row, text, 10);
}

static void draw_user_cursor(void)
{
    uint8_t row = (uint8_t)(3u + t.user_cursor * 2u);

    blank(6, row, 11);
    screen_put((uint8_t)(6u + t.entry_len), row, SCREEN_CURSOR);
}

static void draw_user(void)
{
    screen_clear();
    screen_print(1, 0, "TONE   FREQ");
    screen_print(18, 0, "USER DEFINED STANDARD");
    for (uint8_t col = 1; col < 5; col++)
        screen_put(col, 1, SCREEN_UNDERLINE);
    for (uint8_t col = 8; col < 12; col++)
        screen_put(col, 1, SCREEN_UNDERLINE);
    for (uint8_t col = 18; col < 39; col++)
        screen_put(col, 1, SCREEN_UNDERLINE);
    screen_print(17, 2, "ENTER FREQ USING");
    screen_print(17, 4, "DATA KEYS");
    screen_print(17, 10, "ENTER TONE DURATION");
    screen_print(17, 12, "IN \x27!");
    screen_print(17, 17, "DURATION:");
    box(POS_1, 9, "NEXT TONE");
    box(POS_3, 9, "DURATION");
    box(POS_4, 2, "\x27!");
    box(POS_5, 9, "CONTINUE");
    box(POS_6, 9, "RETURN");
    t.view = VIEW_USER;
    start_entry(ENTRY_USER_FREQ);
    for (uint8_t i = 0; i < TONE_NUMBERS; i++)
        draw_user_row(i);
    draw_user_duration();
    draw_user_cursor();
}

/* ---- keys --------------------------------------------------------------------------------- */

static void close_tones(void)
{
    t.view = VIEW_CLOSED;
    app_restore_screen();
}

/* Continues into the sequential screen for the chosen standard. */
static void open_standard(uint8_t standard)
{
    t.standard = standard;
    if (t.transmit) {
        draw_receive();
    } else {
        t.shift = 0;
        draw_send();
    }
}

static void menu_key(uint8_t code)
{
    switch (code) {
    case SK_0:
        if (t.transmit)
            draw_two_tone();
        else
            draw_sat();
        break;
    case SK_1:
        open_standard(0);
        break;
    case SK_2:
        open_standard(1);
        break;
    case SK_3:
        open_standard(2);
        break;
    case SK_4:
        open_standard(3);
        break;
    case SK_5:
        t.standard = USER_STANDARD;
        t.user_cursor = 0;
        draw_user();
        break;
    case SK_6:
        close_tones();
        break;
    default:
        break;
    }
}

static void entry_add(uint8_t code, uint8_t col, uint8_t row, uint8_t width)
{
    int8_t c = entry_char(code);

    if (c && t.entry_len + 1u < ENTRY_SIZE) {
        t.entry[t.entry_len++] = (char)c;
        show_entry(col, row, width);
    }
}

static void sat_key(uint8_t code)
{
    int32_t value;
    uint8_t row = t.entry_kind == ENTRY_SAT_LEVEL ? 11u : 4u;

    if (code == SK_1 || code == SK_3) {
        start_entry(code == SK_1 ? ENTRY_SAT_FREQ : ENTRY_SAT_LEVEL);
        row = code == SK_1 ? 4u : 11u;
        show_entry(16, row, 10);
    } else if (code == SK_5) {
        app_sat_toggle();
        draw_sat();
    } else if (code == SK_6) {
        close_tones();
    } else if (t.entry_kind != ENTRY_NONE && entry_char(code)) {
        entry_add(code, 16, row, 10);
    } else if (t.entry_kind != ENTRY_NONE && code == KEY_DELETE) {
        if (t.entry_len)
            t.entry_len--;
        show_entry(16, row, 10);
    } else if (t.entry_kind != ENTRY_NONE && (code == KEY_HZ || code == KEY_KHZ || code == KEY_AM_PERCENT)) {
        int32_t scale = code == KEY_KHZ ? 10000 : 10;

        if (code == KEY_AM_PERCENT)
            scale = 1;
        if (!parse_entry(t.entry, t.entry_len, scale, &value))
            return;
        if (t.entry_kind == ENTRY_SAT_FREQ && code != KEY_AM_PERCENT) {
            if (value < (int32_t)SAT_FREQ_MIN || value > (int32_t)SAT_FREQ_MAX)
                return;
            app_sat_set_freq((uint32_t)value);
        } else if (t.entry_kind == ENTRY_SAT_LEVEL) {
            if (value > (int32_t)SAT_LEVEL_MAX && code != KEY_AM_PERCENT)
                value = SAT_LEVEL_MAX;
            app_sat_set_level((uint32_t)value);
        } else {
            return;
        }
        blank(16, (uint8_t)(row + 1u), 10);
        start_entry(ENTRY_NONE);
        draw_sat_values();
    }
}

static void two_tone_key(uint8_t code)
{
    if (code == SK_5) {
        app_two_tone_toggle();
        draw_two_tone();
    } else if (code == SK_6) {
        close_tones();
    }
}

static void commit_tone(void)
{
    int8_t tone = TONE_NULL;

    if (t.entry_len) {
        int32_t value;

        if (!parse_entry(t.entry, t.entry_len, 1, &value) || value < 0 || value >= (int32_t)TONE_NUMBERS)
            return;
        tone = (int8_t)value;
    }
    if (t.cursor >= t.count && t.count < SENT_TONES_MAX)
        t.count = (uint8_t)(t.cursor + 1u);
    t.list[t.cursor] = tone;
    start_entry(ENTRY_TONE_NUMBER);
    t.cursor = (uint8_t)((t.cursor + 1u) % SENT_TONES_MAX);
    draw_tone_list();
}

static void toggle_extended(void)
{
    if (!t.extended && t.count < EXTENDED_FROM_LAST)
        return;
    t.extended = !t.extended;
    draw_send();
}

static void send_key(uint8_t code)
{
    if (t.entry_kind == ENTRY_SHIFT) {
        if (code == SK_5 || code == KEY_DELETE) {
            int32_t value = 0;

            if (t.entry_len && parse_entry(t.entry, t.entry_len, 1, &value)
                && value >= -SHIFT_MAX && value <= SHIFT_MAX) {
                t.shift = (int8_t)value;
                draw_tone_list();
            }
            blank(22, 24, 6);
            start_entry(ENTRY_TONE_NUMBER);
            draw_shift();
            return;
        }
        if (entry_char(code)) {
            entry_add(code, 22, 23, 6);
            return;
        }
    }
    switch (code) {
    case SK_1:
        commit_tone();
        break;
    case SK_2:
        stop_sending();
        start_sending(SEND_BURST);
        break;
    case SK_3:
        if (t.mode == SEND_CONTINUOUS) {
            stop_sending();
        } else {
            stop_sending();
            start_sending(SEND_CONTINUOUS);
        }
        draw_send();
        break;
    case SK_4:
        step_tone();
        break;
    case SK_5:
        start_entry(ENTRY_SHIFT);
        draw_shift();
        show_entry(22, 23, 6);
        break;
    case SK_6:
        close_tones();
        break;
    case SK_7:
        toggle_extended();
        break;
    case KEY_DELETE:
        if (t.cursor < t.count) {
            t.list[t.cursor] = TONE_NULL;
            start_entry(ENTRY_TONE_NUMBER);
            draw_tone_list();
        }
        break;
    default:
        if (entry_char(code) && code != KEY_POINT && code != KEY_MINUS && t.cursor < SENT_TONES_MAX
            && t.entry_len + 1u < TONE_ENTRY_SIZE) {
            int8_t c = entry_char(code);

            t.entry[t.entry_len++] = (char)c;
            t.entry[t.entry_len] = 0;
            print_field(3, list_row(t.cursor), t.entry, 2);
            blank(3, (uint8_t)(list_row(t.cursor) + 1u), 4);
            draw_list_cursor();
        }
        break;
    }
}

static void receive_key(uint8_t code)
{
    if (code == SK_2) {
        decode_reset();
    } else if (code == SK_6) {
        close_tones();
    }
}

static void next_user_tone(void)
{
    blank(6, (uint8_t)(3u + t.user_cursor * 2u), 11);
    start_entry(ENTRY_USER_FREQ);
    t.user_cursor = (uint8_t)((t.user_cursor + 1u) % TONE_NUMBERS);
    draw_user_cursor();
}

static void user_key(uint8_t code)
{
    int32_t value;
    uint8_t row = (uint8_t)(2u + t.user_cursor * 2u);

    switch (code) {
    case SK_1:
        draw_user_row(t.user_cursor);
        next_user_tone();
        return;
    case SK_3:
        start_entry(ENTRY_DURATION);
        show_entry(27, 17, 7);
        return;
    case SK_5:
        open_standard(USER_STANDARD);
        return;
    case SK_6:
        close_tones();
        return;
    default:
        break;
    }

    if (t.entry_kind == ENTRY_DURATION) {
        if (entry_char(code) && code != KEY_MINUS) {
            entry_add(code, 27, 17, 7);
        } else if (code == KEY_MILLISECONDS) {
            if (parse_entry(t.entry, t.entry_len, 1, &value) && value >= (int32_t)USER_DURATION_MIN_MS
                && value <= (int32_t)USER_DURATION_MAX_MS) {
                store_word(USER_DURATION_ADDRESS, (uint16_t)(value * 10));
                blank(27, 18, 7);
                draw_user_duration();
                start_entry(ENTRY_USER_FREQ);
                draw_user_cursor();
            }
        }
        return;
    }

    if (entry_char(code) && code != KEY_MINUS) {
        entry_add(code, 6, row, 10);
    } else if (code == KEY_DELETE) {
        if (t.entry_len)
            t.entry_len--;
        show_entry(6, row, 10);
    } else if (code == KEY_HZ || code == KEY_KHZ || code == KEY_MHZ) {
        int32_t scale = code == KEY_HZ ? 10 : (code == KEY_KHZ ? 10000 : 10000000);

        if (!parse_entry(t.entry, t.entry_len, scale, &value) || value < (int32_t)USER_MIN || value > (int32_t)USER_MAX)
            return;
        store_word((uint16_t)(USER_FREQ_ADDRESS + t.user_cursor * 2u),
                   (uint32_t)value < USER_FREQ_TENTHS_LIMIT ? (uint16_t)value
                                                            : (uint16_t)(USER_FREQ_HZ_FLAG | (uint16_t)(value / 10)));
        draw_user_row(t.user_cursor);
        next_user_tone();
    }
}

void tones_init(void)
{
    t.view = VIEW_CLOSED;
    t.mode = SEND_IDLE;
    t.count = t.cursor = 0;
    t.shift = 0;
    t.extended = 0;
    for (uint8_t i = 0; i < SENT_TONES_MAX; i++)
        t.list[i] = TONE_EMPTY;
}

void tones_open(uint8_t transmit)
{
    t.transmit = transmit;
    draw_menu();
}

void tones_open_two_tone(void)
{
    t.transmit = 1;
    draw_two_tone();
}

uint8_t tones_active(void)
{
    return t.view != VIEW_CLOSED;
}

void tones_key(uint8_t code)
{
    switch (t.view) {
    case VIEW_MENU:
        menu_key(code);
        break;
    case VIEW_SAT:
        sat_key(code);
        break;
    case VIEW_TWO_TONE:
        two_tone_key(code);
        break;
    case VIEW_SEND:
        send_key(code);
        break;
    case VIEW_RECEIVE:
        receive_key(code);
        break;
    case VIEW_USER:
        user_key(code);
        break;
    default:
        break;
    }
}

void tones_redraw(void)
{
    switch (t.view) {
    case VIEW_MENU:
        draw_menu();
        break;
    case VIEW_SAT:
        draw_sat();
        break;
    case VIEW_TWO_TONE:
        draw_two_tone();
        break;
    case VIEW_SEND:
        draw_send();
        break;
    case VIEW_RECEIVE:
        draw_receive();
        draw_received();
        break;
    case VIEW_USER:
        draw_user();
        break;
    default:
        break;
    }
}
