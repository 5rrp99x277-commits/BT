cmake_minimum_required(VERSION 3.13)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/pico-sdk")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(bt_dummy C CXX ASM)
pico_sdk_init()

add_executable(bt_dummy
    src/bt_dummy.c
)

target_link_libraries(bt_dummy
    pico_stdlib
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(bt_dummy)
pico_enable_stdio_uart(bt_dummy 1)
