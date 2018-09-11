#include "../include/crux/poll.h"
#include "../include/crux/hashmap.h"

#include "config.h"

#if HAS_KQUEUE
# include <sys/event.h>
#elif HAS_EPOLL
# include <sys/epoll.h>
#else
# error unsupported platform
#endif

struct xfdent {
	int fd;
	bool ein;  // is event scheduled for EPOLLIN/EVFILT_READ
	bool eout; // is event scheduled for EPOLLOUT/EVFILT_WRITE
	bool din;  // has dispatched EPOLLIN/EVFILT_READ
	bool dout; // has dispatched EPOLLOUT/EVFILT_WRITE
	void *ptr[2];
};

struct xfdmap {
	XHASHMAP(xfdmap, struct xfdent, 2);
};

struct xpoll {
	struct xclock clock;
	struct xfdmap fdmap;
	void *sig[31];
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

