#include "../include/crux/poll.h"
#include "config.h"

#if HAS_KQUEUE
#include <sys/event.h>

struct xpoll {
	struct xclock clock;
	int fd;
	uint16_t rpos, rlen, wpos;
	struct kevent events[64];
};


#elif HAS_EPOLL
#include <sys/epoll.h>

struct xpoll {
	struct xclock clock;
	void *sig[31];
	int fd, sigfd;
	uint16_t rpos, rlen;
	struct epoll_event events[64];
};


#else
#error unsupported platform

#endif

extern int
xpoll_init (struct xpoll *poll);

extern void
xpoll_final (struct xpoll *poll);

