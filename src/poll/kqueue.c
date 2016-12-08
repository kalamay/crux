static const struct timespec zero = { 0, 0 };

static int
update (struct xpoll *poll, struct kevent *ev, int n, const struct timespec *ts)
{
	int rc = kevent (poll->fd,
			ev, n,
			poll->pev + poll->rlen, xlen (poll->pev) - poll->rlen,
			ts);
	if (rc < 0) {
		return XERRNO;
	}
	poll->rlen += rc;
	return rc;
}

int
xpoll__init (struct xpoll *poll)
{
	int fd = kqueue ();
	if (fd < 0) { return XERRNO; }
	sigemptyset (&poll->sigmask);
    poll->fd = fd;
	return 0;
}

void
xpoll__final (struct xpoll *poll)
{
	(void)poll;
}

int
xpoll__more (struct xpoll *poll, int64_t ms, const struct timespec *ts)
{
	(void)ms;
	return update (poll, NULL, 0, ts);
}

int
xpoll__add_in (struct xpoll *poll, int fd, void *ptr)
{
	struct kevent ev;
	EV_SET (&ev, fd, EVFILT_READ, EV_ADD|EV_ONESHOT, 0, 0, ptr);
	int rc = update (poll, &ev, 1, &zero);
	return rc < 0 ? rc : 0;
}

int
xpoll__add_out (struct xpoll *poll, int fd, void *ptr)
{
	struct kevent ev;
	EV_SET (&ev, fd, EVFILT_WRITE, EV_ADD|EV_ONESHOT, 0, 0, ptr);
	int rc = update (poll, &ev, 1, &zero);
	return rc < 0 ? rc : 0;
}

int
xpoll__add_sig (struct xpoll *poll, int signum, void *ptr)
{
	struct kevent ev;
	EV_SET (&ev, signum, EVFILT_SIGNAL, EV_ADD|EV_ONESHOT, 0, 0, ptr);
	int rc = update (poll, &ev, 1, &zero);
	return rc < 0 ? rc : 0;
}

int
xpoll__del_in (struct xpoll *poll, int fd)
{
	struct kevent ev;
	EV_SET (&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	int rc = update (poll, &ev, 1, &zero);
	return rc < 0 ? rc : 0;
}

int
xpoll__del_out (struct xpoll *poll, int fd)
{
	struct kevent ev;
	EV_SET (&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	int rc = update (poll, &ev, 1, &zero);
	return rc < 0 ? rc : 0;
}

int
xpoll__del_sig (struct xpoll *poll, int signum)
{
	struct kevent ev;
	EV_SET (&ev, signum, EVFILT_SIGNAL, EV_DELETE, 0, 0, NULL);
	int rc = update (poll, &ev, 1, &zero);
	return rc < 0 ? rc : 0;
}

int
xpoll__copy (struct xevent *dst, const struct kevent *src)
{
	dst->ptr = src->udata;
	dst->id = (int)src->ident;

	switch (src->filter) {
	case EVFILT_READ:   dst->type = XPOLL_IN;   break;
	case EVFILT_WRITE:  dst->type = XPOLL_OUT;  break;
	case EVFILT_SIGNAL: dst->type = XPOLL_SIG;  break;
	default:
		return 0;
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

