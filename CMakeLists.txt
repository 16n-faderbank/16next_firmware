cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

set(target_proj 16next)
project(${target_proj} C CXX ASM)

set(PICO_BOARD 16nx)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR}/board)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(${target_proj}
  16next.cpp
)

target_sources(${target_proj}
  PRIVATE
  lib/config.cpp
  lib/flash_onboard.cpp
  lib/i2c_utils.cpp
  lib/sysex.cpp
  lib/ResponsiveAnalogRead.hpp
  usb_descriptors.c
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/midi_uart_lib)

# begin uart config
if(DEFINED ENV{MIDI_UART_NUM})
  set(MIDI_UART_NUM $ENV{MIDI_UART_NUM})
endif()

if(DEFINED ENV{MIDI_UART_TX_GPIO})
  set(MIDI_UART_TX_GPIO $ENV{MIDI_UART_TX_GPIO})
endif()

if(DEFINED ENV{MIDI_UART_RX_GPIO})
  set(MIDI_UART_RX_GPIO $ENV{MIDI_UART_RX_GPIO})
endif()

if(DEFINED MIDI_UART_NUM)
  if(${MIDI_UART_NUM} EQUAL 1)
    # MIDI is UART 1, so can enable UART 0 debugging
    pico_enable_stdio_uart(${target_proj} 1)

    if(DEFINED MIDI_UART_TX_GPIO AND DEFINED MIDI_UART_RX_GPIO)
      target_compile_options(${target_proj} PRIVATE -DMIDI_UART_NUM=${MIDI_UART_NUM} -DMIDI_UART_TX_GPIO=${MIDI_UART_TX_GPIO} -DMIDI_UART_RX_GPIO=${MIDI_UART_RX_GPIO})
    endif()
  elseif(NOT ${MIDI_UART_NUM} EQUAL 0)
    message(FATAL_ERROR "Legal values for MIDI_UART_NUM are 0 or 1, CMake will exit.")
  elseif(NOT DEFINED MIDI_UART_TX_GPIO OR NOT DEFINED MIDI_UART_RX_GPIO)
    message(FATAL_ERROR "You must define MIDI_UART_TX_GPIO and MIDI_UART_RX_GPIO if you define MIDI_UART_NUM 0, CMake will exit.")
  else()
    message("UART debugging is disabled if you use UART 0 for MIDI")
    pico_enable_stdio_uart(${target_proj} 0)
    target_compile_options(${target_proj} PRIVATE -DMIDI_UART_NUM=${MIDI_UART_NUM} -DMIDI_UART_TX_GPIO=${MIDI_UART_TX_GPIO} -DMIDI_UART_RX_GPIO=${MIDI_UART_RX_GPIO})
  endif()
else()
  # not defined, so will be UART 1. Can enable UART 0 debugging
  pico_enable_stdio_uart(${target_proj} 1)
endif()

# end uart config
target_include_directories(${target_proj} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(16next
  PRIVATE
  pico_stdlib
  pico_unique_id
  hardware_adc
  hardware_flash
  hardware_i2c
  hardware_sync
  midi_uart_lib
  pico_i2c_slave
  ring_buffer_lib
  tinyusb_device
  tinyusb_board
)

pico_add_extra_outputs(16next)
