#include "../include/crux/poll.h"

#include "config.h"

#if HAS_KQUEUE
# include <sys/event.h>
#elif HAS_EPOLL
# include <sys/epoll.h>
#else
# error unsupported platform
#endif

struct xpoll {
	struct timespec clock;
	sigset_t sigset;
	int fd;
#if HAS_KQUEUE
	uint16_t rpos, rlen, wpos;
	struct kevent events[64];
#elif HAS_EPOLL
	int sigfd;
	uint16_t rpos, rlen;
	struct epoll_event events[64];
#endif
};

extern int
xpoll_init(struct xpoll *poll);

extern void
xpoll_final(struct xpoll *poll);

