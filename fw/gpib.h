/* GPIB interface: command parser and readings output, on a uPD7210 talker/listener. */
#ifndef GPIB_H
#define GPIB_H

#include <stdint.h>

/* Counts the VARIABLE control for the RV command. */
void gpib_note_knob(int8_t direction);
void gpib_init(void);

/* Call it regularly from the main loop. */
void gpib_poll(void);

/* Redraws the GPIB indicators after the screen has been cleared. */
void gpib_redraw_indicators(void);

/* True if the address switches select talk-only, for a printer. */
uint8_t gpib_talk_only(void);

/* Sends the page of settings and readings to a printer in talk-only mode. */
void gpib_print_page(void);

/* Offers a front panel key press to the interface; returns 1 if the instrument should act on it.
 * In the remote state the keys only report to SK, and HELP returns to local unless locked out. */
uint8_t gpib_front_panel_key(uint8_t code);

/* The numbers the ER command reports for a rejected entry. */
#define ENTRY_ERROR_LEVEL_HIGH 4u
#define ENTRY_ERROR_LEVEL_LOW 5u
#define ENTRY_ERROR_RF_FREQ_RANGE 6u
#define ENTRY_ERROR_INC_TOO_SMALL 10u
#define ENTRY_ERROR_MOD_FREQ_HIGH 13u
#define ENTRY_ERROR_MOD_FREQ_LOW 14u
#define ENTRY_ERROR_AF_LEVEL_UNIT 17u
#define ENTRY_ERROR_MOD_LEVEL_HIGH 18u
#define ENTRY_ERROR_RF_FREQ_UNIT 21u
#define ENTRY_ERROR_LEVEL_INC_UNIT 25u
#define ENTRY_ERROR_MOD_FREQ_UNIT 26u

/* Records a rejected numeric entry, which only matters to a controller in the remote state. */
void gpib_report_entry_error(uint8_t code);

/* Returns 0 while the remote state disables the VARIABLE control (see the EV and DV commands). */
uint8_t gpib_variable_enabled(void);

#endif
