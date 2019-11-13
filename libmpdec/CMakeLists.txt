# See Agreement in LICENSE
# Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>

cmake_minimum_required(VERSION 3.4.1)

set(SOURCES basearith.c context.c constants.c convolute.c crt.c
  mpdecimal.c mpsignal.c difradix2.c fnt.c fourstep.c
  io.c memory.c numbertheory.c sixstep.c transpose.c)
set_source_files_properties(${SOURCES} PROPERTIES C_STANDARD 99)
  
math(EXPR CPU_CAPACITY "${CMAKE_SIZEOF_VOID_P} * 8")
message(STATUS "CPU Capacity: ${CPU_CAPACITY}")

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  configure_file("mpdecimal${CPU_CAPACITY}vc.h" "mpdecimal.h" COPYONLY)
else()
  set(MPD_HEADER_CONFIG "")
  configure_file("mpdecimal.h.in" "mpdecimal.h" @ONLY)
endif()

add_library(mpdec STATIC ${SOURCES})
if(MSVC)
  target_compile_definitions(mpdec PUBLIC "CONFIG_${CPU_CAPACITY}" PPRO MASM)
else()
  target_compile_definitions(mpdec PUBLIC "CONFIG_${CPU_CAPACITY}" ANSI)
endif()

target_include_directories(mpdec PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
