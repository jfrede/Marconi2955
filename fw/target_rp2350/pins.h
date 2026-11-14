/* GPIO use of the RP2350 that replaces the 8085. See hardware/cpu-adapter/SPEC.md. */
#ifndef PINS_H
#define PINS_H

/* The bus master (PIO) drives these. Address and data must stay consecutive from GPIO 0. */
#define PIN_AD0 0u      /* AD0-7: multiplexed address low byte and data, GPIO 0-7 */
#define PIN_A8 8u       /* A8-15, GPIO 8-15 */
#define PIN_ALE 16u     /* side-set bit 0 */
#define PIN_RD 17u      /* side-set bit 1, active low */
#define PIN_WR 18u      /* side-set bit 2, active low */
#define PIN_AD_DIR 19u  /* side-set bit 3: 1 = towards the instrument */

/* Plain outputs. */
#define PIN_RESET_OUT 20u /* active high, resets the GPIB board */
#define PIN_SOD 21u       /* 1 = the VARIABLE knob drives RST 7.5, 0 = the tones timer */

/* Only on the instrument board. The Pico Plus 2 does not bring these out. */
#ifdef MARCONI2955_CPU
#define HAVE_BOARD_SIGNALS 1
#define PIN_BUS_OE_N 23u  /* low enables both level translators; pulled high on the board */
#define PIN_RST55 24u     /* GPIB interrupt (not used: the firmware polls the uPD7210) */
#define PIN_RST65 25u     /* key pressed latch */
#define PIN_RST75 26u     /* knob step, rising edge */
#define PIN_SID 27u       /* knob direction: 1 = clockwise (+1) */
#define PIN_RESET_IN 28u  /* the instrument's power-on reset, active low */
#define PIN_CLK_5MHZ 29u  /* 5 MHz from AB3 (spare, not used yet) */
#endif

#endif
