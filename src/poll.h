#include "../include/crux/poll.h"
#include "config.h"

#define XPOLL_HEADER \
	struct xclock clock; \
	sigset_t sigmask; \
	int fd; \
	uint16_t rpos, rlen

#if HAS_KQUEUE
#include <sys/event.h>

struct xpoll {
	XPOLL_HEADER;
	struct kevent pev[64];
};


#elif HAS_EPOLL
#include <sys/epoll.h>

struct xpoll {
	XPOLL_HEADER;
	void *sig[31];
	int sigfd;
	struct epoll_event pev[64];
};


#else
#error unsupported platform

#endif

extern int
xpoll_init (struct xpoll *poll);

extern void
xpoll_final (struct xpoll *poll);

