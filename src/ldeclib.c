/*
** Decimal floating point arithmetic library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#define declib_c
#define LUA_LIB

#include "lprefix.h"

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#include "lstate.h"


#ifdef LUAEX_MPDECIMAL
#include <malloc.h>
#include <mpdecimal.h>

#define LUA_DECIMALHANDLE "DECIMAL*"

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
    
    int flags = (ctx->traps & status);
    if (flags & MPD_IEEE_Invalid_operation)
        luaL_error(L, _("IEEE invalide operation"));
    else if (flags & MPD_Division_by_zero)
        luaL_error(L, _("division by zero"));
    else if (flags & MPD_Overflow)
        luaL_error(L, _("overflow"));
    else if (flags & MPD_Underflow)
        luaL_error(L, _("underflow"));
    else if (flags & MPD_Subnormal)
        luaL_error(L, _("subnormal"));
    else if (flags & MPD_Inexact)   
        luaL_error(L, _("inexact"));
    else if (flags & MPD_Rounded)    
        luaL_error(L, _("rounded"));
    else if (flags & MPD_Clamped)    
        luaL_error(L, _("clamped"));
      
    luaL_error(L, _("operation failed"));
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
  luaL_setmetatable(L, LUA_DECIMALHANDLE);
  return ds;
}


static DecimalState * todecimal (lua_State *L, int idx, int *convidx) {
  DecimalState *ds = (DecimalState *) luaL_testudata(L, idx, LUA_DECIMALHANDLE);
  if (ds)
    *convidx = 0;
  else {
    const char *s;
    size_t l;
    switch (lua_type(L, idx)) {
      case LUA_TBOOLEAN:
        s = (lua_toboolean(L, idx)) ? "1" : "0";
        break;
        
      case LUA_TNUMBER:
      case LUA_TSTRING:
      case LUA_TTABLE:
      case LUA_TUSERDATA:
        s = luaL_tolstring(L, idx, &l);
        if (s == NULL)
          luaL_error(L, _("cannot convert value to decimal"));
        break;
        
      default:
        luaL_error(L, _("cannot convert type %s to decimal"), lua_typename(L, lua_type(L, idx)));
        break;
    }
    ds = create(L);
    mpd_context_t maxctx;
    mpd_maxcontext(&maxctx);
    uint32_t status = 0;
    mpd_qset_string(&ds->v, s, &maxctx, &status);
    dec_addstatus(L, status);
    *convidx = lua_gettop(L);
  }
  return ds;
}


#define UNARY_OP(FUNCNAME) \
static int dec_##FUNCNAME (lua_State *L) { \
  DecimalState *a = (DecimalState *) luaL_checkudata(L, 1, LUA_DECIMALHANDLE); \
  DecimalState *r = create(L); \
  uint32_t status = 0; \
  FUNCNAME(&r->v, &a->v, &L->mpd_ctx, &status); \
  dec_addstatus(L, status); \
  return 1; \
}


#define BINARY_OP(FUNCNAME) \
static int dec_##FUNCNAME (lua_State *L) { \
  int convidx;  \
  DecimalState *a = (DecimalState *) luaL_checkudata(L, 1, LUA_DECIMALHANDLE); \
  DecimalState *b = todecimal(L, 2, &convidx);  \
  DecimalState *r = create(L);  \
  uint32_t status = 0; \
  FUNCNAME(&r->v, &a->v, &b->v, &L->mpd_ctx, &status); \
  if (convidx) lua_remove(L, convidx); \
  dec_addstatus(L, status); \
  return 1; \
}


static int dec_new (lua_State *L) {
  int convidx;
  luaL_checkany(L, 1);
  DecimalState *ds = todecimal(L, 1, &convidx);
  if (!convidx) {
    DecimalState *r = create(L);
    uint32_t status = 0;
    mpd_qcopy(&r->v, &ds->v, &status);
    dec_addstatus(L, status);
  }
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


static int dec_round (lua_State *L) {
  DecimalState *a = (DecimalState *) luaL_checkudata(L, 1, LUA_DECIMALHANDLE);
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


static int dec_shl (lua_State *L) {
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


static int dec_shr (lua_State *L) {
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
static int dec_##FUNCNAME (lua_State *L) { \
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


static int dec_tostring (lua_State *L) {
  DecimalState *ds = (DecimalState *) luaL_checkudata(L, 1, LUA_DECIMALHANDLE);
  mpd_ssize_t size;
  char *s;
  size = mpd_to_sci_size(&s, &ds->v, 1);
  if (size < 0)
    luaL_error(L, _("not enough memory"));
  lua_pushlstring(L, s, (size_t) size);
  mpd_free(s);
  return 1;
}


static int dec_gc (lua_State *L) {
  DecimalState *ds = (DecimalState *) luaL_checkudata(L, 1, LUA_DECIMALHANDLE);
  mpd_del(&ds->v);
  return 0;
}


/*
** functions for 'number' library
*/
static const luaL_Reg dec_lib[] = {
  {"new", dec_new},
  {NULL, NULL}
};


/*
** methods for decimal handles
*/
static const luaL_Reg dec_methods[] = {
  {"abs", dec_mpd_qabs},
  {"floor", dec_mpd_qfloor},
  {"ceil", dec_mpd_qceil},
  {"trunc", dec_mpd_qtrunc},
  {"round", dec_round},
  {"__add", dec_mpd_qadd},
  {"__sub", dec_mpd_qsub},
  {"__mul", dec_mpd_qmul},
  {"__div", dec_mpd_qdiv},
  {"__mod", dec_mpd_qrem},
  {"__pow", dec_mpd_qpow},
  {"__unm", dec_mpd_qminus},
  {"__idiv", dec_mpd_qdivint},
  {"__band", dec_mpd_qand},
  {"__bor", dec_mpd_qor},
  {"__bxor", dec_mpd_qxor},
  {"__shl", dec_shl},
  {"__shr", dec_shr},
  {"__eq", dec_eq},
  {"__lt", dec_lt},
  {"__le", dec_le},
  {"__tostring", dec_tostring},
  {"__serialize", dec_tostring},
  {"__deserialize", dec_new},
  {"__gc", dec_gc},
  {NULL, NULL}
};


static void createmeta (lua_State *L) {
  luaL_newmetatable(L, LUA_DECIMALHANDLE);  /* create metatable for thread handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, dec_methods, 0);  /* add file methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
}


static int dec_call (lua_State *L) {
  int convidx;
  lua_remove(L, 1);
  luaL_checkany(L, 1);
  DecimalState *ds = todecimal(L, 1, &convidx);
  if (!convidx) {
    DecimalState *r = create(L);
    uint32_t status = 0;
    mpd_qcopy(&r->v, &ds->v, &status);
    dec_addstatus(L, status);
  }
  return 1;
}


static const luaL_Reg dec_mt[] = {
  {"__call", dec_call},
  {NULL, NULL}
};


static void dec_traphandler(mpd_context_t *ctx) {
  (void) ctx;
}

#define DEC_DFLT_EMAX 999999
#define DEC_DFLT_EMIN -999999

static mpd_context_t dflt_ctx = {
  28, DEC_DFLT_EMAX, DEC_DFLT_EMIN,
  MPD_IEEE_Invalid_operation | MPD_Division_by_zero | MPD_Overflow,
  0, 0, MPD_ROUND_HALF_EVEN, 0, 1
};


LUAMOD_API int luaopen_decimal (lua_State *L) {
  if (!initialized) {
    mpd_traphandler = dec_traphandler;
    mpd_mallocfunc = malloc;
    mpd_reallocfunc = realloc;
    mpd_callocfunc = mpd_callocfunc_em;
    mpd_free = free;
    mpd_setminalloc(DEC_MINALLOC);
    initialized = 1;
  }
  L->mpd_ctx = dflt_ctx;
  /*mpd_maxcontext(&L->mpd_ctx);*/
  /* register mpdec library */
  luaL_newlib(L, dec_lib);  /* new module */
  luaL_newlib(L, dec_mt);
  lua_setmetatable(L, -2);
  createmeta(L);
  /* register PI constant */
  lua_pushcfunction(L, dec_new);
  lua_pushstring(L, "3.141592653589793238462643383279502884");
  lua_call(L, 1, 1);
  lua_setfield(L, -2, "PI");
  return 1;
}

#endif