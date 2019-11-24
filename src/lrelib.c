/*
** Perl-Compatible Regular Expressions library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#define relib_c
#define LUA_LIB

#include "lprefix.h"

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#ifdef LUAEX_PCRE
#include <stdio.h>
#include <pcre.h>


#define LUA_PCREHANDLE "RE*"
#define OVECCOUNT 64


typedef struct {
  pcre *re;
	int ovector[OVECCOUNT];
} reState;


static reState * compile (lua_State *L, const char *s) {
  reState *state = (reState *) lua_newuserdata(L, sizeof(reState));
  const char *error;
  int erroffset;
  state->re = pcre_compile(s, PCRE_UTF8, &error, &erroffset, NULL);
  if (state->re)
    luaL_setmetatable(L, LUA_PCREHANDLE);
  else
    luaL_error(L, _("pattern error: %s at %d"), error, erroffset);
  return state;
}


static int re_compile (lua_State *L) {
  compile(L, luaL_checkstring(L, 1));
  return 1;
}


static int re_exec (lua_State *L) {
  reState *state = (reState *) luaL_checkudata(L, 1, LUA_PCREHANDLE);
  size_t l;
  const char *s = luaL_checklstring(L, 2, &l);
  int count = pcre_exec(state->re, NULL, s, l, 0, 0, state->ovector, OVECCOUNT);
  if (count > 0) {
    int i;
    for (i = 0; i < count; i++)
      lua_pushlstring(L, s + state->ovector[2 * i], (size_t) (state->ovector[2 * i + 1] - state->ovector[2 * i]));
  }
  return count;
}


static int gexec_iter (lua_State *L) {
  reState *state = (reState *) luaL_checkudata(L, lua_upvalueindex(1), LUA_PCREHANDLE);
  size_t l;
  const char *s = luaL_checklstring(L, lua_upvalueindex(2), &l);
  int off = (int) luaL_checkinteger(L, lua_upvalueindex(3));
  int count = pcre_exec(state->re, NULL, s, l, off, 0, state->ovector, OVECCOUNT);
  if (count > 0) {
    lua_pushinteger(L, state->ovector[1]);
    lua_replace(L, lua_upvalueindex(3));
    int i;
    for (i = 0; i < count; i++)
      lua_pushlstring(L, s + state->ovector[2 * i], (size_t) (state->ovector[2 * i + 1] - state->ovector[2 * i]));
    return count;
  }
  return 0;
}


static int re_gexec (lua_State *L) {
  luaL_checkudata(L, 1, LUA_PCREHANDLE);
  luaL_checkstring(L, 2);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_pushinteger(L, 0);
  lua_pushcclosure(L, gexec_iter, 3);
  return 1;
}


static int re_gc (lua_State *L) {
  reState *state = (reState *) luaL_checkudata(L, 1, LUA_PCREHANDLE);
  pcre_free(state->re);
  return 0;
}


static int re_match (lua_State *L) {
	size_t l;
	const char *s = luaL_checklstring(L, 1, &l);
	reState *state = compile(L, luaL_checkstring(L, 2));
  int count = pcre_exec(state->re, NULL, s, l, 0, 0, state->ovector, OVECCOUNT);
  if (count > 0) {
    int i;
    for (i = 0; i < count; i++)
      lua_pushlstring(L, s + state->ovector[2 * i], (size_t) (state->ovector[2 * i + 1] - state->ovector[2 * i]));
    return count;
  }
  return 0;
}

static int gmatch_iter (lua_State *L) {
	size_t l;
	const char *s = luaL_checklstring(L, lua_upvalueindex(1), &l);
	int off = (int) luaL_checkinteger(L, lua_upvalueindex(2));
  reState *state = (reState *) luaL_checkudata(L, lua_upvalueindex(3), LUA_PCREHANDLE);
	int count = pcre_exec(state->re, NULL, s, l, off, 0, state->ovector, OVECCOUNT);
	if (count > 0) {
		lua_pushinteger(L, state->ovector[1]);
		lua_replace(L, lua_upvalueindex(2));
		int i;
		for (i = 0; i < count; i++)
      lua_pushlstring(L, s + state->ovector[2 * i], (size_t) (state->ovector[2 * i + 1] - state->ovector[2 * i]));
    return count;
	}
	return 0;
}


static int re_gmatch (lua_State *L) {
	luaL_checkstring(L, 1);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 0);
	compile(L, luaL_checkstring(L, 2));
	lua_pushcclosure(L, gmatch_iter, 3);
	return 1;
}


static int re_gsub (lua_State *L) {
	size_t l;
	const char *s = luaL_checklstring(L, 1, &l);
  reState *state = compile(L, luaL_checkstring(L, 2));
  const char *r = luaL_checkstring(L, 3);
  luaL_Buffer b;
	luaL_buffinit(L, &b);
  int off = 0;
  for (;;) {
    int count = pcre_exec(state->re, NULL, s, l, off, 0, state->ovector, OVECCOUNT);
    if (count > 0) {
      luaL_addlstring(&b, s + off, (size_t) (state->ovector[0] - off));
      off = state->ovector[1];
      const char *rep = r;
      while (*rep != 0) {
        if (*rep == '\\') {
          rep++;
          int i = 0;
          while (*rep >= '0' && *rep <= '9') {
            i *= 10;
            i += (*rep - '0');
            rep++;
          }
          if (i >= 0 && i <= count)
            lua_pushlstring(L, s + state->ovector[2 * i], (size_t) (state->ovector[2 * i + 1] - state->ovector[2 * i]));
          else
            return luaL_error(L, _("Out of range replace %d of %d"), i, count - 1);
        } else
          luaL_addlstring(&b, rep++, 1);
      }
    } else {
        luaL_addstring(&b, s + off);
        break;
    } 
  }
  luaL_pushresult(&b);
  lua_remove(L, -2);
	return 1;
}


/*
** functions for 're' library
*/
static const luaL_Reg re_lib[] = {
  {"compile", re_compile},
  {"exec", re_exec},
  {"gexec", re_gexec},
  {"match", re_match},
  {"gmatch", re_gmatch},
  {"gsub", re_gsub},
  {NULL, NULL}
};


/*
** methods for re handles
*/
static const luaL_Reg re_methods[] = {
  {"exec", re_exec},
  {"gexec", re_gexec},
  {"__gc", re_gc},
  {NULL, NULL}
};


LUAMOD_API int luaopen_re (lua_State *L) {
  /* register library */
  luaL_newlib(L, re_lib);  /* new module */
  /* create metatable for byte handles */
  luaL_newmetatable(L, LUA_PCREHANDLE);
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, re_methods, 0);  /* add methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
  return 1;
}


#endif