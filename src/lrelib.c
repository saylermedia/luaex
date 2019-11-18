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


struct re_context
{
  pcre *re;
	int ovector[OVECCOUNT];
};


static struct re_context * compile (lua_State *L, const char *pattern)
{
  struct re_context *ctx = (struct re_context *) lua_newuserdata(L, sizeof(struct re_context));
  const char *error;
  int erroffset;
  
  ctx->re = pcre_compile(pattern, PCRE_UTF8, &error, &erroffset, NULL);
  if (ctx->re == NULL)
    luaL_error(L, _("pattern error: %s at %d"), error, erroffset);
  
  luaL_setmetatable(L, LUA_PCREHANDLE);
  return ctx;
}


static int re_compile (lua_State *L)
{
  compile(L, luaL_checkstring(L, 1));
  return 1;
}


static int re_exec (lua_State *L)
{
  struct re_context *ctx = (struct re_context *) luaL_checkudata(L, 1, LUA_PCREHANDLE);
  size_t l;
  const char *str = luaL_checklstring(L, 2, &l);
  int count = pcre_exec(ctx->re, NULL, str, l, 0, 0, ctx->ovector, OVECCOUNT);
  if (count > 0)
  {
    int i;
    for (i = 0; i < count; i++)
    {
      int noff = ctx->ovector[2 * i];
      lua_pushlstring(L, str + noff, (size_t) (ctx->ovector[2 * i + 1] - noff));
    }
  }
  return count;
}


static int gexec_iter (lua_State *L)
{
  struct re_context *ctx = (struct re_context *) luaL_checkudata(L, lua_upvalueindex(1), LUA_PCREHANDLE);
  
  size_t l;
  const char *str = luaL_checklstring(L, lua_upvalueindex(2), &l);
  int stroff = (int) luaL_checkinteger(L, lua_upvalueindex(3));
  
  int count = pcre_exec(ctx->re, NULL, str, l, stroff, 0, ctx->ovector, OVECCOUNT);
  if (count > 0)
  {
    lua_pushinteger(L, ctx->ovector[1]);
    lua_replace(L, lua_upvalueindex(3));
    
    int i;
    for (i = 0; i < count; i++)
    {
      int noff = ctx->ovector[2 * i];
      lua_pushlstring(L, str + noff, (size_t) (ctx->ovector[2 * i + 1] - noff));
    }
    return count;
  }
  return 0;
}


static int re_gexec (lua_State *L)
{
  luaL_checkudata(L, 1, LUA_PCREHANDLE);
  luaL_checkstring(L, 2);
  
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_pushinteger(L, 0);
  
  lua_pushcclosure(L, gexec_iter, 3);
  return 1;
}


static int re_gc (lua_State *L)
{
  struct re_context *ctx = (struct re_context *) luaL_checkudata(L, 1, LUA_PCREHANDLE);
  pcre_free(ctx);
  return 0;
}


static int re_match (lua_State *L)
{
	size_t l;
	const char *str = luaL_checklstring(L, 1, &l);
	struct re_context *ctx = compile(L, luaL_checkstring(L, 2));
  
  int count = pcre_exec(ctx->re, NULL, str, l, 0, 0, ctx->ovector, OVECCOUNT);
  if (count > 0)
  {
    int i;
    for (i = 0; i < count; i++)
    {
      int noff = ctx->ovector[2 * i];
      lua_pushlstring(L, str + noff, (size_t) (ctx->ovector[2 * i + 1] - noff));
    }
    return count;
  }
  return 0;
}

static int gmatch_iter (lua_State *L)
{
	size_t l;
	const char *str = luaL_checklstring(L, lua_upvalueindex(1), &l);
	int stroff = (int) luaL_checkinteger(L, lua_upvalueindex(2));
  struct re_context *ctx = (struct re_context *) luaL_checkudata(L, lua_upvalueindex(3), LUA_PCREHANDLE);

	int count = pcre_exec(ctx->re, NULL, str, l, stroff, 0, ctx->ovector, OVECCOUNT);
	if (count > 0)
  {
		lua_pushinteger(L, ctx->ovector[1]);
		lua_replace(L, lua_upvalueindex(2));
    
		int i;
		for (i = 0; i < count; i++)
    {
			int noff = ctx->ovector[2 * i];
			lua_pushlstring(L, str + noff, (size_t) (ctx->ovector[2 * i + 1] - noff));
		}
    return count;
	}
	return 0;
}


static int re_gmatch (lua_State *L)
{
	luaL_checkstring(L, 1);
  
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 0);
	compile(L, luaL_checkstring(L, 2));
  
	lua_pushcclosure(L, gmatch_iter, 3);
	return 1;
}


static int re_gsub (lua_State *L)
{
	size_t l;
	const char *str = luaL_checklstring(L, 1, &l);
  struct re_context *ctx = compile(L, luaL_checkstring(L, 2));
  const char *replace = luaL_checkstring(L, 3);
  
  luaL_Buffer b;
	luaL_buffinit(L, &b);
  int stroff = 0;
	int count;
  
  for (;;)
  {
    count = pcre_exec(ctx->re, NULL, str, l, stroff, 0, ctx->ovector, OVECCOUNT);
    if (count > 0)
    {
      luaL_addlstring(&b, str + stroff, (size_t) (ctx->ovector[0] - stroff));
      stroff = ctx->ovector[1];
      const char *rep = replace;
      
      while (*rep != 0)
      {
        if (*rep == '\\')
        {
          rep++;
          int i = 0;
          while (*rep >= '0' && *rep <= '9')
          {
            i *= 10;
            i += (*rep - '0');
            rep++;
          }
          if (i >= 0 && i <= count)
          {
            int noff = ctx->ovector[2 * i];
            luaL_addlstring(&b, str + noff, (size_t) (ctx->ovector[2 * i + 1] - noff));
          }
          else
            return luaL_error(L, _("Out of range replace %d of %d"), i, count - 1);
        } else
          luaL_addlstring(&b, rep++, 1);
      }
    }
    else
    {
        luaL_addstring(&b, str + stroff);
        break;
    } 
  }
  lua_pop(L, 1);
  luaL_pushresult(&b);
	return 1;
}


/*
** functions for 're' library
*/
static const luaL_Reg re_lib[] =
{
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
static const luaL_Reg re_methods[] =
{
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