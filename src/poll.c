#include "poll.h"
#include "../include/crux/err.h"
#include "../include/crux/common.h"
#include "../include/crux/clock.h"

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

static int
xpoll__init (struct xpoll *poll);

static void
xpoll__final (struct xpoll *poll);

static int
xpoll__ctl (struct xpoll *poll, int op, int type, int id, void *ptr);

static bool
xpoll__has_more (struct xpoll *poll);

static int
xpoll__update (struct xpoll *poll, int64_t ms, const struct timespec *ts);

static int
xpoll__next (struct xpoll *poll, struct xevent *dst);

#if HAS_KQUEUE
#include "poll/kqueue.c"

#elif HAS_EPOLL
#include "poll/epoll.c"

#endif

int
xpoll_new (struct xpoll **pollp)
{
	struct xpoll *poll = malloc (sizeof *poll);
	if (poll == NULL) {
		return XERRNO;
	}

	int rc = xpoll_init (poll);
	if (rc < 0) {
		free (poll);
		return rc;
	}

	*pollp = poll;
	return 0;
}

int
xpoll_init (struct xpoll *poll)
{
	int rc = xclock_real (&poll->clock);
	if (rc < 0) { return rc; }
	sigemptyset (&poll->sigmask);
	return xpoll__init (poll);
}

void
xpoll_free (struct xpoll **pollp)
{
	struct xpoll *poll = *pollp;
	if (poll != NULL) {
		*pollp = NULL;
		xpoll_final (poll);
		free (poll);
	}
}

void
xpoll_final (struct xpoll *poll)
{
	if (poll != NULL) { 
		xpoll__final (poll);
	}
}

int
xpoll_ctl (struct xpoll *poll, int op, int type, int id, void *ptr)
{
	assert (poll != NULL);

	if (op != XPOLL_ADD && op == XPOLL_DEL) { return -EINVAL; }

	sigset_t mask;

	switch (type) {
	case XPOLL_IN:
	case XPOLL_OUT:
		if (id < 0) { return -EBADF; }
		break;
	case XPOLL_SIG:
		if (id < 1 || id > 31) { return -EINVAL; }
		mask = poll->sigmask;
		op == XPOLL_ADD ? sigaddset (&mask, id) : sigdelset (&mask, id);
		if (sigprocmask (SIG_SETMASK, &mask, NULL) < 0) {
			return XERRNO;
		}
		break;
	default:
		return -EINVAL;
	}

	int rc = xpoll__ctl (poll, op, type, id, ptr);
	if (type == XPOLL_SIG) {
		if (rc < 0) {
			sigprocmask (SIG_SETMASK, &poll->sigmask, NULL);
		}
		else {
			poll->sigmask = mask;
		}
	}
	return rc;
}

int
xpoll (struct xpoll *poll, int64_t ms, struct xevent *ev)
{
	int rc;
	struct timespec ts = { 0, 0 }, *tsp = NULL;
	int64_t abstime;

	ev->poll = poll;
	ev->type = 0;
	ev->id = -1;
	ev->ptr = NULL;
	ev->errcode = -1;

read:
	xclock_real (&poll->clock);

	while (xpoll__has_more (poll)) {
		rc = xpoll__next (poll, ev);
		if (rc != 0) {
			return rc;
		}
	}

poll:
	if (ms >= 0) {
		// capture absolute time to remove elapsed time ms later
		abstime = XCLOCK_ABS_MSEC (&poll->clock, ms);

		// calculate timeout in milliseconds as a timespec
		ts.tv_sec = ms / 1000;
		ts.tv_nsec = (ms % 1000) * 1000000;
		tsp = &ts;
	}

	rc = xpoll__update (poll, ms, tsp);
	if (rc > 0) {
		goto read;
	}

	xclock_real (&poll->clock);

	if (rc == -ETIMEDOUT) {
		return 0;
	}

	if (rc == 0 || rc == -EINTR) {
		if (ms >= 0) {
			ms = XCLOCK_REL_MSEC (&poll->clock, abstime);
			if (ms < 0) {
				ms = 0;
			}
		}
		goto poll;
	}
	return rc;
}

const struct xclock *
xpoll_clock (const struct xpoll *poll)
{
	return &poll->clock;
}

