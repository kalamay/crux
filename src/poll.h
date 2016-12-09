#include "../include/crux/poll.h"
#include "config.h"

#define XPOLL_HEADER \
	struct xclock clock; \
	sigset_t sigmask

#if HAS_KQUEUE
#include <sys/event.h>

struct xpoll {
	XPOLL_HEADER;
	int fd;
	uint16_t rpos, rlen, wpos;
	struct kevent pev[64];
};


#elif HAS_EPOLL
#include <sys/epoll.h>

struct xpoll {
	XPOLL_HEADER;
	void *sig[31];
	int fd, sigfd;
	uint16_t rpos, rlen;
	struct epoll_event pev[64];
};


#else
#error unsupported platform

#endif

extern int
xpoll_init (struct xpoll *poll);

extern void
xpoll_final (struct xpoll *poll);

