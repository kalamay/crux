#ifndef CRUX_HUB_H
#define CRUX_HUB_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

struct xhub;

extern int
xhub_new (struct xhub **hubp);

extern void
xhub_free (struct xhub **hupb);

extern int
xhub_run (struct xhub *hub);

extern void
xhub_stop (struct xhub *hub);

#define xspawn(hub, fn, data) \
	xspawnf (hub, __FILE__, __LINE__, fn, data)

extern int
xspawnf (struct xhub *hub, const char *file, int line,
		void (*fn)(struct xhub *, void *), void *data);

#if defined (__BLOCKS__)

extern int
xspawn_b (struct xhub *hub, void (^block)(void));

#endif

extern void
xexit (int ec);

extern const struct xclock *
xclock (void);

extern int
xsleep (unsigned ms);

extern int
xsignal (int signum, int timeoutms);

extern ssize_t
xread (int fd, void *buf, size_t len, int timeoutms);

extern ssize_t
xreadv (int fd, struct iovec *iov, int iovcnt, int timeoutms);

extern ssize_t
xreadn (int fd, void *buf, size_t len, int timeoutms);

extern ssize_t
xwrite (int fd, const void *buf, size_t len, int timeoutms);

extern ssize_t
xwritev (int fd, const struct iovec *iov, int iovcnt, int timeoutms);

extern ssize_t
xwriten (int fd, const void *buf, size_t len, int timeoutms);

extern ssize_t
xrecvfrom (int s, void *buf, size_t len, int flags,
	 struct sockaddr *src_addr, socklen_t *src_len, int timeoutms);

extern ssize_t
xsendto (int s, const void *buf, size_t len, int flags,
	 const struct sockaddr *dest_addr, socklen_t dest_len, int timeoutms);

typedef ssize_t (*xio_fn) (int fd, void *buf, size_t len, int timeoutms);

extern ssize_t
xio (int fd, void *buf, size_t len, int timeoutms, xio_fn fn);

extern int
xpipe (int fds[static 2]);

extern int
xsocket (int domain, int type, int protocol);

extern int
xaccept (int s, struct sockaddr *addr, socklen_t *addrlen, int timeoutms);

extern int
xunblock (int fd);

extern int
xcloexec (int fd);

#endif

