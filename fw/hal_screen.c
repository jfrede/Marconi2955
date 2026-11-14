/* Screen timing shared by the targets that run on the instrument's bus. */
#include "hal.h"
#include "regs.h"

#define STATUS_SCREEN_SYNC 0x80u

/* The screen memory may be written right after the sync bit falls; the original does the same. */
void hal_screen_wait(void)
{
    while (!(hal_read(REG_STATUS) & STATUS_SCREEN_SYNC))
        ;
    while (hal_read(REG_STATUS) & STATUS_SCREEN_SYNC)
        ;
}
