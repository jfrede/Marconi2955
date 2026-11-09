/* Persistent instrument parameters kept in the EEPROM. */
#ifndef PARAMS_H
#define PARAMS_H

#include <stdint.h>

#define PARAM_LEVEL_EMF 0xF6B8u       /* non-zero: voltages are EMF rather than PD */
#define PARAM_COUNTER_FINE 0xF6B9u    /* non-zero: the RF counter resolves 1 Hz, otherwise 10 Hz */
#define PARAM_SN_DEFAULT 0xF6BAu      /* bit 0 set: S/N is the default instead of SINAD */
#define PARAM_STORE_DISABLE 0xF6BBu   /* non-zero: storing settings and poking memory are refused */
#define PARAM_NO_EMF_CHOICE 0xF6DFu   /* non-zero: the PD/EMF parameter is not offered */
#define PARAM_AF_ATTENUATOR 0xF7A8u   /* non-zero: the 20 dB AF generator accessory is fitted */
#define PARAM_AF_600_OHM 0xF7A9u      /* non-zero: the 600 ohm dBm accessory is fitted */

/* Not used by the original firmware. Two bytes each, low first, in 0.1 dB; 0 is off. */
#define PARAM_EXT_ATTEN_N 0xF68Du
#define PARAM_EXT_ATTEN_BNC 0xF68Fu

uint8_t param_get(uint16_t address);
void param_set(uint16_t address, uint8_t value);
void param_toggle(uint16_t address);

#endif
