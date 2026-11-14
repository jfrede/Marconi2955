/* Board that plugs into the 8085 socket of the Marconi 2955: RP2354B, 2 MB flash in the package. */
#ifndef _BOARDS_MARCONI2955_CPU_H
#define _BOARDS_MARCONI2955_CPU_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)

#define MARCONI2955_CPU
#define PICO_RP2350A 0 /* the B package, 48 GPIO */

#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif
#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 32
#endif
#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 33
#endif
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 34
#endif

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif
pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (2 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

#endif
