/*
** Serialize and deserialize values
** author: Alexey Smirnov
** 
*/

#ifdef LUAEX_SERIALIZE
#define serialize_c
#define LUA_CORE

#include "lprefix.h"

#include <math.h>

#include "lua.h"
#include "lauxlib.h"
#include "luaex.h"

#define _(x) x


static int writer (lua_State *L, const void *b, size_t size, void *B) {
  (void)L;
  luaL_addlstring((luaL_Buffer *) B, (const char *)b, size);
  return 0;
}


static void serialize (lua_State *L, luaL_Buffer *b, int idx, int top) {
  unsigned char *cur;
  size_t l, i;
  luaL_addchar(b, '{');
  switch (lua_type(L, idx)) {
    case LUA_TNIL:
      luaL_addchar(b, '#');
      break;
	  
    case LUA_TBOOLEAN:
      luaL_addchar(b, 'B');
      luaL_addchar(b, (lua_toboolean(L, idx)) ? '1' : '0');
      break;
	  
    case LUA_TLIGHTUSERDATA:
      luaL_error(L, _("cannot serialize light userdata"));
      break;
	  
    case LUA_TNUMBER: {
      luaL_addchar(b, 'N');
      lua_Number value = lua_tonumber(L, idx);
      if (trunc(value) == value)
        lua_pushfstring(L, "%I", (lua_Integer) value);
      else
        lua_pushfstring(L, "%f", value);
      luaL_addstring(b, lua_tostring(L, -1));
      lua_pop(L, 1);
      break;
    }
	
    case LUA_TSTRING:
      luaL_addchar(b, 'S');
      cur = (unsigned char*) lua_tolstring(L, idx, &l);
      for (i = 0; i < l; i++) {
        luaL_addchar(b, (char) ('A' + (*cur >> 4)));
        luaL_addchar(b, (char) ('A' + (*cur & 0x0F)));
        cur++;
      }
      break;
	  
    case LUA_TTABLE: {
      const void *p = lua_topointer(L, idx);
      lua_pushvalue(L, idx);
      lua_rawget(L, top);
      if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        /* hash table */
        lua_pushvalue(L, idx);
        lua_pushboolean(L, 1);
        lua_rawset(L, top);
      } else {
        lua_pop(L, 1);
        /* table is already, write pointer */
        luaL_addlstring(b, "P(", 2);
        lua_pushinteger(L, (lua_Integer) p);
        serialize(L, b, -1, top);
        lua_pop(L, 1);
        luaL_addchar(b, ')');
        break;
      }
      luaL_addlstring(b, "T(", 2);
      lua_pushinteger(L, (lua_Integer) p);
      serialize(L, b, -1, top);
      lua_pop(L, 1);
      luaL_addlstring(b, ")[", 2);
      if (lua_getmetatable(L, idx)) {
        luaL_addlstring(b, "(#,", 3);
        serialize(L, b, -1, top);
        lua_pop(L, 1);
        luaL_addchar(b, ')');
      }
      /* iterate pairs */
      lua_pushvalue(L, idx);
      lua_pushnil(L);
      while (lua_next(L, -2)) {
        luaL_addchar(b, '(');
        serialize(L, b, -2, top);
        luaL_addchar(b, ',');
        serialize(L, b, -1, top);
        luaL_addchar(b, ')');
        lua_pop(L, 1);
      }
      luaL_addchar(b, ']');
      lua_pop(L, 1);
      break;
    }
    case LUA_TFUNCTION: {
      luaL_addlstring(b, "F(", 2);
      luaL_Buffer bb;
      luaL_buffinit(L, &bb);
      lua_pushvalue(L, idx);
      if (lua_dump(L, writer, &bb, 0))
		  luaL_error(L, _("cannot serialize function"));
      luaL_pushresult(&bb);
      serialize(L, b, -1, top);
      lua_pop(L, 2);
      luaL_addchar(b, ')');
      break;
    }
    case LUA_TUSERDATA:
      /*if (lua_getmetatable(L, idx)) {
        lua_getfield(L, -1, "__serialize");
        if (lua_isfunction(L, -1)) {
          lua_pushvalue(L, idx);
          lua_call(L, 1, 1);
          luaL_addlstring(b, "O(", 2);
          serialize(L, b, -1, top);
          luaL_addchar(b, ')');
          lua_pop(L, 3);
          break;
        }
        lua_pop(L, 2);
      }*/
      luaL_error(L, _("cannot serialize userdata"));
      break;
	  
    case LUA_TTHREAD:
      luaL_error(L, _("cannot serialize thread"));
      break;
	  
    default:
      luaL_error(L, _("cannot serialize"));
      break;
  }
  luaL_addchar(b, '}');
}


LUA_API void lua_serialize (lua_State *L, int idx) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  lua_newtable(L);
  int top = lua_gettop(L);
  serialize(L, &b, idx, top);
  lua_remove(L, top);
  luaL_pushresult(&b);
}


static void check (lua_State *L, int cond, const char *v) {
  (cond) || luaL_error(L, _("serialized data malformed, expected '%s'"), v);
}


static void checkavail (lua_State *L, int cond) {
  (cond) || luaL_error(L, _("serialized data break"));
}


static void deserialize (lua_State *L, const char *s, const char *e, int top) {
  while (s < e) {
    check(L, *s++ == '{', "{");
    checkavail(L, s < e);
    const char c = *s++;
    checkavail(L, s < e);
    switch (c) {
      case '#': /* nil */
        lua_pushnil(L);
        break;
      case 'B': /* boolean */
        check(L, *s == '0' || *s == '1', _("0 or 1"));
        lua_pushboolean(L, (*s++ == '1') ? 1 : 0);
        break;
      case 'N': { /* number */
        const char n = (*s == '-') ? *s++ : 0;
        checkavail(L, s < e);
        check(L, *s >= '0' && *s <= '9', "0-9");
        lua_Integer v1 = 0;
        lua_Number v2 = 0;
        while (*s >= '0' && *s <= '9') {
          v1 *= 10;
          v1 += (*s++ - '0');
          checkavail(L, s < e);
        }
        if (*s == '.') {
          s++;
          checkavail(L, s < e);
          lua_Number d = 10;
          while (*s >= '0' && *s <= '9') {
            v2 += (*s++ - '0') / d;
            d *= 10;
            checkavail(L, s < e);
          }
        }
        if (v2 != 0) {
          v2 += v1;
          lua_pushnumber(L,  (n == '-') ? -v2 : v2);
        } else
          lua_pushinteger(L, (n == '-') ? -v1 : v1);
        break;
      }
      case 'S': /* string */
        if (*s != '}') {
          check(L, *s >= 'A' && *s <= 'P', "A-P");
          luaL_Buffer b;
          luaL_buffinit(L, &b);
          while (*s >= 'A' && *s <= 'P') {
            const char c1 = *s++; checkavail(L, s < e);
            const char c2 = *s++; checkavail(L, s < e);
            check(L, c2 >= 'A' && c2 <= 'P', "A-P");
            luaL_addchar(&b, ((c1 - 'A') << 4) | (c2 - 'A'));
          }
          luaL_pushresult(&b);
        } else
          lua_pushstring(L, "");
        break;
      case 'T': { /* table */
        check(L, *s == '(', "(");
        checkavail(L, ++s < e);
        const char *p = s;
        while (*s != ')') checkavail(L, ++s < e);
        lua_newtable(L);
        /* hash table */
        lua_pushlstring(L, p, s - p);
        lua_pushvalue(L, -2);
        lua_rawset(L, top);
        /**/
        checkavail(L, ++s < e);
        check(L, *s == '[', "[");
        checkavail(L, ++s < e);
        check(L, *s == '(' || *s == ']', _("( or ]"));
        while (*s == '(') {
          checkavail(L, ++s < e);
          const char *s1 = s;
          const char *s2 = NULL;
          int k = 1;
          for (;;) {
            switch (*s) {
              case '(':
                k++;
                break;
              case ',':
                if (k == 1) {
                  checkavail(L, ++s < e);
                  check(L, *s == '{', "{");
                  s2 = s;
                }
                break;
              case ')':
                k--;
                break;
            }
            if (k == 0)
              break;
            checkavail(L, ++s < e);
          }
          check(L, s2 != NULL, ",");
          if (*s1 == '#') {
            deserialize(L, s2, s, top);
            lua_setmetatable(L, -2);
          } else {
            deserialize(L, s1, s2 - 1, top);
            deserialize(L, s2, s, top);
            lua_settable(L, -3);
          }
          checkavail(L, ++s < e);
        }
        check(L, *s++ == ']', "]");
        break;
      }
      case 'F': { /* function */
        check(L, *s == '(', "(");
        checkavail(L, ++s < e);
        const char *p = s;
        while (*s != ')') checkavail(L, ++s < e);
        deserialize(L, p, s, top);
        size_t l;
        const char *v = lua_tolstring(L, -1, &l);
        (luaL_loadbuffer(L, v, l, "") == LUA_OK) || lua_error(L);
        lua_remove(L, -2);
        checkavail(L, ++s < e);
        break;
      }
      case 'P': {
        check(L, *s == '(', "(");
        checkavail(L, ++s < e);
        const char *p = s;
        while (*s != ')') checkavail(L, ++s < e);
        lua_pushlstring(L, p, s - p);
        lua_rawget(L, top);
        checkavail(L, ++s < e);
        break;
      }
      default:
        luaL_error(L, _("type '%c' malformed"), c);
        break;
    }
    checkavail(L, s < e);
    check(L, *s++ == '}', "}");
  }
}


LUA_API void lua_deserialize (lua_State *L, int idx) {
  size_t l;
  const char *s = luaL_checklstring(L, idx, &l);
  lua_newtable(L);
  int top = lua_gettop(L);
  deserialize(L, s, s + l, top);
  lua_remove(L, top);
}
#endif