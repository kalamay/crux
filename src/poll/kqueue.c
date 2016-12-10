#include <string.h>

static const struct timespec zero = { 0, 0 };

int
xpoll__init (struct xpoll *poll)
{
    poll->fd = kqueue ();
	poll->rpos = 0;
	poll->rlen = 0;
	poll->wpos = 0;
	return poll->fd < 0 ? XERRNO : 0;
}

void
xpoll__final (struct xpoll *poll)
{
	if (poll->fd >= 0) {
		close (poll->fd);
		poll->fd = -1;
	}
}

bool
xpoll__has_more (struct xpoll *poll)
{
	return poll->rpos < poll->rlen;
}

int
xpoll__update (struct xpoll *poll, const struct timespec *ts)
{
	assert (!xpoll__has_more (poll));

	struct kevent *events = poll->events, *changes = events + poll->rlen;
	int nevents = xlen (poll->events), nchanges = poll->wpos - poll->rlen;

	int rc = kevent (poll->fd, changes, nchanges, events, nevents, ts);
	if (rc < 0) { return XERRNO; }

	poll->rpos = 0;

	if (rc == 0) {
		assert (nchanges == 0);
		poll->rlen = 0;
		goto done;
	}

	// remove all successful EV_RECEIPT events
	struct kevent *p = events, *pe = p + rc, *mark = p;
	for (; p < pe; p++) {
		if ((p->flags & EV_ERROR) && p->data == 0) {
			// count all sequential receipts
			rc--;
			continue;
		}
		// only do a memmove if a non-receipt event is found
		memmove (mark, p, (pe - p) * sizeof (*p));
		pe = p - rc;
		p = mark;
		mark++;
	}
	poll->rlen = rc;
	if (!rc) { rc = -EINTR; }

done:
	poll->wpos = poll->rlen;
	return rc;
}

int
xpoll__next (struct xpoll *poll, struct xevent *dst)
{
	struct kevent *src = &poll->events[poll->rpos++];

	// EV_RECEIPT uses EV_ERROR to set registration status, but these should be
	// removed by xpoll__update. However, this behavior is also used to mark
	// events that have been removed. If data is 0, no error occurred.
	if ((src->flags & EV_ERROR) && src->data == 0) {
		return 0;
	}

	switch (src->filter) {
	case EVFILT_READ:   dst->type = XPOLL_IN;  break;
	case EVFILT_WRITE:  dst->type = XPOLL_OUT; break;
	case EVFILT_SIGNAL: dst->type = XPOLL_SIG; break;
	default:
		return 0;
	}

	dst->ptr = src->udata;
	dst->id = (int)src->ident;

	if (dst->type == XPOLL_SIG) {
		signal (dst->id, SIG_DFL);
	}

	if (src->flags & EV_ERROR) {
		dst->type |= XPOLL_ERR;
		dst->errcode = (int)src->data;
	}

	if (src->flags & EV_EOF) {
		dst->type |= XPOLL_EOF;
	}

	return 1;
}

int
xpoll__ctl (struct xpoll *poll, int op, int type, int id, void *ptr)
{
#ifdef __NetBSD__
	// NetBSD uses different filter values:
	//     #define EVFILT_READ    0U
	//     #define EVFILT_WRITE   1U
	//     #define EVFILT_AIO     2U
	//     #define EVFILT_VNODE   3U
	//     #define EVFILT_PROC    4U
	//     #define EVFILT_SIGNAL  5U
	//     #define EVFILT_TIMER   6U
	type = -1 - type;
#endif

	// if deleting, find matching events in the pending list and "disable" them
	if (op == XPOLL_DEL && poll->rlen > 0) {
		struct kevent *p = poll->events, *pe = p + poll->rlen;
		for (; p < pe; p++) {
			if (p->filter == type && p->ident == (uintptr_t)id) {
				p->flags |= EV_ERROR;
				p->data = 0;
			}
		}
	}

	// signal events must be registered immediately, otherwise only if full
	if (type == EVFILT_SIGNAL || poll->wpos == xlen (poll->events)) {
		struct kevent ev;
		EV_SET (&ev, id, type, op|EV_ONESHOT, 0, 0, ptr);
		int rc = kevent (poll->fd, &ev, 1, NULL, 0, &zero);
		if (rc < 0) { return XERRNO; }
		signal (id, XPOLL_ADD ? SIG_IGN : SIG_DFL);
		return 0;
	}

	// queue up the event change for the next xpoll__update
	struct kevent *ev = &poll->events[poll->wpos++];
	EV_SET (ev, id, type, op|EV_ONESHOT|EV_RECEIPT, 0, 0, ptr);
	return 0;
}

