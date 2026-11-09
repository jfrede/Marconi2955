#include "level.h"

#include "generated/level_table.h"
#include "hal.h"
#include "params.h"
#include "regs.h"
#include "synth.h"

#define CAL_STEP_OFFSET 0xF700u /* EEPROM: table offset for each 10 dB step */
#define CAL_DB_TRIM 0xF70Cu     /* EEPROM: signed DAC trim for each 1 dB position */

#define N_SOCKET_PAD 200 /* 20 dB, in 0.1 dB units */
#define BNC_SOCKET_PAD 0
#define ONE_PORT_LOSS_TENTHS_DB 260 /* the signal path to the shared socket */
#define LAST_STEP 12             /* the bottom of the range uses the end of the previous step */
#define ATTENUATION_MAX (LAST_STEP * 100)
#define END_OF_STEP_DB 10

#define ATTEN_BNC_SOCKET 0x10u   /* REG_ATTEN: RF socket selection */
#define ATTEN_LOW_BITS 0x03u     /* unrelated bits sharing REG_ATTEN */
#define ATTEN_RANGE_BIT 0x08u    /* power measurement range while transmitting */

/* Attenuator pattern for each 10 dB step. Bits 7-5 go to REG_ATTEN, bit 4 is the fourth pad. */
static const uint8_t step_pattern[] = {
    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0xC0, 0xD0, 0xE0, 0xF0, 0xF0,
};

static uint8_t dac_low_bits;
static uint8_t output_mode = 4;
static uint8_t step_position;
static uint8_t range_bit = ATTEN_RANGE_BIT;
static uint8_t atten_value;
static uint8_t one_port;
static int16_t external_attenuator[2]; /* 0.1 dB: N socket, BNC socket */

/* In one-port duplex the generator's path leaves this switch alone. */
static uint8_t active_range_bit(void)
{
    return one_port ? 0u : range_bit;
}

static void write_dac_low(void)
{
    hal_write(REG_LEVEL_DAC_LOW, (uint8_t)(dac_low_bits | output_mode));
}

void level_set_power_range_bit(uint8_t on)
{
    range_bit = on ? ATTEN_RANGE_BIT : 0;
    hal_write(REG_ATTEN, (uint8_t)(atten_value | active_range_bit()));
}

uint8_t level_step_position(void)
{
    return step_position;
}

void level_set_output_mode(uint8_t mode)
{
    output_mode = mode;
    write_dac_low();
}

static void set_attenuation(int16_t attenuation, uint8_t socket_bits);

int16_t level_socket_loss(uint8_t bnc)
{
    return (int16_t)((bnc ? BNC_SOCKET_PAD : N_SOCKET_PAD) + external_attenuator[bnc ? 1 : 0]);
}

int16_t level_external_attenuator(uint8_t bnc)
{
    return external_attenuator[bnc ? 1 : 0];
}

uint8_t level_external_valid(int16_t tenths_db)
{
    return tenths_db == 0 || (tenths_db >= LEVEL_EXTERNAL_MIN && tenths_db <= LEVEL_EXTERNAL_MAX);
}

void level_store_external_attenuator(uint8_t bnc, int16_t tenths_db)
{
    uint16_t address = bnc ? PARAM_EXT_ATTEN_BNC : PARAM_EXT_ATTEN_N;

    param_set(address, (uint8_t)tenths_db);
    param_set((uint16_t)(address + 1u), (uint8_t)((uint16_t)tenths_db >> 8));
    external_attenuator[bnc ? 1 : 0] = tenths_db;
}

static int16_t stored_external(uint16_t address)
{
    int16_t value = (int16_t)(param_get(address) | (uint16_t)param_get((uint16_t)(address + 1u)) << 8);

    return level_external_valid(value) ? value : 0; /* other instruments may hold anything here */
}

void level_load_external_attenuators(void)
{
    external_attenuator[0] = stored_external(PARAM_EXT_ATTEN_N);
    external_attenuator[1] = stored_external(PARAM_EXT_ATTEN_BNC);
}

void level_set_without_external(int16_t tenths_dbm, uint8_t bnc)
{
    one_port = 0;
    set_attenuation((int16_t)(-tenths_dbm - (bnc ? BNC_SOCKET_PAD : N_SOCKET_PAD)),
                    (uint8_t)((bnc ? ATTEN_BNC_SOCKET : 0) | ATTEN_LOW_BITS));
}

void level_set(int16_t tenths_dbm, uint8_t bnc)
{
    one_port = 0;
    set_attenuation((int16_t)(-tenths_dbm - level_socket_loss(bnc)),
                    (uint8_t)((bnc ? ATTEN_BNC_SOCKET : 0) | ATTEN_LOW_BITS));
}

void level_set_one_port(int16_t tenths_dbm)
{
    one_port = 1;
    set_attenuation((int16_t)(-tenths_dbm - ONE_PORT_LOSS_TENTHS_DB - external_attenuator[0]), ATTEN_LOW_BITS);
}

void level_set_self_test(int16_t attenuation)
{
    one_port = 1;
    set_attenuation(attenuation, ATTEN_LOW_BITS);
}

static void set_attenuation(int16_t attenuation, uint8_t socket_bits)
{
    if (attenuation < 0)
        attenuation = 0;
    else if (attenuation > ATTENUATION_MAX)
        attenuation = ATTENUATION_MAX;

    uint8_t step = (uint8_t)(attenuation / 100);
    uint8_t db = (uint8_t)(attenuation / 10 % 10);
    uint8_t tenths = (uint8_t)(attenuation % 10);

    if (step == LAST_STEP)
        db = END_OF_STEP_DB;

    step_position = (uint8_t)(db * 10u + tenths);
    uint16_t index = (uint16_t)(step_position + hal_read((uint16_t)(CAL_STEP_OFFSET + step)));
    if (index >= LEVEL_TABLE_SIZE)
        index = LEVEL_TABLE_SIZE - 1; /* only possible with corrupt calibration data */

    int8_t trim = (int8_t)hal_read((uint16_t)(CAL_DB_TRIM + db));
    uint16_t dac = (uint16_t)((uint16_t)level_table[index] << 8);
    dac = (uint16_t)(dac + (int16_t)trim * 64);

    uint8_t pattern = step_pattern[step];
    atten_value = (uint8_t)((pattern & 0xE0u) | socket_bits);
    hal_write(REG_ATTEN, (uint8_t)(atten_value | active_range_bit()));
    synth_set_aux(!(pattern & 0x10u));
    dac_low_bits = (uint8_t)(dac & 0xC0u);
    write_dac_low();
    hal_write(REG_LEVEL_DAC_HIGH, (uint8_t)(dac >> 8));
    synth_update_tracking();
}
