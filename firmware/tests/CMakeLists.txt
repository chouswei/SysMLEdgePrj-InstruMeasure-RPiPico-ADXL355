# Standalone host test project (plain gcc, no Pico SDK): cmake -S firmware/tests -B <dir>
# Compiles ../src/*.c except main.c into one module library, pico355_mods, on the REAL config.h
# (no -D injection: the link settings and CFG_PROTO_* are settled, see docs/protocol.md).
# ctest names: protocol sample_buffer adxl355 adxl355_tbd adxl355_partial acquisition usb_stream command fsm config
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

# Register-header variants for the adxl355 TBD tests, generated from the real
# adxl355_regs.h at configure time (never hand-copied, so they cannot drift):
#   regs_all_tbd  every register entry replaced by ADXL355_TBD (ADXL355_SAMPLE_BITS kept)
#   regs_partial  only ADXL355_REG_XDATA3 replaced by ADXL355_TBD (per-operation TBD check)
set(REGS_H ${FW_INC}/adxl355_regs.h)
foreach(v all_tbd partial)
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/regs_${v})
endforeach()
execute_process(COMMAND sed -E
  -e "s/^(#define ADXL355_[A-Z0-9_]+) (0x[0-9A-Fa-f]+|[0-9]+) \\/\\*/\\1 ADXL355_TBD \\/*/"
  -e "s/^(#define ADXL355_SAMPLE_BITS) ADXL355_TBD/\\1 20/"
  ${REGS_H} OUTPUT_FILE ${CMAKE_BINARY_DIR}/regs_all_tbd/adxl355_regs.h RESULT_VARIABLE rc1)
execute_process(COMMAND sed -E
  -e "s/^(#define ADXL355_REG_XDATA3) 0x08/\\1 ADXL355_TBD/"
  ${REGS_H} OUTPUT_FILE ${CMAKE_BINARY_DIR}/regs_partial/adxl355_regs.h RESULT_VARIABLE rc2)
if(NOT rc1 EQUAL 0 OR NOT rc2 EQUAL 0)
  message(FATAL_ERROR "could not generate adxl355 register header variants")
endif()
foreach(v all_tbd partial)
  add_library(adxl355_${v} STATIC ${FW_SRC}/adxl355.c)
  target_include_directories(adxl355_${v} PUBLIC ${CMAKE_BINARY_DIR}/regs_${v} ${FW_INC})
  target_compile_options(adxl355_${v} PUBLIC ${WARN})
endforeach()

# pf_test(<ctest name> <test source> <lib>)
function(pf_test tname src lib)
  add_executable(test_${tname} ${src})
  target_link_libraries(test_${tname} PRIVATE ${lib})
  add_test(NAME ${tname} COMMAND test_${tname})
endfunction()

pf_test(protocol        test_protocol.c        pico355_mods)
pf_test(sample_buffer   test_sample_buffer.c   pico355_mods)
pf_test(adxl355         test_adxl355.c         pico355_mods)
pf_test(adxl355_tbd     test_adxl355_tbd.c     adxl355_all_tbd)
pf_test(adxl355_partial test_adxl355_partial.c adxl355_partial)
pf_test(acquisition     test_acquisition.c     pico355_mods)
pf_test(usb_stream      test_usb_stream.c      pico355_mods)
pf_test(command         test_command.c         pico355_mods)
pf_test(fsm             test_fsm.c             pico355_mods)
pf_test(config          test_config.c          pico355_mods)
