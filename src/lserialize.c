/*
** Serialize / Deserialize library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#define serialize_c
#define LUA_CORE

#include "lprefix.h"

#include "lua.h"
#include "lauxlib.h"


#ifdef LUAEX_SERIALIZE
#include <string.h>
#include <math.h>
#include <malloc.h>

#define GROW_POINTER 64

struct StringBuilder {
  char *b;
  size_t size;
  size_t n;
  void **p;
  size_t psize;
  size_t pn;
  lua_State *L;
};


static int bgc (lua_State *L) {
  struct StringBuilder *B = (struct StringBuilder *) lua_touserdata(L, 1);
  if (B->b)
    free(B->b);
  if (B->p)
    free(B->p);
  return 0;
}


static struct StringBuilder * newbuilder (lua_State *L) {
  struct StringBuilder *B = (struct StringBuilder *) lua_newuserdata(L, sizeof(struct StringBuilder));
  B->b = NULL;
  B->size = 0;
  B->n = 0;
  B->p = NULL;
  B->psize = 0;
  B->pn = 0;
  B->L = L;
  if (luaL_newmetatable(L, "StringBuilder")) {
    lua_pushcfunction(L, bgc);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);
  return B;
}


static void b_grow (struct StringBuilder *B, size_t sz) {
  if (B->size - B->n < sz) {
    size_t newsize = B->n + sz + LUAL_BUFFERSIZE;
    char *temp = (char *) realloc(B->b, newsize);
    if (temp == NULL)
      luaL_error(B->L, _("not enough memory"));
    B->b = temp;
    B->size = newsize;
  }
}


#define b_addlstring(B,s,sz) (b_grow(B, sz), memcpy(&B->b[B->n], s, sz), B->n += sz)
#define b_addchar(B,c) (b_grow(B, 1), (B)->b[(B)->n++] = c)


static int b_hashing (struct StringBuilder *B, const void *hashptr) {
  size_t i;
  for (i = 0; i < B->pn; i++) {
    if (B->p[i] == hashptr)
      return 1;
  }
  if (B->psize - B->pn < 1) {
    size_t newsize = B->pn + GROW_POINTER;
    void **temp = (void **) realloc(B->p, newsize * sizeof(void *));
    if (temp == NULL)
      luaL_error(B->L, _("not enough memory"));
    B->p = temp;
    B->psize = newsize;
  }
  B->p[B->pn++] = (void *) hashptr;
  return 0;
}


static int writer (lua_State *L, const void *b, size_t size, void *B) {
  (void) L;
  unsigned char *s = (unsigned char *) b;
  unsigned char *e = s + size;
  for (; s < e; s++) {
    b_addchar((struct StringBuilder *) B, (char) ('A' + (*s >> 4)));    /* high 4 bits as A-P char */
    b_addchar((struct StringBuilder *) B, (char) ('A' + (*s & 0x0F)));  /* low 4 bits as A-P char */
  }
  return 0;
}


static void serialize (lua_State *L, struct StringBuilder *b, int idx) {
  b_addchar(b, '{');
  switch (lua_type(L, idx)) {
    case LUA_TNIL:
      /* serialize nil */
      b_addchar(b, '#');
      break;
	  
    case LUA_TBOOLEAN:
      /* serialize boolean */
      b_addchar(b, 'B');
      b_addchar(b, (lua_toboolean(L, idx)) ? '1' : '0');
      break;
	  
    case LUA_TNUMBER: {
      /* serialize number */
      lua_Number number = lua_tonumber(L, idx);
      if (trunc(number) == number) {
        /* serialize as integer */
        b_addchar(b, 'I');
        lua_pushfstring(L, "%I", lua_tointeger(L, idx));
      } else {
        /* serialize as float */
        b_addchar(b, 'N');
        lua_pushfstring(L, "%f", number);
      }
      size_t l;
      const char *s = lua_tolstring(L, -1, &l);
      b_addlstring(b, s, l);
      lua_pop(L, 1);
      break;
    }
	
    case LUA_TSTRING:
      /* serialize string as binary */
      b_addchar(b, 'S');
      size_t l;
      unsigned char *s = (unsigned char *) lua_tolstring(L, idx, &l);
      unsigned char *e = s + l;
      for (; s < e; s++) {
        b_addchar(b, (char) ('A' + (*s >> 4)));    /* high 4 bits as A-P char */
        b_addchar(b, (char) ('A' + (*s & 0x0F)));  /* low 4 bits as A-P char */
      }
      break;
	  
    case LUA_TTABLE: {
      /* serialize table */
      const void *p = lua_topointer(L, idx);
      if (b_hashing(b, p)) {
        b_addchar(b, 'P');
        lua_pushinteger(L, (lua_Integer) p);
        serialize(L, b, -1);
        lua_pop(L, 1);
        break;
      }
      b_addchar(b, 'T');
      /* serialize table pointer as identificator */
      lua_pushinteger(L, (lua_Integer) p);
      serialize(L, b, -1);
      lua_pop(L, 1);
      /* serialize metatable */
      if (lua_getmetatable(L, idx)) {
        b_addchar(b, '#');
        serialize(L, b, -1);
        lua_pop(L, 1);
      }
      /* serialize entries */
      lua_pushvalue(L, idx);
      lua_pushnil(L);
      while (lua_next(L, -2)) {
        serialize(L, b, -2);
        serialize(L, b, -1);
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
      break;
    }
    case LUA_TFUNCTION: {
      /* serialize function */
      const void *p = lua_topointer(L, idx);
      if (b_hashing(b, p)) {
        b_addchar(b, 'P');
        lua_pushinteger(L, (lua_Integer) p);
        serialize(L, b, -1);
        lua_pop(L, 1);
        break;
      }
      b_addlstring(b, "F{S", 3);
      /* dump function */
      lua_pushvalue(L, idx);
      if (lua_dump(L, writer, b, 0))
        luaL_error(L, _("cannot serialize function"));
      lua_pop(L, 1);
      b_addchar(b, '}');
      /* serialize pointer */
      lua_pushinteger(L, (lua_Integer) p);
      serialize(L, b, -1);
      lua_pop(L, 1);
      /* serialize upvalues */
      int i;
      const char *name;
      for (i = 1; (name = lua_getupvalue(L, idx, i)); i++) {
        /* skip global table _G */
        if (strcmp(name, "_ENV") != 0)
          serialize(L, b, -1);
        else
          b_addchar(b, '#');
        lua_pop(L, 1);
      }
      break;
    }
    case LUA_TUSERDATA:
      /* serialize userdata if possible */
      if (luaL_getmetafield(L, idx, "__serialize")) {
        /* get metatable name */
        if (luaL_getmetafield(L, idx, "__name")) {
          b_addchar(b, 'U');
          /* serialize metatable name */
          serialize(L, b, -1);
          lua_pop(L, 1);
          /* call method __serialize */
          lua_pushvalue(L, idx);
          lua_call(L, 1, 1);
          /* serialize result */
          serialize(L, b, -1);
          lua_pop(L, 1);
        } else
          lua_pop(L, 1);
        break;
      }
	  
    default:
      luaL_error(L, _("cannot serialize %s"), lua_typename(L, lua_type(L, idx)));
      break;
  }
  b_addchar(b, '}');
}


LUA_API void lua_serialize (lua_State *L, int idx, int lastidx) {
  /* to absolute index */
  idx = lua_absindex(L, idx);
  lastidx = lua_absindex(L, lastidx);
  /* initialize buffer with hash pointers */
  struct StringBuilder *B = newbuilder(L);
  int top = lua_gettop(L);
  /* serialize values */
  if (lastidx >= idx) {
    for (; idx <= lastidx; idx++)
      serialize(L, B, idx);
  } else {
    for (; idx >= lastidx; idx--)
      serialize(L, B, idx);
  }
  /* push serialized as string */
  lua_pushlstring(B->L, B->b, B->n);
  lua_remove(L, top);
}


#define check(L,cond,v) if (!(cond)) luaL_error(L, _("serialized data malformed, expected '%s'"))
#define checkavail(L,cond) if (!(cond)) luaL_error(L, _("serialized data break"))


static const char* nextclose (lua_State *L, const char *s, const char *e) {
  check(L, *s == '{', "{");
  int nrep = 0;
  for (;;) {
    checkavail(L, ++s < e);
    if (*s == '}')
      if (nrep == 0)
        break;
      else
        nrep--;
    else if (*s == '{')
      nrep++;
  }
  return s + 1;
}

static int deserialize (lua_State *L, const char *s, const char *e, int top) {
  int nresults = 0;
  const char *p;
  int neg;
  while (s < e) {
    nresults++;
    check(L, *s++ == '{', "{");
    checkavail(L, s < e);
    const char c = *s++;
    checkavail(L, s < e);
    switch (c) {
      case '#':
        /* deserialize nil */
        lua_pushnil(L);
        break;
    
      case 'B':
        /* deserialize boolean */
        check(L, *s == '0' || *s == '1', _("0 or 1"));
        lua_pushboolean(L, (*s++ == '1') ? 1 : 0);
        break;
      
      case 'I': {
        /* deserialize integer */
        neg = (*s == '-') ? *s++ : 0;
        checkavail(L, s < e);
        check(L, *s >= '0' && *s <= '9', "0-9");
        lua_Integer number = 0;
        while (*s >= '0' && *s <= '9') {
          number *= 10;
          number += (*s++ - '0');
          checkavail(L, s < e);
        }
        lua_pushinteger(L, (neg == '-') ? -number : number);
        break;
      }
      case 'N': {
        /* deserialize float */
        neg = (*s == '-') ? *s++ : 0;
        checkavail(L, s < e);
        check(L, *s >= '0' && *s <= '9', "0-9");
        lua_Number number = 0;
        while (*s >= '0' && *s <= '9') {
          number *= 10;
          number += (*s++ - '0');
          checkavail(L, s < e);
        }
        if (*s == '.') {
          checkavail(L, ++s < e);
          lua_Number delim = 10;
          while (*s >= '0' && *s <= '9') {
            number += (*s++ - '0') / delim;
            delim *= 10;
            checkavail(L, s < e);
          }
        }
        lua_pushnumber(L, (neg == '-') ? -number : number);
        break;
      }
      case 'S':
        /* deserialize string */
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
      
      case 'T': {
        /* deserialize table */
        p = s; s = nextclose(L, s, e);
        lua_newtable(L);
        /* hash table */
        lua_pushlstring(L, p, s - p);
        lua_pushvalue(L, -2);
        lua_rawset(L, top);
        /* deserialize metatable if exists */
        if (*s == '#') {
          checkavail(L, ++s < e);
          p = s; s = nextclose(L, s, e);
          deserialize(L, p, s, top);
          lua_setmetatable(L, -2);
        }
        /* deserialize entries */
        while (*s == '{') {
          p = s; s = nextclose(L, s, e);
          deserialize(L, p, s, top);      /* deserialize key */
          p = s; s = nextclose(L, s, e);
          deserialize(L, p, s, top);      /* deserialize value */
          lua_rawset(L, -3);              /* [key] = value */
        }
        break;
      }
      case 'F': {
        /* deserialize function */
        p = s; s = nextclose(L, s, e);
        deserialize(L, p, s, top);
        size_t l;
        p = lua_tolstring(L, -1, &l);
        if (luaL_loadbuffer(L, p, l, ""))
          lua_error(L);
        lua_remove(L, -2);
        /* hash function */
        p = s; s = nextclose(L, s, e);
        lua_pushlstring(L, p, s - p);
        lua_pushvalue(L, -2);
        lua_rawset(L, top);
        /* deserialize upvalues */
        int n = 1;
        for (;;) {
          if (*s == '#') {
            n++;
            checkavail(L, ++s < e);
          } else if (*s == '{') {
            p = s; s = nextclose(L, s, e);
            deserialize(L, p, s, top);
            lua_setupvalue(L, -2, n++);
          } else
            break;
        }
        break;
      }
      case 'P': {
        /* deserialize hash pointer */
        p = s; s = nextclose(L, s, e);
        lua_pushlstring(L, p, s - p);
        lua_rawget(L, top);
        break;
      }
      case 'U': {
        /* deserialize userdata */
        p = s; s = nextclose(L, s, e);
        deserialize(L, p, s, top);
        lua_getfield(L, LUA_REGISTRYINDEX, lua_tostring(L, -1));
        lua_remove(L, -2);
        lua_getfield(L, -1, "__deserialize");
        lua_remove(L, -2);
        p = s; s = nextclose(L, s, e);
        deserialize(L, p, s, top);
        lua_call(L, 1, 1);
        break;
      }
      default:
        luaL_error(L, _("type '%c' malformed"), c);
        break;
    }
    checkavail(L, s < e);
    check(L, *s++ == '}', "}");
  }
  return nresults;
}


LUA_API int lua_deserialize (lua_State *L, int idx) {
  size_t l;
  const char *s = luaL_checklstring(L, idx, &l);
  /* create hash table */
  lua_newtable(L);
  int top = lua_gettop(L);
  /* deserialize to values */
  int nresults = deserialize(L, s, s + l, top);
  /* remove hash table */
  lua_remove(L, top);
  return nresults;
}

#endif