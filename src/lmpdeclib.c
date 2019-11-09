/*
** Decimal floating point arithmetic library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#define mpdeclib_c
#define LUA_LIB

#include "lprefix.h"

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#include "lstate.h"


#ifdef LUAEX_MPDECIMAL
#include <malloc.h>
#include <mpdecimal.h>

#define LUA_NUMBERHANDLE "NUMBER*"

#define DEC_MINALLOC 4

static volatile int initialized = 0;

typedef struct DecimalState {
  mpd_t v;
  mpd_uint_t data[DEC_MINALLOC];
} DecimalState;


static void dec_addstatus (lua_State *L, uint32_t status) {
  mpd_context_t *ctx = &L->mpd_ctx;
  ctx->status |= status;
  if (status & (ctx->traps | MPD_Malloc_error)) {
    if (status & MPD_Malloc_error)
      luaL_error(L, _("not enough memory"));
    
    switch ((ctx->traps & status)) {
      case MPD_IEEE_Invalid_operation:
        luaL_error(L, _("IEEE invalide operation"));
        break;
        
      case MPD_Division_by_zero:
        luaL_error(L, _("division by zero"));
        break;
        
      case MPD_Overflow:
        luaL_error(L, _("overflow"));
        break;
        
      case MPD_Underflow:
        luaL_error(L, _("underflow"));
        break;
    }
  }
}

static DecimalState * create(lua_State *L) {
  DecimalState *ds = (DecimalState *) lua_newuserdata(L, sizeof(DecimalState));
  ds->v.flags = MPD_STATIC | MPD_STATIC_DATA;
  ds->v.exp = 0;
  ds->v.digits = 0;
  ds->v.len = 0;
  ds->v.alloc = DEC_MINALLOC;
  ds->v.data = ds->data;
  luaL_setmetatable(L, LUA_NUMBERHANDLE);
  return ds;
}


static DecimalState * todecimal (lua_State *L, int idx, int *convidx) {
  DecimalState *ds = (DecimalState *) luaL_testudata(L, idx, LUA_NUMBERHANDLE);
  if (ds)
    *convidx = 0;
  else {
    const char *s = luaL_checkstring(L, idx);
    ds = create(L);
    mpd_context_t maxctx;
    mpd_maxcontext(&maxctx);
    uint32_t status = 0;
    mpd_qset_string(&ds->v, s, &maxctx, &status);
    if (status & (MPD_Inexact | MPD_Rounded | MPD_Clamped))
        mpd_seterror(&ds->v, MPD_Invalid_operation, &status);
    status &= MPD_Errors;
    dec_addstatus(L, status);
    *convidx = lua_gettop(L);
  }
  return ds;
}


#define UNARY_OP(FUNCNAME) \
static int n_##FUNCNAME (lua_State *L) { \
  DecimalState *a = (DecimalState *) luaL_checkudata(L, 1, LUA_NUMBERHANDLE); \
  DecimalState *r = create(L); \
  uint32_t status = 0; \
  FUNCNAME(&r->v, &a->v, &L->mpd_ctx, &status); \
  dec_addstatus(L, status); \
  return 1; \
}


#define BINARY_OP(FUNCNAME) \
static int n_##FUNCNAME (lua_State *L) { \
  int convidx;  \
  DecimalState *a = (DecimalState *) luaL_checkudata(L, 1, LUA_NUMBERHANDLE); \
  DecimalState *b = todecimal(L, 2, &convidx);  \
  DecimalState *r = create(L);  \
  uint32_t status = 0; \
  FUNCNAME(&r->v, &a->v, &b->v, &L->mpd_ctx, &status); \
  if (convidx) lua_remove(L, convidx); \
  dec_addstatus(L, status); \
  return 1; \
}


static int n_new (lua_State *L) {
  int convidx;
  todecimal(L, 1, &convidx);
  if (!convidx)
    lua_pushvalue(L, 1);
  return 1;
}


BINARY_OP(mpd_qadd)
BINARY_OP(mpd_qsub)
BINARY_OP(mpd_qmul)
BINARY_OP(mpd_qdiv)
BINARY_OP(mpd_qrem)
UNARY_OP(mpd_qabs)
BINARY_OP(mpd_qpow)
UNARY_OP(mpd_qminus)
BINARY_OP(mpd_qdivint)
BINARY_OP(mpd_qand)
BINARY_OP(mpd_qor)
BINARY_OP(mpd_qxor)
UNARY_OP(mpd_qfloor)
UNARY_OP(mpd_qceil)
UNARY_OP(mpd_qtrunc)


static int n_round (lua_State *L) {
  DecimalState *a = (DecimalState *) luaL_checkudata(L, 1, LUA_NUMBERHANDLE);
  mpd_ssize_t y = (mpd_ssize_t) luaL_optinteger(L, 2, 0);
  DecimalState *r = create(L);
  mpd_uint_t dq[1] = {1};
  mpd_t q = {MPD_STATIC | MPD_CONST_DATA, 0, 1, 1, 1, dq};
  uint32_t status = 0;
  q.exp = (y == MPD_SSIZE_MIN) ? MPD_SSIZE_MAX : -y;
  mpd_qquantize(&r->v, &a->v, &q, &L->mpd_ctx, &status);
  dec_addstatus(L, status);
  return 1;
}


static int n_shl (lua_State *L) {
  int convidx1, convidx2;
  DecimalState *a = todecimal(L, 1, &convidx1);
  DecimalState *b = todecimal(L, 2, &convidx2);
  DecimalState *r = create(L);
  uint32_t status = 0;
  mpd_t *q = mpd_qnew();
  if (q == NULL)
    luaL_error(L, _("not enough memory"));
  if (mpd_sign(&b->v))
    mpd_qminus(q, &b->v, &L->mpd_ctx, &status);
  else
    mpd_qcopy(q, &b->v, &status);
  status = 0;
  mpd_qshift(&r->v, &a->v, q, &L->mpd_ctx, &status);
  mpd_del(q);
  if (convidx2) lua_remove(L, convidx2);
  if (convidx1) lua_remove(L, convidx1);
  dec_addstatus(L, status);
  return 1;
}


static int n_shr (lua_State *L) {
  int convidx1, convidx2;
  DecimalState *a = todecimal(L, 1, &convidx1);
  DecimalState *b = todecimal(L, 2, &convidx2);
  DecimalState *r = create(L);
  uint32_t status = 0;
  mpd_t *q = mpd_qnew();
  if (q == NULL)
    luaL_error(L, _("not enough memory"));
  if (mpd_sign(&b->v))
    mpd_qcopy(q, &b->v, &status);
  else
    mpd_qminus(q, &b->v, &L->mpd_ctx, &status);
  status = 0;
  mpd_qshift(&r->v, &a->v, q, &L->mpd_ctx, &status);
  mpd_del(q);
  if (convidx2) lua_remove(L, convidx2);
  if (convidx1) lua_remove(L, convidx1);
  dec_addstatus(L, status);
  return 1;
}


#define COMPARE(FUNCNAME,SIGN) \
static int n_##FUNCNAME (lua_State *L) { \
  int convidx1, convidx2; \
  DecimalState *a = todecimal(L, 1, &convidx1); \
  DecimalState *b = todecimal(L, 2, &convidx2); \
  uint32_t status = 0; \
  lua_pushboolean(L, (mpd_qcmp(&a->v, &b->v, &status) SIGN 0)); \
  if (convidx2) lua_remove(L, convidx2); \
  if (convidx1) lua_remove(L, convidx1); \
  dec_addstatus(L, status); \
  return 1; \
}


COMPARE(eq, ==)
COMPARE(lt, <)
COMPARE(le, <=)


static int n_tostring (lua_State *L) {
  DecimalState *ds = (DecimalState *) luaL_checkudata(L, 1, LUA_NUMBERHANDLE);
  char *rstring = mpd_to_sci(&ds->v, 1);
  if (rstring == NULL)
    luaL_error(L, _("not enough memory"));
  lua_pushstring(L, rstring);
  mpd_free(rstring);
  return 1;
}


static int n_gc (lua_State *L) {
  DecimalState *ds = (DecimalState *) luaL_checkudata(L, 1, LUA_NUMBERHANDLE);
  mpd_del(&ds->v);
  return 0;
}


/*
** functions for 'number' library
*/
static const luaL_Reg numberlib[] = {
  {"new", n_new},
  {NULL, NULL}
};


/*
** methods for number handles
*/
static const luaL_Reg nlib[] = {
  {"abs", n_mpd_qabs},
  {"floor", n_mpd_qfloor},
  {"ceil", n_mpd_qceil},
  {"trunc", n_mpd_qtrunc},
  {"round", n_round},
  {"__add", n_mpd_qadd},
  {"__sub", n_mpd_qsub},
  {"__mul", n_mpd_qmul},
  {"__div", n_mpd_qdiv},
  {"__mod", n_mpd_qrem},
  {"__pow", n_mpd_qpow},
  {"__unm", n_mpd_qminus},
  {"__idiv", n_mpd_qdivint},
  {"__band", n_mpd_qand},
  {"__bor", n_mpd_qor},
  {"__bxor", n_mpd_qxor},
  {"__shl", n_shl},
  {"__shr", n_shr},
  {"__eq", n_eq},
  {"__lt", n_lt},
  {"__le", n_le},
  {"__tostring", n_tostring},
  {"__serialize", n_tostring},
  {"__deserialize", n_new},
  {"__gc", n_gc},
  {NULL, NULL}
};


static void createmeta (lua_State *L) {
  luaL_newmetatable(L, LUA_NUMBERHANDLE);  /* create metatable for thread handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, nlib, 0);  /* add file methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
}


static int n_call (lua_State *L) {
  int convidx;
  todecimal(L, 2, &convidx);
  if (!convidx)
    lua_pushvalue(L, 2);
  return 1;
}


static const luaL_Reg mtlib[] = {
  {"__call", n_call},
  {NULL, NULL}
};


/*static void dec_traphandler(mpd_context_t *ctx) {
  (void) ctx;
}*/


LUAMOD_API int luaopen_number (lua_State *L) {
  /*if (!initialized) {
    mpd_traphandler = dec_traphandler;
    mpd_mallocfunc = malloc;
    mpd_reallocfunc = realloc;
    mpd_callocfunc = mpd_callocfunc_em;
    mpd_free = free;
    mpd_setminalloc(DEC_MINALLOC);
    initialized = 1;
  }*/
  mpd_maxcontext(&L->mpd_ctx);
  /* register mpdec library */
  luaL_newlib(L, numberlib);  /* new module */
  luaL_newlib(L, mtlib);
  lua_setmetatable(L, -2);
  createmeta(L);
  /* register PI constant */
  lua_pushcfunction(L, n_new);
  lua_pushstring(L, "3.141592653589793238462643383279502884");
  lua_call(L, 1, 1);
  lua_setfield(L, -2, "PI");
  return 1;
}

#endif