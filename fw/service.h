/* The calibration data menu: reads and writes any memory byte or hardware register. */
#ifndef SERVICE_H
#define SERVICE_H

#include <stdint.h>

void service_open(void);
uint8_t service_active(void);
void service_key(uint8_t code);
void service_redraw(void);

#endif
