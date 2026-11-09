#include "gpib.h"

#include "app.h"
#include "hal.h"
#include "keys.h"
#include "params.h"
#include "readings.h"
#include "regs.h"
#include "screen.h"
#include "settings.h"
#include "tx.h"

#define REG_DATA 0xE048u
#define REG_ISR1 0xE049u
#define REG_ISR2 0xE04Au
#define REG_SERIAL_POLL 0xE04Bu
#define REG_ADDR_MODE 0xE04Cu
#define REG_AUX 0xE04Du
#define REG_ADDR 0xE04Eu
#define REG_EOS 0xE04Fu
#define REG_SWITCHES 0xE040u

#define ISR1_DATA_IN 0x01u
#define ISR1_DATA_OUT 0x02u
#define ISR1_DEVICE_CLEAR 0x08u
#define ISR1_END 0x10u
#define ISR2_ADDRESS_CHANGE 0x01u
#define ISR2_REMOTE_CHANGE 0x02u
#define ISR2_LOCKOUT_CHANGE 0x04u
#define ISR2_REMOTE 0x10u
#define ISR2_LOCKOUT 0x20u
#define ADDR_TALK_OR_LISTEN 0x06u
#define ADDR_TALKER 0x02u

#define AUX_RESET 0x02u
#define AUX_CLEAR 0x00u
#define AUX_RETURN_TO_LOCAL 0x05u
#define AUX_REGISTER_B_SETUP 0x23u
#define AUX_EOS_MODE 0x8Cu /* end a reception and send EOI on the EOS byte */
#define AUX_REGISTER_E_SETUP 0xA0u
#define ADDR_MODE_NORMAL 0x01u
#define ADDR_MODE_TALK_ONLY 0x80u
#define ADDR_DISABLE_SECOND 0xE0u
#define SWITCH_ADDRESS_MASK 0x1Fu
#define SWITCH_TALK_ONLY 0x20u
#define SWITCH_UPPER_CASE_FROM 0x30u
#define EOS_LF 0x0Au

#define INPUT_SIZE 128u
#define OUTPUT_SIZE 512u

#define TERM_ETX 0x03u
#define TERM_ETB 0x17u
#define TERM_LF 0x0Au

#define READING_ALL 39u
#define READING_LAST_PLAIN 38u
#define MEMORY_READ_START 0xE000u
#define EEPROM_START 0xF000u
#define EEPROM_END 0xF800u
#define EEPROM_WRITE_MS 10u
#define MAX_POKE_BYTES 64u
#define SETTINGS_RAM 0xEC00u
#define SETTINGS_RAM_END (SETTINGS_RAM + SETTINGS_IMAGE_SIZE)
#define SAVE_LINE_BYTES 16u
#define VERSION_TEXT "16"

/* The indicators in reverse field at the right of the screen. */
#define INDICATOR_COL 36u
#define INDICATOR_REM_ROW 16u
#define INDICATOR_ADR_ROW 18u
#define INDICATOR_SRQ_ROW 20u

/* The box beside the HELP key that offers a return to local. */
#define LOCAL_BOX_POSITION 7u
#define LOCAL_BOX_WIDTH 2u
#define LOCAL_BOX_COL 35u
#define LOCAL_BOX_TOP_ROW 28u
#define LOCAL_TEXT_COL 36u

/* The status byte sent in a serial poll. */
#define STATUS_DATA_READY 0x80u
#define STATUS_SERVICE_REQUEST 0x40u
#define STATUS_ERROR 0x20u
#define ERROR_NUMERIC_ENTRY 0x10u
#define ERROR_DATA 0x08u
#define ERROR_ABNORMAL 0x04u
#define ERROR_SYNTAX 0x02u
#define ERROR_OVERFLOW 0x01u

/* The levels of the SQ command. */
#define SRQ_ERRORS_ONLY 1u
#define SRQ_ALWAYS 2u

static struct {
    uint8_t present;
    uint8_t upper_case;
    uint8_t data_out_ready;
    uint8_t remote, addressed;
    uint8_t input[INPUT_SIZE];
    uint8_t input_len;
    uint8_t output[OUTPUT_SIZE];
    uint16_t output_head, output_tail;
    uint8_t line_ready;
    uint8_t key_wanted;
    uint8_t talk_only;
    uint8_t indicators_shown;
    uint8_t lockout;
    uint8_t request_shown;
    uint8_t request_level;
    uint8_t suppress_indicators;
    uint8_t variable_enabled;
    uint8_t last_error;
    uint8_t output_added;
    uint8_t settings_image[SETTINGS_IMAGE_SIZE];
    uint8_t settings_received;
} gpib;

typedef struct {
    char first, second;
    uint8_t key;
} key_command;

/* Commands that are simply the equivalent of a front panel key. */
static const key_command key_commands[] = {
    {'A', 'G', 2},  {'A', 'M', 62}, {'B', 'C', 33}, {'D', 'B', 46}, {'D', 'I', 59}, {'D', 'M', 22},
    {'D', 'X', 57}, {'F', 'D', 23}, {'F', 'M', 54}, {'F', 'R', 3},  {'F', 'U', 47}, {'H', 'P', 17},
    {'H', 'Z', 14}, {'K', 'Z', 30}, {'L', 'D', 24}, {'L', 'U', 48}, {'L', 'V', 51}, {'M', 'V', 30},
    {'M', 'Z', 6},  {'P', 'M', 38}, {'R', 'C', 35}, {'R', 'G', 58}, {'R', 'P', 63}, {'R', 'T', 50},
    {'R', 'X', 49}, {'S', 'C', 9},  {'S', 'M', 26}, {'S', 'T', 27}, {'S', 'W', 64}, {'T', 'D', 32},
    {'T', 'N', 25}, {'T', 'U', 40}, {'T', 'X', 1},  {'U', 'V', 14}, {'V', 'D', 31}, {'V', 'L', 6},
    {'V', 'U', 39},
};

/* Keys for the characters '-', '.', '/', '0' to '9'; the slash has none. */
static const uint8_t number_keys[13] = {45, 37, 0, 13, 4, 12, 20, 28, 36, 44, 52, 60, 5};

/* Soft keys XA to XG. */
static const uint8_t soft_keys[7] = {1, 49, 57, 25, 33, 9, 41};

static const char *line_pos;
static const char *line_end;

/* ---- status byte --------------------------------------------------------------------- */

static void set_status(uint8_t bits, uint8_t request_service)
{
    uint8_t status = (uint8_t)(hal_read(REG_SERIAL_POLL) | bits);

    if (request_service) {
        status |= STATUS_SERVICE_REQUEST;
        if (!gpib.request_shown) {
            gpib.request_shown = 1;
            gpib_redraw_indicators();
        }
    }
    hal_write(REG_SERIAL_POLL, status);
}

static void report_error(uint8_t error_bits)
{
    set_status((uint8_t)(STATUS_ERROR | error_bits), gpib.request_level >= SRQ_ERRORS_ONLY);
}

static void report_data_ready(void)
{
    if (!gpib.talk_only)
        set_status(STATUS_DATA_READY, gpib.request_level == SRQ_ALWAYS);
}

/* Data ready and the error bits last until the controller has read from the instrument. */
static void clear_read_status(void)
{
    hal_write(REG_SERIAL_POLL, hal_read(REG_SERIAL_POLL) & STATUS_SERVICE_REQUEST);
}

/* ---- output -------------------------------------------------------------------------- */

static void put_char(char c)
{
    uint16_t next = (uint16_t)((gpib.output_head + 1u) % OUTPUT_SIZE);

    if (next == gpib.output_tail) {
        report_error(ERROR_OVERFLOW);
        return;
    }
    gpib.output_added = 1;
    if (gpib.upper_case && c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    gpib.output[gpib.output_head] = (uint8_t)c;
    gpib.output_head = next;
}

static void put_string(const char *text)
{
    while (*text)
        put_char(*text++);
}

static void put_line_end(void)
{
    put_string("\r\n");
}

static void put_reading(uint8_t index)
{
    const char *text = readings_get(index);

    put_string(*text ? text : "NULL");
}

static void put_number(uint16_t value)
{
    char digits[6];
    uint8_t count = 0;

    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (count--)
        put_char(digits[count]);
}

static void put_labelled(const char *label, uint8_t index)
{
    put_string(label);
    put_reading(index);
    put_line_end();
}

/* The whole page of settings and readings, RD39. */
static void put_page(void)
{
    uint8_t transmit = app_is_transmit();

    put_string(transmit ? "    TRANSMITTER TEST\r\n    ----------------\r\n" : "     RECEIVER TEST\r\n     -------------\r\n");
    put_line_end();
    put_string("SETTINGS\r\n--------\r\n");
    put_line_end();
    if (transmit) {
        put_labelled(" AF FREQ:", READING_AF_GEN_FREQ);
        put_labelled("   LEVEL:", READING_AF_GEN_LEVEL);
    } else {
        put_labelled("GEN FREQ:", READING_GEN_FREQ);
        put_labelled("   LEVEL:", READING_GEN_LEVEL);
        put_labelled("MOD FREQ:", READING_MOD_SET_FREQ);
        put_labelled("   LEVEL:", READING_MOD_SET_LEVEL);
    }
    put_labelled("  FILTER:", READING_FILTER);
    put_string("\r\nREADINGS\r\n--------\r\n");
    put_line_end();
    if (transmit) {
        put_labelled(app_tx_manual() ? "  OFFSET:" : " TX FREQ:", READING_RF_COUNTER);
        put_labelled("   POWER:", READING_POWER);
        put_labelled("MOD FREQ:", READING_MOD_FREQ);
        put_labelled("   LEVEL:", READING_MOD_LEVEL);
        if (app_distortion_on())
            put_labelled("   DISTN:", READING_TX_DISTORTION);
    } else {
        put_labelled("AF VOLTS:", READING_AF_LEVEL);
        put_labelled(" AF FREQ:", READING_AF_FREQ);
        if (app_distortion_on())
            put_labelled("   DISTN:", READING_RX_SECOND);
        else if (app_sinad_mode() == 1u)
            put_labelled("   SINAD:", READING_RX_SECOND);
        else if (app_sinad_mode() == 2u)
            put_labelled("     S-N:", READING_RX_SECOND);
    }
    put_string("\r\n MODEL:..........\r\nSER NO:..........\r\n  DATE:..........\r\n");
    put_line_end();
}

/* ---- parser helpers ------------------------------------------------------------------ */

static uint8_t is_letter(char c) { return c >= 'A' && c <= 'Z'; }
static uint8_t is_digit(char c) { return c >= '0' && c <= '9'; }
static uint8_t is_separator(char c) { return c == ' ' || c == ',' || c == ';' || c == '\r'; }

static uint8_t is_number_char(char c)
{
    return c == '/' || c == '+' || c == '-' || c == '.' || is_digit(c);
}

static uint8_t has_more(void) { return line_pos < line_end; }

/* Reads an unsigned number; returns 0 for a missing one. */
static uint8_t read_number(uint16_t *value)
{
    uint8_t digits = 0;

    *value = 0;
    while (has_more() && (*line_pos == ' '))
        line_pos++;
    while (has_more() && is_digit(*line_pos)) {
        *value = (uint16_t)(*value * 10u + (uint16_t)(*line_pos++ - '0'));
        digits++;
    }
    if (has_more() && (*line_pos == ',' || *line_pos == ';'))
        line_pos++;
    return digits != 0;
}

static uint8_t read_single_digit(uint8_t limit, uint8_t *digit)
{
    if (!has_more() || !is_digit(*line_pos)) {
        report_error(ERROR_SYNTAX);
        return 0;
    }
    *digit = (uint8_t)(*line_pos++ - '0');
    if (*digit < limit)
        return 1;
    report_error(ERROR_DATA);
    return 0;
}

/* Skips the data of a command that is accepted but has no function yet. */
static void skip_data(void)
{
    while (has_more() && !is_letter(*line_pos) && *line_pos != ';')
        line_pos++;
}

/* ---- commands ------------------------------------------------------------------------ */

static void press(uint8_t key)
{
    app_command_key(key);
}

static void set_second_reading(uint8_t n)
{
    uint8_t sinad = app_sinad_mode();
    uint8_t default_is_sn = hal_read(PARAM_SN_DEFAULT) != 0;

    switch (n) {
    case 0:
        if (sinad != 0) {
            press(KEY_DISTORTION);
            press(KEY_DISTORTION);
        } else if (app_distortion_on()) {
            press(KEY_DISTORTION);
        }
        break;
    case 1:
        if (sinad == 2u || (sinad == 0u && !default_is_sn)) {
            press(KEY_SINAD_SN);
        } else if (sinad == 0u) {
            press(KEY_SINAD_SN);
            press(KEY_SINAD_SN);
        }
        break;
    case 2:
        if (sinad == 1u || (sinad == 0u && default_is_sn)) {
            press(KEY_SINAD_SN);
        } else if (sinad == 0u) {
            press(KEY_SINAD_SN);
            press(KEY_SINAD_SN);
        }
        break;
    default:
        if (!app_distortion_on())
            press(KEY_DISTORTION);
        break;
    }
}

static void set_filter(uint8_t n)
{
    uint8_t filter = app_filter();

    if (n == 0)
        press(KEY_AF_BAND_PASS);
    else if (n == 1 && filter != 1u)
        press(KEY_AF_LOW_PASS);
    else if (n == 2 && filter != 2u)
        press(KEY_AF_LOW_PASS_300);
}

static void set_input(uint8_t n)
{
    if ((n == 0 && app_input_select() != 0) || (n == 1 && app_input_select() != 1))
        press(KEY_SELECT);
}

static void read_memory(void)
{
    uint16_t address;

    if (read_number(&address) && address >= MEMORY_READ_START) {
        put_number(hal_read(address));
        put_line_end();
    } else {
        report_error(ERROR_DATA);
    }
}

static void poke_memory(uint8_t max_bytes)
{
    uint16_t address, value;
    uint8_t count = 0;

    if (hal_read(PARAM_STORE_DISABLE) || !read_number(&address)) {
        report_error(ERROR_DATA);
        return;
    }
    while (count < max_bytes && read_number(&value) && value < 256u) {
        if (address >= SETTINGS_RAM && address < SETTINGS_RAM_END) {
            gpib.settings_image[address - SETTINGS_RAM] = (uint8_t)value;
            gpib.settings_received = 1;
        } else {
            hal_write(address, (uint8_t)value);
        }
        if (address >= EEPROM_START && address < EEPROM_END)
            hal_delay_ms(EEPROM_WRITE_MS);
        address++;
        count++;
    }
}

static void write_screen(void)
{
    uint16_t col, row;

    if (!read_number(&col) || !read_number(&row) || col >= SCREEN_COLS || row >= SCREEN_ROWS)
        return;
    while (has_more()) {
        char c = *line_pos++;

        if (c == '\r') {
            col = 0;
            if (row + 1u < SCREEN_ROWS)
                row++;
        } else {
            screen_put((uint8_t)col, (uint8_t)row, (uint8_t)c);
            if (col + 1u < SCREEN_COLS)
                col++;
        }
    }
}

/* A string of commands that restores the current settings when sent back. */
static void put_settings(void)
{
    instrument_settings settings = {0};

    app_get_settings(&settings);
    settings_to_image(&settings, gpib.settings_image);
    gpib.settings_received = 0;
    put_string("SE");
    for (uint8_t i = 0; i < SETTINGS_IMAGE_SIZE; i++) {
        if (i % SAVE_LINE_BYTES == 0u) {
            if (i)
                put_char('\n');
            put_string("DU");
            put_number((uint16_t)(SETTINGS_RAM + i));
        }
        put_char(',');
        put_number(gpib.settings_image[i]);
    }
    put_char('\n');
    put_string("RS");
    put_char('\n');
}

static void write_poke_flag(uint8_t value)
{
    hal_write(PARAM_STORE_DISABLE, value);
    hal_delay_ms(EEPROM_WRITE_MS);
}

static void reading_command(void)
{
    uint16_t n;

    if (!read_number(&n) || n > READING_ALL) {
        report_error(ERROR_DATA);
        return;
    }
    if (n == READING_ALL) {
        put_page();
    } else if (n != 0 && n <= READING_LAST_PLAIN) {
        put_reading((uint8_t)n);
        put_line_end();
    }
}

/* Commands accepted without effect for now. */
static uint8_t known_unimplemented(char first, char second)
{
    static const char names[] = "DSESBXBPTFTEETDTPRRREXLFBDBEDRURRVSSSA";

    for (const char *p = names; *p; p += 2) {
        if (p[0] == first && p[1] == second)
            return 1;
    }
    return 0;
}

static void run_command(char first, char second)
{
    uint8_t digit;

    for (uint8_t i = 0; i < sizeof key_commands / sizeof key_commands[0]; i++) {
        if (key_commands[i].first == first && key_commands[i].second == second) {
            press(key_commands[i].key);
            return;
        }
    }

    switch ((uint16_t)((first << 8) | second)) {
    case ('A' << 8) | 'C':
        if (!app_ac_coupled())
            press(KEY_AC_DC);
        break;
    case ('D' << 8) | 'C':
        if (app_ac_coupled())
            press(KEY_AC_DC);
        break;
    case ('F' << 8) | 'I':
        if (read_single_digit(3, &digit))
            set_filter(digit);
        break;
    case ('H' << 8) | 'D':
        if (read_single_digit(2, &digit) && digit != app_held())
            press(KEY_HOLD_DISPLAY);
        break;
    case ('M' << 8) | 'D':
        /* MD0 is modulation off, MD1 on. */
        if (read_single_digit(2, &digit) && (digit == 0) != app_mod_off())
            press(KEY_MOD_ON_OFF);
        break;
    case ('S' << 8) | 'N':
        if (read_single_digit(4, &digit))
            set_second_reading(digit);
        break;
    case ('I' << 8) | 'P':
        if (read_single_digit(3, &digit))
            set_input(digit);
        break;
    case ('R' << 8) | 'D':
        reading_command();
        break;
    case ('V' << 8) | 'N':
        put_string(VERSION_TEXT);
        put_line_end();
        break;
    case ('E' << 8) | 'R':
        put_number(gpib.last_error);
        put_line_end();
        break;
    case ('S' << 8) | 'Q':
        if (read_single_digit(3, &digit))
            gpib.request_level = digit;
        break;
    case ('E' << 8) | 'V':
        gpib.variable_enabled = 1;
        break;
    case ('D' << 8) | 'V':
        gpib.variable_enabled = 0;
        break;
    case ('S' << 8) | 'P':
        gpib.suppress_indicators = 1;
        break;
    case ('R' << 8) | 'S':
        gpib.suppress_indicators = 0;
        if (gpib.settings_received) {
            instrument_settings settings = {0};

            gpib.settings_received = 0;
            settings_from_image(gpib.settings_image, &settings);
            app_set_settings(&settings);
        }
        if (app_is_transmit())
            tx_refresh();
        else
            app_redraw();
        gpib_redraw_indicators();
        break;
    case ('S' << 8) | 'V':
        put_settings();
        break;
    case ('S' << 8) | 'K':
        gpib.key_wanted = 1;
        break;
    case ('P' << 8) | 'E':
        read_memory();
        break;
    case ('P' << 8) | 'O':
        poke_memory(1);
        break;
    case ('D' << 8) | 'U':
        poke_memory(MAX_POKE_BYTES);
        break;
    case ('S' << 8) | 'E':
        write_poke_flag(0);
        break;
    case ('S' << 8) | 'D':
        write_poke_flag(1);
        break;
    case ('P' << 8) | 'G':
        gpib.output_head = gpib.output_tail;
        clear_read_status();
        break;
    case ('U' << 8) | 'C':
        gpib.upper_case = 1;
        break;
    case ('L' << 8) | 'C':
        gpib.upper_case = 0;
        break;
    case ('P' << 8) | 'D':
    case ('E' << 8) | 'M':
        hal_write(PARAM_LEVEL_EMF, first == 'E');
        hal_delay_ms(EEPROM_WRITE_MS);
        app_level_type_changed();
        break;
    case ('C' << 8) | 'S':
        screen_clear();
        break;
    case ('W' << 8) | 'R':
        write_screen();
        break;
    default:
        if (first == 'X' && second >= 'A' && second <= 'G')
            press(soft_keys[second - 'A']);
        else if (known_unimplemented(first, second))
            skip_data();
        else
            report_error(ERROR_SYNTAX);
        break;
    }
}

static void run_line(void)
{
    gpib.output_added = 0;
    line_pos = (const char *)gpib.input;
    line_end = line_pos + gpib.input_len;

    while (has_more()) {
        char first = *line_pos++;

        if (is_separator(first) || first == '+')
            continue;
        if (is_number_char(first)) {
            uint8_t key = number_keys[first - '-'];
            if (key)
                press(key);
            continue;
        }
        if (!is_letter(first) || !has_more() || !is_letter(*line_pos)) {
            report_error(ERROR_SYNTAX); /* and this character is dropped */
            continue;
        }
        run_command(first, *line_pos++);
    }
    if (gpib.output_added && gpib.output_head != gpib.output_tail)
        report_data_ready();
}

/* ---- hardware ------------------------------------------------------------------------ */

static void draw_indicator(uint8_t row, const char *text, uint8_t show)
{
    if (show) {
        screen_print_reverse(INDICATOR_COL, row, text);
    } else {
        for (uint8_t i = 0; i < 3u; i++) {
            screen_put((uint8_t)(INDICATOR_COL + i), row, ' ');
            screen_put((uint8_t)(INDICATOR_COL + i), (uint8_t)(row - 1u), ' ');
        }
    }
}

/* The characters the SK command reports, by key code. */
static const char key_chars[] = "\0AIQ19%\0\0FN\00020*$\0HPW3\x7F=,>DLT4\0/(?EMU5.Z)!GOV6-+;<BJR7\0X\0\0CKS8\0Y#\"";

uint8_t gpib_front_panel_key(uint8_t code)
{
    if (!gpib.remote)
        return 1;
    if (code == KEY_HELP && !gpib.lockout)
        hal_write(REG_AUX, AUX_RETURN_TO_LOCAL);
    if (gpib.key_wanted && code < sizeof key_chars && key_chars[code]) {
        gpib.key_wanted = 0;
        put_char(key_chars[code]);
        put_line_end();
        report_data_ready();
    }
    return 0;
}

uint8_t gpib_talk_only(void)
{
    return gpib.talk_only;
}

void gpib_print_page(void)
{
    if (gpib.talk_only)
        put_page();
}

void gpib_report_entry_error(uint8_t code)
{
    if (gpib.remote) {
        gpib.last_error = code;
        report_error(ERROR_NUMERIC_ENTRY);
    }
}

uint8_t gpib_variable_enabled(void)
{
    return !gpib.remote || gpib.variable_enabled;
}

static void draw_local_box(uint8_t show)
{
    if (show) {
        screen_softkey_box(LOCAL_BOX_POSITION, LOCAL_BOX_WIDTH);
        screen_print(LOCAL_TEXT_COL, LOCAL_BOX_TOP_ROW + 1u, "LCL");
    } else {
        for (uint8_t row = LOCAL_BOX_TOP_ROW; row < LOCAL_BOX_TOP_ROW + 3u; row++) {
            for (uint8_t col = LOCAL_BOX_COL; col < SCREEN_COLS; col++)
                screen_put(col, row, ' ');
        }
    }
}

void gpib_redraw_indicators(void)
{
    uint8_t active = gpib.remote || gpib.addressed || gpib.request_shown;

    if (gpib.suppress_indicators || (!active && !gpib.indicators_shown))
        return;
    gpib.indicators_shown = active;
    draw_indicator(INDICATOR_REM_ROW, "REM", active && gpib.remote);
    draw_indicator(INDICATOR_ADR_ROW, "ADR", active && gpib.addressed);
    draw_indicator(INDICATOR_SRQ_ROW, "SRQ", active && gpib.request_shown);
    draw_local_box(active && gpib.remote && !gpib.lockout);
}

void gpib_init(void)
{
    gpib.present = 0;
    gpib.upper_case = 0;
    gpib.data_out_ready = 0;
    gpib.remote = gpib.addressed = 0;
    gpib.input_len = 0;
    gpib.output_head = gpib.output_tail = 0;
    gpib.line_ready = 0;
    gpib.key_wanted = 0;
    gpib.talk_only = 0;
    gpib.indicators_shown = 0;
    gpib.lockout = 0;
    gpib.request_shown = 0;
    gpib.request_level = 0;
    gpib.suppress_indicators = 0;
    gpib.variable_enabled = 0;
    gpib.last_error = 0;
    gpib.settings_received = 0;

    hal_write(REG_AUX, AUX_RESET);
    hal_write(REG_ADDR_MODE, ADDR_MODE_NORMAL);

    /* The interface is an option: without it these registers do not read back. */
    hal_write(REG_ADDR, 0);
    if (hal_read(REG_ADDR) != 0)
        return;
    hal_write(REG_ADDR, SWITCH_ADDRESS_MASK);
    if (hal_read(REG_ADDR) != SWITCH_ADDRESS_MASK)
        return;
    gpib.present = 1;

    uint8_t switches = (uint8_t)~hal_read(REG_SWITCHES);

    hal_write(REG_ADDR_MODE, ADDR_MODE_NORMAL);
    hal_write(REG_ADDR, switches & SWITCH_ADDRESS_MASK);
    hal_write(REG_ADDR, ADDR_DISABLE_SECOND);
    gpib.upper_case = (switches & 0x3Fu) >= SWITCH_UPPER_CASE_FROM;
    gpib.talk_only = (switches & SWITCH_TALK_ONLY) != 0;
    if (gpib.talk_only)
        hal_write(REG_ADDR_MODE, ADDR_MODE_TALK_ONLY);

    hal_write(REG_ISR2, 0); /* polled, so no interrupts */
    hal_write(REG_ISR1, 0);
    hal_write(REG_AUX, AUX_REGISTER_B_SETUP);
    hal_write(REG_AUX, AUX_EOS_MODE);
    hal_write(REG_AUX, AUX_REGISTER_E_SETUP);
    hal_write(REG_EOS, EOS_LF);
    hal_write(REG_AUX, AUX_CLEAR);
}

static void receive_byte(uint8_t byte)
{
    if (byte == TERM_LF || byte == TERM_ETX || byte == TERM_ETB) {
        gpib.line_ready = 1;
        return;
    }
    if (gpib.input_len < INPUT_SIZE)
        gpib.input[gpib.input_len++] = byte;
    else
        report_error(ERROR_OVERFLOW);
}

void gpib_poll(void)
{
    if (!gpib.present)
        return;

    uint8_t isr1 = hal_read(REG_ISR1);
    uint8_t isr2 = hal_read(REG_ISR2);

    if (isr1 & ISR1_DATA_IN)
        receive_byte(hal_read(REG_DATA));
    if (isr1 & ISR1_END)
        gpib.line_ready = 1;
    if (isr1 & ISR1_DATA_OUT)
        gpib.data_out_ready = 1;
    if (isr1 & ISR1_DEVICE_CLEAR) {
        gpib.input_len = 0;
        gpib.output_head = gpib.output_tail;
        app_init();
        return;
    }

    if (isr2 & (ISR2_ADDRESS_CHANGE | ISR2_REMOTE_CHANGE | ISR2_LOCKOUT_CHANGE)) {
        gpib.addressed = (hal_read(REG_ADDR_MODE) & ADDR_TALK_OR_LISTEN) != 0;
        gpib.remote = (isr2 & ISR2_REMOTE) != 0;
        gpib.lockout = (isr2 & ISR2_LOCKOUT) != 0;
        gpib_redraw_indicators();
    }

    /* A serial poll withdraws the request in the chip. */
    if (gpib.request_shown && !(hal_read(REG_SERIAL_POLL) & STATUS_SERVICE_REQUEST)) {
        gpib.request_shown = 0;
        gpib_redraw_indicators();
    }

    if (gpib.line_ready) {
        run_line();
        gpib.input_len = 0;
        gpib.line_ready = 0;
    }

    /* Only an addressed talker may send, otherwise the byte would wait in the chip. */
    if (gpib.data_out_ready && gpib.output_tail != gpib.output_head
        && (gpib.talk_only || (hal_read(REG_ADDR_MODE) & ADDR_TALKER))) {
        gpib.data_out_ready = 0;
        hal_write(REG_DATA, gpib.output[gpib.output_tail]);
        gpib.output_tail = (uint16_t)((gpib.output_tail + 1u) % OUTPUT_SIZE);
        if (gpib.output_tail == gpib.output_head)
            clear_read_status();
    }
}
