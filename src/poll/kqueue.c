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
xpoll__update (struct xpoll *poll, int64_t ms, const struct timespec *ts)
{
	(void)ms;

	struct kevent *events = poll->events, *changes = events + poll->rlen;
	int nevents = xlen (poll->events), nchanges = poll->wpos - poll->rlen;
	if (xpoll__has_more (poll)) {
		events += poll->rlen;
		nevents -= poll->rlen;
	}

	int rc = kevent (poll->fd, changes, nchanges, events, nevents, ts);
	if (rc < 0) { return XERRNO; }
	if (!xpoll__has_more (poll)) {
		poll->rpos = 0;
		poll->rlen = 0;
	}
	if (rc == 0) {
		assert (nchanges == 0);
		rc = -ETIMEDOUT;
		goto done;
	}

	struct kevent *p = events, *pe = p + rc, *mark = p;
	for (; p < pe; p++) {
		if ((p->flags & EV_ERROR) && p->data == 0) {
			rc--;
			continue;
		}
		memmove (mark, p, (pe - p) * sizeof (*p));
		pe = p - rc;
		p = mark;
		mark++;
	}
	poll->rlen += rc;

done:
	poll->wpos = poll->rlen;
	return rc;
}

int
xpoll__next (struct xpoll *poll, struct xevent *dst)
{
	struct kevent *src = &poll->events[poll->rpos++];

	switch (src->filter) {
	case EVFILT_READ:   dst->type = XPOLL_IN;  break;
	case EVFILT_WRITE:  dst->type = XPOLL_OUT; break;
	case EVFILT_SIGNAL: dst->type = XPOLL_SIG; break;
	default:
		return 0;
	}

	dst->ptr = src->udata;
	dst->id = (int)src->ident;

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
	if (type == EVFILT_SIGNAL || poll->wpos == xlen (poll->events)) {
		struct kevent ev;
		EV_SET (&ev, id, type, op|EV_ONESHOT, 0, 0, ptr);
		int rc = kevent (poll->fd, &ev, 1, NULL, 0, &zero);
		return rc < 0 ? XERRNO : 0;
	}
	struct kevent *ev = &poll->events[poll->wpos++];
	EV_SET (ev, id, type, op|EV_ONESHOT|EV_RECEIPT, 0, 0, ptr);
	return 0;
}

