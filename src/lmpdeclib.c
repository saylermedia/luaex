/*
** Decimal floating point arithmetic library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#ifdef LUAEX_MPDECIMAL
#define mpdeclib_c
#define LUA_LIB

#include "lprefix.h"

#include <mpdecimal.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "luaex.h"


#define _(x) x

#define LUA_NUMBERHANDLE "NUMBER*"

#define ARITHM(x) static int n##x (lua_State *L) { return arith(L, mpd_##x); }
#define ARITHM_S(x) static int n##x (lua_State *L) { return arith_s(L, mpd_##x); }

static mpd_context_t ctx;
typedef void (*arith_func) (mpd_t *, const mpd_t *, const mpd_t *, mpd_context_t *);
typedef void (*arith_self) (mpd_t *, const mpd_t *, mpd_context_t *);

static mpd_t * create (lua_State *L, int idx, int *convidx) {
  mpd_t **decimal = (mpd_t **) luaL_testudata(L, idx, LUA_NUMBERHANDLE);
  if (!decimal) {
    const char *s = luaL_checkstring(L, idx);
    decimal = (mpd_t **) lua_newuserdata(L, sizeof(mpd_t *));
    if (convidx)
      *convidx = lua_gettop(L);
    *decimal = mpd_new(&ctx);
    if (*decimal) {
      mpd_set_string(*decimal, s, &ctx);
      luaL_setmetatable(L, LUA_NUMBERHANDLE);
    } else
      luaL_error(L, _("not enough memory"));
  }
  return *decimal;
}


static int arith (lua_State *L, arith_func func) {
  int convidx = 0;
  mpd_t **decimal = (mpd_t **) luaL_checkudata(L, 1, LUA_NUMBERHANDLE);
  mpd_t *other = create(L, 2, &convidx);
  mpd_t **result = (mpd_t **) lua_newuserdata(L, sizeof(mpd_t *));
  *result = mpd_new(&ctx);
  if (*result) {
    func(*result, *decimal, other, &ctx);
    luaL_setmetatable(L, LUA_NUMBERHANDLE);
    if (convidx > 0)
      lua_remove(L, convidx);
  } else
    return luaL_error(L, _("not enough memory"));
  return 1;
}


static int arith_s (lua_State *L, arith_self func) {
  mpd_t **decimal = (mpd_t **) luaL_checkudata(L, 1, LUA_NUMBERHANDLE);
  mpd_t **result = (mpd_t **) lua_newuserdata(L, sizeof(mpd_t *));
  *result = mpd_new(&ctx);
  if (*result) {
    func(*result, *decimal, &ctx);
    luaL_setmetatable(L, LUA_NUMBERHANDLE);
  } else
    return luaL_error(L, _("not enough memory"));
  return 1;
}

static int nnew (lua_State *L) {
  create(L, 1, NULL);
  return 1;
}


ARITHM(add)
ARITHM(sub)
ARITHM(mul)
ARITHM(div)
ARITHM_S(abs)
ARITHM(pow)
ARITHM(divint)
ARITHM(and)
ARITHM(or)
ARITHM(xor)
ARITHM_S(floor)
ARITHM_S(ceil)


static int ntostring (lua_State *L) {
  mpd_t **decimal = (mpd_t **) luaL_checkudata(L, 1, LUA_NUMBERHANDLE);
  char *rstring = mpd_to_sci(*decimal, 1);
  lua_pushstring(L, rstring);
  mpd_free(rstring);
  return 1;
}


static int neq (lua_State *L) {
  int idx1 = 0;
  int idx2 = 0;
  mpd_t *a = create(L, 1, &idx1);
  mpd_t *b = create(L, 2, &idx2);
  lua_pushboolean(L, mpd_cmp(a, b, &ctx) == 0);
  if (idx2 > 0)
    lua_remove(L, idx2);
  if (idx1 > 0)
    lua_remove(L, idx1);
  return 1;
}


static int nlt (lua_State *L) {
  int idx1 = 0;
  int idx2 = 0;
  mpd_t *a = create(L, 1, &idx1);
  mpd_t *b = create(L, 2, &idx2);
  lua_pushboolean(L, mpd_cmp(a, b, &ctx) < 0);
  if (idx2 > 0)
    lua_remove(L, idx2);
  if (idx1 > 0)
    lua_remove(L, idx1);
  return 1;
}


static int nle (lua_State *L) {
  int idx1 = 0;
  int idx2 = 0;
  mpd_t *a = create(L, 1, &idx1);
  mpd_t *b = create(L, 2, &idx2);
  lua_pushboolean(L, mpd_cmp(a, b, &ctx) <= 0);
  if (idx2 > 0)
    lua_remove(L, idx2);
  if (idx1 > 0)
    lua_remove(L, idx1);
  return 1;
}


static int ngc (lua_State *L) {
  mpd_t **decimal = (mpd_t **) luaL_checkudata(L, 1, LUA_NUMBERHANDLE);
  mpd_del(*decimal);
  return 0;
}


/*
** functions for 'number' library
*/
static const luaL_Reg numberlib[] = {
  {"new", nnew},
  {NULL, NULL}
};


/*
** methods for number handles
*/
static const luaL_Reg nlib[] = {
  {"abs", nabs},
  {"floor", nfloor},
  {"ceil", nceil},
  {"__add", nadd},
  {"__sub", nsub},
  {"__mul", nmul},
  {"__div", ndiv},
  {"__mod", nabs},
  {"__pow", npow},
  {"__idiv", ndivint},
  {"__band", nand},
  {"__bor", nor},
  {"__bxor", nxor},
  {"__eq", neq},
  {"__lt", nlt},
  {"__le", nle},
  {"__tostring", ntostring},
  {"__serialize", ntostring},
  {"__deserialize", nnew},
  {"__gc", ngc},
  {NULL, NULL}
};


static void createmeta (lua_State *L) {
  luaL_newmetatable(L, LUA_NUMBERHANDLE);  /* create metatable for thread handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, nlib, 0);  /* add file methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
}


static int ncall (lua_State *L) {
  create(L, 2, NULL);
  return 1;
}


static const luaL_Reg mtlib[] = {
  {"__call", ncall},
  {NULL, NULL}
};


LUAMOD_API int luaopen_number (lua_State *L) {
  mpd_init(&ctx, 300);
	ctx.traps = 0;
  luaL_newlib(L, numberlib);  /* new module */
  luaL_newlib(L, mtlib);
  lua_setmetatable(L, -2);
  createmeta(L);
  
  lua_pushcfunction(L, nnew);
  lua_pushstring(L, "3.141592653589793238462643383279502884");
  lua_call(L, 1, 1);
  lua_setfield(L, -2, "PI");
  
  return 1;
}

#endif