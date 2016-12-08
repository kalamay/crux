#include "poll.h"
#include "../include/crux/err.h"
#include "../include/crux/common.h"
#include "../include/crux/clock.h"

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

static int
xpoll__init (struct xpoll *poll);

static void
xpoll__final (struct xpoll *poll);

static int
xpoll__more (struct xpoll *poll, int64_t ms, const struct timespec *ts);

static int
xpoll__add_in (struct xpoll *poll, int fd, void *ptr);

static int
xpoll__add_out (struct xpoll *poll, int fd, void *ptr);

static int
xpoll__add_sig (struct xpoll *poll, int signum, void *ptr);

static int
xpoll__del_in (struct xpoll *poll, int fd);

static int
xpoll__del_out (struct xpoll *poll, int fd);

static int
xpoll__del_sig (struct xpoll *poll, int signum);

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
    poll->fd = -1;
    poll->rpos = 0;
	poll->rlen = 0;
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
	if (poll == NULL) { return; }
	if (poll->fd >= 0) {
		close (poll->fd);
		poll->fd = -1;
	}
	xpoll__final (poll);
}

int
xpoll_add (struct xpoll *poll, int type, int id, void *ptr)
{
	assert (poll != NULL);

	switch (type & 0xFFFF) {

	case XPOLL_IN:
		if (id < 0) { return -EBADF; }
		return xpoll__add_in (poll, id, ptr);

	case XPOLL_OUT:
		if (id < 0) { return -EBADF; }
		return xpoll__add_out (poll, id, ptr);

	case XPOLL_SIG:
		if (id < 1 || id > 31) { return -EINVAL; }
		sigset_t mask = poll->sigmask;
		sigaddset (&mask, id);
		if (sigprocmask (SIG_SETMASK, &mask, NULL) < 0) {
			return XERRNO;
		}
		int rc = xpoll__add_sig (poll, id, ptr);
		if (rc < 0) {
			sigprocmask (SIG_SETMASK, &poll->sigmask, NULL);
			return rc;
		}
		poll->sigmask = mask;
		return 0;

	default:
		return -EINVAL;
	}
}

int
xpoll_del (struct xpoll *poll, int type, int id)
{
	assert (poll != NULL);

	switch (type & 0xFFFF) {

	case XPOLL_IN:
		if (id < 0) { return -EBADF; }
		return xpoll__del_in (poll, id);

	case XPOLL_OUT:
		if (id < 0) { return -EBADF; }
		return xpoll__del_out (poll, id);

	case XPOLL_SIG:
		if (id < 1 || id > 31) { return -EINVAL; }
		sigset_t mask = poll->sigmask;
		sigdelset (&mask, id);
		if (sigprocmask (SIG_SETMASK, &mask, NULL) < 0) {
			return XERRNO;
		}
		int rc = xpoll__del_sig (poll, id);
		if (rc < 0) {
			sigprocmask (SIG_SETMASK, &poll->sigmask, NULL);
			return rc;
		}
		poll->sigmask = mask;
		return 0;

	default:
		return -EINVAL;
	}
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

	while (poll->rpos < poll->rlen) {
		rc = xpoll__copy (ev, &poll->pev[poll->rpos++]);
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

	assert (poll->rpos == poll->rlen); // all pending events must be processed
	poll->rpos = 0;
	poll->rlen = 0;

	rc = xpoll__more (poll, ms, tsp);
	if (rc > 0) {
		goto read;
	}

	xclock_real (&poll->clock);

	if (rc == -EINTR) {
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

