# Standalone host test project (plain gcc, no Pico SDK): cmake -S firmware/tests -B <dir>
# Compiles ../src/*.c except main.c into two module libraries:
#   pico355_mods         config.h defaults (every TBD constant is still the sentinel)
#   pico355_mods_inject  CFG_PROTO_* injected with TEST DATA via -D (never the real protocol)
# ctest names: protocol sample_buffer adxl355 acquisition usb_stream command fsm
#              protocol_inject usb_stream_inject command_inject
cmake_minimum_required(VERSION 3.13)
project(pico355_host_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
enable_testing()

set(FW_INC ${CMAKE_CURRENT_LIST_DIR}/../include)
set(FW_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)
set(MODULES acquisition adxl355 command fsm protocol sample_buffer usb_stream)
set(WARN -Wall -Wextra -Werror)

# TEST DATA ONLY: made-up values so encode/decode mechanics can run. Not the protocol.
set(INJECT_DEFS
    PF_REQUIRE_INJECT=1
    CFG_PROTO_SYNC=0xA5 CFG_PROTO_HEADER_LEN=4 CFG_PROTO_LENGTH_FIELD_BYTES=2
    CFG_PROTO_CHECKSUM_KIND=0 CFG_PROTO_MAX_PAYLOAD=64
    CFG_PROTO_TYPE_SAMPLE_BATCH=1 CFG_PROTO_TYPE_COMMAND=2 CFG_PROTO_TYPE_RESPONSE=3
    CFG_PROTO_CMD_SET_ODR=1 CFG_PROTO_CMD_SET_RANGE=2 CFG_PROTO_CMD_SET_FILTER=3
    CFG_PROTO_RESP_ACK=1 CFG_PROTO_RESP_ERROR=2 CFG_PROTO_RESP_STATUS=3
    CFG_SAMPLE_BUFFER_DEPTH=4u)

function(pf_lib name)
  set(srcs)
  foreach(m ${MODULES})
    list(APPEND srcs ${FW_SRC}/${m}.c)
  endforeach()
  add_library(${name} STATIC ${srcs})
  target_include_directories(${name} PUBLIC ${FW_INC})
  target_compile_options(${name} PUBLIC ${WARN})
  if(ARGN)
    target_compile_definitions(${name} PUBLIC ${ARGN})
  endif()
endfunction()

pf_lib(pico355_mods)
pf_lib(pico355_mods_inject ${INJECT_DEFS})

# pf_test(<ctest name> <test source> <lib>)
function(pf_test tname src lib)
  add_executable(test_${tname} ${src})
  target_link_libraries(test_${tname} PRIVATE ${lib})
  add_test(NAME ${tname} COMMAND test_${tname})
endfunction()

pf_test(protocol        test_protocol.c        pico355_mods)
pf_test(sample_buffer   test_sample_buffer.c   pico355_mods)
pf_test(adxl355         test_adxl355.c         pico355_mods)
pf_test(acquisition     test_acquisition.c     pico355_mods)
pf_test(usb_stream      test_usb_stream.c      pico355_mods)
pf_test(command         test_command.c         pico355_mods)
pf_test(fsm             test_fsm.c             pico355_mods)
# Second target, same sources: frame format / command set injected as test data.
pf_test(protocol_inject   test_protocol.c   pico355_mods_inject)
pf_test(usb_stream_inject test_usb_stream.c pico355_mods_inject)
pf_test(command_inject    test_command.c    pico355_mods_inject)
