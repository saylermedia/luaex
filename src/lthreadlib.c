/*
** Native threads library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#ifdef LUAEX_THREADLIB
#ifndef LUAEX_SERIALIZE
#error "added -DLUAEX_SERIALIZE to compile options"
#endif
#define threadlib_c
#define LUA_LIB

#include "lprefix.h"

#include <math.h>
#include <malloc.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "luaex.h"

#define _(x) x

#ifdef _WIN32
/* windows threads */
#include <windows.h>
#include <process.h>

/*#ifdef LoadString
#undef LoadString
#endif*/

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
#define COND_waitfor(x,y,z) SleepConditionVariableCS(x, y, (DWORD)z)

#define COND_notify(x) WakeConditionVariable(x)
#define COND_broadcast(x) WakeAllConditionVariable(x)

#define THREAD_id() GetCurrentThreadId()
/* windows EOF */
#else
/* posix threads */
#include <time.h>
#include <sys/time.h>
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
static int COND_waitfor (COND *cond, MUTEX *mutex, unsigned long msec) {
  struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	
	ts.tv_nsec += msec % 1000 * 1000000;
	ts.tv_sec  += msec / 1000 + ts.tv_nsec / 1000000000;
	ts.tv_nsec %= 1000000000;
	
	return (pthread_cond_timedwait(cond, mutex, &ts) == 0);
}

#define COND_notify(x) pthread_cond_signal(x)
#define COND_broadcast(x) pthread_cond_broadcast(x)

#define THREAD_id() pthread_self()
/* posix EOF  */
#endif


#define LUA_THREADHANDLE "THREAD*"
#define LUA_POOLHANDLE "POOL*"

#define LUA_THREAD_USERDATA "_THREAD"


typedef struct PoolState {
  MUTEX mutex;
  COND cond;
  int nref;
  int *ref;
  size_t refsize;
} PoolState;


typedef struct ThreadState {
  lua_State *L;
  char *var;
  size_t varsize;
  int status;
#ifdef _WIN32
  DWORD threadId;
#endif
  THREAD thread;
  volatile int running;
  volatile int interrupted;
  MUTEX mutex;
  COND cond;
  PoolState *ps;
} ThreadState;


static int thread_call (lua_State *L) {
  ThreadState *ts = (ThreadState *) lua_touserdata(L, 1);
  /* initialize state */
  luaL_openlibs(L);
  lua_pushlightuserdata(L, ts);
  lua_setfield(L, LUA_REGISTRYINDEX, LUA_THREAD_USERDATA);
  /* deserialize variables */
  lua_pushlstring(L, ts->var, ts->varsize);
  int top = lua_gettop(L);
  int nvars = lua_deserialize(L, -1);
  lua_remove(L, top);
  /* calling function */
  top = lua_gettop(L);
  lua_call(L, nvars - 1, LUA_MULTRET);
  int nresults = lua_gettop(L) - top + nvars;
  /* serialize results */
  if (nresults > 0)
    lua_serialize(L, -(nresults), -1);
  else
    lua_pushnil(L);
  return 1;
}


static void* thread_proc (void *par) {
  ThreadState *ts = (ThreadState *) par;
  /* todo something */
  lua_pushcfunction(ts->L, thread_call);
  lua_pushlightuserdata(ts->L, ts);
  ts->status = lua_pcall(ts->L, 1, 1, 0);
  /* running = false */
  MUTEX_lock(&ts->mutex);
  ts->running = 0;
  MUTEX_unlock(&ts->mutex);
  if (ts->ps) {
    PoolState *ps = ts->ps;
    MUTEX_lock(&ps->mutex);
    ps->nref--;
    COND_broadcast(&ps->cond);
    MUTEX_unlock(&ps->mutex);
  }
  return NULL;
}


static ThreadState * create (lua_State *L, int idx, PoolState *ps) {
  luaL_checktype(L, idx, LUA_TFUNCTION);
  int count = lua_gettop(L);
  ThreadState *ts = (ThreadState *) lua_newuserdata(L, sizeof(ThreadState));
  ts->L = luaL_newstate();
  if (ts->L == NULL)
    luaL_error(L, _("cannot create state: not enough memory"));
  ts->var = NULL;
  ts->varsize = 0;
  ts->status = 1;
#ifdef _WIN32
  ts->threadId = 0;
  ts->thread = NULL;
#endif
  ts->running = 0;
  ts->interrupted = 0;
  ts->ps = ps;
  MUTEX_init(&ts->mutex);
  COND_init(&ts->cond);
  luaL_setmetatable(L, LUA_THREADHANDLE);
  /* serialize variables */
  lua_serialize(L, idx, count);
  const char *s = lua_tolstring(L, -1, &ts->varsize);
  ts->var = (char *) malloc(sizeof(char) * ts->varsize);
  if (ts->var == NULL)
    luaL_error(L, _("not enough memory"));
  memcpy(ts->var, s, ts->varsize);
  lua_pop(L, 1);
  /* create thread */
  MUTEX_lock(&ts->mutex);
#ifdef _WIN32
  ts->thread = CreateThread(NULL, 0x10000, (LPTHREAD_START_ROUTINE) thread_proc, ts, 0, &ts->threadId);
  ts->running = (ts->thread != NULL);
#else
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  ts->running = (pthread_create(&ts->thread, &attr, thread_proc, ts) == 0);
  pthread_attr_destroy(&attr);
#endif
  if (!ts->running) {
    MUTEX_unlock(&ts->mutex);
    luaL_error(L, _("cannot create thread"));
  }
  MUTEX_unlock(&ts->mutex);
  return ts;
}


static int tnew (lua_State *L) {
  create(L, 1, NULL);
  return 1;
}


static int tpool (lua_State *L) {
  PoolState *ps = (PoolState *) lua_newuserdata(L, sizeof(PoolState));
  MUTEX_init(&ps->mutex);
  COND_init(&ps->cond);
  ps->nref = 0;
  ps->ref = NULL;
  ps->refsize = 0;
  luaL_setmetatable(L, LUA_POOLHANDLE);
  return 1;
}


static int padd (lua_State *L) {
  PoolState *ps = (PoolState *) luaL_checkudata(L, 1, LUA_POOLHANDLE);
  int *ref = (int *) realloc(ps->ref, sizeof(ThreadState) * (ps->refsize + 1));
  if (ref == NULL)
      return luaL_error(L, _("not enough memory"));
  ps->ref = ref;
  MUTEX_lock(&ps->mutex);
  ps->nref++;
  MUTEX_unlock(&ps->mutex);
  ThreadState *ts = create(L, 2, ps);
  lua_pushvalue(L, -1);
  ps->ref[ps->refsize] = luaL_ref(L, LUA_REGISTRYINDEX);
  ps->refsize++;
  return 1;
}


static int tjoin (lua_State *L) {
  ThreadState *ts = (ThreadState *) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  MUTEX_lock(&ts->mutex);
  if (ts->running) {
    MUTEX_unlock(&ts->mutex);
  #ifdef _WIN32
    WaitForSingleObject(ts->thread, INFINITE);
  #else
    pthread_join(ts->thread, NULL);
  #endif
    MUTEX_lock(&ts->mutex);
    ts->running = 0;
  }
  MUTEX_unlock(&ts->mutex);
  size_t l;
  const char *s;
  if (ts->status) {
    /* if fail then copy error message to main thread */
    lua_pushboolean(L, 0);
    s = lua_tolstring(ts->L, -1, &l);
    lua_pushlstring(L, s, l);
    return 2;
  } else {
    int nresults = 1;
    /* if success then copy result to main thread */
    lua_pushboolean(L, 1);
    if (lua_isstring(ts->L, -1)) {
      s = lua_tolstring(ts->L, -1, &l);
      lua_pushlstring(L, s, l);
      int top = lua_gettop(L);
      nresults += lua_deserialize(L, -1);
      lua_remove(L, top);
    }
    return nresults;
  }
}


static int tcancel (lua_State *L) {
  ThreadState *ts = (ThreadState *) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  lua_cancel(ts->L);
  MUTEX_lock(&ts->mutex);
  COND_broadcast(&ts->cond);
  MUTEX_unlock(&ts->mutex);
  return 0;
}

static int trunning (lua_State *L) {
  ThreadState *ts = (ThreadState *) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  lua_pushboolean(L, ts->running);
  return 1;
}


static int tinterrupt (lua_State *L) {
  ThreadState *ts = (ThreadState *) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  MUTEX_lock(&ts->mutex);
  ts->interrupted = 1;
  COND_broadcast(&ts->cond);
  MUTEX_unlock(&ts->mutex);
  return 0;
}


static int tinterrupted (lua_State *L) {
  ThreadState *ts;
  if (lua_gettop(L) > 0)
    ts = (ThreadState *) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  else {
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_THREAD_USERDATA);
    if (lua_isnil(L, -1))
      return luaL_error(L, _("not implement to main thread"));
    else {
      ts = (ThreadState *) lua_topointer(L, -1);
      lua_pop(L, 1);
    }
  }
  MUTEX_lock(&ts->mutex);
  int interrupted = ts->interrupted;
  MUTEX_unlock(&ts->mutex);
  lua_pushboolean(L, interrupted);
  return 1;
}


static int tid (lua_State *L) {
  if (lua_gettop(L) > 0) {
    ThreadState *ts = (ThreadState *) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  #ifdef _WIN32
    lua_pushinteger(L, (lua_Integer) ts->threadId);
  #else
    lua_pushinteger(L, (lua_Integer) ts->thread);
  #endif
  } else
    lua_pushinteger(L, (lua_Integer) THREAD_id());
  return 1;
}


static int ttime (lua_State *L) {
#ifdef _WIN32
	SYSTEMTIME time;
	GetLocalTime(&time);
	lua_pushinteger(L, (lua_Integer) ((time.wHour * 3600 + time.wMinute * 60 + time.wSecond) * 1000 + time.wMilliseconds));
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	lua_pushinteger(L, (lua_Integer) (tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
  return 1;
}


static int tsleep (lua_State *L) {
  lua_Integer timeout = luaL_checkinteger(L, 1);
  if (timeout <= 0)
    return 0;
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_THREAD_USERDATA);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
  #ifdef _WIN32
    Sleep((DWORD) timeout);
  #else
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = timeout * 1000000;
    nanosleep(&ts, NULL);
  #endif
  } else {
    ThreadState *ts = (ThreadState *) lua_topointer(L, -1);
    lua_pop(L, 1);
    MUTEX_lock(&ts->mutex);
    COND_waitfor(&ts->cond, &ts->mutex, timeout);
    MUTEX_unlock(&ts->mutex);
  }
  return 0;
}


static int tgc (lua_State *L) {
  ThreadState *ts = (ThreadState *) luaL_checkudata(L, 1, LUA_THREADHANDLE);
  MUTEX_lock(&ts->mutex);
  if (ts->running) {
    lua_cancel(ts->L);
    COND_broadcast(&ts->cond);
    MUTEX_unlock(&ts->mutex);
  #ifdef _WIN32
    WaitForSingleObject(ts->thread, INFINITE);
  #else
    pthread_join(ts->thread, NULL);
  #endif
  } else
    MUTEX_unlock(&ts->mutex);
#ifdef _WIN32
  if (ts->thread)
    CloseHandle(ts->thread);
#endif
  MUTEX_destroy(&ts->mutex);
  COND_destroy(&ts->cond);
  lua_close(ts->L);
  if (ts->var)
    free(ts->var);
  return 0;
}


static int plen (lua_State *L) {
  PoolState *ps = (PoolState *) luaL_checkudata(L, 1, LUA_POOLHANDLE);
  lua_pushinteger(L, ps->refsize);
  return 1;
}


static int pindex (lua_State *L) {
  PoolState *ps = (PoolState *) luaL_checkudata(L, 1, LUA_POOLHANDLE);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    lua_Integer idx = luaL_checkinteger(L, 2);
    luaL_argcheck(L, idx > 0 && idx <= ps->refsize, 1, "index out of range");
    lua_rawgeti(L, LUA_REGISTRYINDEX, ps->ref[idx - 1]);
  } else {
    luaL_getmetatable(L, LUA_POOLHANDLE);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    lua_remove(L, -2);
  }
  return 1;
}


static int pairs_next (lua_State *L) {
  PoolState *ps = (PoolState *) luaL_checkudata(L, lua_upvalueindex(1), LUA_POOLHANDLE);
  size_t i = (size_t) luaL_checkinteger(L, lua_upvalueindex(2));
  if (i < ps->refsize) {
    lua_pushinteger(L, i + 1);
    lua_replace(L, lua_upvalueindex(2));
    lua_rawgeti(L, LUA_REGISTRYINDEX, ps->ref[i]);
    return 1;
  }
  return 0;
}


static int ppairs (lua_State *L) {
  luaL_checkudata(L, 1, LUA_POOLHANDLE);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, 0);
  lua_pushcclosure(L, pairs_next, 2);
  return 1;
}


static int pwait (lua_State *L) {
  PoolState *ps = (PoolState *) luaL_checkudata(L, 1, LUA_POOLHANDLE);
  MUTEX_lock(&ps->mutex);
  while (ps->nref > 0)
    COND_wait(&ps->cond, &ps->mutex);
  MUTEX_unlock(&ps->mutex);
  return 0;
}


static int pinterrupt (lua_State *L) {
  PoolState *ps = (PoolState *) luaL_checkudata(L, 1, LUA_POOLHANDLE);
  size_t i;
  for (i = 0; i < ps->refsize; i++) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ps->ref[i]);
    ThreadState *ts = (ThreadState *) luaL_checkudata(L, -1, LUA_THREADHANDLE);
    lua_pop(L, 1);
    MUTEX_lock(&ts->mutex);
    ts->interrupted = 1;
    COND_broadcast(&ts->cond);
    MUTEX_unlock(&ts->mutex);
  }
  return 0;
}


static int pcancel (lua_State *L) {
  PoolState *ps = (PoolState *) luaL_checkudata(L, 1, LUA_POOLHANDLE);
  size_t i;
  for (i = 0; i < ps->refsize; i++) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ps->ref[i]);
    ThreadState *ts = (ThreadState *) luaL_checkudata(L, -1, LUA_THREADHANDLE);
    lua_pop(L, 1);
    lua_cancel(ts->L);
    MUTEX_lock(&ts->mutex);
    COND_broadcast(&ts->cond);
    MUTEX_unlock(&ts->mutex);
  }
  return 0;
}


static int pgc (lua_State *L) {
  PoolState *ps = (PoolState *) luaL_checkudata(L, 1, LUA_POOLHANDLE);
  MUTEX_destroy(&ps->mutex);
  COND_destroy(&ps->cond);
  if (ps->ref) {
    size_t i;
    for (i = 0; i < ps->refsize; i++)
      luaL_unref(L, LUA_REGISTRYINDEX, ps->ref[i]);
    free(ps->ref);
  }
  return 1;
}


/*
** functions for 'thread' library
*/
static const luaL_Reg threadlib[] = {
  {"new", tnew},
  {"pool", tpool},
  {"join", tjoin},
  {"cancel", tcancel},
  {"running", trunning},
  {"interrupt", tinterrupt},
  {"interrupted", tinterrupted},
  {"id", tid},
  {"time", ttime},
  {"sleep", tsleep},
  {NULL, NULL}
};


/*
** methods for thread handles
*/
static const luaL_Reg tlib[] = {
  {"join", tjoin},
  {"cancel", tcancel},
  {"running", trunning},
  {"interrupt", tinterrupt},
  {"interrupted", tinterrupted},
  {"id", tid},
  {"__gc", tgc},
  {NULL, NULL}
};


static const luaL_Reg plib[] = {
  {"add", padd},
  {"wait", pwait},
  {"interrupt", pinterrupt},
  {"cancel", pcancel},
  {"__len", plen},
  {"__index", pindex},
  {"__pairs", ppairs},
  {"__gc", pgc},
  {NULL, NULL}
};


static void createmeta (lua_State *L) {
  luaL_newmetatable(L, LUA_THREADHANDLE);  /* create metatable for thread handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, tlib, 0);  /* add file methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
  luaL_newmetatable(L, LUA_POOLHANDLE);  /* create metatable for pool handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, plib, 0);  /* add file methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
}


static int tcall (lua_State *L) {
  create(L, 2, NULL);
  return 1;
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