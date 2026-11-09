/*
 * RF input overload warning.
 *
 * The hardware protects the input by itself. It reports an overload on one status line, for a
 * tripped BNC input and for an overheated pad at the N socket alike. The firmware only warns.
 */
#ifndef OVERLOAD_H
#define OVERLOAD_H

#include <stdint.h>

/* Returns non-zero while the warning owns the instrument: skip everything else. */
uint8_t overload_poll(void);

/* Keys and the knob are ignored during the warning. */
uint8_t overload_active(void);

#endif
