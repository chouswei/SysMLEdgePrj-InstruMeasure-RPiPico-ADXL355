# Pico355 firmware (RP2040, Pico SDK, C). Requirements PICO355-R4..R7.
# Output target: pico355_fw -> pico355_fw.uf2 (name fixed in docs/protocol.md).
cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico355_fw C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico355_fw
    src/main.c
    src/adxl355.c
    src/acquisition.c
    src/sample_buffer.c
    src/usb_stream.c
    src/command.c
    src/fsm.c
    src/protocol.c
)

target_include_directories(pico355_fw PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_compile_options(pico355_fw PRIVATE -Wall -Wextra)

# main.c is the only user of SDK headers: SPI0, GPIO and USB CDC (via stdio over USB).
target_link_libraries(pico355_fw PRIVATE pico_stdlib hardware_spi hardware_gpio)

pico_enable_stdio_usb(pico355_fw 1)
pico_enable_stdio_uart(pico355_fw 0)

pico_add_extra_outputs(pico355_fw)
