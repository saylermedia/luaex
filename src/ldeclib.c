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
#define LUA_DECCTXHANDLE "DECCTX*"
#define DEC_MINALLOC 4

#define MPD(x) (&x->v)
#define CTX(L) (&L->decctx)
#define MAXCTX(L) (&L->maxdecctx)

#define DEC_DFLT_EMAX 999999
#define DEC_DFLT_EMIN -999999

static mpd_context_t dflt_ctx = {
  38, DEC_DFLT_EMAX, DEC_DFLT_EMIN,
  MPD_IEEE_Invalid_operation | MPD_Division_by_zero | MPD_Overflow,
  0, 0, MPD_ROUND_HALF_EVEN, 0, 1
};

typedef struct DecimalState {
  mpd_t v;
  mpd_uint_t data[DEC_MINALLOC];
  char *s;
} DecimalState;


static void dec_addstatus (lua_State *L, uint32_t status) {
  mpd_context_t *ctx = CTX(L);
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

static DecimalState * create (lua_State *L) {
  DecimalState *dec = (DecimalState *) lua_newuserdata(L, sizeof(DecimalState));
  dec->v.flags = MPD_STATIC | MPD_STATIC_DATA;
  dec->v.exp = 0;
  dec->v.digits = 0;
  dec->v.len = 0;
  dec->v.alloc = DEC_MINALLOC;
  dec->v.data = dec->data;
  dec->s = NULL;
  luaL_setmetatable(L, LUA_DECIMALHANDLE);
  return dec;
}


static DecimalState * convert (lua_State *L, int idx) {
  const char *s = NULL;
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
  DecimalState *dec = create(L);
  uint32_t status = 0;
  mpd_qset_string(MPD(dec), s, MAXCTX(L), &status);
  dec_addstatus(L, status);
  return dec;
}


static DecimalState * todecimal (lua_State *L, int idx) {
  DecimalState *dec = (DecimalState *) luaL_testudata(L, idx, LUA_DECIMALHANDLE);
  if (dec == NULL)
    dec = convert(L, idx);
  return dec;
}


static int dec_unary (lua_State *L, void (*func) (mpd_t *, const mpd_t *, const mpd_context_t *, uint32_t *)) {
  DecimalState *a = todecimal(L, 1);
  DecimalState *r = create(L);
  uint32_t status = 0;
  func(MPD(r), MPD(a), CTX(L), &status);
  dec_addstatus(L, status);
  return 1;
}


static int dec_binary (lua_State *L, void (*func) (mpd_t *, const mpd_t *, const mpd_t *, const mpd_context_t *, uint32_t *)) {
  DecimalState *a = todecimal(L, 1);
  DecimalState *b = todecimal(L, 2);
  DecimalState *r = create(L);
  uint32_t status = 0;
  func(MPD(r), MPD(a), MPD(b), CTX(L), &status);
  dec_addstatus(L, status);
  return 1;
}


#define UNARY_OP(FUNCNAME) \
static int dec_##FUNCNAME (lua_State *L) { \
  return dec_unary(L, FUNCNAME); \
}


#define BINARY_OP(FUNCNAME) \
static int dec_##FUNCNAME (lua_State *L) { \
  return dec_binary(L, FUNCNAME); \
}


#define COMPARE(FUNCNAME,SIGN) \
static int dec_##FUNCNAME (lua_State *L) { \
  DecimalState *a = todecimal(L, 1); \
  DecimalState *b = todecimal(L, 2); \
  uint32_t status = 0; \
  lua_pushboolean(L, (mpd_qcmp(MPD(a), MPD(b), &status) SIGN 0)); \
  dec_addstatus(L, status); \
  return 1; \
}


#define BOOL_VAL(FUNCNAME) \
static int dec_##FUNCNAME (lua_State *L) { \
  DecimalState *dec = todecimal(L, 1); \
  lua_pushboolean(L, FUNCNAME(MPD(dec))); \
  return 1; \
}

static int dec_new (lua_State *L) {
  luaL_checkany(L, 1);
  DecimalState *dec = (DecimalState *) luaL_testudata(L, 1, LUA_DECIMALHANDLE);
  if (dec) {
    DecimalState *r = create(L);
    uint32_t status = 0;
    mpd_qcopy(MPD(r), MPD(dec), &status);
    dec_addstatus(L, status);
  } else
    convert(L, 1);
  return 1;
}


static int dec_context (lua_State *L) {
  mpd_context_t decctx;
  if (lua_gettop(L) > 0) {
    if (lua_type(L, 1) == LUA_TTABLE) {
      mpd_context_t *ctx = CTX(L);
      lua_pushvalue(L, 1);
      lua_pushnil(L);
      while (lua_next(L, -2)) {
        if (lua_type(L, -2) != LUA_TSTRING)
          luaL_error(L, _("<name> expected (string)"));
        if (lua_type(L, -1) != LUA_TSTRING && lua_type(L, -1) != LUA_TNUMBER)
          luaL_error(L, _("<value> expected (string or number)"));
        const char *s = lua_tostring(L, -2);
        if (strcmp(s, "prec") == 0)
          ctx->prec = (mpd_ssize_t) lua_tointeger(L, -1);
        else if (strcmp(s, "emax") == 0)
          ctx->emax = (mpd_ssize_t) lua_tointeger(L, -1);
        else if (strcmp(s, "emin") == 0)
          ctx->emin = (mpd_ssize_t) lua_tointeger(L, -1);
        else
            luaL_error(L, _("unknown option '%s'"), s);
        /*else if (strcmp(s, "traps") == 0)*/
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
      return 0;
    } else if (lua_type(L, 1) == LUA_TSTRING) {
      const char *s = lua_tostring(L, 1);
      if (strcmp(s, "default") == 0)
        decctx = dflt_ctx;
      else if (strcmp(s, "max") == 0)
        mpd_maxcontext(&decctx);
      else if (strcmp(s, "basic") == 0)
        mpd_basiccontext(&decctx);
      else if (strcmp(s, "ieee") == 0)
        mpd_ieee_context(&decctx, (int) luaL_checkinteger(L, 2));
      else
        luaL_error(L, _("unknown argument '%s'"), s);
    } else
      luaL_argerror(L, 1, _("string or table expected"));
  } else
    decctx = *CTX(L);
  lua_newtable(L);
  lua_pushstring(L, "prec");
  lua_pushinteger(L, decctx.prec);
  lua_rawset(L, -3);
  lua_pushstring(L, "emax");
  lua_pushinteger(L, decctx.emax);
  lua_rawset(L, -3);
  lua_pushstring(L, "emin");
  lua_pushinteger(L, decctx.emin);
  lua_rawset(L, -3);
  return 1;
}


BINARY_OP(mpd_qadd)
BINARY_OP(mpd_qsub)
BINARY_OP(mpd_qmul)
BINARY_OP(mpd_qdiv)


/* fused-multiply-add: a * b + c */
static int dec_mpd_qfma (lua_State *L) {
  DecimalState *a = todecimal(L, 1);
  DecimalState *b = todecimal(L, 2);
  DecimalState *c = todecimal(L, 3);
  DecimalState *r = create(L);
  uint32_t status = 0;
  mpd_qfma(MPD(r), MPD(a), MPD(b), MPD(c), CTX(L), &status);
  dec_addstatus(L, status);
  return 1;
}


BINARY_OP(mpd_qdivint)
BINARY_OP(mpd_qrem)

/* Set q and r to the integer part and remainder of a / b */
static int dec_mpd_qdivmod (lua_State *L) {
  DecimalState *a = todecimal(L, 1);
  DecimalState *b = todecimal(L, 2);
  DecimalState *q = create(L);
  DecimalState *r = create(L);
  uint32_t status = 0;
  mpd_qdivmod(MPD(q), MPD(r), MPD(a), MPD(b), CTX(L), &status);
  dec_addstatus(L, status);
  return 2;
}


UNARY_OP(mpd_qexp)
UNARY_OP(mpd_qln)
UNARY_OP(mpd_qlog10)

BINARY_OP(mpd_qpow)

UNARY_OP(mpd_qsqrt)
UNARY_OP(mpd_qinvroot)

UNARY_OP(mpd_qminus)
UNARY_OP(mpd_qabs)


COMPARE(eq, ==)
COMPARE(lt, <)
COMPARE(le, <=)


/* Set result to the maximum of a and b */
static int dec_mpd_qmax (lua_State *L) {
  luaL_checkany(L, 2);
  int i, count = lua_gettop(L);
  uint32_t status = 0;
  DecimalState *a = todecimal(L, 1);
  DecimalState *b = todecimal(L, 2);
  DecimalState *r = create(L);
  mpd_qmax(MPD(r), MPD(a), MPD(b), CTX(L), &status);
  dec_addstatus(L, status);
  for (i = 3; i <= count; i++) {
    a = r;
    b = todecimal(L, i);
    r = create(L);
    status = 0;
    mpd_qmax(MPD(r), MPD(a), MPD(b), CTX(L), &status);
    dec_addstatus(L, status);
  }
  return 1;
}


/* Set result to the minimum of a and b */
static int dec_mpd_qmin (lua_State *L) {
  luaL_checkany(L, 2);
  int i, count = lua_gettop(L);
  uint32_t status = 0;
  DecimalState *a = todecimal(L, 1);
  DecimalState *b = todecimal(L, 2);
  DecimalState *r = create(L);
  mpd_qmin(MPD(r), MPD(a), MPD(b), CTX(L), &status);
  dec_addstatus(L, status);
  for (i = 3; i <= count; i++) {
    a = r;
    b = todecimal(L, i);
    r = create(L);
    status = 0;
    mpd_qmin(MPD(r), MPD(a), MPD(b), CTX(L), &status);
    dec_addstatus(L, status);
  }
  return 1;
}


BINARY_OP(mpd_qand)
BINARY_OP(mpd_qor)
BINARY_OP(mpd_qxor)
UNARY_OP(mpd_qinvert)
BINARY_OP(mpd_qshift)
BINARY_OP(mpd_qquantize)
UNARY_OP(mpd_qreduce)
UNARY_OP(mpd_qfloor)
UNARY_OP(mpd_qceil)
UNARY_OP(mpd_qtrunc)

static int dec_round (lua_State *L) {
  DecimalState *a = todecimal(L, 1);
  mpd_ssize_t y = (mpd_ssize_t) luaL_optinteger(L, 2, 0);
  DecimalState *r = create(L);
  mpd_uint_t dq[1] = {1};
  mpd_t q = {MPD_STATIC | MPD_CONST_DATA, 0, 1, 1, 1, dq};
  uint32_t status = 0;
  q.exp = (y == MPD_SSIZE_MIN) ? MPD_SSIZE_MAX : -y;
  mpd_qquantize(MPD(r), MPD(a), &q, CTX(L), &status);
  dec_addstatus(L, status);
  return 1;
}


/* = a * (2 ^ b) */
static int dec_shl (lua_State *L) {
  DecimalState *a = todecimal(L, 1);
  luaL_argcheck(L, mpd_isinteger(MPD(a)), 1, _("expected integer value"));
  DecimalState *b = todecimal(L, 2);
  luaL_argcheck(L, mpd_isinteger(MPD(b)), 2, _("expected integer value"));
  DecimalState *c = create(L);
  uint32_t status = 0;
  mpd_qset_uint(MPD(c), 2, MAXCTX(L), &status);
  dec_addstatus(L, status);
  DecimalState *d = create(L);
  status = 0;
  mpd_qpow(MPD(d), MPD(c), MPD(b), CTX(L), &status);
  dec_addstatus(L, status);
  DecimalState *r = create(L);
  status = 0;
  mpd_qmul(MPD(r), MPD(a), MPD(d), CTX(L), &status);
  dec_addstatus(L, status);
  return 1;
}


/* = a // (2 ^ b) */
static int dec_shr (lua_State *L) {
  DecimalState *a = todecimal(L, 1);
  luaL_argcheck(L, mpd_isinteger(MPD(a)), 1, _("expected integer value"));
  DecimalState *b = todecimal(L, 2);
  luaL_argcheck(L, mpd_isinteger(MPD(b)), 2, _("expected integer value"));
  DecimalState *c = create(L);
  uint32_t status = 0;
  mpd_qset_uint(MPD(c), 2, MAXCTX(L), &status);
  dec_addstatus(L, status);
  DecimalState *d = create(L);
  status = 0;
  mpd_qpow(MPD(d), MPD(c), MPD(b), CTX(L), &status);
  dec_addstatus(L, status);
  DecimalState *r = create(L);
  status = 0;
  mpd_qdivint(MPD(r), MPD(a), MPD(d), CTX(L), &status);
  dec_addstatus(L, status);
  return 1;
}


BOOL_VAL(mpd_isfinite)
BOOL_VAL(mpd_isinfinite)
BOOL_VAL(mpd_isnan)
BOOL_VAL(mpd_isqnan)
BOOL_VAL(mpd_issigned)
BOOL_VAL(mpd_issnan)
BOOL_VAL(mpd_isinteger)


static int dec_tostring (lua_State *L) {
  DecimalState *dec = (DecimalState *) luaL_checkudata(L, 1, LUA_DECIMALHANDLE);
  mpd_ssize_t size;
  if (dec->s) {
    mpd_free(dec->s);
    dec->s = NULL;
  }
  size = mpd_to_sci_size(&dec->s, MPD(dec), 1);
  if (size < 0)
    luaL_error(L, _("not enough memory"));
  lua_pushlstring(L, dec->s, (size_t) size);
  mpd_free(dec->s);
  dec->s = NULL;
  return 1;
}


static int dec_gc (lua_State *L) {
  DecimalState *dec = (DecimalState *) luaL_checkudata(L, 1, LUA_DECIMALHANDLE);
  mpd_del(MPD(dec));
  if (dec->s)
    mpd_free(dec->s);
  return 0;
}


/*
** functions for 'decimal' library
*/
static const luaL_Reg dec_lib[] = {
  {"new", dec_new},
  {"context", dec_context},
  {"shift", dec_mpd_qshift},
  {"quantize", dec_mpd_qquantize},
  {"reduce", dec_mpd_qreduce},
  {"abs", dec_mpd_qabs},
  {"floor", dec_mpd_qfloor},
  {"ceil", dec_mpd_qceil},
  {"trunc", dec_mpd_qtrunc},
  {"round", dec_round},
  {"exp", dec_mpd_qexp},
  {"sqrt", dec_mpd_qsqrt},
  {"invroot", dec_mpd_qinvroot},  /* inverse-square-root */
  {"ln", dec_mpd_qln},
  {"log10", dec_mpd_qlog10},
  {"fma", dec_mpd_qfma},
  {"divmod", dec_mpd_qdivmod},
  {"max", dec_mpd_qmax},
  {"min", dec_mpd_qmin},
  {"isfinite", dec_mpd_isfinite},
  {"isinfinite", dec_mpd_isinfinite},
  {"isnan", dec_mpd_isnan},
  {"isqnan", dec_mpd_isqnan},
  {"issigned", dec_mpd_issigned},
  {"issnan", dec_mpd_issnan},
  {"isinteger", dec_mpd_isinteger},
  {NULL, NULL}
};


/*
** methods for decimal handles
*/
static const luaL_Reg dec_methods[] = {
  {"shift", dec_mpd_qshift},
  {"quantize", dec_mpd_qquantize},
  {"reduce", dec_mpd_qreduce},
  {"abs", dec_mpd_qabs},
  {"floor", dec_mpd_qfloor},
  {"ceil", dec_mpd_qceil},
  {"trunc", dec_mpd_qtrunc},
  {"exp", dec_mpd_qexp},
  {"sqrt", dec_mpd_qsqrt},
  {"invroot", dec_mpd_qinvroot},  /* inverse-square-root */
  {"ln", dec_mpd_qln},
  {"log10", dec_mpd_qlog10},
  {"round", dec_round},
  {"isfinite", dec_mpd_isfinite},
  {"isinfinite", dec_mpd_isinfinite},
  {"isnan", dec_mpd_isnan},
  {"isqnan", dec_mpd_isqnan},
  {"issigned", dec_mpd_issigned},
  {"issnan", dec_mpd_issnan},
  {"isinteger", dec_mpd_isinteger},
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
  {"__not", dec_mpd_qinvert},
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


static int dec_call (lua_State *L) {
  lua_remove(L, 1);
  return dec_new(L);
}


static const luaL_Reg dec_mt[] = {
  {"__call", dec_call},
  {NULL, NULL}
};


static void dec_traphandler(mpd_context_t *ctx) {
  (void) ctx;
}


LUAMOD_API int luaopen_decimal (lua_State *L) {
  static int initialized = 0;
  if (!initialized) {
    mpd_traphandler = dec_traphandler;
    mpd_mallocfunc = malloc;
    mpd_reallocfunc = realloc;
    mpd_callocfunc = mpd_callocfunc_em;
    mpd_free = free;
    mpd_setminalloc(DEC_MINALLOC);
    initialized = 1;
  }
  *CTX(L) = dflt_ctx;
  mpd_maxcontext(MAXCTX(L));
  /* register library */
  luaL_newlib(L, dec_lib);  /* new module */
  luaL_newlib(L, dec_mt);
  lua_setmetatable(L, -2);
  /* create metatable for decimal handles */
  luaL_newmetatable(L, LUA_DECIMALHANDLE);
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, dec_methods, 0);  /* add methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
  /* register PI constant */
  lua_pushcfunction(L, dec_new);
  lua_pushstring(L, "3.141592653589793238462643383279502884");
  lua_call(L, 1, 1);
  lua_setfield(L, -2, "PI");
  /* register todecimal */
  lua_pushcfunction(L, dec_new);
  lua_setglobal(L, "todecimal");
  return 1;
}

#endif