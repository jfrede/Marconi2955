/* Start-up, reset and interrupt lines of the board that replaces the 8085. */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

/* Waits for the instrument's reset, starts the bus and releases the GPIB board from reset. */
void board_start(void);

/* Knob steps counted since the last call: positive is clockwise. */
int32_t board_take_knob_steps(void);

/* The key-pressed latch (RST 6.5). */
bool board_key_pressed(void);

/* RST 5.5, RST 6.5, RST 7.5, SID, RESET IN and 5 MHz as bits 0-5; 0 on the Pico Plus 2. */
uint32_t board_inputs(void);

#endif
