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
#include <iphlpapi.h>
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
#include <net/if.h>
#ifdef HAVE_WIRELESS_H
#include <linux/wireless.h>
#endif
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

struct socket_addr_t {
  struct sockaddr_in sin;
};


/*
* Enumerate network interfaces
*/

#define LUA_SOCKETIFRHANDLE "socket_ifr"
#define MAX_NETWORK_INTERFACES 16

struct socket_ifr_t {
#ifdef _WIN32
	PIP_ADAPTER_ADDRESSES	addresses;
	PIP_ADAPTER_ADDRESSES	address;
	ULONG	size;
#else
	int	socket;
	struct ifreq	ifs[MAX_NETWORK_INTERFACES];
	struct ifconf ifc;
	struct ifreq *ifr, *ifend;
#endif
	char addr[16];
	char hwaddr[18];
};

LUA_API int socket_ifr_next (struct socket_ifr_t *ctx);

LUA_API struct socket_ifr_t * socket_ifr_new (void)
{
  struct socket_ifr_t *ctx = (struct socket_ifr_t *) malloc(sizeof(struct socket_ifr_t));
  if (ctx)
  {
  #ifdef _WIN32
    ctx->addresses = NULL;
    ctx->address = NULL;
    ctx->size = 0;
  #else
    ctx->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->socket < 0)
    {
      fprintf(stderr, "socket() failed, errno = %s (%d)", strerror(errno), errno);
      free(ctx);
      return NULL;
    }
    ctx->ifr = NULL;
    ctx->ifend = NULL;
  #endif
  }
  socket_ifr_rewind(ctx);
  return ctx;
}


LUA_API struct socket_ifr_t * lua_newsocket_ifr (lua_State *L)
{
  struct socket_ifr_t *ctx = (struct socket_ifr_t *) lua_newuserdata(L, sizeof(struct socket_ifr_t));
  if (ctx)
  {
  #ifdef _WIN32
    ctx->addresses = NULL;
    ctx->address = NULL;
    ctx->size = 0;
  #else
    ctx->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->socket < 0)
      return luaL_error(L, "socket() failed, errno = %s (%d)", strerror(errno), errno);
    ctx->ifr = NULL;
    ctx->ifend = NULL;
  #endif
  }
  luaL_setmetatable(L, LUA_SOCKETIFRHANDLE);
  socket_ifr_rewind(ctx);
  return ctx;
}


LUA_API void socket_ifr_free (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
	if (ctx->addresses)
		free(ctx->addresses);
#else
  close(ctx->socket);
#endif
  free(ctx);
}


static int lsocket_ifr_free (lua_State *L)
{
  struct socket_ifr_t *ctx = (struct socket_ifr_t *) luaL_checkudata(L, 1, LUA_SOCKETIFRHANDLE);
#ifdef _WIN32
	if (ctx->addresses)
		free(ctx->addresses);
#else
  close(ctx->socket);
#endif
  return 0;
}


LUA_API void socket_ifr_rewind (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
	ctx->address = NULL;
	const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;
	for (;;)
	{
		ULONG retVal = GetAdaptersAddresses(AF_INET, flags, NULL, ctx->addresses, &ctx->size);
		if (retVal == ERROR_BUFFER_OVERFLOW)
		{
			if (ctx->addresses)
				free(ctx->addresses);

			ctx->addresses = (PIP_ADAPTER_ADDRESSES) malloc(ctx->size);
			if (ctx->addresses)
				continue;
			else
			{
        fprintf(stderr, _("not enough memory"));
				break;
			}
		}
		else
		{
			if (retVal != NO_ERROR)
			{
				if (ctx->addresses)
				{
					free(ctx->addresses);
					ctx->addresses = NULL;
				}
			}
			break;
		}
	}
#else
	ctx->ifc.ifc_len = sizeof(ctx->ifs);
	ctx->ifc.ifc_req = ctx->ifs;
	if (ioctl(ctx->socket, SIOCGIFCONF, &ctx->ifc) < 0)
	{
    fprintf(stderr, "ioctl() SIOCGIFCONF failed, errno = %s (%d)", strerror(errno), errno);
		ctx->ifend = NULL;
	}
	else
	{
		ctx->ifend = ctx->ifs + (ctx->ifc.ifc_len / sizeof(struct ifreq));
		ctx->ifr = NULL;
	}
#endif
}


LUA_API int socket_ifr_next (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
  if (ctx->address)
    ctx->address = ctx->address->Next;
  else
    ctx->address = ctx->addresses;
  return (ctx->address != NULL);
#else
  if (ctx->ifend)
  {
    if (ctx->ifr)
      ctx->ifr++;
    else
      ctx->ifr = ctx->ifc.ifc_req;
    return (ctx->ifr < ctx->ifend);
  }
  return 0;
#endif
}


LUA_API const char * socket_ifr_name (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
  return ctx->address->AdapterName;
#else
  return ctx->ifr->ifr_name;
#endif
}


LUA_API const char * socket_ifr_addr (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
  snprintf(ctx->addr, sizeof(ctx->addr), "%lu.%lu.%lu.%lu",
    (long unsigned int)((unsigned char *)& ctx->address->FirstUnicastAddress->Address.lpSockaddr->sa_data)[2],
    (long unsigned int)((unsigned char *)& ctx->address->FirstUnicastAddress->Address.lpSockaddr->sa_data)[3],
    (long unsigned int)((unsigned char *)& ctx->address->FirstUnicastAddress->Address.lpSockaddr->sa_data)[4],
    (long unsigned int)((unsigned char *)& ctx->address->FirstUnicastAddress->Address.lpSockaddr->sa_data)[5]);
#else
  snprintf(ctx->addr, sizeof(ctx->addr), "%lu.%lu.%lu.%lu",
    (long unsigned int)((unsigned char *)& ctx->ifr->ifr_addr.sa_data)[2],
    (long unsigned int)((unsigned char *)& ctx->ifr->ifr_addr.sa_data)[3],
    (long unsigned int)((unsigned char *)& ctx->ifr->ifr_addr.sa_data)[4],
    (long unsigned int)((unsigned char *)& ctx->ifr->ifr_addr.sa_data)[5]);
#endif
  return ctx->addr;
}


LUA_API const char * socket_ifr_hwaddr (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
  snprintf(ctx->hwaddr, sizeof(ctx->hwaddr), "%02X:%02X:%02X:%02X:%02X:%02X",
    ctx->address->PhysicalAddress[0], ctx->address->PhysicalAddress[1],
    ctx->address->PhysicalAddress[2], ctx->address->PhysicalAddress[3],
    ctx->address->PhysicalAddress[4], ctx->address->PhysicalAddress[5]);
#else
  struct ifreq ifreq;
  strncpy(ifreq.ifr_name, ctx->ifr->ifr_name, sizeof(ifreq.ifr_name));
  if (ioctl(ctx->socket, SIOCGIFHWADDR, &ifreq) < 0)
  {
    fprintf(stderr, "ioctl() SIOCGIFHWADDR failed, errno = %s (%d)", strerror(errno), errno);
    return NULL;
  }
  else
    snprintf(ctx->hwaddr, sizeof(ctx->hwaddr), "%02X:%02X:%02X:%02X:%02X:%02X",
      (unsigned char) ifreq.ifr_addr.sa_data[0], (unsigned char) ifreq.ifr_addr.sa_data[1],
      (unsigned char) ifreq.ifr_addr.sa_data[2], (unsigned char) ifreq.ifr_addr.sa_data[3],
      (unsigned char) ifreq.ifr_addr.sa_data[4], (unsigned char) ifreq.ifr_addr.sa_data[5]);
#endif
  return ctx->hwaddr;
}


LUA_API size_t socket_ifr_bmask (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
  return (size_t) ctx->address->FirstUnicastAddress->OnLinkPrefixLength;
#else
  struct ifreq ifreq;
  strncpy(ifreq.ifr_name, ctx->ifr->ifr_name, sizeof(ifreq.ifr_name));
  if (ioctl(ctx->socket, SIOCGIFNETMASK, &ifreq) < 0)
  {
    fprintf(stderr, "ioctl() SIOCGIFNETMASK failed, errno = %s (%d)", strerror(errno), errno);
    return 0;
  }
  int i;
  size_t mask = 0;
  for (i = 2; i < 6; i++)
  {
    long unsigned int value = (long unsigned int)((unsigned char *) &ifreq.ifr_netmask.sa_data)[i];
    if (value == 255)
      mask += 8;
    else
    {
      long unsigned int bit = 128;
      int nbit;
      for (nbit = 0; nbit < 8 && ((value & bit) == bit); nbit++)
      {
        mask++;
        bit = bit >> 1;
      }
      break;
    }
  }
  return mask;	
#endif
}


LUA_API int socket_ifr_ispriv (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
  long unsigned int a = (long unsigned int)((unsigned char *) &ctx->address->FirstUnicastAddress->Address.lpSockaddr->sa_data)[2];
  long unsigned int b = (long unsigned int)((unsigned char *) &ctx->address->FirstUnicastAddress->Address.lpSockaddr->sa_data)[3];
#else
  long unsigned int a = (long unsigned int)((unsigned char *) &ctx->ifr->ifr_addr.sa_data)[2];
  long unsigned int b = (long unsigned int)((unsigned char *) &ctx->ifr->ifr_addr.sa_data)[3];
#endif
  return (a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168));
}


LUA_API int socket_ifr_isloop (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
  return (ctx->address->IfType == IF_TYPE_SOFTWARE_LOOPBACK);
#else
  long unsigned int a = (long unsigned int)((unsigned char *) &ctx->ifr->ifr_addr.sa_data)[2];
  return (a == 127 || a == 0);
#endif
}


LUA_API int socket_ifr_isup (struct socket_ifr_t *ctx)
{
#ifdef _WIN32
  return (ctx->address->OperStatus == IfOperStatusUp);
#else
  struct ifreq ifreq;
  strncpy(ifreq.ifr_name, ctx->ifr->ifr_name, sizeof(ifreq.ifr_name));
  if (ioctl(ctx->socket, SIOCGIFFLAGS, &ifreq) < 0)
  {
    fprintf(stderr, "ioctl() SIOCGIFFLAGS failed, errno = %s (%d)", strerror(errno), errno);
    return 0;
  }
  return ((ifreq.ifr_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING));
#endif
}


LUA_API int socket_ifr_iswir (struct socket_ifr_t *ctx)
{
#if defined(_WIN32)
  return (ctx->address->IfType == IF_TYPE_IEEE80211);
#elif defined(HAVE_WIRELESS_H)
  struct iwreq iwreq;
  strcpy(iwreq.ifr_name, ifr->ifr_name);
  return (ioctl(ctx->handle, SIOCGIWNAME, &iwreq) == 0);
#else
  fprintf(stderr, "'wireless' not supported, errno = %s (%d)", strerror(errno), errno);
  return 0;
#endif
}


/*
* Socket
*/

#define MAX_PACKETSIZE 65536

#define LUA_SOCKETHANDLE "socket"

struct socket_t {
  int	type;
  int	handle;
};


LUA_API struct socket_t * socket_new (int type)
{
  struct socket_t *ctx = (struct socket_t *) malloc(sizeof(struct socket_t));
  if (ctx)
  {
    ctx->type = type;
    switch (type)
    {
      case SOCKET_TCP:
        ctx->handle = socket(AF_INET, SOCK_STREAM, 0);
        break;
        
      case SOCKET_UDP:
        ctx->handle = socket(AF_INET, SOCK_DGRAM, 0);
        break;
        
      default:
        free(ctx);
        return NULL;
    }
    if (ctx->handle < 0)
    {
      free(ctx);
      return NULL;
    }
  }
  return ctx;
}


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


static struct socket_t * create (lua_State *L, int type, int inet, int subnet)
{
  struct socket_t *ctx = (struct socket_t *) lua_newuserdata(L, sizeof(struct socket_t));

  ctx->type = type;
  ctx->handle = socket(inet, subnet, 0);
  if (ctx->handle < 0)
    luaL_error(L, _("socket() failed, %s (%d)"), sock_strerror(sock_errno), sock_errno);
  
  luaL_setmetatable(L, LUA_SOCKETHANDLE);
  return ctx;
}


static int lsocket_ifr (lua_State *L)
{
  struct socket_ifr_t *ctx = lua_newsocket_ifr(L);
  lua_newtable(L);
  
  while (socket_ifr_next(ctx))
  {
    lua_pushstring(L, socket_ifr_name(ctx));
    lua_newtable(L);

    lua_pushstring(L, "addr"); 
    lua_pushstring(L, socket_ifr_addr(ctx));
    lua_rawset(L, -3);
    
    lua_pushstring(L, "hwaddr"); 
    lua_pushstring(L, socket_ifr_hwaddr(ctx));
    lua_rawset(L, -3);
    
    lua_pushstring(L, "mask");
    lua_pushinteger(L, (lua_Integer) socket_ifr_bmask(ctx));
    lua_rawset(L, -3);
    
    lua_pushstring(L, "loopback");
    lua_pushboolean(L, socket_ifr_isloop(ctx));
    lua_rawset(L, -3);
    
    lua_pushstring(L, "connected");
    lua_pushboolean(L, socket_ifr_isup(ctx));
    lua_rawset(L, -3);
    
    lua_pushstring(L, "wireless");
    lua_pushboolean(L, socket_ifr_iswir(ctx));
    lua_rawset(L, -3);
    
    lua_rawset(L, -3);
  }
  return 1;
}


static int sock_tcp (lua_State *L)
{
  struct socket_t *ctx = (struct socket_t *) lua_newuserdata(L, sizeof(struct socket_t));

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
  struct socket_t *ctx = (struct socket_t *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
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
  struct socket_t *ctx = (struct socket_t *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
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
  struct socket_t *ctx = (struct socket_t *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
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
  struct socket_t *ctx = (struct socket_t *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  struct timeval tv;
  tv.tv_sec = (int) luaL_checkinteger(L, 2);
	tv.tv_usec = 0;
  setsockopt(ctx->handle, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv));
  return 0;
}


static int sock_sendtimeo (lua_State *L) {
  struct socket_t *ctx = (struct socket_t *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  struct timeval tv;
  tv.tv_sec = (int) luaL_checkinteger(L, 2);
	tv.tv_usec = 0;
  setsockopt(ctx->handle, SOL_SOCKET, SO_SNDTIMEO, (char *) &tv, sizeof(tv));
  return 0;
}


static int sock_recv (lua_State *L)
{
  struct socket_t *ctx = (struct socket_t *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
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
  struct socket_t *ctx = (struct socket_t *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
  size_t l;
  const char *s = luaL_checklstring(L, 2, &l);
  lua_pushinteger(L, send(ctx->handle, (LPBUFFER) s, l, 0));
  return 1;
}


static int sock_close (lua_State *L)
{
  struct socket_t *ctx = (struct socket_t *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
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
  struct socket_t *ctx = (struct socket_t *) luaL_checkudata(L, 1, LUA_SOCKETHANDLE);
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
static const luaL_Reg sock_lib[] = {
  {"ifr", lsocket_ifr},
  {"tcp", sock_tcp},
  {"err", sock_err},
  {"strerr", sock_strerr},
  {NULL, NULL}
};


/*
** methods for socket_ifr handles
*/
static const luaL_Reg socket_ifr_methods[] = {
  {"__gc", lsocket_ifr_free},
  {NULL, NULL}
};


/*
** methods for socket handles
*/
static const luaL_Reg sock_methods[] = {
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


static int sock_call (lua_State *L)
{
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
  /* create metatable for socket_ifr handles */
  luaL_newmetatable(L, LUA_SOCKETIFRHANDLE);
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, socket_ifr_methods, 0);  /* add methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
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