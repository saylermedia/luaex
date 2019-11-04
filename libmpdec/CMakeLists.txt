# See Agreement in LICENSE
# Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>

cmake_minimum_required(VERSION 3.4.1)

set(SOURCES basearith.c context.c constants.c convolute.c crt.c
  mpdecimal.c mpsignal.c difradix2.c fnt.c fourstep.c
  io.c memory.c numbertheory.c sixstep.c transpose.c)
  
math(EXPR CPU_CAPACITY "${CMAKE_SIZEOF_VOID_P} * 8")
message(STATUS "CPU Capacity: ${CPU_CAPACITY}")

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/mpdecimal${CPU_CAPACITY}vc.h" "${CMAKE_BINARY_DIR}/mpdecimal.h" COPYONLY)
else()
  set(MPD_HEADER_CONFIG "")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/mpdecimal.h.in" "${CMAKE_BINARY_DIR}/mpdecimal.h" @ONLY)
endif()

if(BUILD_SHARED_LIBS AND WIN32)
  add_library(mpdec SHARED ${SOURCES})
  set_property(TARGET mpdec PROPERTY OUTPUT_NAME mpdec)
  if(MSVC)
    target_compile_definitions(mpdec PUBLIC "CONFIG_${CPU_CAPACITY}" PPRO MASM LEGACY_COMPILER PRIVATE BUILD_DLL INTERFACE USE_DLL)
  else()
    target_compile_definitions(mpdec PUBLIC "CONFIG_${CPU_CAPACITY}" ANSI LEGACY_COMPILER PRIVATE BUILD_DLL INTERFACE USE_DLL)
  endif()
  declare_shared("mpdec")
else()
  add_library(mpdec STATIC ${SOURCES})
  target_compile_definitions(mpdec PUBLIC "CONFIG_${CPU_CAPACITY}" LEGACY_COMPILER ANSI)
  declare_static("mpdec")
endif()

target_include_directories(mpdec PUBLIC ${CMAKE_BINARY_DIR} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS mpdec RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
#install(FILES mpdecimal.h DESTINATION include)

