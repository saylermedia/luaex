/*
** Lua Extreme - Box library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#ifndef luaex_h
#define luaex_h

#include "lua.h"


/* running on side: server, client or both */
#ifdef LUAEX_CLNTSRV
#define LUAEX_SBOTH 0
#define LUAEX_SSERVER 1
#define LUAEX_SCLIENT 2
LUA_API void  (lua_setside) (lua_State *L, int side, lua_CFunction scall, lua_CFunction ccall);
LUA_API int   (lua_getside) (lua_State *L);
#endif

#ifdef LUAEX_SERIALIZE
/* serialize and deserialize values */
LUA_API void (lua_serialize) (lua_State *L, int idx, int lastidx);
LUA_API int (lua_deserialize) (lua_State *L, int idx);
#endif

#ifdef LUAEX_THREADLIB
/* cancel execution */
LUA_API void (lua_cancel) (lua_State *L);
#endif

#endif
