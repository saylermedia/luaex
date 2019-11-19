/*
** Socket library
** See Agreement in LICENSE
** Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
*/

#define socklib_c
#define LUA_LIB

#include "lprefix.h"

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#ifdef LUAEX_SOCKET
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ctype.h>
#include <fcntl.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#define close closesocket
#define bzero RtlZeroMemory
#define LPBUFFER char *
#define sock_errno WSAGetLastError()
#else
#include <getopt.h>
#include <sys/types.h>
#include <sys/time.h>
/*#include <resolv.h>*/
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <libgen.h>
#include <errno.h>
#ifndef errno
extern int errno;
#endif
#define LPBUFFER void *
#define sock_errno errno
#define sock_strerror(x) strerror(x)
#endif


#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#define MAX_PACKETSIZE 65536

#define LUA_SOCKETHANDLE "SOCKET*"

#define SOCKET_TCP 0
#define SOCKET_UDP 1

struct sock_context
{
  int	type;
  int	handle;
};


#ifdef _WIN32
static const char * sock_strerror (int errcode)
{
  switch (errcode)
  {
  case EXIT_SUCCESS:
    return _("no error");

  case WSAEBADF:
  case WSAENOTCONN:
    return _("connection error");

  case WSAEINTR:
    return _("call was interrupted by a signal that was caught before a valid connection arrived");

  case WSAEACCES:
  case WSAEAFNOSUPPORT:
  case WSAEINVAL:
  case WSAEMFILE:
  case WSAENOBUFS:
  case WSAEPROTONOSUPPORT:
  case WSAENOTSOCK:
    return _("invalid socket");

  case WSAECONNREFUSED:
    return _("no server is listening at remote address");

  case WSAETIMEDOUT:
    return _("timed out while attempting operation");

  case WSAEINPROGRESS:
    return _("socket is non-blocking and the connection cannot be completed immediately");

  case WSAECONNABORTED:
    return _("the connection has been aborted");

  case WSAEWOULDBLOCK:
    return _("operation would block if socket were blocking");

  case WSAECONNRESET:
    return _("connection was forcibly closed by the remote host");

  case WSANO_DATA:
  case WSAEADDRNOTAVAIL:
    return _("invalid destination address specified");

  case WSAEADDRINUSE:
    return _("address already in use");

  case WSAEFAULT:
    return _("pointer type supplied as argument is invalid");

  default:
    return _("unknown error");
  }
}
#endif


static int sock_tcp (lua_State *L)
{
  struct sock_context *ctx = (struct sock_context *) lua_newuserdata(L, sizeof(struct sock_context));

  ctx->type = 0;
  ctx->handle = socket(AF_INET, SOCK_STREAM, 0);
  if (ctx->handle < 0)
    luaL_error(L, _("socket() failed, %s (%d)"), sock_strerror(sock_errno), sock_errno);
  
  luaL_setmetatable(L, LUA_SOCKETHANDLE);
  return 1;
}


static int sock_err (lua_State *L)
{
  lua_pushinteger(L, (lua_Integer) sock_errno);
  return 1;
}


static int sock_strerr (lua_State *L)
{
  lua_pushstring(L, sock_strerror(sock_errno));
  return 1;
}


static int sock_setblocking (lua_State *L)
{
  struct sock_context *ctx = (struct sock_context *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  luaL_checktype(L, 2, LUA_TBOOLEAN);
  
#ifdef _WIN32
  u_long nonb = (u_long) lua_toboolean(L, 2);
  lua_pushboolean(L, (ioctlsocket(ctx->handle, FIONBIO, &nonb) == 0));
#else
  if (lua_toboolean(L, 2))
    lua_pushboolean(L, fcntl(ctx->handle, F_SETFL, fcntl(ctx->handle, F_GETFL, NULL) & (~O_NONBLOCK)) == 0);
  else
    lua_pushboolean(L, fcntl(ctx->handle, F_SETFL, fcntl(ctx->handle, F_GETFL, NULL) | O_NONBLOCK) == 0);
#endif
  return 1;
}


static int sock_connect (lua_State *L)
{
  struct sock_context *ctx = (struct sock_context *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  const char *addr = luaL_checkstring(L, 2);
  unsigned short port = (unsigned short) luaL_checkinteger(L, 3);
  
  struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr(addr);  /* probe addr by ip */
  if (sin.sin_addr.s_addr == INADDR_NONE)
  {
    struct hostent *host = gethostbyname(addr); /* probe addr by name */
    if (host)
			memcpy((char *) &sin.sin_addr.s_addr, host->h_addr, sizeof(sin.sin_addr.s_addr));
		else
      luaL_error(L, _("unresolve hostname '%s'"), addr);
  }
  
  /* probe connecting */
  lua_pushboolean(L, (connect(ctx->handle, (struct sockaddr *) &sin, sizeof(struct sockaddr)) == 0));
  return 1;
}


static int sock_select (lua_State *L)
{
  struct sock_context *ctx = (struct sock_context *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  struct timeval tv;
  tv.tv_sec = (int) luaL_checkinteger(L, 2);
	tv.tv_usec = 0;
  fd_set fdset;
  FD_ZERO(&fdset);
  FD_SET(ctx->handle, &fdset);
  /* waiting ... */
  if (select(ctx->handle + 1, NULL, &fdset, NULL, &tv) > 0)
  {
    /* update error */
    socklen_t lon = sizeof(int);
    int error;
    getsockopt(ctx->handle, SOL_SOCKET, SO_ERROR, (char *) &error, &lon);
    lua_pushboolean(L, (error == 0));
  } else
    lua_pushboolean(L, 0);
  return 1;
}


static int sock_recvtimeo (lua_State *L)
{
  struct sock_context *ctx = (struct sock_context *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  struct timeval tv;
  tv.tv_sec = (int) luaL_checkinteger(L, 2);
	tv.tv_usec = 0;
  setsockopt(ctx->handle, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv));
  return 0;
}


static int sock_sendtimeo (lua_State *L) {
  struct sock_context *ctx = (struct sock_context *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  struct timeval tv;
  tv.tv_sec = (int) luaL_checkinteger(L, 2);
	tv.tv_usec = 0;
  setsockopt(ctx->handle, SOL_SOCKET, SO_SNDTIMEO, (char *) &tv, sizeof(tv));
  return 0;
}


static int sock_recv (lua_State *L)
{
  struct sock_context *ctx = (struct sock_context *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  size_t size = (size_t) luaL_optinteger(L, 2, MAX_PACKETSIZE);
  
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  
  int len = recv(ctx->handle, (LPBUFFER) luaL_prepbuffsize(&b, size), size, 0);
  if (len > 0)
    luaL_pushresultsize(&b, (size_t) len);
  else
    lua_pushnil(L);
  return 1;
}


static int sock_send (lua_State *L)
{
  struct sock_context *ctx = (struct sock_context *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  size_t l;
  const char *s = luaL_checklstring(L, 2, &l);
  lua_pushinteger(L, send(ctx->handle, (LPBUFFER) s, l, 0));
  return 1;
}


static int sock_close (lua_State *L)
{
  struct sock_context *ctx = (struct sock_context *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  if (ctx->handle > 0)
  {
    shutdown(ctx->handle, 2);
    close(ctx->handle);
    ctx->handle = -1;
  }
  return 0;
}


static int sock_gc (lua_State *L)
{
  struct sock_context *ctx = (struct sock_context *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  if (ctx->handle > 0)
  {
    shutdown(ctx->handle, 2);
    close(ctx->handle);
  }
  return 0;
}


/*
** functions for 'socket' library
*/
static const luaL_Reg sock_lib[] =
{
  {"tcp", sock_tcp},
  {"err", sock_err},
  {"strerr", sock_strerr},
  {NULL, NULL}
};


/*
** methods for socket handles
*/
static const luaL_Reg sock_methods[] =
{
  {"setblocking", sock_setblocking},
  {"connect", sock_connect},
  {"select", sock_select},
  {"recvtimeo", sock_recvtimeo},
  {"sendtimeo", sock_sendtimeo},
  {"recv", sock_recv},
  {"send", sock_send},
  {"close", sock_close},
  {"__gc", sock_gc},
  {NULL, NULL}
};


static int sock_call (lua_State *L) {
  lua_remove(L, 1);
  return sock_tcp(L);
}


static const luaL_Reg sock_mt[] = {
  {"__call", sock_call},
  {NULL, NULL}
};


LUAMOD_API int luaopen_socket (lua_State *L) {
#ifdef _WIN32
  WSADATA ws;
  if (WSAStartup(MAKEWORD(2, 2), &ws))
    luaL_error(L, _("WSAStartup() initialized failed!"));
#endif
  /* register library */
  luaL_newlib(L, sock_lib);  /* new module */
  luaL_newlib(L, sock_mt);
  lua_setmetatable(L, -2);
  /* create metatable for socket handles */
  luaL_newmetatable(L, LUA_SOCKETHANDLE);
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, sock_methods, 0);  /* add methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
/*#ifdef _WIN32
	WSACleanup();
#endif*/
  return 1;
}

#endif