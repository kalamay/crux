#ifndef CRUX_POLL_H
#define CRUX_POLL_H

#include <signal.h>

#include "clock.h"

#define XPOLL_ADD 0x0001
#define XPOLL_DEL 0x0002

#define XPOLL_IN  (-1)
#define XPOLL_OUT (-2)
#define XPOLL_SIG (-6)

#define XPOLL_ERR (1 << 16)
#define XPOLL_EOF (1 << 17)

#define XPOLL_OP(n)    (int)((n) & 0x000F)
#define XPOLL_TYPE(n)  (int)((n) & 0x0F00)
#define XPOLL_ISERR(n) (!!((n) & XPOLL_ERR))
#define XPOLL_ISEOF(n) (!!((n) & XPOLL_EOF))

#define XEVENT_TYPE(ev)  XPOLL_TYPE((ev)->type)
#define XEVENT_ISERR(ev) XPOLL_ISERR((ev)->type)
#define XEVENT_ISEOF(ev) XPOLL_ISEOF((ev)->type)

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
xpoll_ctl (struct xpoll *poll, int op, int type, int id, void *ptr);

/**
 * <0 on error
 *  0 on timeout
 *  1 on event
 */
extern int
xpoll_wait (struct xpoll *poll, int64_t ms, struct xevent *ev);

extern const struct xclock *
xpoll_clock (const struct xpoll *poll);

#endif

