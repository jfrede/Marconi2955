/*
 * Marconi 2955 machine model.
 *
 * Memory map (Service Manual Fig. 36):
 *   0000-DFFF  program ROM
 *   E000-E7FF  off-board I/O
 *   E800-EFFF  on-board I/O; EC00-EC3F holds the instrument settings
 *   F000-F7FF  EEPROM (calibration and stores)
 *   F800-FFFF  RAM
 */
#include "machine.h"

#include <stdlib.h>
#include <string.h>

enum {
    REG_ROWS = 0xE022,
    REG_COLS = 0xE023,
    REG_LEDS = 0xE024,
    REG_CHAR = 0xE025,
    REG_ADDRLO = 0xE026,
    REG_ADDRHI = 0xE027,
    REG_CONTROL = 0xEE00,
    /* uPD7210: read/write meanings differ per register */
    REG_GPIB_SWITCH = 0xE040,
    REG_GPIB_DATA = 0xE048,
    REG_GPIB_ISR1 = 0xE049,
    REG_GPIB_ISR2 = 0xE04A,
    REG_GPIB_ADDR = 0xE04C,
    REG_GPIB_AUX = 0xE04D,
    REG_ADC = 0xE800,          /* ADC0809: write starts a conversion, read returns it */
    REG_TIMER_DATA = 0xEA00,   /* Am9513 data port */
    REG_TIMER_CONTROL = 0xEA01, /* Am9513 command and status port */
    REG_RF_DATA = 0xE004,      /* RF counter digit */
    REG_RF_SELECT = 0xE005,    /* RF counter digit number, in the high nibble */
    REG_RF_CONTROL = 0xE006    /* AA16 */
};

#define TIMER_OUT2 0x04u
#define TIMER_ARM2 0x22u
#define RF_GATE_CYCLES 50000u /* assumed, the real gate length depends on the counter setup */

#define GPIB_DI 0x01u
#define GPIB_DO 0x02u
#define GPIB_ADSC 0x01u
#define GPIB_REMC 0x02u
#define GPIB_REM 0x10u
#define GPIB_LOKC 0x04u
#define GPIB_LOK 0x20u
#define GPIB_SERVICE_REQUEST 0x40u
#define GPIB_AUX_RETURN_TO_LOCAL 0x05u
#define GPIB_STATUS_TALK 0x02u
#define GPIB_STATUS_LISTEN 0x04u
#define GPIB_BYTE_DELAY 400u

/* EE00 bit 7 toggles and paces screen writes. Its real rate is not known yet. */
#define SYNC_HALF_PERIOD 80u
#define KEY_IRQ_CLEAR_BIT 0x20u

static uint8_t read_rows(const machine *m)
{
    uint8_t rows = 0xFF;
    if (m->key_row >= 0 && !(m->columns >> m->key_col & 1))
        rows &= (uint8_t)~(1u << m->key_row);
    return rows;
}

static uint8_t read_status(const machine *m)
{
    uint8_t value = m->status_in & 0x7F;
    if ((m->cpu.cycles / SYNC_HALF_PERIOD) & 1)
        value |= 0x80;
    return value;
}

/*
 * Voltmeter front end: ten ranges selected by E03E bits 7, 1 and E03D bits 3, 2.
 * Each range gives volts per ADC count. E03D bit 0 inserts the notch filter.
 */
static uint8_t read_af_adc(const machine *m)
{
    static const struct {
        uint8_t switches;
        double volts_per_count;
    } ranges[] = {
        {0x8E, 0.0005}, {0x8A, 0.001}, {0x86, 0.0025}, {0x8C, 0.005}, {0x88, 0.01},
        {0x84, 0.025}, {0x0C, 0.05}, {0x08, 0.1}, {0x04, 0.25}, {0x00, 0.5},
    };
    uint8_t e3d = m->io[0xE03D - IO_BASE], e3e = m->io[0xE03E - IO_BASE];
    uint8_t key = (uint8_t)((e3e & 0x82u) | (e3d & 0x0Cu));
    double input = (e3d & 1u) ? m->af_notch_volts : m->af_volts;

    for (size_t i = 0; i < sizeof ranges / sizeof ranges[0]; i++) {
        if (ranges[i].switches == key) {
            double counts = input / ranges[i].volts_per_count;
            return counts >= 255.0 ? 255 : (uint8_t)counts;
        }
    }
    return 0;
}

static uint8_t mem_read(void *ctx, uint16_t a)
{
    machine *m = ctx;
    if (m->watch_addr && a == m->watch_addr)
        m->watch_hits[m->cpu.pc]++;
    if (a < ROM_SIZE)
        return m->rom[a];
    if (a >= 0xF800)
        return m->ram[a - 0xF800];
    if (a >= 0xF000)
        return m->eeprom[a - 0xF000];
    m->io_reads[a - IO_BASE]++;
    if (a == REG_ROWS)
        return read_rows(m);
    if (a == REG_CONTROL)
        return read_status(m);
    if (a >= REG_GPIB_SWITCH && a < REG_GPIB_DATA)
        return m->gpib_switches;
    if (a >= REG_ADC && a < REG_ADC + 4)
        return m->adc_output;
    if (a == REG_TIMER_DATA) {
        uint16_t value = m->timer_hold[m->timer_group];
        uint8_t result = (uint8_t)(m->timer_high_byte ? value >> 8 : value);
        m->timer_high_byte = !m->timer_high_byte;
        return result;
    }
    if (a == REG_TIMER_CONTROL)
        return m->rf_gate_done ? TIMER_OUT2 : 0; /* other status bits are not modelled */
    if (a == REG_RF_DATA) {
        unsigned digit = m->io[REG_RF_SELECT - IO_BASE] >> 4;
        /* AA16 bit 2 follows the gate selection: set = 1 Hz per count, clear = 10 Hz. */
        uint64_t count = m->rf_hz / ((m->io[REG_RF_CONTROL - IO_BASE] & 4) ? 1 : 10);
        if (digit >= RF_DIGITS)
            return 0;
        while (digit--)
            count /= 10;
        return (uint8_t)(count % 10);
    }
    switch (a) {
    case REG_GPIB_DATA: {
        uint8_t value = 0xFF;
        if (m->gpib_in_pos < m->gpib_in_len) {
            value = m->gpib_in[m->gpib_in_pos++];
            if (m->gpib_in_pos < m->gpib_in_len)
                m->gpib_di_at = m->cpu.cycles + GPIB_BYTE_DELAY;
        }
        return value;
    }
    case REG_GPIB_ISR1: {
        uint8_t value = m->gpib_isr1;
        m->gpib_isr1 = 0;
        return value;
    }
    case REG_GPIB_ISR2: {
        uint8_t value = (uint8_t)(m->gpib_isr2 | (m->gpib_remote ? GPIB_REM : 0) | (m->gpib_lockout ? GPIB_LOK : 0));
        m->gpib_isr2 = 0;
        return value;
    }
    case REG_GPIB_ADDR:
        return m->gpib_addr_status;
    default:
        return m->io[a - IO_BASE];
    }
}

static void mem_write(void *ctx, uint16_t a, uint8_t v)
{
    machine *m = ctx;
    if (a < ROM_SIZE) {
        m->rom_writes++;
        return;
    }
    if (a >= 0xF800) {
        m->ram[a - 0xF800] = v;
        return;
    }
    if (a >= 0xF000) {
        m->eeprom[a - 0xF000] = v;
        m->eeprom_writes++;
        return;
    }
    m->io_writes[a - IO_BASE]++;
    m->io[a - IO_BASE] = v;
    /* ADC0809: a write starts a conversion; the result stays until the next one. */
    if (a >= REG_ADC && a < REG_ADC + 4)
        m->adc_output = (a == REG_ADC + 2 && m->af_model) ? read_af_adc(m) : m->adc[a - REG_ADC];
    if (m->trace && !(a >= REG_CHAR && a <= REG_ADDRHI))
        fprintf(m->trace, "%llu %04X %04X %02X\n", (unsigned long long)m->cpu.cycles, m->cpu.pc, a, v);
    switch (a) {
    case REG_COLS:
        m->columns = v;
        break;
    case REG_LEDS:
        m->leds = v;
        break;
    case REG_ADDRLO:
        m->screen_col = v;
        break;
    case REG_ADDRHI:
        m->screen_row = v;
        break;
    case REG_CHAR:
        if (m->screen_row < SCREEN_ROWS && m->screen_col < SCREEN_COLS)
            m->screen[m->screen_row][m->screen_col] = v;
        break;
    case REG_CONTROL:
        if (!(v & KEY_IRQ_CLEAR_BIT))
            m->key_irq = false;
        break;
    case REG_GPIB_DATA:
        if (m->gpib_out_len < GPIB_BUFFER)
            m->gpib_out[m->gpib_out_len++] = v;
        if (v == '\n')
            m->gpib_line_done = true;
        else if (m->gpib_addr_status & GPIB_STATUS_TALK)
            m->gpib_do_at = m->cpu.cycles + GPIB_BYTE_DELAY;
        break;
    case REG_TIMER_CONTROL:
        /* Commands 00h-1Fh load the data pointer; the low 3 bits select the counter. */
        if (v < 0x20 && (v & 7) >= 1 && (v & 7) <= 5) {
            m->timer_group = v & 7;
            m->timer_high_byte = false;
        }
        if (v == TIMER_ARM2) {
            m->rf_gate_done = false;
            m->rf_gate_at = m->cpu.cycles + RF_GATE_CYCLES;
        }
        break;
    case REG_GPIB_ISR1:
        m->gpib_imr1 = v;
        break;
    case REG_GPIB_ISR2:
        m->gpib_imr2 = v;
        break;
    case REG_GPIB_AUX:
        if (v == GPIB_AUX_RETURN_TO_LOCAL && m->gpib_remote && !m->gpib_lockout) {
            m->gpib_remote = false;
            m->gpib_isr2 |= GPIB_REMC;
        }
        break;
    default:
        break;
    }
}

void machine_init(machine *m)
{
    memset(m, 0, sizeof *m);
    memset(m->io, 0xFF, sizeof m->io);
    memset(m->rom, 0xFF, sizeof m->rom);
    memset(m->eeprom, 0xFF, sizeof m->eeprom);
    memset(m->screen, ' ', sizeof m->screen);
    m->key_row = m->key_col = -1;
    m->status_in = 0x04; /* found by experiment: measurements run with bit 2 set, bits 0-1 clear */
    m->gpib_switches = (uint8_t)~6u; /* switches read inverted; this instrument is at address 6 */
    m->io[0xE04B - IO_BASE] = 0;     /* serial poll status */
    m->timer_hold[3] = 2;            /* the firmware reports 0.0 Hz when counter 3 stays at 2 */
    m->cpu.read = mem_read;
    m->cpu.write = mem_write;
    m->cpu.ctx = m;
    cpu_reset(&m->cpu);
}

bool machine_load(const char *path, uint8_t *dst, size_t size, bool exact)
{
    FILE *f = fopen(path, "rb");
    size_t got;
    int extra;
    if (!f)
        return false;
    got = fread(dst, 1, size, f);
    extra = fgetc(f);
    fclose(f);
    return extra == EOF && (!exact || got == size);
}

/* Reads the text produced by the GPIB SV command: DU<addr>,<byte>,<byte>,... */
bool machine_load_settings(machine *m, const char *path)
{
    char line[512];
    FILE *f = fopen(path, "r");
    bool any = false;
    if (!f)
        return false;
    while (fgets(line, sizeof line, f)) {
        char *p = strstr(line, "DU"), *end;
        unsigned long addr;
        if (!p)
            continue;
        addr = strtoul(p + 2, &end, 10);
        while (*end == ',' && addr >= IO_BASE && addr < IO_BASE + IO_SIZE) {
            m->io[addr++ - IO_BASE] = (uint8_t)strtoul(end + 1, &end, 10);
            any = true;
        }
    }
    fclose(f);
    return any;
}

void machine_press_key(machine *m, int row, int col, uint64_t hold)
{
    m->key_row = row;
    m->key_col = col;
    m->key_release = m->cpu.cycles + hold;
    m->key_irq = true;
}

void machine_knob(machine *m, int direction)
{
    m->cpu.sid = direction > 0;
    cpu_pulse_rst75(&m->cpu);
}

void machine_gpib_send(machine *m, const char *text)
{
    size_t len = strlen(text);
    if (len + 1 > GPIB_BUFFER)
        return;
    memcpy(m->gpib_in, text, len);
    m->gpib_in[len] = '\n';
    m->gpib_in_len = len + 1;
    m->gpib_in_pos = 0;
    m->gpib_addr_status = GPIB_STATUS_LISTEN;
    m->gpib_isr2 |= GPIB_ADSC;
    m->gpib_di_at = m->cpu.cycles + GPIB_BYTE_DELAY;
}

void machine_set_rf(machine *m, uint64_t hz, bool present)
{
    m->rf_hz = hz;
    m->rf_present = present;
    m->rf_gate_done = present;
}

void machine_gpib_set_remote(machine *m, bool remote)
{
    m->gpib_remote = remote;
    m->gpib_isr2 |= GPIB_REMC;
}

void machine_gpib_set_lockout(machine *m, bool lockout)
{
    m->gpib_lockout = lockout;
    m->gpib_isr2 |= GPIB_LOKC;
}

uint8_t machine_gpib_serial_poll(machine *m)
{
    uint8_t status = m->io[0xE04B - IO_BASE];

    m->io[0xE04B - IO_BASE] = (uint8_t)(status & ~GPIB_SERVICE_REQUEST);
    return status;
}

static void gpib_tick(machine *m)
{
    uint64_t now = m->cpu.cycles;
    if (m->gpib_di_at && now >= m->gpib_di_at) {
        m->gpib_di_at = 0;
        m->gpib_isr1 |= GPIB_DI;
    }
    if (m->gpib_do_at && now >= m->gpib_do_at) {
        m->gpib_do_at = 0;
        m->gpib_isr1 |= GPIB_DO;
    }
    if (m->rf_gate_at && now >= m->rf_gate_at) {
        m->rf_gate_at = 0;
        m->rf_gate_done = m->rf_present;
    }
    m->cpu.line55 = (m->gpib_isr1 & m->gpib_imr1) || (m->gpib_isr2 & m->gpib_imr2);
}

void machine_tick(machine *m)
{
    gpib_tick(m);
}

void machine_run(machine *m, uint64_t cycles)
{
    uint64_t end = m->cpu.cycles + cycles;
    while (m->cpu.cycles < end && !m->cpu.bad_opcode) {
        if (m->key_row >= 0 && m->cpu.cycles >= m->key_release)
            m->key_row = m->key_col = -1;
        m->cpu.line65 = m->key_irq;
        gpib_tick(m);
        cpu_step(&m->cpu);
    }
}

bool machine_run_to(machine *m, uint16_t pc, uint64_t max_cycles)
{
    uint64_t end = m->cpu.cycles + max_cycles;
    /* Step once first so that repeated calls advance past the same address. */
    machine_run(m, 1);
    while (m->cpu.pc != pc) {
        if (m->cpu.cycles >= end || m->cpu.bad_opcode)
            return false;
        machine_run(m, 1);
    }
    return true;
}

size_t machine_gpib_read_line(machine *m, uint64_t max_cycles)
{
    uint64_t end = m->cpu.cycles + max_cycles;

    m->gpib_out_len = 0;
    m->gpib_line_done = false;
    m->gpib_addr_status = GPIB_STATUS_TALK;
    m->gpib_isr2 |= GPIB_ADSC;
    m->gpib_do_at = m->cpu.cycles + GPIB_BYTE_DELAY;
    while (m->cpu.cycles < end && !m->gpib_line_done && !m->cpu.bad_opcode)
        machine_run(m, 100);
    m->gpib_addr_status = 0;
    m->gpib_isr2 |= GPIB_ADSC;
    m->gpib_do_at = 0;
    machine_run(m, 20000);
    return m->gpib_out_len;
}

/*
 * Text approximation of the instrument's character set (Operation Manual p. 3-48).
 * Reverse-field letters are shown in lower case, reverse-field digits as plain digits.
 */
static int screen_glyph(uint8_t ch)
{
    switch (ch) {
    case 2: return '0';
    case 4: case 5: case 6: case 7: case 8: case 9: return '1' + (ch - 4);
    case 11: return '8';
    case 12: return '9';
    case 19: return '7';
    case 20: return '$';
    case 21: return '#';
    case 24: return '?';
    case 25: return '@';
    case 28: return '(';
    case 29: return '[';
    case 30: return ']';
    case 31: return ' ';
    case 33: return 's';
    case 34: return 'z';
    case 35: return '_';
    case 36: return 'u';
    case 38: return 'O';
    case 39: return 'm';
    case 40: return 'd';
    case 41: return 'k';
    case 42: return 'o';
    case 93: return ')';
    case 96: return '>';
    case 126: return 'v';
    case 127: return '^';
    default:
        break;
    }
    if (ch == 32 || ch == 37 || (ch >= 43 && ch <= 58) || (ch >= 65 && ch <= 90))
        return ch;
    if (ch >= 97 && ch <= 122)
        return ch;              /* reverse-field A-Z */
    if (ch >= 193 && ch <= 218)
        return ch - 128;        /* flashing A-Z */
    if ((ch >= 61 && ch <= 64) || (ch >= 91 && ch <= 95))
        return '+';             /* box characters */
    return '#';                 /* graphics, graticule and bar chart characters */
}

void machine_print_screen(const machine *m, FILE *out)
{
    for (int r = 0; r < SCREEN_ROWS; r++) {
        int last = SCREEN_COLS - 1;
        while (last >= 0 && m->screen[r][last] == ' ')
            last--;
        fprintf(out, "%2d|", r);
        for (int c = 0; c <= last; c++) {
            fputc(screen_glyph(m->screen[r][c]), out);
        }
        fputc('\n', out);
    }
}
