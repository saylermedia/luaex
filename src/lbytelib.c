/*
** Byte (binary data) library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#define bytelib_c
#define LUA_LIB

#include "lprefix.h"

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#ifdef LUAEX_BYTE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>


#define LUA_BYTEHANDLE "SOCKET*"


typedef struct ByteState {
  unsigned char *data;
  size_t size;
} ByteState;


static int byte_alloc (lua_State *L) {
  luaL_checkany(L, 1);
  ByteState *byte = (ByteState *) lua_newuserdata(L, sizeof(ByteState));
  size_t l;
  if (lua_type(L, 1) == LUA_TNUMBER) {
    l = (size_t) lua_tointeger(L, 1);
    byte->data = (unsigned char *) malloc(l);
    if (byte->data == NULL)
      luaL_error(L, _("not enough memory"));
  } else if (lua_type(L, 1) == LUA_TSTRING) {
    const char *s = lua_tolstring(L, 1, &l);
    if (l > 0) {
      byte->data = (unsigned char *) malloc(l);
      if (byte->data)
        memcpy(byte->data, s, l);
      else
        luaL_error(L, _("not enough memory"));
    }
  } else
    luaL_error(L, _("string or integer expected"));
  byte->size = l;
  luaL_setmetatable(L, LUA_BYTEHANDLE);
  return 1;
}


static int byte_resize (lua_State *L) {
  ByteState *byte = (ByteState *) luaL_checkudata(L, 1, LUA_BYTEHANDLE);
  size_t size = (size_t) luaL_checkinteger(L, 2);
  if (byte->size != size) {
    if (size == 0) {
      if (byte->data) {
        free(byte->data);
        byte->data = NULL;
        byte->size = 0;
      }
    } else {
      char *data = (unsigned char *) realloc(byte->data, size);
      if (data == NULL)
        luaL_error(L, _("not enough memory"));
      byte->size = size;
      byte->data = data;
    }
  }
  return 0;
}


static int byte_clear (lua_State *L) {
  ByteState *byte = (ByteState *) luaL_checkudata(L, 1, LUA_BYTEHANDLE);
  if (byte->data) {
    free(byte->data);
    byte->data = NULL;
    byte->size = 0;
  }
  return 0;
}


static int byte_size (lua_State *L) {
  ByteState *byte = (ByteState *) luaL_checkudata(L, 1, LUA_BYTEHANDLE);
  lua_pushinteger(L, byte->size);
  return 1;
}


static int byte_add (lua_State *L) {
  ByteState *byte1 = (ByteState *) luaL_checkudata(L, 1, LUA_BYTEHANDLE);
  ByteState *byte2 = (ByteState *) luaL_checkudata(L, 2, LUA_BYTEHANDLE);
  ByteState *byte = (ByteState *) lua_newuserdata(L, sizeof(ByteState));
  byte->size = byte1->size + byte2->size;
  byte->data = (unsigned char *) malloc(byte->size);
  if (byte->data) {
    memcpy(byte->data, byte1->data, byte1->size);
    memcpy(&byte->data[byte1->size], byte2->data, byte2->size);
  } else
    luaL_error(L, _("not enough memory"));
  luaL_setmetatable(L, LUA_BYTEHANDLE);
  return 1;
}


static int byte_index (lua_State *L) {
  ByteState *byte = (ByteState *) luaL_checkudata(L, 1, LUA_BYTEHANDLE);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    size_t i = (size_t) lua_tointeger(L, 2);
    if (i == 0 || i > byte->size)
      luaL_error(L, _("index out of range"));
    lua_pushinteger(L, byte->data[i - 1]);
  } else {
    luaL_getmetatable(L, LUA_BYTEHANDLE);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    lua_remove(L, -2);
  }
  return 1;
}


static int byte_newindex (lua_State *L) {
  ByteState *byte = (ByteState *) luaL_checkudata(L, 1, LUA_BYTEHANDLE);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    size_t i = (size_t) lua_tointeger(L, 2);
    if (i == 0 || i > byte->size)
      luaL_error(L, _("index out of range"));
    if (lua_type(L, 3) == LUA_TSTRING) {
      const char *s = lua_tostring(L, 3);
      byte->data[i - 1] = (unsigned char) ((s) ? s[0] : 0);
    } else
      byte->data[i - 1] = (unsigned char) luaL_checkinteger(L, 3);
  } else {
    luaL_getmetatable(L, LUA_BYTEHANDLE);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, -3);
  }
  return 0;
}


static int byte_tostring (lua_State *L) {
  ByteState *byte = (ByteState *) luaL_checkudata(L, 1, LUA_BYTEHANDLE);
  lua_pushlstring(L, byte->data, byte->size);
  return 1;
}


static int byte_gc (lua_State *L) {
  ByteState *byte = (ByteState *) luaL_checkudata(L, 1, LUA_BYTEHANDLE);
  if (byte->data)
    free(byte->data);
  return 1;
}


/*
** functions for 'byte' library
*/
static const luaL_Reg byte_lib[] = {
  {"alloc", byte_alloc},
  {"resize", byte_resize},
  {"clear", byte_clear},
  {"size", byte_size},
  {NULL, NULL}
};


/*
** methods for byte handles
*/
static const luaL_Reg byte_methods[] = {
  {"resize", byte_resize},
  {"size", byte_size},
  {"__len", byte_size},
  {"__add", byte_add},
  {"__concat", byte_add},
  {"__index", byte_index},
  {"__newindex", byte_newindex},
  {"__tostring", byte_tostring},
  {"__serialize", byte_tostring},
  {"__deserialize", byte_alloc},
  {"__gc", byte_gc},
  {NULL, NULL}
};


LUAMOD_API int luaopen_byte (lua_State *L) {
  /* register library */
  luaL_newlib(L, byte_lib);  /* new module */
  /* create metatable for byte handles */
  luaL_newmetatable(L, LUA_BYTEHANDLE);
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, byte_methods, 0);  /* add methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
  return 1;
}


LUA_API unsigned char * lua_byte (lua_State *L, int idx, size_t *l) {
  ByteState *byte = (ByteState *) luaL_testudata(L, idx, LUA_BYTEHANDLE);
  if (byte) {
    *l = byte->size;
    return byte->data;
  }
  return NULL;
}

#endif