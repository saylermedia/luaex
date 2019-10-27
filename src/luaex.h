/*
** Lua Extreme - A Scripting Language
** author: Alexey Smirnov
** 
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
LUA_API int   (lua_side) (lua_State *L);
#endif

#ifdef LUAEX_SERIALIZE
/* serialize and deserialize values */
LUA_API void (lua_serialize) (lua_State *L, int idx);
LUA_API void (lua_deserialize) (lua_State *L, int idx);
#endif


#endif
