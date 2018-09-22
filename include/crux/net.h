#ifndef CRUX_NET_H
#define CRUX_NET_H

#include "hub.h"

#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/un.h>

union xaddr
{
	struct sockaddr sa;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;
	struct sockaddr_un un;
	struct sockaddr_storage ss;
};

enum xaddr_flag
{
	XREUSEADDR     = 1<<0, /* Allow reuse of local addresses. */
	XREUSEPORT     = 1<<1, /* Allow multiple binds to the same address and port. */
	XNODELAY       = 1<<2, /* Disable TCP Nagle algorithm. */
	XDEFER_ACCEPT  = 1<<3, /* Awaken listener only when data arrives on the socket. */
	XKEEPALIVE     = 1<<4, /* Send TCP keep alive probes. */
};

XEXTERN int
xbind(const char *net, int type, int flags, union xaddr *addr);

XEXTERN int
xdial(const char *net, int type, int flags, int timeoutms, union xaddr *addr);

XEXTERN int
xsocket(int domain, int type);

XEXTERN int
xaccept(int s, int flags, int timeoutms, union xaddr *addr);

XEXTERN int
xconnect(int s, const struct sockaddr *addr, socklen_t addrlen, int timeoutms);

XEXTERN const char *
xaddrstr(const union xaddr *addr);

XEXTERN int
xsockaddr(int fd, union xaddr *addr);

XEXTERN int
xpeeraddr(int fd, union xaddr *addr);

#endif

