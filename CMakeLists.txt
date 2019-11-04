# See Agreement in LICENSE
# Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>

cmake_minimum_required(VERSION 3.4.1)
project(lua)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXECUTABLES "Build executables (lua, luac & etc)" ON)
option(STRIP_EXECUTABLES "Strip executables and shared libraries" ON)

option(LUAEX_BASE "Enable extreme addition" ON)
option(LUAEX_TRYCATCH "Enable try...catch([e])...end" ON)
option(LUAEX_CLNTSRV "Enable syntax constructs to support client-server architecture" ON)
option(LUAEX_SERIALIZE "Enable serialize and deserialize functions" ON)
option(LUAEX_THREADLIB "Enable native threads library" ON)
option(LUAEX_MPDECIMAL "Enable decimal floating point arithmetic" ON)

if(NOT LUAEX_BASE)
  set(LUAEX_TRYCATCH OFF)
  set(LUAEX_CLNTSRV OFF)
  set(LUAEX_SERIALIZE OFF)
  set(LUAEX_THREADLIB OFF)
  set(LUAEX_MPDECIMAL OFF)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL GNU)
  add_definitions(-Wall -Wextra)
endif()

if(STRIP_EXECUTABLES)
  if(CMAKE_C_COMPILER_ID STREQUAL GNU)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
  endif()
endif()

if(MSVC)
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MANIFEST:NO")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFEST:NO")
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

set_property(GLOBAL PROPERTY PROJECT_SHARED_LIBS "")
set_property(GLOBAL PROPERTY PROJECT_STATIC_LIBS "")
set_property(GLOBAL PROPERTY PROJECT_EXECUTABLES "")

macro (declare_shared name)
  get_property(_tmp GLOBAL PROPERTY PROJECT_SHARED_LIBS)
  set(_tmp "${_tmp} ${CMAKE_SHARED_LIBRARY_PREFIX}${name}${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set_property(GLOBAL PROPERTY PROJECT_SHARED_LIBS "${_tmp}")
endmacro()

macro (declare_static name)
  get_property(_tmp GLOBAL PROPERTY PROJECT_STATIC_LIBS)
  set(_tmp "${_tmp} ${CMAKE_STATIC_LIBRARY_PREFIX}${name}${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set_property(GLOBAL PROPERTY PROJECT_STATIC_LIBS "${_tmp}")
endmacro()

macro (declare_executable name)
  get_property(_tmp GLOBAL PROPERTY PROJECT_EXECUTABLES)
  set(_tmp "${_tmp} ${name}${CMAKE_EXECUTABLE_SUFFIX}")
  set_property(GLOBAL PROPERTY PROJECT_EXECUTABLES "${_tmp}")
endmacro()

message(STATUS "Compiler ID: " ${CMAKE_C_COMPILER_ID})
message(STATUS "Install to: " ${CMAKE_INSTALL_PREFIX})

set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

if(LUAEX_MPDECIMAL)
  add_subdirectory(libmpdec)
endif()

add_subdirectory(src)

get_property(_shared GLOBAL PROPERTY PROJECT_SHARED_LIBS)
get_property(_static GLOBAL PROPERTY PROJECT_STATIC_LIBS)
get_property(_executables GLOBAL PROPERTY PROJECT_EXECUTABLES)

message(STATUS "Shared libraries:" ${_shared})
message(STATUS "Static libraries:" ${_static})
message(STATUS "Executables:" ${_executables})

if(NOT WIN32)
  install(FILES doc/lua.1 doc/luac.1 DESTINATION share/man/man1)
endif()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_CONTACT "Alexey Smirnov <saylermedia@gmail.com>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION "${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}")
include(CPack)

if(BUILD_EXECUTABLES)
  enable_testing()
  file(GLOB _FILES ${CMAKE_SOURCE_DIR}/examples/*.lua)
  foreach(_FILE ${_FILES})
    get_filename_component(_NAME "${_FILE}" NAME)
    add_test(NAME ${_NAME} COMMAND $<TARGET_FILE:lua> ${_FILE} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples)
  endforeach()
endif()
