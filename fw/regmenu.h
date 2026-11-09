/* A menu for looking at and switching the instrument's hardware registers, bit by bit. */
#ifndef REGMENU_H
#define REGMENU_H

#include <stdint.h>

void regmenu_open(void);
uint8_t regmenu_active(void);
void regmenu_key(uint8_t code);
void regmenu_knob(int8_t direction);
void regmenu_poll(void);
void regmenu_redraw(void);

#endif
