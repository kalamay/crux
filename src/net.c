#include "../include/crux/net.h"
#include "../include/crux/err.h"
#include "../include/crux/poll.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define XPASSIVE (1<<30)

static bool
parse_int(const char *net, int *out)
{
	char *end;
	long l = strtol(net, &end, 10);
	if (*end != '\0' || l < INT_MIN || l > INT_MAX) {
		return false;
	}
	*out = (int)l;
	return true;
}

static int
set_flags(int fd, int type, int flags)
{
	int on = 1;

	if ((flags & XREUSEADDR) &&
			setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		return XERRNO;
	}

	if (flags & XREUSEPORT) {
#ifdef SO_REUSEPORT
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0) {
			return XERRNO;
		}
#else
		return XESYS(ENOTSUP);
#endif
	}

	if ((flags & XKEEPALIVE) &&
			setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0) {
		return XERRNO;
	}

	if (type == SOCK_STREAM) {
#ifdef TCP_DEFER_ACCEPT
		if (flags & XDEFER_ACCEPT) {
			setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &on, sizeof(on));
		}
#endif
		if (flags & XNODELAY) {
			setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
		}
	}
#ifdef SO_NOSIGPIPE
	setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
	return fd;
}

static int
open_addr(const struct sockaddr *addr, socklen_t addrlen, int domain, int type, int flags)
{
	int ec;
	int fd = xsocket(domain, type);

	if (fd < 0) {
		ec = XERRNO;
		goto error;
	}

	/* Apply socket options. */
	if ((ec = set_flags(fd, type, flags)) < 0) {
		goto error;
	}

	if (flags & XPASSIVE) {
		if (bind(fd, addr, addrlen) < 0) {
			ec = XERRNO;
			goto error;
		}
		if (type == SOCK_STREAM && listen(fd, 256) < 0) {
			ec = XERRNO;
			goto error;
		}
	}
	else {
		if (xretry(connect(fd, addr, addrlen)) < 0 && errno != EINPROGRESS) {
			ec = XERRNO;
			goto error;
		}
	}

	return fd;

error:
	if (fd > -1) {
		xretry(close(fd));
	}
	return ec;
}

static int
open_inet(const char *host, const char *serv, int type, int flags, union xaddr *addr)
{
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = type;
	hints.ai_flags = (flags & XPASSIVE) ? AI_PASSIVE : 0;

	int ec = getaddrinfo(host, serv, &hints, &res);
	if (ec < 0) {
		return XEADDR(ec);
	}

	for (struct addrinfo *r = res; res; res = res->ai_next) {
		if ((ec = open_addr(r->ai_addr, r->ai_addrlen, r->ai_family, type, flags)) >= 0) {
			memcpy(&addr->ss, r->ai_addr, r->ai_addrlen);
			break;
		}
	}
	freeaddrinfo(res);

	return ec;
}

static int
open_un(const char *path, int type, int flags, union xaddr *addr)
{
	size_t n = strnlen(path, sizeof(addr->un.sun_path));
	if (n >= sizeof(addr->un.sun_path)) {
		return XESYS(ENAMETOOLONG);
	}

	addr->un.sun_family = AF_UNIX;
	memcpy(addr->un.sun_path, path, n);
	addr->un.sun_path[n] = '\0';

	if ((flags & (XREUSEADDR|XPASSIVE)) == (XREUSEADDR|XPASSIVE)) {
		if (unlink(path) < 0 && errno != ENOENT) {
			return XERRNO;
		}
	}

	return open_addr(&addr->sa, sizeof(addr->un), AF_UNIX, type, flags);
}

static int
open_fd(int fd, int type, int flags, union xaddr *addr)
{
	int sval;
	socklen_t slen;

	/* Check the socket type of the file descriptor. The getsockopt call will
	 * fail if it is not a socket, thereby ensuring we have both a socket and
	 * it is of the desired type. */
	slen = sizeof(sval);
	if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &sval, &slen) < 0) {
		return XERRNO;
	}
	if (sval != type) {
		return XESYS(EINVAL);
	}

	/* Check if the socket listening is enabled. This allows us to update the
	 * passive field of the sock struct. */
	slen = sizeof(sval);
	if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &sval, &slen) < 0) {
		return XERRNO;
	}
	if (sval != ((flags & XPASSIVE) == XPASSIVE)) {
		return XESYS(EINVAL);
	}

	/* Get the address of the socket. */
	slen = sizeof(addr->ss);
	if (getsockname(fd, &addr->sa, &slen) < 0) {
		return XERRNO;
	}

	return fd;
}

static int
open_sock(const char *net, int type, int flags, int timeoutms, union xaddr *addr)
{
	(void)timeoutms;
	/* If the full net value is an integer, interpret the input as using an
	 * existing (and presumably inherited) file descriptor. */
	int fd;
	if (parse_int(net, &fd)) {
		return open_fd(fd, type, flags, addr);
	}

	char host[256];
	const char *serv, *start, *end;

	/* If net start with an open bracket then the host is delimited by the
	 * closing bracket. This allows support for IPv6 format, but does not
	 * require it. */
	if (*net == '[') {
		start = net + 1;
		end = strchr(start, ']');
		if (end == NULL) { return XEADDR(EAI_NONAME); }
		if (end[1] != ':') { return XEADDR(EAI_SERVICE); }
		serv = end + 2;
	}
	/* Otherwise search for a ':' character separating the host name and the
	 * service name or port number. If no such character is found, treat net
	 * as a path for a UNIX socket. */
	else {
		start = net;
		end = strchr(start, ':');
		if (end == NULL) {
			return open_un(net, type, flags, addr);
		}
		serv = end + 1;
	}

	if (end - start >= (ssize_t)sizeof(host)) {
		return XESYS(ENAMETOOLONG);
	}

	/* If net omits the the host, default to INADDR_ANY. */
	if (start == end) {
		strcpy(host, "0.0.0.0");
	}
	else {
		strncpy(host, start, end - start);
	}

	/* If the service is empty, default to INPORT_ANY. */
	if (*serv == '\0') {
		serv = "0";
	}

	return open_inet(host, serv, type, flags, addr);
}

int
xbind(const char *net, int type, int flags, union xaddr *addr)
{
	return open_sock(net, type, flags | XPASSIVE, -1, addr);
}

int
xdial(const char *net, int type, int flags, int timeoutms, union xaddr *addr)
{
	return open_sock(net, type, flags & ~XPASSIVE, timeoutms, addr);
}

int
xsocket(int domain, int type)
{
	int s, rc;

#if HAS_SOCK_FLAGS
	s = socket(domain, type|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
	if (s < 0) { return XERRNO; }
#else
	s = socket(domain, type, 0);
	if (s < 0) { return XERRNO; }
	rc = xcloexec(xunblock(s));
	if (rc < 0) { goto error; }
#endif

	rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	if (rc < 0) {
		rc = XERRNO;
		goto error;
	}

	return s;

error:
	close(s);
	return rc;
}

int
xaccept(int s, int flags, int timeoutms, union xaddr *addr)
{
	for (;;) {
		socklen_t len = sizeof(addr->ss);
#if HAS_ACCEPT4
		int fd = accept4(s, &addr->sa, &len, SOCK_NONBLOCK|SOCK_CLOEXEC);
		if (fd >= 0) {
			return set_flags(fd, SOCK_STREAM, flags);
		}
#else
		int fd = accept(s, &addr->sa, &len);
		if (fd >= 0) {
			int rc = xcloexec(xunblock (fd));
			if (rc < 0) {
				close(fd);
				return rc;
			}
			return set_flags(fd, SOCK_STREAM, flags);
		}
#endif

		int rc = XERRNO;
		if (rc == XESYS(EAGAIN)) {
			rc = xwait(s, XPOLL_IN, timeoutms);
			if (rc == 0) { continue; }
			if (rc == XEIO(XECLOSE)) { rc = XESYS(ECONNABORTED); }
		}
		return rc;
	}
}

const char *
xaddrstr(const union xaddr *addr)
{
	_Thread_local static char buf[512];

	const char *rc;
	in_port_t port;

	switch(addr->sa.sa_family) {

	default:
		*buf = '\0';
		return buf;

	case AF_UNIX:
		return strncpy(buf, addr->un.sun_path, sizeof(buf));

	case AF_INET:
		rc = inet_ntop(AF_INET, &addr->in.sin_addr, buf, sizeof(buf));
		port = addr->in.sin_port;
		break;

	case AF_INET6:
		rc = inet_ntop(AF_INET6, &addr->in6.sin6_addr, buf+1, sizeof(buf)-2);
		port = addr->in6.sin6_port;
		if (rc) {
			buf[0] = '[';
			rc = strncat(buf, "]", sizeof(buf)-1);
		}
		break;
    }

	if (rc) {
		ssize_t slen = strlen(buf), rem = (ssize_t)sizeof(buf) - slen, end;
		end = snprintf(buf+slen, rem, ":%d", ntohs(port));
		if (end < 0 || end > rem) {
			buf[slen] = '\0';
		}
	}

	return rc;
}

int
xsockaddr(int fd, union xaddr *addr)
{
	socklen_t len = sizeof(addr->ss);
	int rc = getsockname(fd, &addr->sa, &len);
	return rc == 0 ? 0 : XERRNO;
}

int
xpeeraddr(int fd, union xaddr *addr)
{
	socklen_t len = sizeof(addr->ss);
	int rc = getpeername(fd, &addr->sa, &len);
	return rc == 0 ? 0 : XERRNO;
}

