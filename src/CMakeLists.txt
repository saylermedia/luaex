# See Agreement in LICENSE
# Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>

cmake_minimum_required(VERSION 3.4.1)

set(SOURCES lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c
  llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c
  lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lbitlib.c lcorolib.c ldblib.c liolib.c
  lmathlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c loadlib.c linit.c
  lserialize.c lthreadlib.c ldeclib.c lgettext.c lsocklib.c lbytelib.c lrelib.c lzlib.c)

set_source_files_properties(${SOURCES} lua.c luac.c PROPERTIES C_STANDARD 99)
configure_file("luaconf.h.in" "luaconf.h")

set(LUA_LIBS "")

if(BUILD_SHARED_LIBS)
  add_library(liblua SHARED ${SOURCES})
  set_property(TARGET liblua PROPERTY OUTPUT_NAME exlua${LUA_VERSION_SUFFIX})
  if(WIN32)
    target_compile_definitions(liblua PUBLIC LUA_COMPAT_5_2 PRIVATE LUA_BUILD_AS_DLL)
  else()
    target_compile_definitions(liblua PUBLIC LUA_COMPAT_5_2)
  endif()
  install(TARGETS liblua RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
else()
  add_library(liblua STATIC ${SOURCES})
  set_property(TARGET liblua PROPERTY OUTPUT_NAME lua)
  target_compile_definitions(liblua PUBLIC LUA_COMPAT_5_2)
endif()

if(WIN32)
  if(LUAEX_SOCKET)
    list(APPEND LUA_LIBS wsock32.lib ws2_32.lib iphlpapi.lib)
  endif()
else()
  list(APPEND LUA_LIBS m)
  if(LUAEX_THREADLIB)
    list(APPEND LUA_LIBS pthread)
  endif()
endif()

if(LUAEX_MPDECIMAL)
  list(APPEND LUA_LIBS mpdec)
endif()

if(LUAEX_PCRE)
  list(APPEND LUA_LIBS pcre)
endif()

if(LUAEX_ZLIB)
  include_directories(${ZLIB_INCLUDE_DIR})
  list(APPEND LUA_LIBS ${ZLIB_LIBRARY})
endif()

target_link_libraries(liblua PUBLIC ${LUA_LIBS})

if(ANDROID)
  target_compile_options(liblua PRIVATE -Wno-deprecated)
endif()

target_include_directories(liblua PUBLIC ${CMAKE_CURRENT_BINARY_DIR} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
install(FILES lua.h "${CMAKE_CURRENT_BINARY_DIR}/luaconf.h" lualib.h lauxlib.h lua.hpp DESTINATION include)

if(BUILD_EXECUTABLES)
  add_executable(exlua lua.c)
  target_link_libraries(exlua liblua)
  install(TARGETS exlua RUNTIME DESTINATION bin)
  
  add_executable(exluac luac.c)
  target_link_libraries(exluac liblua)
  install(TARGETS exluac RUNTIME DESTINATION bin)
endif()
