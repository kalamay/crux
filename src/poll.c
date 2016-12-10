#include "poll.h"
#include "../include/crux/err.h"
#include "../include/crux/common.h"
#include "../include/crux/clock.h"

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

/**
 * @brief  Implementation-specific initialization function
 *
 * @param  poll  partially initialized poll instance
 * @return  0 on success, -errno on error
 */
static int
xpoll__init (struct xpoll *poll);

/**
 * @brief  Implementation-specific finalization function
 *
 * @param  poll  partially finalized poll instance
 */
static void
xpoll__final (struct xpoll *poll);

/**
 * @brief  Modifies an event of interest for the poller
 *
 * @param  poll  poll instance
 * @param  op    operation type to perform (XPOLL_ADD or XPOLL_DEL)
 * @param  type  type of event (XPOLL_IN, XPOLL_OUT, or XPOLL_SIG)
 * @param  id    identifier for the event (file descriptor or signal number)
 * @param  ptr   user-supplied pointer to associate with the event
 * @return  0 on success, -errno on error
 */
static int
xpoll__ctl (struct xpoll *poll, int op, int type, int id, void *ptr);

/**
 * @brief  Checks if the poller has more event in the event list
 *
 * @param  poll  poll instance
 * @return  `true` if there are more events
 */
static bool
xpoll__has_more (struct xpoll *poll);

/**
 * @brief  Reads new events into the poller
 *
 * This function should return the number of events acquired. If a timeout
 * is reached, it shuold return 0. Errors should be returned using XERRNO,
 * however, the value `-EINTR` directs the poller to re-invoke the update
 * with an adjusted timeout.
 *
 * This is allowed to reset any events in poller. It should only be called
 * once the event list is cleared.
 *
 * @param  poll  poll instance
 * @param  ts    timeout duration or `NULL` for infinite
 * @return  number of events acquired
 */
static int
xpoll__update (struct xpoll *poll, const struct timespec *ts);

/**
 * @brief  Copies the next event into `dst`
 *
 * This function may return `0` to indicate that the next event shouldn't be
 * copied. In this case, the poller will call `xpoll__next` again as long as
 * `xpoll__has_more` returns `true`. Therefore, this function must still
 * advance the event list position.
 *
 * @param  poll  poll instance
 * @param  dst   target to copy the event information to
 * @return  1 if copied, 0 to ignore the next event, -errno on error
 */
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
	int rc = xclock_mono (&poll->clock);
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

	if (op != XPOLL_ADD && op != XPOLL_DEL) { return -EINVAL; }

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
xpoll_wait (struct xpoll *poll, int64_t ms, struct xevent *ev)
{
	int rc;
	struct xclock wait;
	struct timespec *tsp = NULL;
	int64_t abs, rel;

	ev->poll = poll;
	ev->type = 0;
	ev->id = -1;
	ev->ptr = NULL;
	ev->errcode = -1;

	if (ms >= 0) {
		xclock_mono (&poll->clock);
		abs = X_MSEC_TO_NSEC (ms) + XCLOCK_NSEC (&poll->clock);
	}

read:
	while (xpoll__has_more (poll)) {
		rc = xpoll__next (poll, ev);
		if (rc != 0) {
			goto done;
		}
	}

poll:
	if (ms >= 0) {
		xclock_mono (&poll->clock);
		rel = abs - XCLOCK_NSEC (&poll->clock);
		if (rel < 0) { rel = 0; }
		XCLOCK_SET_NSEC (&wait, rel);
		tsp = &wait.ts;
	}

	rc = xpoll__update (poll, tsp);
	if (rc > 0)       { goto read; }
	if (rc == -EINTR) { goto poll; }

done:
	xclock_mono (&poll->clock);
	return rc;
}

const struct xclock *
xpoll_clock (const struct xpoll *poll)
{
	return &poll->clock;
}

