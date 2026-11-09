/* The HELP key: operating summaries and the persistent instrument parameters. */
#ifndef HELP_H
#define HELP_H

#include <stdint.h>

void help_open(void);

/* The calibration data menu has handed the screen back. */
void help_service_closed(void);

/* The self test has finished and handed the screen back. */
void help_selftest_closed(uint8_t single_step);

/* Draws the open page again, after the screen has been overwritten. */
void help_redraw(void);
uint8_t help_active(void);

/* Takes every key while the menu is on the screen. */
void help_key(uint8_t code);

#endif
