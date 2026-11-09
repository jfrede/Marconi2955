#include "regmenu.h"

#include "control.h"
#include "format.h"
#include "hal.h"
#include "keys.h"
#include "regs.h"
#include "screen.h"
#include "service.h"

#define ROW_REGISTER 2u
#define ROW_FIRST_FIELD 4u
#define ROWS_PER_FIELD 2u
#define COL_BITS 0u
#define COL_LABEL 4u
#define COL_VALUE 30u
#define VALUE_WIDTH 3u
#define COL_LOCK 34u
#define COL_ENTRY 34u
#define LABEL_WIDTH 25u
#define ENTRY_DIGITS 3u
#define RETURN_POSITION 6u
#define RETURN_WIDTH 6u
#define LIVE_INTERVAL_MS 200u
#define ADC_BASE 0xE800u
#define ADC_CHANNELS 4u
#define GPIB_SWITCHES 0xE040u
#define STATUS_ROW_FIRST 4u
#define ADC_ROW_FIRST 16u
#define SWITCH_ROW 28u
#define FIRST_REGISTER_SHOWN 0xE017u /* the page most often wanted first */

#define FIELD_LOCKED 1u

typedef struct {
    uint8_t high, low;
    uint8_t flags;
    const char *label;
} reg_field;

typedef struct {
    uint16_t address;
    const char *name;
    const reg_field *fields;
    uint8_t field_count;
} reg_page;

#define FIELDS(array) (array), (uint8_t)(sizeof(array) / sizeof((array)[0]))

/* Bits whose meaning has only been inferred say so in their label. */
static const reg_field band_fields[] = {{7, 0, 0, "BAND AND DIVIDER"}};
static const reg_field synth_data_fields[] = {
    {7, 7, 0, "10 DB PAD INVERTED"},
    {6, 4, FIELD_LOCKED, "SYNTH STROBES"},
    {3, 0, FIELD_LOCKED, "SYNTH DATA"},
};
static const reg_field rf_control_fields[] = {
    {4, 4, 0, "COUNTER RUN"},
    {3, 3, 0, "POWER RANGE"},
    {2, 2, 0, "FINE RESOLUTION"},
    {1, 1, 0, "LOW BAND"},
    {0, 0, FIELD_LOCKED, "TUNING STROBE"},
};
static const reg_field af_low_fields[] = {{7, 0, 0, "AF COUNT LOW BYTE"}};
static const reg_field af_high_fields[] = {{7, 0, 0, "AF COUNT HIGH BYTE"}};
static const reg_field af_level_fields[] = {{7, 0, 0, "VARIABLE OSC LEVEL DAC"}};
static const reg_field mod_level_fields[] = {{7, 0, 0, "FIXED 1 KHZ MOD LEVEL DAC"}};
static const reg_field af_out_fields[] = {{7, 0, 0, "FIXED 1 KHZ AF OUT DAC"}};
static const reg_field af_switch_fields[] = {
    {7, 4, 0, "AF FILTER BAND"},
    {3, 0, 0, "LEVEL RANGE"},
};
static const reg_field routing_fields[] = {
    {7, 7, 0, "MOD LOW RANGE"},
    {6, 6, 0, "TRANSMIT"},
    {5, 4, 0, "AF GENERATOR RANGE"},
    {3, 0, 0, "MOD TYPE AND BAND"},
};
static const reg_field atten_fields[] = {
    {7, 7, 0, "COARSE ATTENUATOR BIT 7"},
    {6, 6, 0, "COARSE ATTENUATOR BIT 6"},
    {5, 5, 0, "COARSE ATTENUATOR BIT 5"},
    {4, 4, 0, "BNC SOCKET"},
    {3, 3, 0, "POWER RANGE"},
    {2, 2, FIELD_LOCKED, "TUNER STROBE MAYBE"},
    {1, 0, 0, "OTHER SWITCHES"},
};
static const reg_field tracking_fields[] = {{7, 0, 0, "MODULATION TRACKING DAC"}};
static const reg_field if_fields[] = {{7, 0, 0, "IF CORRECTION DAC"}};
static const reg_field fine_low_fields[] = {
    {7, 6, 0, "FINE LEVEL DAC LOW BITS"},
    {3, 0, 0, "OUTPUT 4 FM 3 AM 7 OFF"},
};
static const reg_field fine_high_fields[] = {{7, 0, 0, "FINE LEVEL DAC HIGH BITS"}};
static const reg_field filter_fields[] = {
    {7, 6, 0, "POWER METER RANGE"},
    {3, 2, 0, "AF FILTER"},
    {1, 0, 0, "DEMODULATOR"},
};
static const reg_field path_fields[] = {
    {7, 6, 0, "1 RECEIVE 2 TRANSMIT"},
    {5, 4, 0, "SCOPE AND TRIGGER GAIN"},
    {3, 2, 0, "VOLTMETER RANGE"},
    {1, 0, 0, "MEASURING PATH"},
};
static const reg_field range_fields[] = {
    {7, 7, 0, "VOLTMETER RANGE BIT"},
    {6, 6, 0, "SCOPE GAIN BIT"},
    {5, 4, 0, "MEANING UNKNOWN"},
    {3, 3, 0, "SCOPE GAIN BIT"},
    {2, 2, 0, "AC COUPLING"},
    {1, 1, 0, "RANGE AND GAIN BIT"},
    {0, 0, 0, "PEAK HOLD"},
};
static const reg_field control_fields[] = {
    {7, 3, FIELD_LOCKED, "OTHER FUNCTIONS"},
    {2, 2, FIELD_LOCKED, "NOVRAM RECALL PULSE"},
    {1, 1, 0, "STOP SCOPE TIMEBASE"},
    {0, 0, 0, "RF COUNTER GATE"},
};

static const reg_page pages[] = {
    {0xE000u, "SYNTH BAND", FIELDS(band_fields)},
    {0xE001u, "SYNTH DATA", FIELDS(synth_data_fields)},
    {0xE006u, "RF COUNTER", FIELDS(rf_control_fields)},
    {0xE010u, "AF COUNT", FIELDS(af_low_fields)},
    {0xE011u, "AF COUNT", FIELDS(af_high_fields)},
    {0xE012u, "AF OSCILLATOR", FIELDS(af_level_fields)},
    {0xE013u, "MODULATION", FIELDS(mod_level_fields)},
    {0xE014u, "AF OUTPUT", FIELDS(af_out_fields)},
    {0xE015u, "AF SWITCHING", FIELDS(af_switch_fields)},
    {0xE016u, "ROUTING", FIELDS(routing_fields)},
    {0xE017u, "ATTENUATOR", FIELDS(atten_fields)},
    {0xE018u, "TRACKING", FIELDS(tracking_fields)},
    {0xE019u, "IF CORRECTION", FIELDS(if_fields)},
    {0xE01Au, "FINE LEVEL", FIELDS(fine_low_fields)},
    {0xE01Bu, "FINE LEVEL", FIELDS(fine_high_fields)},
    {0xE03Bu, "FILTER AND DEMOD", FIELDS(filter_fields)},
    {0xE03Du, "METER PATH", FIELDS(path_fields)},
    {0xE03Eu, "METER RANGE", FIELDS(range_fields)},
    {REG_STATUS, "CONTROL", FIELDS(control_fields)},
};

#define PAGE_COUNT ((uint8_t)(sizeof pages / sizeof pages[0]))
#define INPUTS_PAGE PAGE_COUNT /* one more page, showing the inputs */

static struct {
    uint8_t active;
    uint8_t page;
    uint8_t field;
    char entry[ENTRY_DIGITS + 1u];
    uint8_t entry_len;
    uint32_t next_update;
    uint8_t adc_channel;
    uint8_t adc[ADC_CHANNELS];
} rm;

static char hex_digit(uint8_t value)
{
    return (char)(value < 10u ? (char)('0' + value) : (char)('A' + value - 10u));
}

static void put_hex(char *out, uint8_t value)
{
    out[0] = hex_digit((uint8_t)(value >> 4));
    out[1] = hex_digit((uint8_t)(value & 0x0Fu));
    out[2] = 0;
}

static uint8_t field_mask(const reg_field *f)
{
    return (uint8_t)((0xFFu >> (7u - (f->high - f->low))) << f->low);
}

static uint8_t register_value(const reg_page *p, uint8_t *value)
{
    if (p->address == REG_STATUS) {
        *value = control_get();
        return 1;
    }
    return hal_last_written(p->address, value);
}

static void write_register(const reg_page *p, uint8_t value)
{
    if (p->address == REG_STATUS)
        control_set(0xFFu, value);
    else
        hal_write(p->address, value);
}

static void draw_common(const char *name)
{
    char text[12];

    screen_clear();
    screen_print(4, 0, "HARDWARE REGISTERS");
    for (uint8_t col = 4; col < 22; col++)
        screen_put(col, 1, SCREEN_UNDERLINE);
    put_text(put_fixed(text, (uint32_t)rm.page + 1u, 0), " OF ");
    screen_print(29, 0, text);
    put_fixed(text, (uint32_t)PAGE_COUNT + 1u, 0);
    screen_print(35, 0, text);
    screen_print(0, ROW_REGISTER, name);
    screen_softkey_box(RETURN_POSITION, RETURN_WIDTH);
    screen_print((uint8_t)(SCREEN_COLS - 2u - RETURN_WIDTH), (uint8_t)(RETURN_POSITION * 4u + 1u), "RETURN");
}

static void draw_field_value(const reg_field *f, uint8_t row, uint8_t known, uint8_t value)
{
    char text[8];
    uint8_t mask = field_mask(f);

    if (known)
        put_fixed(text, (uint32_t)((value & mask) >> f->low), 0);
    else
        put_text(text, "--");
    print_field(COL_VALUE, row, text, VALUE_WIDTH);
}

static void draw_page(void)
{
    const reg_page *p = &pages[rm.page];
    uint8_t value = 0;
    uint8_t known = register_value(p, &value);
    char text[8];

    draw_common("");
    text[0] = hex_digit((uint8_t)(p->address >> 12));
    text[1] = hex_digit((uint8_t)((p->address >> 8) & 0x0Fu));
    text[2] = hex_digit((uint8_t)((p->address >> 4) & 0x0Fu));
    text[3] = hex_digit((uint8_t)(p->address & 0x0Fu));
    text[4] = 0;
    screen_print(0, ROW_REGISTER, text);
    screen_print(5, ROW_REGISTER, p->name);
    if (known) {
        put_hex(text, value);
        screen_print(COL_VALUE, ROW_REGISTER, text);
    } else {
        screen_print(COL_VALUE, ROW_REGISTER, "--");
    }

    for (uint8_t i = 0; i < p->field_count; i++) {
        const reg_field *f = &p->fields[i];
        uint8_t row = (uint8_t)(ROW_FIRST_FIELD + i * ROWS_PER_FIELD);

        text[0] = (char)('0' + f->high);
        if (f->high != f->low) {
            text[1] = '-';
            text[2] = (char)('0' + f->low);
            text[3] = 0;
        } else {
            text[1] = 0;
        }
        screen_print(COL_BITS, row, text);
        if (i == rm.field)
            screen_print_reverse(COL_LABEL, row, f->label);
        else
            screen_print(COL_LABEL, row, f->label);
        draw_field_value(f, row, known, value);
        if (f->flags & FIELD_LOCKED)
            screen_print(COL_LOCK, row, "LOCK");
    }
    if (rm.entry_len) {
        rm.entry[rm.entry_len] = 0;
        uint8_t row = (uint8_t)(ROW_FIRST_FIELD + rm.field * ROWS_PER_FIELD);
        print_field(COL_ENTRY, row, rm.entry, ENTRY_DIGITS + 1u);
    }
}

static void update_inputs(void)
{
    char text[12];
    uint8_t status = hal_read(REG_STATUS);

    screen_print(0, STATUS_ROW_FIRST, "STATUS EE00");
    put_hex(text, status);
    print_field(COL_VALUE, STATUS_ROW_FIRST, text, VALUE_WIDTH);
    screen_print(1, 6, "OVERLOAD");
    screen_print(1, 8, "ANALOG SETTLED");
    screen_print(1, 10, "AF COUNTER PRESCALER");
    screen_print(1, 12, "SCREEN CAN BE WRITTEN");
    print_field(COL_VALUE, 6, (status & 0x01u) ? "1" : "0", VALUE_WIDTH);
    print_field(COL_VALUE, 8, (status & 0x04u) ? "1" : "0", VALUE_WIDTH);
    put_fixed(text, (uint32_t)((status >> 3) & 3u), 0);
    print_field(COL_VALUE, 10, text, VALUE_WIDTH);
    print_field(COL_VALUE, 12, (status & 0x80u) ? "1" : "0", VALUE_WIDTH);

    static const char *const names[ADC_CHANNELS] = {"ADC POSITIVE PEAK", "ADC NEGATIVE PEAK", "ADC RMS OR POWER", "ADC HORIZONTAL SHIFT"};
    for (uint8_t i = 0; i < ADC_CHANNELS; i++) {
        uint8_t row = (uint8_t)(ADC_ROW_FIRST + i * ROWS_PER_FIELD);

        screen_print(1, row, names[i]);
        put_fixed(text, rm.adc[i], 0);
        print_field(COL_VALUE, row, text, VALUE_WIDTH);
    }

    screen_print(1, SWITCH_ROW, "GPIB ADDRESS SWITCHES");
    put_hex(text, (uint8_t)~hal_read(GPIB_SWITCHES));
    print_field(COL_VALUE, SWITCH_ROW, text, VALUE_WIDTH);
}

static void draw(void)
{
    if (rm.page == INPUTS_PAGE) {
        draw_common("INPUTS, LIVE");
        update_inputs();
    } else {
        draw_page();
    }
}

static void set_field(uint8_t new_value)
{
    const reg_page *p = &pages[rm.page];
    const reg_field *f = &p->fields[rm.field];
    uint8_t mask = field_mask(f);
    uint8_t value = 0;

    if (f->flags & FIELD_LOCKED)
        return;
    (void)register_value(p, &value);
    write_register(p, (uint8_t)((value & (uint8_t)~mask) | ((uint8_t)(new_value << f->low) & mask)));
}

static void step_field(int8_t direction)
{
    const reg_page *p = &pages[rm.page];
    const reg_field *f = &p->fields[rm.field];
    uint8_t value = 0;
    uint8_t mask = field_mask(f);

    (void)register_value(p, &value);
    set_field((uint8_t)(((value & mask) >> f->low) + direction));
}

static void change_page(int8_t direction)
{
    uint8_t count = (uint8_t)(PAGE_COUNT + 1u);

    rm.page = (uint8_t)((rm.page + count + direction) % count);
    rm.field = 0;
    rm.entry_len = 0;
    draw();
}

static void select_field(int8_t direction)
{
    if (rm.page == INPUTS_PAGE)
        return;
    uint8_t count = pages[rm.page].field_count;

    rm.field = (uint8_t)((rm.field + count + direction) % count);
    rm.entry_len = 0;
    draw();
}

static int8_t key_digit(uint8_t code)
{
    static const uint8_t digit_keys[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};

    for (uint8_t i = 0; i < sizeof digit_keys; i++)
        if (digit_keys[i] == code)
            return (int8_t)i;
    return -1;
}

static void finish_entry(void)
{
    const reg_field *f = &pages[rm.page].fields[rm.field];
    uint16_t value = 0;

    for (uint8_t i = 0; i < rm.entry_len; i++)
        value = (uint16_t)(value * 10u + (uint8_t)(rm.entry[i] - '0'));
    rm.entry_len = 0;
    if (value <= (uint16_t)(field_mask(f) >> f->low))
        set_field((uint8_t)value);
    draw();
}

void regmenu_open(void)
{
    rm.active = 1;
    for (rm.page = 0; pages[rm.page].address != FIRST_REGISTER_SHOWN; rm.page++)
        ;
    rm.field = 0;
    rm.entry_len = 0;
    rm.adc_channel = 0;
    rm.next_update = hal_millis();
    draw();
}

uint8_t regmenu_active(void)
{
    return rm.active;
}

void regmenu_key(uint8_t code)
{
    int8_t digit = key_digit(code);

    if (code == KEY_HOLD_DISPLAY) {
        rm.active = 0;
        service_redraw();
    } else if (code == KEY_FREQ_UP) {
        change_page(1);
    } else if (code == KEY_FREQ_DOWN) {
        change_page(-1);
    } else if (rm.page == INPUTS_PAGE) {
        return;
    } else if (code == KEY_LEVEL_UP) {
        select_field(-1);
    } else if (code == KEY_LEVEL_DOWN) {
        select_field(1);
    } else if (code == KEY_SELECT) {
        step_field(1);
        draw();
    } else if (code == KEY_DELETE) {
        if (rm.entry_len)
            rm.entry_len--;
        else
            step_field(-1);
        draw();
    } else if (code == KEY_DB) {
        finish_entry();
    } else if (digit >= 0 && rm.entry_len < ENTRY_DIGITS) {
        rm.entry[rm.entry_len++] = (char)('0' + digit);
        draw();
    }
}

void regmenu_knob(int8_t direction)
{
    select_field(direction);
}

void regmenu_poll(void)
{
    if (!rm.active || rm.page != INPUTS_PAGE || (int32_t)(hal_millis() - rm.next_update) < 0)
        return;
    rm.next_update = hal_millis() + LIVE_INTERVAL_MS;
    rm.adc[rm.adc_channel] = hal_read((uint16_t)(ADC_BASE + rm.adc_channel));
    rm.adc_channel = (uint8_t)((rm.adc_channel + 1u) % ADC_CHANNELS);
    hal_write((uint16_t)(ADC_BASE + rm.adc_channel), 0);
    update_inputs();
}

void regmenu_redraw(void)
{
    draw();
}
