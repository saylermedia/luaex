# See Agreement in LICENSE
# Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>

cmake_minimum_required(VERSION 3.4.1)
project(exlua)

# external libraries
find_package(ZLIB)
find_package(OpenSSL)

# check configuration
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckTypeSize)

check_include_file(dirent.h     HAVE_DIRENT_H)
check_include_file(stdint.h     HAVE_STDINT_H)
check_include_file(inttypes.h   HAVE_INTTYPES_H)
check_include_file(sys/stat.h   HAVE_SYS_STAT_H)
check_include_file(sys/types.h  HAVE_SYS_TYPES_H)
check_include_file(unistd.h     HAVE_UNISTD_H)
check_include_file(windows.h    HAVE_WINDOWS_H)

check_include_file_cxx(type_traits.h            HAVE_TYPE_TRAITS_H)
check_include_file_cxx(bits/type_traits.h       HAVE_BITS_TYPE_TRAITS_H)

check_function_exists(bcopy     HAVE_BCOPY)
check_function_exists(memmove   HAVE_MEMMOVE)
check_function_exists(strerror  HAVE_STRERROR)
check_function_exists(strtoll   HAVE_STRTOLL)
check_function_exists(strtoq    HAVE_STRTOQ)
check_function_exists(_strtoi64 HAVE__STRTOI64)

check_type_size("long long"             LONG_LONG)
check_type_size("unsigned long long"    UNSIGNED_LONG_LONG)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXECUTABLES "Build executables (lua, luac & etc)" ON)
option(STRIP_EXECUTABLES "Strip executables and shared libraries" ON)
option(BUILD_STATIC_CRT "Build with static system libraries" ON)

option(LUAEX_BASE "Enable extreme addition" ON)
option(LUAEX_TRYCATCH "Enable try...catch([e])...end" ON)
option(LUAEX_CLNTSRV "Enable syntax constructs to support client-server architecture" ON)
option(LUAEX_SERIALIZE "Enable serialize and deserialize functions" ON)
option(LUAEX_THREADLIB "Enable native threads library" ON)
option(LUAEX_MPDECIMAL "Enable decimal floating point arithmetic" ON)
option(LUAEX_I18N "Enable i18n for messages" ON)
option(LUAEX_SOCKET "Enable socket library" ON)
option(LUAEX_BYTE "Enable byte library" ON)
option(LUAEX_PCRE "Enable Perl-Compatible Regular Expressions library" ON)

if(NOT LUAEX_BASE)
  set(LUAEX_TRYCATCH OFF)
  set(LUAEX_CLNTSRV OFF)
  set(LUAEX_SERIALIZE OFF)
  set(LUAEX_THREADLIB OFF)
  set(LUAEX_MPDECIMAL OFF)
  set(LUAEX_I18N OFF)
  set(LUAEX_SOCKET OFF)
  set(LUAEX_BYTE OFF)
  set(LUAEX_PCRE OFF)
endif()

if(NOT LUAEX_SERIALIZE)
  set(LUAEX_THREADLIB OFF)
endif()

# GNU Compiler or Clang
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
  add_definitions(-Wall -Wextra -fPIC)
  
  if(STRIP_EXECUTABLES)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
  endif()
# Microsoft Visual C++
elseif(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS)
  # don't generate manifest files
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MANIFEST:NO")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFEST:NO")
  
  if(BUILD_STATIC_CRT)
    string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "/MD" "/MTd" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
  endif()
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/lua.h" _LUACONF)
string(REGEX MATCH "LUA_VERSION_MAJOR.?\"([0-9]*)" LUA_VERSION_MAJOR "${_LUACONF}")
string(REGEX MATCH "LUA_VERSION_MINOR.?\"([0-9]*)" LUA_VERSION_MINOR "${_LUACONF}")
unset(_LUACONF)

string(REGEX REPLACE "LUA_VERSION_MAJOR.?\"" "" LUA_VERSION_MAJOR "${LUA_VERSION_MAJOR}")
string(REGEX REPLACE "LUA_VERSION_MINOR.?\"" "" LUA_VERSION_MINOR "${LUA_VERSION_MINOR}")
set(LUA_VERSION_SUFFIX "${LUA_VERSION_MAJOR}${LUA_VERSION_MINOR}")

if(WIN32)
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

message(STATUS "Compiler ID: " ${CMAKE_C_COMPILER_ID})
message(STATUS "Install to: " ${CMAKE_INSTALL_PREFIX})

set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

if(LUAEX_MPDECIMAL)
  add_subdirectory(libmpdec)
endif()
if(LUAEX_PCRE)
  add_subdirectory(pcre)
endif()
add_subdirectory(src)

if(NOT WIN32)
  install(FILES doc/lua.1 doc/luac.1 DESTINATION share/man/man1)
endif()

if(LUAEX_I18N)
  install(DIRECTORY locale DESTINATION share FILES_MATCHING PATTERN "*.mo")
endif()

configure_file("LICENSE" "LICENSE.txt" COPYONLY)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "The Lua programming language with extreme functional")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "nobody")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")

set(CPACK_PACKAGE_VERSION "${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}/${CPACK_PACKAGE_VERSION}")

set(CPACK_WIX_UPGRADE_GUID "A8E15764-71EA-4C1D-ABB2-66F54E77F0A1")
set(CPACK_WIX_CULTURES "ru-RU;en-EN;")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "Lua Extreme")

if(BUILD_EXECUTABLES)
  set(CPACK_CREATE_DESKTOP_LINKS lua)
  set(CPACK_PACKAGE_EXECUTABLES lua "Lua stand-alone interpreter")
endif()

include(CPack)

if(BUILD_EXECUTABLES)
  enable_testing()
  file(GLOB _FILES ${CMAKE_SOURCE_DIR}/examples/*.lua)
  foreach(_FILE ${_FILES})
    get_filename_component(_NAME "${_FILE}" NAME)
    add_test(NAME ${_NAME} COMMAND $<TARGET_FILE:exlua> ${_FILE} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples)
  endforeach()
endif()
