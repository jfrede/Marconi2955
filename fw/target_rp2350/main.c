/* The instrument firmware on the RP2350 that replaces the 8085. */
#include <stdio.h>

#include "pico/stdlib.h"

#include "../app.h"
#include "../keys.h"
#include "board.h"

int main(void)
{
    stdio_init_all();
    board_start();
    printf("Marconi 2955 firmware\n");

    app_power_on();
    for (;;) {
        app_poll();

        if (board_key_pressed()) {
            uint8_t code = keys_read();
            if (code)
                app_key(code);
            keys_rearm();
        }

        for (int32_t steps = board_take_knob_steps(); steps != 0; steps += steps > 0 ? -1 : 1)
            app_knob(steps > 0 ? 1 : -1);
    }
}
