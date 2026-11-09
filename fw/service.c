#include "service.h"

#include "format.h"
#include "hal.h"
#include "help.h"
#include "keys.h"
#include "level.h"
#include "params.h"
#include "screen.h"

#define KEY_ENTER_ADDRESS KEY_RX
#define KEY_ENTER_DATA KEY_DUPLEX
#define KEY_RETURN KEY_HOLD_DISPLAY
#define KEY_UP_10 KEY_FREQ_UP
#define KEY_UP_1 KEY_LEVEL_UP
#define KEY_DOWN_10 KEY_FREQ_DOWN
#define KEY_DOWN_1 KEY_LEVEL_DOWN

#define BOX_WIDTH 13u
#define BOX_ADDRESS 1u
#define BOX_DATA 2u
#define BOX_RETURN 6u
#define ROW_PER_KEY 4u
#define MARK_COL 23u
#define MARK_CHAR 0xDBu
#define LABEL_ROW 15u
#define VALUE_ROW 18u
#define HEX_ROW 20u
#define COL_ADDRESS 6u
#define COL_READ 20u
#define COL_WRITE 29u
#define ADDRESS_DIGITS 5u
#define DATA_DIGITS 3u
#define EEPROM_FIRST 0xF000u
#define EEPROM_LAST 0xF7FFu
#define DEFAULT_ADDRESS EEPROM_FIRST
#define BYTE_MAX 255u
#define ADDRESS_MAX 65535UL

typedef enum { MODE_CLOSED, MODE_BROWSE, MODE_ADDRESS, MODE_DATA } service_mode;

static struct {
    service_mode mode;
    uint16_t address;
    char entry[6];
    uint8_t entry_len;
} sv;

static char hex_digit(uint8_t value)
{
    return (char)(value < 10u ? (char)('0' + value) : (char)('A' + value - 10u));
}

static void put_decimal(char *out, uint32_t value, uint8_t digits)
{
    out[digits] = 0;
    while (digits--) {
        out[digits] = (char)('0' + value % 10u);
        value /= 10u;
    }
}

static void draw_values(void)
{
    uint8_t data = hal_read(sv.address);
    char text[8];

    put_decimal(text, sv.address, ADDRESS_DIGITS);
    print_field(COL_ADDRESS, VALUE_ROW, text, ADDRESS_DIGITS);
    put_decimal(text, data, DATA_DIGITS);
    print_field(COL_READ, VALUE_ROW, text, DATA_DIGITS);

    text[0] = hex_digit((uint8_t)(sv.address >> 12));
    text[1] = hex_digit((uint8_t)((sv.address >> 8) & 0x0Fu));
    text[2] = hex_digit((uint8_t)((sv.address >> 4) & 0x0Fu));
    text[3] = hex_digit((uint8_t)(sv.address & 0x0Fu));
    text[4] = 0;
    print_field(COL_ADDRESS, HEX_ROW, text, 4u);
    text[0] = hex_digit((uint8_t)(data >> 4));
    text[1] = hex_digit((uint8_t)(data & 0x0Fu));
    text[2] = 0;
    print_field(COL_READ, HEX_ROW, text, 2u);
}

static void draw_marks(void)
{
    screen_put(MARK_COL, (uint8_t)(BOX_ADDRESS * ROW_PER_KEY + 1u), sv.mode == MODE_ADDRESS ? MARK_CHAR : ' ');
    screen_put(MARK_COL, (uint8_t)(BOX_DATA * ROW_PER_KEY + 1u), sv.mode == MODE_DATA ? MARK_CHAR : ' ');
}

static uint8_t entry_col(void)
{
    return sv.mode == MODE_ADDRESS ? COL_ADDRESS : COL_WRITE;
}

static void clear_cursor_row(void)
{
    for (uint8_t col = COL_ADDRESS; col < SCREEN_COLS; col++)
        screen_put(col, (uint8_t)(VALUE_ROW + 1u), ' ');
}

static void draw_entry(void)
{
    uint8_t col = entry_col();

    sv.entry[sv.entry_len] = 0;
    print_field(col, VALUE_ROW, sv.entry, sv.mode == MODE_ADDRESS ? ADDRESS_DIGITS : DATA_DIGITS);
    clear_cursor_row();
    screen_put((uint8_t)(col + sv.entry_len), (uint8_t)(VALUE_ROW + 1u), SCREEN_CURSOR);
}

static void end_entry(void)
{
    sv.mode = MODE_BROWSE;
    sv.entry_len = 0;
    clear_cursor_row();
    print_field(COL_WRITE, VALUE_ROW, "", DATA_DIGITS);
    draw_marks();
    draw_values();
}

static void begin_entry(service_mode mode)
{
    sv.mode = mode;
    sv.entry_len = 0;
    print_field(COL_WRITE, VALUE_ROW, "", DATA_DIGITS);
    draw_marks();
    draw_entry();
}

static void draw_screen(void)
{
    screen_clear();
    screen_print(4, 2, "CALIBRATION DATA MENU");
    screen_softkey_box(BOX_ADDRESS, BOX_WIDTH);
    screen_print(25, 5, "ENTER ADDRESS");
    screen_softkey_box(BOX_DATA, BOX_WIDTH);
    screen_print(25, 9, "ENTER DATA");
    screen_softkey_box(BOX_RETURN, BOX_WIDTH);
    screen_print(25, 25, "RETURN");
    screen_print(5, LABEL_ROW, "ADDRESS");
    screen_print(COL_READ, LABEL_ROW, "READ");
    screen_print(28, LABEL_ROW, "WRITE");
    screen_print(0, HEX_ROW, "HEX");
    sv.mode = MODE_BROWSE;
    sv.entry_len = 0;
    draw_values();
    draw_marks();
}

static uint32_t entry_number(void)
{
    uint32_t value = 0;

    for (uint8_t i = 0; i < sv.entry_len; i++)
        value = value * 10u + (uint8_t)(sv.entry[i] - '0');
    return value;
}

static void write_byte(void)
{
    uint32_t value = entry_number();

    if (!sv.entry_len || value > BYTE_MAX)
        return;
    if (sv.address >= EEPROM_FIRST && sv.address <= EEPROM_LAST) {
        param_set(sv.address, (uint8_t)value);
        level_load_external_attenuators(); /* the only copy of EEPROM data that is kept in memory */
        end_entry();
        return;
    }
    /* Registers cannot be read back, so the value stays visible in the write column. */
    char text[DATA_DIGITS + 1u];

    hal_write(sv.address, (uint8_t)value);
    end_entry();
    put_decimal(text, value, DATA_DIGITS);
    print_field(COL_WRITE, VALUE_ROW, text, DATA_DIGITS);
}

static void finish_address(void)
{
    uint32_t value = entry_number();

    if (value > ADDRESS_MAX) {
        end_entry();
        return;
    }
    sv.address = (uint16_t)value;
    end_entry();
}

static int8_t key_digit(uint8_t code)
{
    static const uint8_t digit_keys[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};

    for (uint8_t i = 0; i < sizeof digit_keys; i++)
        if (digit_keys[i] == code)
            return (int8_t)i;
    return -1;
}

static void entry_key(uint8_t code)
{
    int8_t digit = key_digit(code);
    uint8_t limit = sv.mode == MODE_ADDRESS ? ADDRESS_DIGITS : DATA_DIGITS;

    if (code == KEY_RETURN) {
        end_entry();
    } else if (code == KEY_DELETE) {
        if (sv.entry_len) {
            sv.entry_len--;
            draw_entry();
        }
    } else if (code == KEY_ENTER_DATA && sv.mode == MODE_DATA) {
        write_byte();
    } else if (digit >= 0 && sv.entry_len < limit) {
        sv.entry[sv.entry_len++] = (char)('0' + digit);
        draw_entry();
        if (sv.mode == MODE_ADDRESS && sv.entry_len == ADDRESS_DIGITS)
            finish_address();
    }
}

static void step_address(int16_t step)
{
    sv.address = (uint16_t)(sv.address + step);
    print_field(COL_WRITE, VALUE_ROW, "", DATA_DIGITS);
    draw_values();
}

void service_open(void)
{
    sv.address = DEFAULT_ADDRESS;
    draw_screen();
}

uint8_t service_active(void)
{
    return sv.mode != MODE_CLOSED;
}

void service_key(uint8_t code)
{
    if (sv.mode != MODE_BROWSE) {
        entry_key(code);
        return;
    }
    switch (code) {
    case KEY_ENTER_ADDRESS:
        begin_entry(MODE_ADDRESS);
        break;
    case KEY_ENTER_DATA:
        begin_entry(MODE_DATA);
        break;
    case KEY_UP_10:
        step_address(10);
        break;
    case KEY_UP_1:
        step_address(1);
        break;
    case KEY_DOWN_10:
        step_address(-10);
        break;
    case KEY_DOWN_1:
        step_address(-1);
        break;
    case KEY_RETURN:
        sv.mode = MODE_CLOSED;
        help_service_closed();
        break;
    default:
        break;
    }
}

void service_redraw(void)
{
    uint16_t address = sv.address;

    draw_screen();
    sv.address = address;
    draw_values();
}
