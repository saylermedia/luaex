/*
** Zlib library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#define zlib_c
#define LUA_LIB

#include "lprefix.h"

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#ifdef LUAEX_ZLIB
#include <string.h>
#include <zlib.h>

#define ZLIB_ENCODING_GZIP		0x1f
#define ZLIB_ENCODING_DEFLATE	0x0f


static int zlib_error (lua_State *L, const char *func, int ret)
{
  switch (ret)
  {
  case Z_STREAM_ERROR:
    return luaL_error(L, _("%s(): invalid compression level"), func);

  case Z_DATA_ERROR:
    return luaL_error(L, _("%s(): invalid or incomplete deflate data"), func);

  case Z_MEM_ERROR:
    return luaL_error(L, _("%s(): not enough memory"), func);

  case Z_VERSION_ERROR:
    return luaL_error(L, _("%s(): zlib version mismatch"), func);

  default:
    return luaL_error(L, _("%s(): unknown error %d"), func, ret);
  }
}


static int zlib_inflate (lua_State *L)
{
  size_t avail_in;
  unsigned char *next_in = (unsigned char *) luaL_checklstring(L, 1, &avail_in);
  int encoding = (int) luaL_optinteger(L, 2, ZLIB_ENCODING_DEFLATE);

  z_stream strm;
  memset(&strm, 0, sizeof(z_stream));

  int ret = inflateInit2(&strm, encoding);
  if (ret != Z_OK)
    return zlib_error(L, "inflateInit2", ret);

  strm.next_in = next_in;
  strm.avail_in = avail_in;
  
  luaL_Buffer b;
  luaL_buffinit(L, &b);

  for (;;)
  {
    strm.next_out = (unsigned char *) luaL_prepbuffer(&b);
    strm.avail_out = LUAL_BUFFERSIZE;
    
    ret = inflate(&strm, Z_FINISH);
    luaL_addsize(&b, LUAL_BUFFERSIZE - strm.avail_out);

    if (ret == Z_STREAM_END)
      break;
    else if (ret != Z_OK && ret != Z_BUF_ERROR)
    {
      inflateEnd(&strm);
      return zlib_error(L, "inflate", ret);
    }
  }

  inflateEnd(&strm);
  luaL_pushresult(&b);
  return 1;
}


static int zlib_deflate (lua_State *L)
{
  size_t avail_in;
  unsigned char *next_in = (unsigned char *) luaL_checklstring(L, 1, &avail_in);
  int encoding = (int) luaL_optinteger(L, 2, ZLIB_ENCODING_DEFLATE);
  int level = (int) luaL_optinteger(L, 3, Z_DEFAULT_COMPRESSION);

  z_stream strm;
  memset(&strm, 0, sizeof(z_stream));

  int ret = deflateInit2(&strm, level, Z_DEFLATED, encoding, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
  if (ret != Z_OK)
    return zlib_error(L, "deflateInit2", ret);

  strm.next_in = next_in;
  strm.avail_in = avail_in;

  luaL_Buffer b;
  luaL_buffinit(L, &b);

  for (;;)
  {
    strm.next_out = (unsigned char *) luaL_prepbuffer(&b);
    strm.avail_out = LUAL_BUFFERSIZE;

    ret = deflate(&strm, Z_FINISH);
    luaL_addsize(&b, LUAL_BUFFERSIZE - strm.avail_out);

    if (ret == Z_STREAM_END)
      break;
    else if (ret != Z_OK)
    {
      deflateEnd(&strm);
      return zlib_error(L, "deflate", ret);
    }
  }

  deflateEnd(&strm);
  luaL_pushresult(&b);
  return 1;
}


static int zlib_adler32 (lua_State *L)
{
  size_t l;
  unsigned char *value = (unsigned char *) luaL_checklstring(L, 1, &l);
  lua_pushinteger(L, adler32(lua_gettop(L) > 1 ? (uLong) luaL_checkinteger(L, 2) : adler32(0L, Z_NULL, 0), value, l));
  return 1;
}


static int zlib_crc32 (lua_State *L)
{
  size_t l;
  unsigned char *value = (unsigned char *) luaL_checklstring(L, 1, &l);
  lua_pushinteger(L, crc32(lua_gettop(L) > 1 ? (uLong) luaL_checkinteger(L, 2) : crc32(0L, Z_NULL, 0), value, l));
  return 1;
}


/*
** functions for 'zlib' library
*/
static const luaL_Reg zlib_lib[] =
{
  {"inflate", zlib_inflate},
  {"deflate", zlib_deflate},
  {"adler32", zlib_adler32},
  {"crc32",   zlib_crc32},
  {NULL, NULL}
};


LUAMOD_API int luaopen_zlib (lua_State *L) {
  /* register library */
  luaL_newlib(L, zlib_lib);  /* new module */
  
  lua_pushinteger(L, Z_NO_COMPRESSION);
  lua_setfield(L, -2, "NO_COMPRESSION");

  lua_pushinteger(L, Z_BEST_SPEED);
  lua_setfield(L, -2, "BEST_SPEED");

  lua_pushinteger(L, Z_BEST_COMPRESSION);
  lua_setfield(L, -2, "BEST_COMPRESSION");

  lua_pushinteger(L, ZLIB_ENCODING_DEFLATE);
  lua_setfield(L, -2, "ENCODING_DEFLATE");

  lua_pushinteger(L, ZLIB_ENCODING_GZIP);
  lua_setfield(L, -2, "ENCODING_GZIP");

  lua_pushstring(L, zlibVersion());
  lua_setfield(L, -2, "_VERSION");
  return 1;
}


#endif