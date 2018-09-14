#ifndef CRUX_HUB_H
#define CRUX_HUB_H

#include "def.h"
#include "buf.h"
#include "task.h"
#include "value.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define XTIMEOUT_NONE -1
#define XTIMEOUT_DETACH INT_MIN

struct xhub;

XEXTERN int
xhub_new(struct xhub **hubp);

XEXTERN void
xhub_free(struct xhub **hupb);

XEXTERN int
xhub_run(struct xhub *hub);

XEXTERN int
xhub_wake(struct xhub *hub);

XEXTERN void
xhub_stop(struct xhub *hub);

XEXTERN void
xhub_remove_io(struct xhub *hub, int fd);

XEXTERN void
xhub_print(struct xhub *hub, FILE *out);

#define xspawn(hub, fn, val) \
	xspawnf(hub, __FILE__, __LINE__, fn, val)

XEXTERN int
xspawnf(struct xhub *hub, const char *file, int line,
		void (*fn)(struct xhub *, union xvalue), union xvalue val);

#if defined(__BLOCKS__)

XEXTERN int
xspawn_b(struct xhub *hub, void (^block)(void));

#endif

XEXTERN void
xexit(int ec);

XEXTERN void
xabort(void);

XEXTERN const struct timespec *
xclock(void);

XEXTERN int
xwait(int fd, int polltype, int timeoutms);

XEXTERN int
xsleep(unsigned ms);

XEXTERN int
xsignal(int signum, int timeoutms);

XEXTERN ssize_t
xread(int fd, void *buf, size_t len, int timeoutms);

XEXTERN ssize_t
xreadv(int fd, struct iovec *iov, int iovcnt, int timeoutms);

XEXTERN ssize_t
xrecv(int fd, void *buf, size_t len, int flags, int timeoutms);

XEXTERN ssize_t
xrecvfrom(int s, void *buf, size_t len, int flags,
	 struct sockaddr *src_addr, socklen_t *src_len, int timeoutms);

XEXTERN ssize_t
xreadn(int fd, void *buf, size_t len, int timeoutms);

XEXTERN ssize_t
xwrite(int fd, const void *buf, size_t len, int timeoutms);

XEXTERN ssize_t
xwritev(int fd, const struct iovec *iov, int iovcnt, int timeoutms);

XEXTERN ssize_t
xsend(int s, const void *buf, size_t len, int flags, int timeoutms);

XEXTERN ssize_t
xsendto(int s, const void *buf, size_t len, int flags,
	 const struct sockaddr *dest_addr, socklen_t dest_len, int timeoutms);

XEXTERN ssize_t
xwriten(int fd, const void *buf, size_t len, int timeoutms);

typedef ssize_t (*xio_fn) (int fd, void *buf, size_t len, int timeoutms);

XEXTERN ssize_t
xio(int fd, void *buf, size_t len, int timeoutms, xio_fn fn);

XEXTERN int
xpipe(int fds[static 2]);

XEXTERN int
xclose(int fd);

XEXTERN int
xunblock(int fd);

XEXTERN int
xcloexec(int fd);

XEXTERN ssize_t
xbuf_read(struct xbuf *buf, int fd, size_t len, int timeoutms);

XEXTERN ssize_t
xbuf_write(struct xbuf *buf, int fd, size_t len, int timeoutms);

#endif

