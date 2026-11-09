#include "help.h"

#include "app.h"
#include "format.h"
#include "keys.h"
#include "level.h"
#include "params.h"
#include "selftest.h"
#include "service.h"
#include "screen.h"

#define FIRMWARE_NAME "2955 OPEN FIRMWARE"

/* The mode keys beside the screen, from the top. */
#define POS_TX 0u
#define POS_RX 1u
#define POS_DUPLEX 2u
#define POS_TONES 3u
#define POS_BAR_CHART 4u
#define POS_SCOPE 5u
#define POS_HOLD 6u

#define MENU_BOX_WIDTH 19u
#define MENU_TEXT_COL 19u
#define PAGE_BOX_WIDTH 9u
#define PAGE_TEXT_COL 29u
#define PARAM_LABEL_COL 6u
#define PARAM_VALUE_COL 22u
#define PARAM_VALUE_WIDTH 6u
#define ROW_PER_KEY 4u
#define EXT_ROW_N 5u
#define EXT_ROW_BNC 9u
#define EXT_ENTRY_MAX 5u
#define ACCESS_KEYS 3u
#define TITLE_LABEL_COL 1u
#define TITLE_KEYS_COL 20u

typedef enum {
    VIEW_CLOSED,
    VIEW_MENU,
    VIEW_TEXT,
    VIEW_PARAMS_1,
    VIEW_PARAMS_2,
    VIEW_PARAMS_3,
} help_view;

typedef struct {
    uint8_t row;
    const char *text;
} text_line;

typedef struct {
    const char *title;
    const text_line *lines;
    uint8_t line_count;
} text_page;

static help_view view;
static uint8_t access_position; /* 0, or the next key of the calibration menu sequence */
static char ext_entry[EXT_ENTRY_MAX + 1u];
static uint8_t ext_entry_len;
static uint8_t ext_entry_row; /* 0 while no attenuator value is being typed */
static const text_page *pages;
static uint8_t page_count, page_index;

/* ---- the operating summaries, in this firmware's own words ----------------------------- */

static const text_line transmitter_lines[] = {
    {4, " MODE                 TX"},
    {6, " MODULATION KIND      FM, AM OR PM"},
    {8, " DEMOD FILTER         BAND PASS,"},
    {9, "                      LOW PASS"},
    {11, " WAVEFORM             SCOPE, BAR CHART"},
    {13, " DISTORTION AT 1 kHz  DISTN"},
    {15, " MANUAL TUNING        FREQ, VALUE, UNIT"},
    {17, " AF GENERATOR         AF GEN, FREQ OR"},
    {18, "                      LEVEL, VALUE, UNIT"},
    {20, " STEP SIZE            INCR BEFORE VALUE"},
    {23, " VARIABLE ADJUSTS THE"},
    {24, " HIGHLIGHTED FIELD."},
    {27, " LOW PASS SWITCHES BETWEEN"},
    {28, " 50 kHz AND 300 Hz."},
};

static const text_line receiver_lines_1[] = {
    {4, " MODE                 RX"},
    {6, " AF FILTER            BAND PASS,"},
    {7, "                      LOW PASS"},
    {9, " SINAD OR S/N         SINAD S/N"},
    {11, " DISTORTION AT 1 kHz  DISTN"},
    {13, " LEVEL IN dBV         DISTN OFF"},
    {15, " dBV OR dBR           dB"},
    {17, " COUPLING             AC/DC, ONLY WITH"},
    {18, "                      DISTN OFF"},
    {28, " LOW PASS SWITCHES BETWEEN"},
    {29, " 50 kHz AND 300 Hz."},
};

static const text_line receiver_lines_2[] = {
    {4, " RF GENERATOR         RF GEN, FREQ OR"},
    {5, "                      LEVEL, VALUE, UNIT"},
    {7, " MODULATION           SET MOD, FREQ OR"},
    {8, "                      LEVEL, VALUE, UNIT"},
    {10, " STEP SIZE            INCR BEFORE VALUE"},
    {12, " SOCKET               SELECT"},
    {14, " MODULATION ON/OFF    MOD ON/OFF"},
    {16, " WAVEFORM             SCOPE, BAR CHART"},
    {19, " VARIABLE ADJUSTS THE"},
    {20, " HIGHLIGHTED FIELD."},
};

static const text_line duplex_lines[] = {
    {4, " MODE                 DUPLEX"},
    {6, " ONE OR TWO PORT      SELECT"},
    {8, " RF GENERATOR         RF GEN, FREQ OR"},
    {9, "                      LEVEL, VALUE, UNIT"},
    {11, " MODULATION LEVEL     SET MOD, LEVEL,"},
    {12, "                      VALUE, UNIT"},
    {14, " AF GENERATOR LEVEL   AF GEN, LEVEL,"},
    {15, "                      VALUE, UNIT"},
    {17, " TRANSMITTER FREQ     FREQ, VALUE, UNIT"},
    {19, " SINAD OR S/N         SINAD S/N"},
    {21, " DISTORTION AT 1 kHz  DISTN"},
    {23, " AF FILTER            BAND PASS,"},
    {24, "                      LOW PASS"},
    {27, " GENERATOR STEP SIZES COME FROM"},
    {28, " THE RECEIVER TEST."},
};

#define LINES(array) (array), (uint8_t)(sizeof(array) / sizeof((array)[0]))

static const text_page transmitter_pages[] = {{"TRANSMITTER TEST HELP", LINES(transmitter_lines)}};
static const text_page receiver_pages[] = {
    {"RECEIVER TEST HELP", LINES(receiver_lines_1)},
    {"RECEIVER TEST HELP", LINES(receiver_lines_2)},
};
static const text_page duplex_pages[] = {{"DUPLEX TEST HELP", LINES(duplex_lines)}};
static const text_line audio_lines[] = {
    {4, " MODE                 AF GEN IN RX"},
    {6, " GENERATOR            FREQ OR LEVEL,"},
    {7, "                      VALUE, UNIT"},
    {9, " STEP SIZE            INCR BEFORE VALUE"},
    {11, " SECOND TONE OF 1 kHz TONES"},
    {13, " DISTORTION AT 1 kHz  DISTN"},
    {15, " dBV OR dB RELATIVE   dB"},
    {17, " AF FILTER            BAND PASS,"},
    {18, "                      LOW PASS"},
    {20, " BACK TO THE RECEIVER RX"},
    {23, " THE GENERATOR AND THE VOLTMETER RUN"},
    {24, " TOGETHER: CONNECT THE UNIT BETWEEN"},
    {25, " AF GEN OUTPUT AND AF INPUT."},
};

static const text_page audio_pages[] = {{"AUDIO TEST HELP", LINES(audio_lines)}};

/* ---- drawing --------------------------------------------------------------------------- */

static void labelled_box(uint8_t position, uint8_t width, uint8_t text_col, const char *text)
{
    screen_softkey_box(position, width);
    screen_print(text_col, (uint8_t)(position * ROW_PER_KEY + 1u), text);
}

static void page_box(uint8_t position, const char *text)
{
    labelled_box(position, PAGE_BOX_WIDTH, PAGE_TEXT_COL, text);
}

static void underline(uint8_t col, uint8_t row, uint8_t length)
{
    for (uint8_t i = 0; i < length; i++)
        screen_put((uint8_t)(col + i), row, SCREEN_UNDERLINE);
}

static void draw_title(uint8_t col, const char *title)
{
    uint8_t length = 0;

    while (title[length])
        length++;
    screen_print(col, 0, title);
    underline(col, 1, length);
}

static void draw_menu(void)
{
    static const char *const entries[] = {
        "TRANSMITTER TESTING", "RECEIVER TESTING", "DUPLEX TESTING", "AUDIO TESTING",
        "CHANGE PARAMETERS",   "SELF TEST",        "RETURN",
    };

    screen_clear();
    draw_title(5, "HELP");
    screen_print(1, 3, "PRESS THE BLUE");
    screen_print(1, 5, "MODE KEY NEXT");
    screen_print(1, 7, "TO THE TOPIC");
    for (uint8_t i = 0; i < sizeof entries / sizeof entries[0]; i++)
        labelled_box(i, MENU_BOX_WIDTH, MENU_TEXT_COL, entries[i]);
    screen_print(1, 29, FIRMWARE_NAME);
    view = VIEW_MENU;
}

static void draw_text_page(void)
{
    const text_page *page = &pages[page_index];

    screen_clear();
    draw_title(TITLE_LABEL_COL, page->title);
    if (page_count > 1u) {
        screen_put(33, 0, (uint8_t)('1' + page_index));
        screen_print(34, 0, " OF ");
        screen_put(38, 0, (uint8_t)('0' + page_count));
    }
    for (uint8_t i = 0; i < page->line_count; i++)
        screen_print(0, page->lines[i].row, page->lines[i].text);
    if (page_count > 1u)
        page_box(POS_SCOPE, "CONTINUE");
    page_box(POS_HOLD, "RETURN");
    view = VIEW_TEXT;
}

static void open_pages(const text_page *first, uint8_t count)
{
    pages = first;
    page_count = count;
    page_index = 0;
    draw_text_page();
}

static void print_value(uint8_t row, const char *text)
{
    uint8_t col = PARAM_VALUE_COL;

    for (; col < PARAM_VALUE_COL + PARAM_VALUE_WIDTH; col++)
        screen_put(col, row, ' ');
    screen_print(PARAM_VALUE_COL, row, text);
}

static void print_ext_value(uint8_t row, uint8_t bnc)
{
    int16_t tenths = level_external_attenuator(bnc);
    char text[12];

    if (tenths)
        put_text(put_fixed(text, (uint32_t)tenths, 1), "dB");
    else
        put_text(text, "OFF");
    print_value(row, text);
}

static void draw_params_values(void)
{
    if (view == VIEW_PARAMS_1) {
        if (!param_get(PARAM_NO_EMF_CHOICE))
            print_value(5, param_get(PARAM_LEVEL_EMF) ? "EMF" : "PD");
        print_value(9, param_get(PARAM_STORE_DISABLE) ? "OFF" : "ON");
        print_value(13, param_get(PARAM_COUNTER_FINE) ? "1Hz" : "10Hz");
        print_value(17, param_get(PARAM_SN_DEFAULT) ? "S/N" : "SINAD");
    } else if (view == VIEW_PARAMS_2) {
        print_value(5, param_get(PARAM_AF_600_OHM) ? "ON" : "OFF");
        print_value(9, param_get(PARAM_AF_ATTENUATOR) ? "ON" : "OFF");
    } else {
        print_ext_value(EXT_ROW_N, 0);
        print_ext_value(EXT_ROW_BNC, 1);
    }
}

static void draw_params_header(uint8_t page_col, const char *page_text)
{
    screen_clear();
    screen_print(page_col, 0, page_text);
    screen_print(PARAM_LABEL_COL, 2, "PARAMETER");
    underline(PARAM_LABEL_COL, 3, 9);
    screen_print(21, 2, "SET TO");
    underline(21, 3, 6);
    page_box(POS_SCOPE, "CONTINUE");
    page_box(POS_HOLD, "RETURN");
}

static void draw_params_1(void)
{
    draw_params_header(33, "1 OF 3");
    if (!param_get(PARAM_NO_EMF_CHOICE)) {
        screen_print(PARAM_LABEL_COL, 5, "RF LEVEL");
        page_box(POS_RX, "PD-EMF");
    }
    screen_print(PARAM_LABEL_COL, 9, "STORE FUNCTION");
    page_box(POS_DUPLEX, "ON-OFF");
    screen_print(PARAM_LABEL_COL, 12, "RF COUNTER");
    screen_print(PARAM_LABEL_COL, 14, "RESOLUTION");
    page_box(POS_TONES, "10Hz-1Hz");
    screen_print(PARAM_LABEL_COL, 17, "DEFAULT");
    page_box(POS_BAR_CHART, "SINAD-S/N");
    view = VIEW_PARAMS_1;
    draw_params_values();
}

static void draw_params_2(void)
{
    draw_params_header(28, "PAGE 2 OF 3");
    screen_print(0, 5, "600 OHM dBm ACCESSORY");
    page_box(POS_RX, "ON-OFF");
    screen_print(0, 8, "20dB AF GEN ATTEN");
    screen_print(0, 10, "ACCESSORY");
    page_box(POS_DUPLEX, "ON-OFF");
    view = VIEW_PARAMS_2;
    draw_params_values();
}

static void draw_params_3(void)
{
    draw_params_header(28, "PAGE 3 OF 3");
    screen_print(0, EXT_ROW_N, "EXT ATTEN AT N");
    page_box(POS_RX, "SET");
    screen_print(0, EXT_ROW_BNC, "EXT ATTEN AT BNC");
    page_box(POS_DUPLEX, "SET");
    screen_print(0, 13, "SET, TYPE 3 TO 40");
    screen_print(0, 15, "AND PRESS dB. 0 IS OFF");
    ext_entry_row = 0;
    view = VIEW_PARAMS_3;
    draw_params_values();
}

/* ---- keys ------------------------------------------------------------------------------ */

static void close_help(void)
{
    view = VIEW_CLOSED;
    app_restore_screen();
}

static void menu_key(uint8_t code)
{
    switch (code) {
    case KEY_TX:
        open_pages(transmitter_pages, 1);
        break;
    case KEY_RX:
        open_pages(receiver_pages, 2);
        break;
    case KEY_DUPLEX:
        open_pages(duplex_pages, 1);
        break;
    case KEY_TONES:
        open_pages(audio_pages, 1);
        break;
    case KEY_BAR_CHART:
        draw_params_1();
        break;
    case KEY_SCOPE:
        selftest_start(0);
        break;
    case KEY_HOLD_DISPLAY:
        close_help();
        break;
    default:
        break;
    }
}

static void text_key(uint8_t code)
{
    if (code == KEY_HOLD_DISPLAY) {
        draw_menu();
    } else if (code == KEY_SCOPE && page_count > 1u) {
        page_index = (uint8_t)((page_index + 1u) % page_count);
        draw_text_page();
    }
}

static void params_toggle(uint16_t address)
{
    param_toggle(address);
    draw_params_values();
}

static void params_1_key(uint8_t code)
{
    switch (code) {
    case KEY_TX:
        selftest_start(1);
        break;
    case KEY_HELP:
        access_position = 1;
        break;
    case KEY_RX:
        if (!param_get(PARAM_NO_EMF_CHOICE))
            params_toggle(PARAM_LEVEL_EMF);
        break;
    case KEY_DUPLEX:
        params_toggle(PARAM_STORE_DISABLE);
        break;
    case KEY_TONES:
        params_toggle(PARAM_COUNTER_FINE);
        break;
    case KEY_BAR_CHART:
        params_toggle(PARAM_SN_DEFAULT);
        break;
    case KEY_SCOPE:
        draw_params_2();
        break;
    case KEY_HOLD_DISPLAY:
        draw_menu();
        break;
    default:
        break;
    }
}

/* Tenths of a dB from the text typed; -1 if it makes no sense. */
static int16_t ext_entry_value(void)
{
    uint32_t tenths = 0;
    uint8_t seen_point = 0;
    uint8_t decimals = 0;

    if (!ext_entry_len)
        return -1;
    for (uint8_t i = 0; i < ext_entry_len; i++) {
        if (ext_entry[i] == '.') {
            if (seen_point)
                return -1;
            seen_point = 1;
        } else if (!decimals) {
            tenths = tenths * 10u + (uint8_t)(ext_entry[i] - '0');
            decimals = seen_point;
        }
    }
    if (!decimals)
        tenths *= 10u;
    return tenths > LEVEL_EXTERNAL_MAX ? -1 : (int16_t)tenths;
}

static void cancel_ext_entry(void);

static void show_ext_entry(void)
{
    ext_entry[ext_entry_len] = 0;
    print_value(ext_entry_row, ext_entry);
    for (uint8_t col = PARAM_VALUE_COL; col < PARAM_VALUE_COL + PARAM_VALUE_WIDTH; col++)
        screen_put(col, (uint8_t)(ext_entry_row + 1u), ' ');
    screen_put((uint8_t)(PARAM_VALUE_COL + ext_entry_len), (uint8_t)(ext_entry_row + 1u), SCREEN_CURSOR);
}

static void begin_ext_entry(uint8_t row)
{
    if (ext_entry_row)
        cancel_ext_entry();
    ext_entry_row = row;
    ext_entry_len = 0;
    show_ext_entry();
}

static void cancel_ext_entry(void)
{
    for (uint8_t col = PARAM_VALUE_COL; col < PARAM_VALUE_COL + PARAM_VALUE_WIDTH; col++)
        screen_put(col, (uint8_t)(ext_entry_row + 1u), ' ');
    ext_entry_row = 0;
    draw_params_values();
}

static int8_t ext_entry_char(uint8_t code)
{
    static const uint8_t digit_keys[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};

    for (uint8_t i = 0; i < sizeof digit_keys; i++)
        if (digit_keys[i] == code)
            return (int8_t)('0' + i);
    return code == KEY_POINT ? '.' : 0;
}

/* Returns non-zero if the key belonged to the entry. */
static uint8_t ext_entry_key(uint8_t code)
{
    int8_t c = ext_entry_char(code);

    if (c) {
        if (ext_entry_len < EXT_ENTRY_MAX) {
            ext_entry[ext_entry_len++] = (char)c;
            show_ext_entry();
        }
        return 1;
    }
    if (code == KEY_DELETE) {
        if (ext_entry_len) {
            ext_entry_len--;
            show_ext_entry();
        }
        return 1;
    }
    if (code == KEY_DB) {
        int16_t value = ext_entry_value();

        if (level_external_valid(value)) {
            level_store_external_attenuator(ext_entry_row == EXT_ROW_BNC, value);
            cancel_ext_entry();
        }
        return 1;
    }
    return 0;
}

static void params_3_key(uint8_t code)
{
    if (ext_entry_row && ext_entry_key(code))
        return;
    switch (code) {
    case KEY_RX:
        begin_ext_entry(EXT_ROW_N);
        break;
    case KEY_DUPLEX:
        begin_ext_entry(EXT_ROW_BNC);
        break;
    case KEY_SCOPE:
        draw_params_1();
        break;
    case KEY_HOLD_DISPLAY:
        draw_menu();
        break;
    default:
        break;
    }
}

static void params_2_key(uint8_t code)
{
    switch (code) {
    case KEY_RX:
        params_toggle(PARAM_AF_600_OHM);
        break;
    case KEY_DUPLEX:
        params_toggle(PARAM_AF_ATTENUATOR);
        break;
    case KEY_SCOPE:
        draw_params_3();
        break;
    case KEY_HOLD_DISPLAY:
        draw_menu();
        break;
    default:
        break;
    }
}

void help_open(void)
{
    draw_menu();
}

void help_redraw(void)
{
    if (service_active()) {
        service_redraw();
        return;
    }
    if (selftest_active()) {
        selftest_redraw();
        return;
    }
    switch (view) {
    case VIEW_MENU:
        draw_menu();
        break;
    case VIEW_TEXT:
        draw_text_page();
        break;
    case VIEW_PARAMS_1:
        draw_params_1();
        break;
    case VIEW_PARAMS_2:
        draw_params_2();
        break;
    case VIEW_PARAMS_3:
        draw_params_3();
        break;
    default:
        break;
    }
}

uint8_t help_active(void)
{
    return view != VIEW_CLOSED;
}

void help_selftest_closed(uint8_t single_step)
{
    if (single_step)
        draw_params_1();
    else
        draw_menu();
}

/* The calibration data menu is behind a short key sequence, so it is not opened by accident. */
static uint8_t access_key(uint8_t code)
{
    static const uint8_t sequence[ACCESS_KEYS] = {KEY_2, KEY_9, KEY_5};

    if (!access_position)
        return 0;
    if (code != sequence[access_position - 1u]) {
        access_position = 0;
        return 1;
    }
    if (++access_position > ACCESS_KEYS) {
        access_position = 0;
        service_open();
    }
    return 1;
}

void help_service_closed(void)
{
    draw_params_1();
}

void help_key(uint8_t code)
{
    if (service_active()) {
        service_key(code);
        return;
    }
    if (selftest_active()) {
        selftest_key(code);
        return;
    }
    if (view == VIEW_PARAMS_1 && access_key(code))
        return;
    switch (view) {
    case VIEW_MENU:
        menu_key(code);
        break;
    case VIEW_TEXT:
        text_key(code);
        break;
    case VIEW_PARAMS_1:
        params_1_key(code);
        break;
    case VIEW_PARAMS_2:
        params_2_key(code);
        break;
    case VIEW_PARAMS_3:
        params_3_key(code);
        break;
    default:
        break;
    }
}
