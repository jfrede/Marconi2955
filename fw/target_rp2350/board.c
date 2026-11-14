/* Start-up and reset handling shared by the firmware and the bus test. */
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

#include "board.h"
#include "hal_rp2350.h"
#include "pins.h"

#define RESET_OUT_MS 10u     /* how long the GPIB board is held in reset after the bus starts */
#define RESET_CONFIRM_US 20u /* the reset line must stay low this long */
#define WAIT_MESSAGE_MS 2000u

static void output(uint pin, bool value)
{
    gpio_init(pin);
    gpio_put(pin, value);
    gpio_set_dir(pin, GPIO_OUT);
}

#ifdef HAVE_BOARD_SIGNALS
static void input(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_disable_pulls(pin); /* the instrument drives every input */
}

static volatile int32_t knob_steps;

/*
 * The instrument's reset line is an RC network on its +5 V: it falls when the power fails or
 * when someone shorts TP1. Like a real 8085, stop using the bus at once and start again when
 * the line is high. The level translators go off and the pull-ups keep /RD and /WR high.
 * The restart waits in board_start, not here: USB may keep the board powered while the
 * instrument is off, and waiting inside this interrupt would stall the USB console.
 */
static void stop_and_restart(void)
{
    busy_wait_us(RESET_CONFIRM_US);
    if (gpio_get(PIN_RESET_IN))
        return; /* a glitch, not a reset */
    gpio_put(PIN_BUS_OE_N, 1);
    watchdog_reboot(0, 0, 0);
    for (;;)
        tight_loop_contents();
}

static void gpio_event(uint gpio, uint32_t events)
{
    if (gpio == PIN_RST75 && (events & GPIO_IRQ_EDGE_RISE))
        knob_steps += gpio_get(PIN_SID) ? 1 : -1;
    else if (gpio == PIN_RESET_IN && (events & GPIO_IRQ_EDGE_FALL))
        stop_and_restart();
}
#endif

void board_start(void)
{
    output(PIN_RESET_OUT, 1);
    output(PIN_SOD, 1);   /* the knob, not the tones timer, drives RST 7.5 */

#ifdef HAVE_BOARD_SIGNALS
    output(PIN_BUS_OE_N, 1);
    input(PIN_RST55);
    input(PIN_RST65);
    input(PIN_RST75);
    input(PIN_SID);
    input(PIN_RESET_IN);
    input(PIN_CLK_5MHZ);

    /* The 8085 waits for the reset RC as well; the NOVRAM and the analog boards need it. */
    /* Repeat the message: a USB terminal may only be opened later. */
    for (uint32_t i = 0; !gpio_get(PIN_RESET_IN); i++) {
        if (i % WAIT_MESSAGE_MS == 0)
            printf("waiting for the instrument's reset line\n");
        sleep_ms(1);
    }
#endif

    bus_init();

#ifdef HAVE_BOARD_SIGNALS
    gpio_put(PIN_BUS_OE_N, 0);
#endif
    sleep_ms(RESET_OUT_MS);
    gpio_put(PIN_RESET_OUT, 0);

#ifdef HAVE_BOARD_SIGNALS
    gpio_set_irq_enabled_with_callback(PIN_RST75, GPIO_IRQ_EDGE_RISE, true, gpio_event);
    gpio_set_irq_enabled(PIN_RESET_IN, GPIO_IRQ_EDGE_FALL, true);
#endif
}

int32_t board_take_knob_steps(void)
{
#ifdef HAVE_BOARD_SIGNALS
    uint32_t saved = save_and_disable_interrupts();
    int32_t steps = knob_steps;
    knob_steps = 0;
    restore_interrupts(saved);
    return steps;
#else
    return 0;
#endif
}

bool board_key_pressed(void)
{
#ifdef HAVE_BOARD_SIGNALS
    return gpio_get(PIN_RST65);
#else
    return false;
#endif
}

uint32_t board_inputs(void)
{
#ifdef HAVE_BOARD_SIGNALS
    return gpio_get_all64() >> PIN_RST55 & 0x3Fu;
#else
    return 0;
#endif
}
