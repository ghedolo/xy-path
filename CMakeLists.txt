cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../pico-sdk)
include(pico_sdk_import.cmake)

project(gplog C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Paths file: override with -DPATHS_FILE=/path/to/file.txt
set(PATHS_FILE "${CMAKE_SOURCE_DIR}/paths.txt" CACHE STRING "Vector paths input file")

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/paths_data.h
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tmp/gen_paths_h.py
                    ${PATHS_FILE}
                    ${CMAKE_CURRENT_BINARY_DIR}/paths_data.h
    DEPENDS ${PATHS_FILE} ${CMAKE_SOURCE_DIR}/tmp/gen_paths_h.py
    COMMENT "Generating paths_data.h from ${PATHS_FILE}"
)
add_custom_target(gplog_paths_h DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/paths_data.h)

add_executable(gplog main.c renderer.c)
set_target_properties(gplog PROPERTIES SUFFIX ".elf")

pico_generate_pio_header(gplog ${CMAKE_CURRENT_LIST_DIR}/audio_i2s.pio)

add_dependencies(gplog gplog_paths_h)

target_include_directories(gplog PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(gplog
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_gpio
    hardware_clocks
)

pico_enable_stdio_usb(gplog 1)
pico_enable_stdio_uart(gplog 0)
pico_add_extra_outputs(gplog)
