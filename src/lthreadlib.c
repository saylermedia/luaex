/*
** Native threads support
** author: Alexey Smirnov
** 
*/

#ifdef LUAEX_THREADLIB
#ifndef LUAEX_SERIALIZE
#error "added -DLUAEX_SERIALIZE to compile options"
#endif
#define threadlib_c
#define LUA_LIB

#include "lprefix.h"

#include <math.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "luaex.h"

#define _(x) x

#ifdef _WIN32
/* windows threads */
#include <windows.h>
#include <process.h>

#ifdef LoadString
#undef LoadString
#endif

typedef CRITICAL_SECTION MUTEX;
typedef CONDITION_VARIABLE COND;
typedef HANDLE THREAD;

#define MUTEX_init(x) InitializeCriticalSection(x)
#define MUTEX_destroy(x) DeleteCriticalSection(x)

#define MUTEX_lock(x) EnterCriticalSection(x)
#define MUTEX_unlock(x) LeaveCriticalSection(x)
#define MUTEX_trylock(x) TryEnterCriticalSection(x)

#define COND_init(x) InitializeConditionVariable(x)
#define COND_destroy(x)

#define COND_wait(x,y) SleepConditionVariableCS(x, y, INFINITE)
#define COND_wait_for(x,y,z) SleepConditionVariableCS(x, y, z)

#define COND_notify(x) WakeConditionVariable(x)
#define COND_broadcast(x) WakeAllConditionVariable(x)

#else
/* posix threads */
#include <pthread.h>

typedef pthread_mutex_t MUTEX;
typedef pthread_cond_t COND;
typedef pthread_t THREAD;

static void MUTEX_init (MUTEX *mutex) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(mutex, &attr);
  pthread_mutexattr_destroy(&attr);
}
#define MUTEX_destroy(x) pthread_mutex_destroy(x)

#define MUTEX_lock(x) pthread_mutex_lock(x)
#define MUTEX_unlock(x) pthread_mutex_unlock(x)
#define MUTEX_trylock(x) (pthread_mutex_trylock(x) == 0)

#define COND_init(x) pthread_cond_init(x, NULL)
#define COND_destroy(x) pthread_cond_destroy(x)

#define COND_wait(x,y) pthread_cond_wait(x, y);
LUA_API int (COND_waitfor) (COND *cond, MUTEX *mutex, unsigned long msec);

#define COND_notify(x) pthread_cond_signal(x)
#define COND_broadcast(x) pthread_cond_broadcast(x)

#endif

typedef unsigned long lua_ThreadId;
#ifdef _WIN32
#define lua_threadid() ((lua_ThreadId) GetCurrentThreadId())
#else
#define lua_threadid() ((lua_ThreadId) pthread_self())
#endif


#define LUA_THREADHANDLE "THREAD*"


typedef struct LThread {
  lua_State *L;
  int nvars;
  int status;
  int nresults;
  MUTEX mutex;
  THREAD thread;
  volatile int running;
  volatile int interrupted;
} LThread;


static int writer (lua_State *L, const void *b, size_t size, void *B) {
  (void)L;
  luaL_addlstring((luaL_Buffer *) B, (const char *)b, size);
  return 0;
}


static void copy (lua_State *L1, lua_State *L, int idx, int top) {
  switch (lua_type(L, idx)) {
    case LUA_TNIL:
      lua_pushnil(L1);
      break;
    case LUA_TBOOLEAN:
      lua_pushboolean(L1, lua_toboolean(L, idx));
      break;
    case LUA_TNUMBER: {
      lua_Number v = lua_tonumber(L, idx);
      if (trunc(v) == v)
        lua_pushinteger(L1, lua_tointeger(L, idx));
      else
        lua_pushnumber(L1, v);
      break;
    }
    case LUA_TSTRING: {
      size_t l;
      const char *s = lua_tolstring(L, idx, &l);
      lua_pushlstring(L1, s, l);
      break;
    }
  case LUA_TTABLE: {
    const char *p = lua_topointer(L, idx);
    lua_pushlightuserdata(L1, (void*) p);
    lua_rawget(L1, top);
    if (lua_istable(L1, -1))
      break;
    lua_pop(L1, 1);
    lua_newtable(L1);
    /* store pointer */
    lua_pushlightuserdata(L1, (void*) p);
    lua_pushvalue(L1, -2);
    lua_rawset(L1, top);
    /**/
    lua_pushvalue(L, idx);
    lua_pushnil(L);
    while (lua_next(L, -2)) {
      copy(L1, L, -2, top);
      copy(L1, L, -1, top);
      lua_rawset(L1, -3);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (lua_getmetatable(L, idx)) {
      copy(L1, L, -1, top);
      lua_setmetatable(L1, -2);
      lua_pop(L, 1);
    }
    break;
  }
    case LUA_TFUNCTION: {
      if (lua_iscfunction(L, idx)) {
        lua_pushcfunction(L1, lua_tocfunction(L, idx));
        break;
      }
      luaL_Buffer b;
      luaL_buffinit(L, &b);
      lua_pushvalue(L, idx);
    if (lua_dump(L, writer, &b, 0))
      luaL_error(L, _("cannot serialize function"));
      luaL_pushresult(&b);
      size_t l;
      const char *s = lua_tolstring(L, -1, &l);
      if (luaL_loadbuffer(L1, s, l, "")) {
        s = lua_tolstring(L1, -1, &l);
        lua_pushlstring(L, s, l);
        lua_error(L);
      }
      lua_pop(L, 2);
      break;
    }
  case LUA_TLIGHTUSERDATA:
    lua_pushlightuserdata(L1, (void*) lua_topointer(L, idx));
    break;
  default:
    luaL_error(L, _("cannot serialize"));
    break;
  }
}


static void* thread_proc (void *par) {
  LThread *t = (LThread *) par;
  int top = lua_gettop(t->L);
  t->status = lua_pcall(t->L, t->nvars, LUA_MULTRET, 0);
  t->nresults = lua_gettop(t->L) - top + t->nvars + 1; /* nvars + func */
  MUTEX_lock(&t->mutex);
  t->running = 0;
  MUTEX_unlock(&t->mutex);
  return NULL;
}


static int copy_values (lua_State *L) {
  luaL_openlibs(L);
  lua_State *L1 = (lua_State *) lua_touserdata(L, 1);
  int idx = (int) lua_tointeger(L, 2);
  int count = (int) lua_tointeger(L, 3);
  lua_newtable(L);
  int top = lua_gettop(L);
  int i, nvars = 0;
  for (i = idx; i <= count; i++) {
    nvars++;
    copy(L, L1, i, top);
  }
  lua_remove(L, top);
  return nvars;
}


static int create (lua_State *L, int idx) {
  luaL_checktype(L, idx, LUA_TFUNCTION);
  int count = lua_gettop(L);
  
  lua_State *L1 = luaL_newstate();
  if (L1 == NULL)
  luaL_error(L, _("cannot create state: not enough memory"));

  lua_pushcfunction(L1, copy_values);
  lua_pushlightuserdata(L1, L);
  lua_pushinteger(L1, idx);
  lua_pushinteger(L1, count);
  if (lua_pcall(L1, 3, LUA_MULTRET, 0)) {
    lua_close(L1);
    luaL_error(L, _("cannot serialize variables"));
  }
  
  LThread *t = (LThread*) lua_newuserdata(L, sizeof(LThread));
  t->L = L1;
  t->nvars = count - idx;
  t->status = 1;
  t->nresults = 0;
  MUTEX_init(&t->mutex);
  t->running = 0;
  t->interrupted = 0;
  luaL_setmetatable(L, LUA_THREADHANDLE);
  MUTEX_lock(&t->mutex);
#ifdef _WIN32
  DWORD threadId;
  t->thread = CreateThread(NULL, 0x10000, (LPTHREAD_START_ROUTINE) thread_proc, t, 0, &threadId);
  t->running = (t->thread != NULL);
#else
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  t->running = (pthread_create(&t->thread, &attr, thread_proc, t) == 0);
  pthread_attr_destroy(&attr);
#endif
  if (!t->running) {
    MUTEX_unlock(&t->mutex);
    luaL_error(L, _("cannot create thread"));
  }
  MUTEX_unlock(&t->mutex);
  return 1;
}


static int createthread (lua_State *L) {
  return create(L, 1);
}


static int join (lua_State *L) {
  LThread *t = (LThread*) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  MUTEX_lock(&t->mutex);
  if (t->running) {
    MUTEX_unlock(&t->mutex);
  #ifdef _WIN32
    WaitForSingleObject(t->thread, INFINITE);
    CloseHandle(t->thread);
  #else
    pthread_join(t->thread, NULL);
  #endif
    MUTEX_lock(&t->mutex);
    t->running = 0;
  }
  MUTEX_unlock(&t->mutex);
  lua_newtable(L);
  int i, top = lua_gettop(L);
  lua_pushboolean(L, (t->status == 0));
  for (i = t->nresults; i >= 1; i--)
    copy(L, t->L, -i, top);
  lua_remove(L, top);
  return t->nresults + 1;
}


static int running (lua_State *L) {
  LThread *t = (LThread*) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  lua_pushboolean(L, (t->running > 0));
  return 1;
}


static int interrupt (lua_State *L) {
  LThread *t = (LThread*) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  t->interrupted = 1;
  return 1;
}


static int interrupted (lua_State *L) {
  LThread *t = (LThread*) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  lua_pushboolean(L, (t->interrupted > 0));
  return 1;
}


static int closethread (lua_State *L) {
  LThread *t = (LThread*) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  join(L);
  MUTEX_destroy(&t->mutex);
  lua_close(t->L);
  return 0;
}


/*
** functions for 'thread' library
*/
static const luaL_Reg threadlib[] = {
  {"new", createthread},
  {"join", join},
  {"running", running},
  {"interrupt", interrupt},
  {"interrupted", interrupted},
  {NULL, NULL}
};


/*
** methods for thread handles
*/
static const luaL_Reg tlib[] = {
  {"join", join},
  {"running", running},
  {"interrupt", interrupt},
  {"interrupted", interrupted},
  {"__gc", closethread},
  {NULL, NULL}
};


static void createmeta (lua_State *L) {
  luaL_newmetatable(L, LUA_THREADHANDLE);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, tlib, 0);  /* add file methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
}


static int tcall (lua_State *L) {
  return create(L, 2);
}


static const luaL_Reg mtlib[] = {
  {"__call", tcall},
  {NULL, NULL}
};


LUAMOD_API int luaopen_thread (lua_State *L) {
  luaL_newlib(L, threadlib);  /* new module */
  luaL_newlib(L, mtlib);
  lua_setmetatable(L, -2);
  createmeta(L);
  return 1;
}

#endif