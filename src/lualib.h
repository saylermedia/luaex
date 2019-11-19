/*
** $Id: lualib.h,v 1.45.1.1 2017/04/19 17:20:42 roberto Exp $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "lua.h"


/* version suffix for environment variable names */
#define LUA_VERSUFFIX          "_" LUA_VERSION_MAJOR "_" LUA_VERSION_MINOR


LUAMOD_API int (luaopen_base) (lua_State *L);

#define LUA_COLIBNAME	"coroutine"
LUAMOD_API int (luaopen_coroutine) (lua_State *L);

#define LUA_TABLIBNAME	"table"
LUAMOD_API int (luaopen_table) (lua_State *L);

#define LUA_IOLIBNAME	"io"
LUAMOD_API int (luaopen_io) (lua_State *L);

#define LUA_OSLIBNAME	"os"
LUAMOD_API int (luaopen_os) (lua_State *L);

#define LUA_STRLIBNAME	"string"
LUAMOD_API int (luaopen_string) (lua_State *L);

#define LUA_UTF8LIBNAME	"utf8"
LUAMOD_API int (luaopen_utf8) (lua_State *L);

#define LUA_BITLIBNAME	"bit32"
LUAMOD_API int (luaopen_bit32) (lua_State *L);

#define LUA_MATHLIBNAME	"math"
LUAMOD_API int (luaopen_math) (lua_State *L);

#define LUA_DBLIBNAME	"debug"
LUAMOD_API int (luaopen_debug) (lua_State *L);

#define LUA_LOADLIBNAME	"package"
LUAMOD_API int (luaopen_package) (lua_State *L);

#ifdef LUAEX_THREADLIB
#define LUA_THREADLIBNAME "thread"
LUAMOD_API int (luaopen_thread) (lua_State *L);
#endif

#ifdef LUAEX_MPDECIMAL
#define LUA_DECIMALLIBNAME "decimal"
LUAMOD_API int (luaopen_decimal) (lua_State *L);
#endif

#ifdef LUAEX_SOCKET
#define LUAEX_SOCKETLIBNAME "socket"
LUAMOD_API int (luaopen_socket) (lua_State *L);
#endif

#ifdef LUAEX_BYTE
#define LUAEX_BYTELIBNAME "byte"
LUAMOD_API int (luaopen_byte) (lua_State *L);
#endif

#ifdef LUAEX_PCRE
#define LUAEX_RELIBNAME "re"
LUAMOD_API int (luaopen_re) (lua_State *L);
#endif

#ifdef LUAEX_ZLIB
#define LUAEX_ZLIBNAME "zlib"
LUAMOD_API int (luaopen_zlib) (lua_State *L);
#endif

/* open all previous libraries */
LUALIB_API void (luaL_openlibs) (lua_State *L);



#if !defined(lua_assert)
#define lua_assert(x)	((void)0)
#endif


#endif
