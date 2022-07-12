# COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.

cmake_minimum_required(VERSION 3.13.1)

# create compile_commands.json for clang
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

# We define the nrf52840dk overlay once in the root boards directory, but now have
# to add it here manually since it is not where west expects it to be
if(${BOARD} STREQUAL nrf52840dk_nrf52840)
  list(APPEND DTC_OVERLAY_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../boards/nrf52840dk_nrf52840.overlay"
  )
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(sentek-iot-probe-lpuart-test)

zephyr_compile_options(-fdiagnostics-color=always)

target_sources(app PRIVATE
  src/main.c
)
