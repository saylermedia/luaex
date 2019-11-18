# See Agreement in LICENSE
# Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>

cmake_minimum_required(VERSION 3.4.1)

set(PCRE_STATIC ON)

set(SUPPORT_PCRE8 ON)
set(SUPPORT_PCRE16 OFF)
set(SUPPORT_PCRE32 OFF)
set(SUPPORT_JIT OFF)
set(SUPPORT_PCREGREP_JIT OFF)
set(SUPPORT_UTF ON)
set(SUPPORT_UCP OFF)
set(EBCDIC OFF)
set(EBCDIC_NL25 OFF)
set(BSR_ANYCRLF OFF)
set(NO_RECURSE ON)

set(SUPPORT_LIBBZ2 OFF)
set(SUPPORT_LIBZ OFF)
set(SUPPORT_LIBEDIT OFF)
set(SUPPORT_LIBREADLINE OFF)

set(SUPPORT_VALGRIND OFF)
set(SUPPORT_GCOV OFF)

set(NEWLINE "10")
set(PCRE_POSIX_MALLOC_THRESHOLD "10")
set(PCRE_LINK_SIZE "2")
set(PCRE_PARENS_NEST_LIMIT "250")
set(PCRE_MATCH_LIMIT "10000000")
set(PCRE_MATCH_LIMIT_RECURSION "MATCH_LIMIT")
set(PCREGREP_BUFSIZE "20480")

configure_file("config-cmake.h.in" "config.h" @ONLY)

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/configure.ac configure_lines LIMIT_COUNT 50)
set(SEARCHED_VARIABLES "pcre_major" "pcre_minor" "pcre_prerelease" "pcre_date")
foreach(configure_line ${configure_lines})
    foreach(_substitution_variable ${SEARCHED_VARIABLES})
        string(TOUPPER ${_substitution_variable} _substitution_variable_upper)
        if (NOT ${_substitution_variable_upper})
            string(REGEX MATCH "m4_define\\(${_substitution_variable}, \\[(.*)\\]" MACTHED_STRING ${configure_line})
            if (CMAKE_MATCH_1)
                set(${_substitution_variable_upper} ${CMAKE_MATCH_1})
            endif()
        endif()
    endforeach()
endforeach()

configure_file("pcre.h.in" "pcre.h" @ONLY)
configure_file("pcre_chartables.c.dist" "pcre_chartables.c" COPYONLY)

add_definitions(-DHAVE_CONFIG_H)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
endif()

set(SOURCES pcre_byte_order.c
  "${CMAKE_CURRENT_BINARY_DIR}/pcre_chartables.c"
  pcre_compile.c
  pcre_config.c
  pcre_dfa_exec.c
  pcre_exec.c
  pcre_fullinfo.c
  pcre_get.c
  pcre_globals.c
  pcre_jit_compile.c
  pcre_maketables.c
  pcre_newline.c
  pcre_ord2utf8.c
  pcre_refcount.c
  pcre_string_utils.c
  pcre_study.c
  pcre_tables.c
  pcre_ucd.c
  pcre_valid_utf8.c
  pcre_version.c
  pcre_xclass.c)

add_library(pcre STATIC ${SOURCES})
target_compile_definitions(pcre PUBLIC PCRE_STATIC)
target_include_directories(pcre PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
