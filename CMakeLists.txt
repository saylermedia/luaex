cmake_minimum_required(VERSION 3.4.1)
project(lua)

option(LUA_BUILD_SHARED_LIB "Build shared or static library (default)" ON)
option(LUA_BUILD_EXECUTABLES "Build lua & luac executables (default)" ON)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/lua.h" _LUACONF)
string(REGEX MATCH "LUA_VERSION_MAJOR	\"([0-9]*)" LUA_VERSION_MAJOR "${_LUACONF}")
string(REGEX MATCH "LUA_VERSION_MINOR	\"([0-9]*)" LUA_VERSION_MINOR "${_LUACONF}")
unset(_LUACONF)

string(REPLACE "LUA_VERSION_MAJOR	\"" "" LUA_VERSION_MAJOR "${LUA_VERSION_MAJOR}")
string(REPLACE "LUA_VERSION_MINOR	\"" "" LUA_VERSION_MINOR "${LUA_VERSION_MINOR}")
set(LUA_VERSION_SUFFIX "${LUA_VERSION_MAJOR}${LUA_VERSION_MINOR}")

if(WIN32)
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

message(STATUS "Compiler ID: " ${CMAKE_C_COMPILER_ID})

if(LUA_BUILD_SHARED_LIB)
	message(STATUS "Shared library: " ${CMAKE_SHARED_LIBRARY_PREFIX} "lua" ${LUA_VERSION_SUFFIX} ${CMAKE_SHARED_LIBRARY_SUFFIX})
else()
	message(STATUS "Static library: " ${CMAKE_STATIC_LIBRARY_PREFIX} "lua" ${LUA_VERSION_SUFFIX} ${CMAKE_STATIC_LIBRARY_SUFFIX})
endif()

if(LUA_BUILD_EXECUTABLES)
	message(STATUS "Executables: lua" ${CMAKE_EXECUTABLE_SUFFIX} " luac" ${CMAKE_EXECUTABLE_SUFFIX})
endif()

message(STATUS "Install to: " ${CMAKE_INSTALL_PREFIX})

set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

add_subdirectory(src)

if(NOT WIN32)
	install(FILES doc/lua.1 doc/luac.1 DESTINATION share/man/man1)
endif()

