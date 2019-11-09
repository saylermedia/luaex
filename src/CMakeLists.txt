# See Agreement in LICENSE
# Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>

cmake_minimum_required(VERSION 3.4.1)

set(SOURCES lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c
  llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c
  lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lbitlib.c lcorolib.c ldblib.c liolib.c
  lmathlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c loadlib.c linit.c
  lserialize.c lthreadlib.c lmpdeclib.c lgettext.c)

set_source_files_properties(${SOURCES} lua.c luac.c PROPERTIES C_STANDARD 99)
configure_file("luaconf.h.in" "luaconf.h")

set(LUA_LIBS "")

if(BUILD_SHARED_LIBS)
  add_library(liblua SHARED ${SOURCES})
  set_property(TARGET liblua PROPERTY OUTPUT_NAME lua${LUA_VERSION_SUFFIX})
  if(WIN32)
    target_compile_definitions(liblua PUBLIC LUA_COMPAT_5_2 PRIVATE LUA_BUILD_AS_DLL)
  else()
    target_compile_definitions(liblua PUBLIC LUA_COMPAT_5_2)
    list(APPEND LUA_LIBS m)
  endif()
  declare_shared("lua${LUA_VERSION_SUFFIX}")
else()
  add_library(liblua STATIC ${SOURCES})
  set_property(TARGET liblua PROPERTY OUTPUT_NAME lua)
  target_compile_definitions(liblua PUBLIC LUA_COMPAT_5_2)
  declare_static("lua")
endif()

if(LUAEX_MPDECIMAL)
  list(APPEND LUA_LIBS mpdec)
endif()

target_link_libraries(liblua PUBLIC ${LUA_LIBS})

if(ANDROID)
  target_compile_options(liblua PRIVATE -Wno-deprecated)
endif()

target_include_directories(liblua PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS liblua RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES lua.h "${CMAKE_CURRENT_BINARY_DIR}/luaconf.h" lualib.h lauxlib.h lua.hpp DESTINATION include)

if(BUILD_EXECUTABLES)
  add_executable(lua lua.c)
  target_link_libraries(lua liblua)
  install(TARGETS lua RUNTIME DESTINATION bin)
  declare_executable("lua")

  add_executable(luac luac.c)
  target_link_libraries(luac liblua)
  install(TARGETS luac RUNTIME DESTINATION bin)
  declare_executable("luac")
endif()
