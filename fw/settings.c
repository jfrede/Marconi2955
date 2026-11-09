#include "settings.h"

#include <math.h>

#include "hal.h"
#include "params.h"

#define EEPROM_SLOTS 0xF000u
#define SLOT_SIZE 45u
#define EEPROM_WRITE_MS 10u

/* Offsets within a slot. */
#define OFS_RF_FREQ 0u
#define OFS_LEVEL 5u
#define OFS_LEVEL_UNIT 9u
#define OFS_LEVEL_OFFSET 10u  /* 0.1 dB, for example -20 dB behind the attenuated socket */
#define OFS_AF_FREQ 12u
#define OFS_AF_LEVEL 16u
#define OFS_MOD_LEVEL 20u
#define OFS_LEVEL_INC 24u
#define OFS_LEVEL_INC_KIND 28u
#define OFS_FREQ_INC 29u
#define OFS_FREQ_INC_KIND 34u
#define OFS_FLAGS1 35u
#define OFS_SCOPE1 36u
#define OFS_SCOPE2 37u
#define OFS_FLAGS2 38u
#define OFS_MODES 39u
#define OFS_FLAGS3 40u
#define FLAGS3_SUBAUDIO 0x20u
#define FLAGS3_TWO_TONE 0x10u
#define OFS_SAT_LEVEL 41u
#define SAT_LEVEL_SCALE 100000.0 /* the stored value is kHz / 10 */
#define AF_FREQ_FIXED 10000
#define AF_FREQ_MIN 200
#define AF_FREQ_MAX 200000
#define AF_LEVEL_MAX 25500
#define AF_LEVEL_DEFAULT 1000
#define MODE_TRANSMIT 0u
#define MODE_RECEIVE 1u
#define MODE_AUDIO 2u
#define MODE_DUPLEX 3u

#define UNIT_DBM 5u
#define INC_KIND_RF 1u
#define INC_KIND_MOD 2u
#define INC_KIND_AF 3u
#define BCD_BYTES 5u

/* Pd in volts at 0 dBm into 50 ohms. */
#define VOLTS_AT_0DBM 0.2236068
/* AM depth is stored as (percent + 1/8) / 39.0625, the modulator's drive level. */
#define AM_LEVEL_DIVISOR 39.0625
#define AM_LEVEL_OFFSET 0.125
#define FM_LEVEL_KHZ_PER_UNIT 10.0

static void put_float(uint8_t *out, double value)
{
    int exponent;
    double scaled;

    if (value == 0.0) {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    scaled = floor(frexp(fabs(value), &exponent) * 65536.0 + 0.5);
    exponent -= 16;
    if (scaled > 65535.0) {
        scaled = 32768.0;
        exponent++;
    }
    out[0] = (uint8_t)(int8_t)exponent;
    out[1] = (uint8_t)((uint32_t)scaled & 0xFFu);
    out[2] = (uint8_t)((uint32_t)scaled >> 8);
    out[3] = value < 0.0 ? 0xFFu : 0u;
}

static double get_float(const uint8_t *in)
{
    double magnitude = ldexp((double)(in[1] | ((uint16_t)in[2] << 8)), (int8_t)in[0]);

    return in[3] ? -magnitude : magnitude;
}

static void put_bcd(uint8_t *out, uint32_t value)
{
    for (uint8_t i = 0; i < BCD_BYTES; i++) {
        uint8_t low = (uint8_t)(value % 10u);
        value /= 10u;
        out[i] = (uint8_t)(low | ((uint8_t)(value % 10u) << 4));
        value /= 10u;
    }
}

static uint32_t get_bcd(const uint8_t *in)
{
    uint32_t value = 0;

    for (uint8_t i = BCD_BYTES; i-- > 0;)
        value = value * 100u + (uint32_t)((in[i] >> 4) * 10u + (in[i] & 0x0Fu));
    return value;
}

static double am_level(int32_t percent)
{
    return (percent + AM_LEVEL_OFFSET) / AM_LEVEL_DIVISOR;
}

static double fm_level(int32_t tenths_hz)
{
    return (tenths_hz / 10000.0) / FM_LEVEL_KHZ_PER_UNIT;
}

static void encode(uint8_t *b, const instrument_settings *s)
{
    for (uint8_t i = 0; i < SLOT_SIZE; i++)
        b[i] = 0;

    put_bcd(b + OFS_RF_FREQ, s->rf_hz);
    if (s->level_unit == LEVEL_UNIT_DBM || s->transmit) {
        put_float(b + OFS_LEVEL, s->level_tenths / 10.0);
        b[OFS_LEVEL_UNIT] = UNIT_DBM;
    } else {
        put_float(b + OFS_LEVEL, s->level_volts);
        b[OFS_LEVEL_UNIT] = s->level_unit;
        b[OFS_LEVEL_OFFSET] = (uint8_t)s->level_offset;
        b[OFS_LEVEL_OFFSET + 1u] = (uint8_t)((uint16_t)s->level_offset >> 8);
    }
    put_float(b + OFS_AF_FREQ, s->af_freq / 10.0);
    put_float(b + OFS_AF_LEVEL, s->af_level / 10000.0);
    if (s->level_is_am)
        put_float(b + OFS_MOD_LEVEL, am_level(s->am_percent));
    else
        put_float(b + OFS_MOD_LEVEL, fm_level((int32_t)s->deviation));

    if (s->transmit) {
        if (s->af_level_inc) {
            put_float(b + OFS_LEVEL_INC, s->af_level_inc / 10000.0);
            b[OFS_LEVEL_INC_KIND] = INC_KIND_AF;
        }
        if (s->af_freq_inc) {
            put_float(b + OFS_FREQ_INC, s->af_freq_inc / 10.0);
            b[OFS_FREQ_INC_KIND] = INC_KIND_AF;
        }
    } else {
        if (s->rf_inc_10hz) {
            put_bcd(b + OFS_FREQ_INC, (uint32_t)s->rf_inc_10hz * 10u);
            b[OFS_FREQ_INC_KIND] = INC_KIND_RF;
        }
        if (s->mod_freq_inc) {
            put_float(b + OFS_FREQ_INC, s->mod_freq_inc / 10.0);
            b[OFS_FREQ_INC_KIND] = INC_KIND_MOD;
        }
        if (s->mod_level_inc) {
            put_float(b + OFS_LEVEL_INC, s->mod_level_inc_am ? am_level(s->mod_level_inc) : fm_level(s->mod_level_inc));
            b[OFS_LEVEL_INC_KIND] = INC_KIND_MOD;
        }
        if (s->rf_level_inc) {
            b[OFS_LEVEL_INC] = (uint8_t)s->rf_level_inc;
            b[OFS_LEVEL_INC + 1u] = (uint8_t)((uint16_t)s->rf_level_inc >> 8);
            b[OFS_LEVEL_INC_KIND] = INC_KIND_RF;
        }
    }

    b[OFS_FLAGS1] = (uint8_t)(s->sinad | (s->input_select << 2) | (s->distortion << 4)
                              | (s->scope_shown << 5) | (s->ac_coupled << 6));
    b[OFS_SCOPE1] = (uint8_t)((s->scope_timebase << 4) | s->scope_rx_am);
    b[OFS_SCOPE2] = (uint8_t)((s->scope_fm << 4) | s->scope_pm);
    b[OFS_FLAGS2] = (uint8_t)((s->mod_off << 2) | ((s->field & 0x06u) << 3) | (s->group << 6) | 1u << 1);
    b[OFS_MODES] = (uint8_t)(((s->duplex ? MODE_DUPLEX : (s->transmit ? MODE_TRANSMIT : (s->audio ? MODE_AUDIO : MODE_RECEIVE))) << 5) | (s->mod_kind << 3) | (s->mod_kind << 1));
    b[OFS_FLAGS3] = (uint8_t)(s->filter | ((s->scope_single ? 0u : 1u) << 3) | (s->manual << 6)
                              | (s->sat_on ? FLAGS3_SUBAUDIO : 0u) | (s->two_tone ? FLAGS3_TWO_TONE : 0u));
    put_float(b + OFS_SAT_LEVEL, s->sat_on && s->level_is_am ? am_level((uint8_t)s->sat_level) : s->sat_level / SAT_LEVEL_SCALE);
}

static void decode(const uint8_t *b, instrument_settings *s)
{
    double level = get_float(b + OFS_LEVEL);
    double modulation = get_float(b + OFS_MOD_LEVEL);

    s->rf_hz = get_bcd(b + OFS_RF_FREQ);
    int16_t offset = (int16_t)(b[OFS_LEVEL_OFFSET] | ((uint16_t)b[OFS_LEVEL_OFFSET + 1u] << 8));
    uint8_t unit = b[OFS_LEVEL_UNIT];

    if (unit == LEVEL_UNIT_V || unit == LEVEL_UNIT_MV || unit == LEVEL_UNIT_UV) {
        s->level_unit = unit;
        s->level_volts = level;
        s->level_offset = offset;
        s->level_tenths = 0; /* worked out by the caller, which knows about EMF */
    } else {
        s->level_unit = LEVEL_UNIT_DBM;
        s->level_volts = 0.0;
        s->level_offset = 0;
        s->level_tenths = (int16_t)(floor(level * 10.0 + 0.5) + offset);
    }
    s->af_freq = (int32_t)floor(get_float(b + OFS_AF_FREQ) * 10.0 + 0.5);
    s->af_level = (int32_t)floor(get_float(b + OFS_AF_LEVEL) * 10000.0 + 0.5);
    s->sat_on = (b[OFS_FLAGS3] & FLAGS3_SUBAUDIO) != 0;
    s->two_tone = (b[OFS_FLAGS3] & FLAGS3_TWO_TONE) != 0;
    s->sat_freq = s->af_freq;
    s->sat_level = (int32_t)floor(get_float(b + OFS_SAT_LEVEL) * SAT_LEVEL_SCALE + 0.5);
    if (s->sat_on || s->af_freq < AF_FREQ_MIN || s->af_freq > AF_FREQ_MAX)
        s->af_freq = AF_FREQ_FIXED; /* the stored frequency is the sub-audible tone */
    if (s->af_level < 0 || s->af_level > AF_LEVEL_MAX)
        s->af_level = AF_LEVEL_DEFAULT;

    s->sinad = b[OFS_FLAGS1] & 0x03u;
    s->input_select = (b[OFS_FLAGS1] >> 2) & 0x03u;
    s->distortion = (b[OFS_FLAGS1] >> 4) & 1u;
    s->scope_shown = (b[OFS_FLAGS1] >> 5) & 1u;
    s->ac_coupled = (b[OFS_FLAGS1] >> 6) & 1u;
    s->scope_timebase = b[OFS_SCOPE1] >> 4;
    s->scope_rx_am = b[OFS_SCOPE1] & 0x0Fu;
    s->scope_fm = b[OFS_SCOPE2] >> 4;
    s->scope_pm = b[OFS_SCOPE2] & 0x0Fu;
    s->mod_off = (b[OFS_FLAGS2] >> 2) & 1u;
    s->field = (b[OFS_FLAGS2] >> 3) & 0x07u;
    s->group = b[OFS_FLAGS2] >> 6;
    s->transmit = ((b[OFS_MODES] >> 5) & 0x03u) == MODE_TRANSMIT;
    s->duplex = ((b[OFS_MODES] >> 5) & 0x03u) == MODE_DUPLEX;
    s->audio = ((b[OFS_MODES] >> 5) & 0x03u) == MODE_AUDIO;
    s->mod_kind = (b[OFS_MODES] >> 1) & 0x03u;
    s->filter = b[OFS_FLAGS3] & 0x07u;
    s->scope_single = !((b[OFS_FLAGS3] >> 3) & 1u);
    s->manual = (b[OFS_FLAGS3] >> 6) & 1u;

    s->am_percent = 0;
    s->deviation = 0;
    s->level_is_am = !s->transmit && s->mod_kind == 1u;
    if (s->level_is_am) {
        if (s->sat_on)
            s->sat_level = (int32_t)floor(get_float(b + OFS_SAT_LEVEL) * AM_LEVEL_DIVISOR - AM_LEVEL_OFFSET + 0.5);
        double percent = modulation * AM_LEVEL_DIVISOR - AM_LEVEL_OFFSET;
        s->am_percent = percent <= 0.0 ? 0u : (uint8_t)floor(percent + 0.5);
    } else {
        s->deviation = (uint32_t)floor(modulation * FM_LEVEL_KHZ_PER_UNIT * 10000.0 + 0.5);
    }

    s->rf_inc_10hz = 0;
    s->af_freq_inc = 0;
    s->af_level_inc = 0;
    if (b[OFS_FREQ_INC_KIND] == INC_KIND_RF)
        s->rf_inc_10hz = (int32_t)(get_bcd(b + OFS_FREQ_INC) / 10u);
    else if (b[OFS_FREQ_INC_KIND] == INC_KIND_AF)
        s->af_freq_inc = (int32_t)floor(get_float(b + OFS_FREQ_INC) * 10.0 + 0.5);
    s->rf_level_inc = 0;
    s->mod_freq_inc = 0;
    s->mod_level_inc = 0;
    s->mod_level_inc_am = s->level_is_am;
    if (b[OFS_FREQ_INC_KIND] == INC_KIND_MOD)
        s->mod_freq_inc = (int32_t)floor(get_float(b + OFS_FREQ_INC) * 10.0 + 0.5);
    if (b[OFS_LEVEL_INC_KIND] == INC_KIND_MOD) {
        double value = get_float(b + OFS_LEVEL_INC);

        s->mod_level_inc = s->level_is_am ? (int32_t)floor(value * AM_LEVEL_DIVISOR - AM_LEVEL_OFFSET + 0.5)
                                          : (int32_t)floor(value * FM_LEVEL_KHZ_PER_UNIT * 10000.0 + 0.5);
    }
    if (b[OFS_LEVEL_INC_KIND] == INC_KIND_RF)
        s->rf_level_inc = (int16_t)(b[OFS_LEVEL_INC] | ((uint16_t)b[OFS_LEVEL_INC + 1u] << 8));
    if (b[OFS_LEVEL_INC_KIND] == INC_KIND_AF)
        s->af_level_inc = (int32_t)floor(get_float(b + OFS_LEVEL_INC) * 10000.0 + 0.5);
}

static uint16_t slot_address(uint8_t slot)
{
    return (uint16_t)(EEPROM_SLOTS + (uint16_t)(slot - 1u) * SLOT_SIZE);
}

static uint8_t slot_valid(uint8_t slot)
{
    return slot >= SETTINGS_SLOT_FIRST && slot <= SETTINGS_SLOT_LAST;
}

uint8_t settings_store(uint8_t slot, const instrument_settings *settings)
{
    uint8_t bytes[SLOT_SIZE];
    uint16_t address = slot_address(slot);

    if (!slot_valid(slot) || hal_read(PARAM_STORE_DISABLE))
        return 0;
    encode(bytes, settings);
    for (uint8_t i = 0; i < SLOT_SIZE; i++) {
        hal_write((uint16_t)(address + i), bytes[i]);
        hal_delay_ms(EEPROM_WRITE_MS);
    }
    return 1;
}

uint8_t settings_recall(uint8_t slot, instrument_settings *settings)
{
    uint8_t bytes[SLOT_SIZE];
    uint16_t address = slot_address(slot);

    if (!slot_valid(slot))
        return 0;
    for (uint8_t i = 0; i < SLOT_SIZE; i++)
        bytes[i] = hal_read((uint16_t)(address + i));
    /* The audio test does not exist yet. */
    uint8_t mode = (bytes[OFS_MODES] >> 5) & 0x03u;
    if (mode != MODE_TRANSMIT && mode != MODE_RECEIVE && mode != MODE_DUPLEX && mode != MODE_AUDIO)
        return 0;
    decode(bytes, settings);
    return 1;
}

/* Positions in the image. */
#define IMG_COPIED 35u  /* the part that is identical to a slot */
#define IMG_SCOPE1_HI 0x28u
#define IMG_SCOPE1_LO 0x29u
#define IMG_SCOPE2_HI 0x2Au
#define IMG_SCOPE2_LO 0x2Bu
#define IMG_GROUP 0x2Cu
#define IMG_FIELD 0x2Du
#define IMG_FLAGS2_2 0x2Eu
#define IMG_FLAGS2_1 0x2Fu
#define IMG_FLAGS2_0 0x30u
#define IMG_MODE 0x31u
#define IMG_MOD_KIND_A 0x32u
#define IMG_MOD_KIND_B 0x33u
#define IMG_MODES_0 0x35u
#define IMG_FLAGS3_6 0x36u
#define IMG_FLAGS3_5 0x37u
#define IMG_FLAGS3_4 0x38u
#define IMG_FLAGS3_3 0x39u
#define IMG_FILTER 0x3Au
#define IMG_TAIL 0x3Bu
#define IMG_FLAGS1 0x23u

void settings_to_image(const instrument_settings *settings, uint8_t *image)
{
    uint8_t b[SLOT_SIZE] = {0};

    encode(b, settings);
    for (uint8_t i = 0; i < SETTINGS_IMAGE_SIZE; i++)
        image[i] = i < IMG_COPIED ? b[i] : 0u;

    image[IMG_FLAGS1 + 0u] = (b[OFS_FLAGS1] >> 6) & 1u;
    image[IMG_FLAGS1 + 1u] = (b[OFS_FLAGS1] >> 5) & 1u;
    image[IMG_FLAGS1 + 2u] = (b[OFS_FLAGS1] >> 4) & 1u;
    image[IMG_FLAGS1 + 3u] = (b[OFS_FLAGS1] >> 2) & 3u;
    image[IMG_FLAGS1 + 4u] = b[OFS_FLAGS1] & 3u;
    image[IMG_SCOPE1_HI] = b[OFS_SCOPE1] >> 4;
    image[IMG_SCOPE1_LO] = b[OFS_SCOPE1] & 0x0Fu;
    image[IMG_SCOPE2_HI] = b[OFS_SCOPE2] >> 4;
    image[IMG_SCOPE2_LO] = b[OFS_SCOPE2] & 0x0Fu;
    image[IMG_GROUP] = b[OFS_FLAGS2] >> 6;
    image[IMG_FIELD] = (b[OFS_FLAGS2] >> 3) & 6u;
    image[IMG_FLAGS2_2] = (b[OFS_FLAGS2] >> 2) & 1u;
    image[IMG_FLAGS2_1] = (b[OFS_FLAGS2] >> 1) & 1u;
    image[IMG_FLAGS2_0] = b[OFS_FLAGS2] & 1u;
    image[IMG_MODE] = b[OFS_MODES] >> 5;
    image[IMG_MOD_KIND_A] = (b[OFS_MODES] >> 3) & 3u;
    image[IMG_MOD_KIND_B] = (b[OFS_MODES] >> 1) & 3u;
    image[IMG_MODES_0] = b[OFS_MODES] & 1u;
    image[IMG_FLAGS3_6] = (b[OFS_FLAGS3] >> 6) & 1u;
    image[IMG_FLAGS3_5] = (b[OFS_FLAGS3] >> 5) & 1u;
    image[IMG_FLAGS3_4] = (b[OFS_FLAGS3] >> 4) & 1u;
    image[IMG_FLAGS3_3] = (b[OFS_FLAGS3] >> 3) & 1u;
    image[IMG_FILTER] = b[OFS_FLAGS3] & 7u;
    for (uint8_t i = 0; i < SLOT_SIZE - (OFS_FLAGS3 + 1u); i++)
        image[IMG_TAIL + i] = b[OFS_FLAGS3 + 1u + i];
}

void settings_from_image(const uint8_t *image, instrument_settings *settings)
{
    uint8_t b[SLOT_SIZE];

    for (uint8_t i = 0; i < IMG_COPIED; i++)
        b[i] = image[i];
    b[OFS_FLAGS1] = (uint8_t)((image[IMG_FLAGS1 + 0u] << 6) | (image[IMG_FLAGS1 + 1u] << 5) | (image[IMG_FLAGS1 + 2u] << 4)
                              | (image[IMG_FLAGS1 + 3u] << 2) | image[IMG_FLAGS1 + 4u]);
    b[OFS_SCOPE1] = (uint8_t)((image[IMG_SCOPE1_HI] << 4) | image[IMG_SCOPE1_LO]);
    b[OFS_SCOPE2] = (uint8_t)((image[IMG_SCOPE2_HI] << 4) | image[IMG_SCOPE2_LO]);
    b[OFS_FLAGS2] = (uint8_t)((image[IMG_GROUP] << 6) | ((image[IMG_FIELD] & 6u) << 3) | (image[IMG_FLAGS2_2] << 2)
                              | (image[IMG_FLAGS2_1] << 1) | image[IMG_FLAGS2_0]);
    b[OFS_MODES] = (uint8_t)((image[IMG_MODE] << 5) | (image[IMG_MOD_KIND_A] << 3) | (image[IMG_MOD_KIND_B] << 1)
                             | image[IMG_MODES_0]);
    b[OFS_FLAGS3] = (uint8_t)((image[IMG_FLAGS3_6] << 6) | (image[IMG_FLAGS3_5] << 5) | (image[IMG_FLAGS3_4] << 4)
                              | (image[IMG_FLAGS3_3] << 3) | image[IMG_FILTER]);
    for (uint8_t i = 0; i < SLOT_SIZE - (OFS_FLAGS3 + 1u); i++)
        b[OFS_FLAGS3 + 1u + i] = image[IMG_TAIL + i];
    decode(b, settings);
}
