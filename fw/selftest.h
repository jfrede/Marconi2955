/* The instrument's self test: the generator is looped back to its own measuring circuits. */
#ifndef SELFTEST_H
#define SELFTEST_H

#include <stdint.h>

/* single_step waits for the NEXT TEST key before each test. */
void selftest_start(uint8_t single_step);
uint8_t selftest_active(void);
void selftest_key(uint8_t code);
void selftest_poll(void);
void selftest_redraw(void);

#endif
