#ifndef CRUX_HUB_H
#define CRUX_HUB_H

#include "def.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

struct xhub;

XEXTERN int
xhub_new(struct xhub **hubp);

XEXTERN void
xhub_free(struct xhub **hupb);

XEXTERN int
xhub_run(struct xhub *hub);

XEXTERN void
xhub_stop(struct xhub *hub);

XEXTERN void
xhub_mark_close(struct xhub *hub, int fd);

#define xspawn(hub, fn, data) \
	xspawnf(hub, __FILE__, __LINE__, fn, data)

XEXTERN int
xspawnf(struct xhub *hub, const char *file, int line,
		void (*fn)(struct xhub *, void *), void *data);

#if defined(__BLOCKS__)

XEXTERN int
xspawn_b(struct xhub *hub, void (^block)(void));

#endif

XEXTERN void
xexit(int ec);

XEXTERN void
xabort(void);

XEXTERN const struct xclock *
xclock(void);

XEXTERN int
xsleep(unsigned ms);

XEXTERN int
xsignal(int signum, int timeoutms);

XEXTERN ssize_t
xread(int fd, void *buf, size_t len, int timeoutms);

XEXTERN ssize_t
xreadv(int fd, struct iovec *iov, int iovcnt, int timeoutms);

XEXTERN ssize_t
xreadn(int fd, void *buf, size_t len, int timeoutms);

XEXTERN ssize_t
xwrite(int fd, const void *buf, size_t len, int timeoutms);

XEXTERN ssize_t
xwritev(int fd, const struct iovec *iov, int iovcnt, int timeoutms);

XEXTERN ssize_t
xwriten(int fd, const void *buf, size_t len, int timeoutms);

XEXTERN ssize_t
xrecvfrom(int s, void *buf, size_t len, int flags,
	 struct sockaddr *src_addr, socklen_t *src_len, int timeoutms);

XEXTERN ssize_t
xsendto(int s, const void *buf, size_t len, int flags,
	 const struct sockaddr *dest_addr, socklen_t dest_len, int timeoutms);

typedef ssize_t (*xio_fn) (int fd, void *buf, size_t len, int timeoutms);

XEXTERN ssize_t
xio(int fd, void *buf, size_t len, int timeoutms, xio_fn fn);

XEXTERN int
xpipe(int fds[static 2]);

XEXTERN int
xsocket(int domain, int type, int protocol);

XEXTERN int
xaccept(int s, struct sockaddr *addr, socklen_t *addrlen, int timeoutms);

XEXTERN int
xclose(int fd);

XEXTERN int
xunblock(int fd);

XEXTERN int
xcloexec(int fd);

#endif

