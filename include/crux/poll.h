#ifndef CRUX_POLL_H
#define CRUX_POLL_H

#include <signal.h>

#include "clock.h"

#define XPOLL_IN  1
#define XPOLL_OUT 2
#define XPOLL_SIG 3

#define XPOLL_ERR (1 << 16)
#define XPOLL_EOF (1 << 17)

#define XEVENT_TYPE(ev) \
	(int)((ev)->type & 0xFFFF)

#define XEVENT_ISERR(ev) \
	(!!((ev)->type & XPOLL_ERR))

#define XEVENT_ISEOF(ev) \
	(!!((ev)->type & XPOLL_EOF))

struct xpoll;

struct xevent {
	struct xpoll *poll;
	void *ptr;
	int id;
	int type;
	int errcode;
};

extern int
xpoll_new (struct xpoll **pollp);

extern void
xpoll_free (struct xpoll **pollp);

extern int
xpoll_add (struct xpoll *poll, int type, int id, void *ptr);

extern int
xpoll_del (struct xpoll *poll, int type, int id);

/**
 * <0 on error
 *  0 on timeout
 *  1 on event
 */
extern int
xpoll (struct xpoll *poll, int64_t ms, struct xevent *ev);

extern const struct xclock *
xpoll_clock (const struct xpoll *poll);

#endif

