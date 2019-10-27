cmake_minimum_required(VERSION 3.4.1)

set(SOURCES lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c
	llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c
	lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lbitlib.c lcorolib.c ldblib.c liolib.c
	lmathlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c loadlib.c linit.c)

set_source_files_properties(${SOURCES} lua.c luac.c PROPERTIES C_STANDARD 99)
add_definitions(-O2 -DLUAEX_BASE -DLUAEX_TRYCATCH -DLUAEX_CLNTSRV)

if(CMAKE_C_COMPILER_ID STREQUAL GNU)
	add_definitions(-Wall -Wextra)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
elseif(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS)
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MANIFEST:NO")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFEST:NO")
endif()

if(LUA_BUILD_SHARED_LIB)
	add_library(liblua SHARED ${SOURCES})
	set_property(TARGET liblua PROPERTY OUTPUT_NAME lua${LUA_VERSION_SUFFIX})
	
	if(WIN32)
		target_compile_definitions(liblua PUBLIC LUA_COMPAT_5_2 PRIVATE LUA_BUILD_AS_DLL)
	else()
		target_compile_definitions(liblua PUBLIC LUA_COMPAT_5_2)
		target_link_libraries(liblua PUBLIC m)
	endif()
else()
	add_library(liblua STATIC ${SOURCES})
	target_compile_definitions(liblua PUBLIC LUA_COMPAT_5_2)
endif()

if(ANDROID)
	target_compile_options(liblua PRIVATE -Wno-deprecated)
endif()

target_include_directories(liblua INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS liblua RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES lua.h luaconf.h lualib.h lauxlib.h lua.hpp luaex.h DESTINATION include)

if(LUA_BUILD_EXECUTABLES)
	add_executable(lua lua.c)
	target_link_libraries(lua liblua)
	install(TARGETS lua RUNTIME DESTINATION bin)

	add_executable(luac luac.c)
	target_link_libraries(luac liblua)
	install(TARGETS luac RUNTIME DESTINATION bin)
endif()
