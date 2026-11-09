/* Memory-mapped registers of the Marconi 2955. */
#ifndef REGS_H
#define REGS_H

#define REG_SYNTH_BAND 0xE000u  /* write: RF frequency band and divider selection */
#define REG_SYNTH_DATA 0xE001u  /* write: serial data and strobes for the synthesizer */
#define REG_AF_COUNT_LOW 0xE010u  /* write: AF generator frequency, low byte */
#define REG_AF_COUNT_HIGH 0xE011u /* write: AF generator frequency, high byte */
#define REG_AF_LEVEL 0xE012u    /* write: AF generator level DAC */
#define REG_MOD_LEVEL 0xE013u   /* write: fixed 1 kHz source level DAC */
#define REG_AF_SWITCHING 0xE015u /* write: AF filter band and level range */
#define REG_ROUTING 0xE016u     /* write: routing and switching, low bits follow the RF band */
#define REG_MOD_TRACKING 0xE018u  /* write: modulation tracking DAC */
#define REG_IF_CORRECTION 0xE019u /* write: IF correction DAC */
#define REG_ATTEN 0xE017u       /* write: coarse attenuator (bits 7-5) and input switching */
#define REG_LEVEL_DAC_LOW 0xE01Au  /* write: fine level DAC bits 1-0 in bits 7-6 */
#define REG_LEVEL_DAC_HIGH 0xE01Bu /* write: fine level DAC bits 9-2 */
#define REG_RANGE_E3D 0xE03Du   /* write: voltmeter range and measurement path */
#define REG_RANGE_E3E 0xE03Eu   /* write: voltmeter range */
#define REG_RF_DATA 0xE004u     /* read: RF counter digit, low nibble */
#define REG_RF_SELECT 0xE005u   /* write: RF counter digit number and receiver tuning data */
#define REG_RF_CONTROL 0xE006u  /* write: RF counter control, tuning strobe, power range bit */
#define REG_AF_RANGE_DAC 0xE014u /* write: AF generator output level DAC */
#define REG_AF_FILTER 0xE03Bu   /* write: AF filter selection, bits 3-2 */
#define REG_TIMER_DATA 0xEA00u  /* Am9513 data port */
#define REG_TIMER_CONTROL 0xEA01u /* Am9513 command port */
#define REG_STATUS 0xEE00u      /* read: status inputs */
#define REG_KEY_ROWS 0xE022u    /* read: keyboard rows, active low */
#define REG_KEY_COLS 0xE023u    /* write: keyboard column drive */
#define REG_SCREEN_CHAR 0xE025u /* write: character, stored at the current position */
#define REG_SCREEN_COL 0xE026u  /* write: column 0-39 */
#define REG_SCREEN_ROW 0xE027u  /* write: row 0-31 */

#endif
