cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(16next C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(16next
  16next.cpp
  lib/HystFilter.cpp
  lib/spi_flash.cpp
  usb_descriptors.c
)

pico_enable_stdio_usb(16next 0)
pico_enable_stdio_uart(16next 1)

target_include_directories(16next PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_add_extra_outputs(16next)
target_link_libraries(16next
  PRIVATE
  pico_stdlib
  pico_unique_id
  hardware_adc
  hardware_i2c
  hardware_spi
  tinyusb_device
  tinyusb_board
)
