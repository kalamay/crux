#include "../include/crux/poll.h"

#if HAS_KQUEUE
# include <sys/event.h>
#elif HAS_EPOLL
# include <sys/epoll.h>
#else
# error unsupported platform
#endif

#define XPOLL_WAKE (1<<3)  /** Type for poll wake up */

struct xpoll
{
	struct timespec clock;
	sigset_t sigset;
	int fd;
#if HAS_KQUEUE
	uint16_t rpos, rlen, wpos;
	struct kevent events[64];
#elif HAS_EPOLL
	int sigfd, evfd;
	uint16_t rpos, rlen;
	struct epoll_event events[64];
#endif
};

XLOCAL int
xpoll_init(struct xpoll *poll);

XLOCAL void
xpoll_final(struct xpoll *poll);

