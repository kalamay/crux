#include <string.h>

static const struct timespec zero = { 0, 0 };

int
xpoll__init (struct xpoll *poll)
{
    poll->fd = kqueue ();
	poll->rpos = 0;
	poll->rlen = 0;
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

	if (poll->rpos == poll->rlen) {
		poll->rpos = 0;
		poll->rlen = 0;
	}

	struct kevent *pev = poll->pev + poll->rlen;
	int nevents = xlen (poll->pev) - poll->rlen;

	int rc = kevent (poll->fd, NULL, 0, pev, nevents, ts);
	if (rc < 0) { return XERRNO; }
	if (rc == 0) { return -ETIMEDOUT; }
	poll->rlen += rc;
	return rc;
}

int
xpoll__next (struct xpoll *poll, struct xevent *dst)
{
	struct kevent *src = &poll->pev[poll->rpos++];

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
	struct kevent ev;
	EV_SET (&ev, id, type, op|EV_ONESHOT, 0, 0, ptr);
	int rc = kevent (poll->fd, &ev, 1, NULL, 0, &zero);
	return rc < 0 ? XERRNO : 0;
}

