/* Front panel key matrix. */
#ifndef KEYS_H
#define KEYS_H

#include <stdint.h>

#define KEY_CODE(row, col) ((row) * 8 + (col) + 1)

#define KEY_SELECT KEY_CODE(1, 6)
#define KEY_HELP KEY_CODE(2, 0)
#define KEY_DUPLEX KEY_CODE(7, 0)
#define KEY_RX_EQUALS_TX KEY_CODE(6, 1)
#define KEY_TONES KEY_CODE(3, 0)
#define KEY_RF_GEN KEY_CODE(7, 1)
#define KEY_FREQ KEY_CODE(0, 2)
#define KEY_LEVEL KEY_CODE(6, 2)
#define KEY_DELTA_INCR KEY_CODE(7, 2)
#define KEY_0 KEY_CODE(1, 4)
#define KEY_1 KEY_CODE(0, 3)
#define KEY_2 KEY_CODE(1, 3)
#define KEY_3 KEY_CODE(2, 3)
#define KEY_4 KEY_CODE(3, 3)
#define KEY_5 KEY_CODE(4, 3)
#define KEY_6 KEY_CODE(5, 3)
#define KEY_7 KEY_CODE(6, 3)
#define KEY_8 KEY_CODE(7, 3)
#define KEY_9 KEY_CODE(0, 4)
#define KEY_POINT KEY_CODE(4, 4)
#define KEY_MINUS KEY_CODE(5, 4)
#define KEY_DELETE KEY_CODE(2, 4)
#define KEY_MHZ KEY_CODE(0, 5)
#define KEY_KHZ KEY_CODE(3, 5)
#define KEY_HZ KEY_CODE(1, 5)
#define KEY_DBM KEY_CODE(2, 5)
#define KEY_SET_MOD KEY_CODE(3, 1)
#define KEY_MOD_ON_OFF KEY_CODE(4, 1)
#define KEY_SINAD_SN KEY_CODE(1, 1)
#define KEY_DISTORTION KEY_CODE(5, 1)
#define KEY_AC_DC KEY_CODE(2, 1)
#define KEY_TX KEY_CODE(0, 0)
#define KEY_RX KEY_CODE(6, 0)
#define KEY_AF_GEN KEY_CODE(0, 1)
#define KEY_FM KEY_CODE(6, 5)
#define KEY_PM KEY_CODE(4, 5)
#define KEY_SCOPE KEY_CODE(1, 0)
#define KEY_BAR_CHART KEY_CODE(4, 0)
#define KEY_HOLD_DISPLAY KEY_CODE(5, 0)
#define KEY_DB KEY_CODE(5, 5)
#define KEY_SCOPE_VERT_EXPAND KEY_CODE(3, 6)  /* VERT: arrows outwards, more sensitive */
#define KEY_SCOPE_VERT_COMPRESS KEY_CODE(4, 6)
#define KEY_SCOPE_HORIZ_EXPAND KEY_CODE(3, 7)
#define KEY_SCOPE_HORIZ_COMPRESS KEY_CODE(4, 7)
#define KEY_SCOPE_REPEAT KEY_CODE(7, 6)
#define KEY_SCOPE_SINGLE KEY_CODE(7, 7)
#define KEY_AF_LOW_PASS_300 KEY_CODE(0, 6)
#define KEY_AF_BAND_PASS KEY_CODE(5, 2)
#define KEY_AF_LOW_PASS KEY_CODE(2, 2)
#define KEY_AM_PERCENT KEY_CODE(7, 5)
#define KEY_STORE KEY_CODE(3, 2)
#define KEY_RECALL KEY_CODE(4, 2)
#define KEY_LEVEL_UP KEY_CODE(5, 7)
#define KEY_LEVEL_DOWN KEY_CODE(2, 7)
#define KEY_FREQ_UP KEY_CODE(5, 6)
#define KEY_FREQ_DOWN KEY_CODE(2, 6)

/* Returns 0 when no key is held, otherwise row * 8 + column + 1 (1-64). */
uint8_t keys_scan(void);

#endif
