cmake_minimum_required(VERSION 3.13)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(pico-sdk/pico_sdk_init.cmake)

project(my_project)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(pcmeter-pico)

pico_generate_pio_header(pcmeter-pico ${CMAKE_CURRENT_LIST_DIR}/src/WS2812.pio)

target_sources(pcmeter-pico PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.c
        ${CMAKE_CURRENT_LIST_DIR}/src/meters.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/WS2812.cpp
        )

# Make sure TinyUSB can find tusb_config.h
target_include_directories(pcmeter-pico PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/src)

# In addition to pico_stdlib required for common PicoSDK functionality, add dependency on tinyusb_device
# for TinyUSB device support and tinyusb_board for the additional board support library used by the example
target_link_libraries(pcmeter-pico PUBLIC pico_stdlib pico_unique_id tinyusb_device tinyusb_board hardware_pwm hardware_pio)

# Uncomment this line to enable fix for Errata RP2040-E5 (the fix requires use of GPIO 15)
#target_compile_definitions(dev_hid_composite PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)

pico_add_extra_outputs(pcmeter-pico)

# add url via pico_set_program_url
# example_auto_set_url(pcmeter-pico)
pico_enable_stdio_usb(pcmeter-pico 1)
pico_enable_stdio_uart(pcmeter-pico 0)
