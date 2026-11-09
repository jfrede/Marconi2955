/* Stored instrument settings, in the 45-byte slot format of the original firmware. */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

#define SETTINGS_SLOT_FIRST 1u
#define SETTINGS_SLOT_LAST 37u
#define LEVEL_UNIT_V 1u
#define LEVEL_UNIT_MV 2u
#define LEVEL_UNIT_UV 3u
#define LEVEL_UNIT_DBM 5u

typedef struct {
    uint8_t transmit;
    uint8_t duplex;
    uint8_t audio;
    uint32_t rf_hz;           /* generator frequency, or the manual transmitter frequency */
    int16_t level_tenths;     /* 0.1 dBm */
    uint8_t level_unit;       /* 1 V, 2 mV, 3 uV as entered, 5 dBm */
    double level_volts;       /* the value as entered, when the unit is a voltage */
    int16_t level_offset;     /* 0.1 dB added to a voltage by the knob and the step keys */
    int32_t af_freq;          /* 0.1 Hz: modulation frequency (RX) or AF generator (TX) */
    int32_t af_level;         /* 0.1 mV */
    uint32_t deviation;       /* 0.1 Hz */
    uint8_t am_percent;
    uint8_t mod_kind;         /* 1 AM, 2 FM, 3 PM */
    uint8_t level_is_am;      /* the stored modulation level is an AM depth */
    uint8_t mod_off;
    uint8_t input_select;     /* 0 BNC, 1 N type */
    uint8_t filter;
    uint8_t ac_coupled;
    uint8_t sinad;            /* 0 off, 1 SINAD, 2 S/N */
    uint8_t distortion;
    uint8_t scope_shown;
    uint8_t scope_timebase;
    uint8_t scope_rx_am;      /* vertical index shared by RX and AM */
    uint8_t scope_fm, scope_pm;
    uint8_t scope_single;
    uint8_t group;            /* 0 none, 1 modulation, 2 RF, 3 AF */
    uint8_t field;            /* 0 none, 2 level, 4 frequency */
    uint8_t manual;
    int32_t rf_inc_10hz;
    int16_t rf_level_inc;     /* 0.1 dB */
    int32_t mod_freq_inc;     /* 0.1 Hz */
    int32_t mod_level_inc;    /* 0.1 Hz of deviation, or percent */
    uint8_t mod_level_inc_am;
    int32_t af_freq_inc;      /* 0.1 Hz */
    int32_t af_level_inc;     /* 0.1 mV */
    uint8_t sat_on;           /* sub-audible tone, kept in place of the modulation frequency */
    int32_t sat_freq;         /* 0.1 Hz */
    int32_t sat_level;        /* 0.1 Hz of deviation, or percent AM */
    uint8_t two_tone;
} instrument_settings;

/* Returns 0 if storing is disabled or the slot number is not valid. */
uint8_t settings_store(uint8_t slot, const instrument_settings *settings);

/* Returns 0 if the slot number is not valid. */
uint8_t settings_recall(uint8_t slot, instrument_settings *settings);

/* The settings as a 64-byte image for the SV command: the slot contents with the
 * packed flags spread over one byte each, as the original keeps them in NOVRAM. */
#define SETTINGS_IMAGE_SIZE 64u
void settings_to_image(const instrument_settings *settings, uint8_t *image);
void settings_from_image(const uint8_t *image, instrument_settings *settings);

#endif
