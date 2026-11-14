# Firmware for the RP2350 board in the 8085 socket.
#
#   cmake -B build -DPICO_BOARD=marconi2955_cpu          # the instrument board
#   cmake -B build -DPICO_BOARD=pimoroni_pico_plus2_rp2350  # bus test on the bench
#   cmake --build build
#
# PICO_SDK_PATH must point to the Pico SDK (2.3.0 or later).
cmake_minimum_required(VERSION 3.13)

list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
if(NOT PICO_BOARD)
    set(PICO_BOARD marconi2955_cpu)
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(marconi2955 C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

set(FW ${CMAKE_CURRENT_LIST_DIR}/..)

option(HAL_PROTECT_EEPROM "Refuse writes to the EEPROM and NOVRAM" ON)

# What both programs need: the bus master and the start-up.
add_library(bus INTERFACE)
target_sources(bus INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/board.c
    ${CMAKE_CURRENT_LIST_DIR}/hal_rp2350.c
    ${FW}/hal_common.c
    ${FW}/hal_screen.c)
target_compile_definitions(bus INTERFACE HAL_PROTECT_EEPROM=$<BOOL:${HAL_PROTECT_EEPROM}>)
target_link_libraries(bus INTERFACE pico_stdlib hardware_pio hardware_watchdog)

function(add_program name)
    pico_generate_pio_header(${name} ${CMAKE_CURRENT_LIST_DIR}/bus.pio)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    target_link_libraries(${name} bus)
    pico_enable_stdio_usb(${name} 1)
    pico_enable_stdio_uart(${name} 0)
    pico_add_extra_outputs(${name})
endfunction()

add_executable(bustest ${CMAKE_CURRENT_LIST_DIR}/bustest.c)
add_program(bustest)

# The firmware itself waits for the instrument's signals, so only build it for the real board.
if(PICO_BOARD STREQUAL "marconi2955_cpu")
    add_executable(marconi2955
        ${CMAKE_CURRENT_LIST_DIR}/main.c
        ${FW}/afcounter.c ${FW}/app.c ${FW}/bargraph.c ${FW}/control.c ${FW}/format.c
        ${FW}/gpib.c ${FW}/help.c ${FW}/keys.c ${FW}/level.c ${FW}/meter.c
        ${FW}/modulation.c ${FW}/overload.c ${FW}/params.c ${FW}/readings.c
        ${FW}/regmenu.c ${FW}/rfcounter.c ${FW}/scope.c ${FW}/screen.c ${FW}/selftest.c
        ${FW}/service.c ${FW}/settings.c ${FW}/synth.c ${FW}/tones.c ${FW}/tx.c)
    target_link_libraries(marconi2955 m)
    add_program(marconi2955)
endif()
