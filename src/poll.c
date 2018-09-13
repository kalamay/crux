#include "poll.h"
#include "../include/crux/def.h"
#include "../include/crux/err.h"
#include "../include/crux/clock.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#if HAS_KQUEUE

#define c_init kq_init
#define c_final kq_final
#define c_ctl_io kq_ctl_io
#define c_ctl_sig kq_ctl_sig
#define c_next kq_next
#define c_wait kq_wait

static const struct timespec zero = { 0, 0 };

static int
kq_init(struct xpoll *poll)
{
    poll->fd = kqueue();
	poll->wpos = 0;
	return poll->fd < 0 ? XERRNO : 0;
}

static void
kq_final(struct xpoll *poll)
{
	xretry(close(poll->fd));
}

static int
kq_ctl_io(struct xpoll *poll, int id, int oldtype, int newtype)
{
	struct kevent ev[2];
	size_t n = 0;

	if ((oldtype^newtype) & XPOLL_IN) {
		int flags = (newtype & XPOLL_IN) ? EV_ADD|EV_CLEAR : EV_DELETE;
		EV_SET(&ev[n++], id, EVFILT_READ, flags, 0, 0, NULL);
	}

	if ((oldtype^newtype) & XPOLL_OUT) {
		int flags = (newtype & XPOLL_OUT) ? EV_ADD|EV_CLEAR : EV_DELETE;
		EV_SET(&ev[n++], id, EVFILT_WRITE, flags, 0, 0, NULL);
	}

	uint16_t wpos = poll->wpos;

	// if the change list is full, register these events now
	if (n >= xlen(poll->events) - wpos) {
		if (kevent(poll->fd, ev, n, NULL, 0, &zero) < 0) {
			return XERRNO;
		}
	}
	else {
		ev[0].flags |= EV_RECEIPT;
		ev[1].flags |= EV_RECEIPT;
		memcpy(poll->events+wpos, ev, sizeof(ev[0]) * n);
		poll->wpos += n;
	}

	// synchronize delete operations
	for (size_t i = 0; i < n; i++) {
		if (!(ev[i].flags & EV_DELETE)) { continue; }
		for (uint16_t p = poll->rpos; p < wpos; p++) {
			if (poll->events[p].ident == (uintptr_t)id &&
					poll->events[p].filter == ev[i].filter) {
				poll->events[p].flags = EV_ERROR;
				poll->events[p].data = 0;
			}
		}
	}

	return 0;
}

static int
kq_ctl_sig(struct xpoll *poll, int id, int oldtype, int newtype)
{
	(void)oldtype;

	// signal events must be registered immediately
	int flags = (newtype & XPOLL_SIG) ? EV_ADD|EV_CLEAR : EV_DELETE;
	struct kevent ev;
	EV_SET(&ev, id, EVFILT_SIGNAL, flags, 0, 0, NULL);
	int rc = kevent(poll->fd, &ev, 1, NULL, 0, &zero);
	return rc < 0 ? XERRNO : 0;
}

static int
kq_next(struct xpoll *poll, struct xevent *ev)
{
	struct kevent *src = &poll->events[poll->rpos++];

	// EV_RECEIPT uses EV_ERROR to set registration status so ignore
	// these if data is 0. This behavior is also used to mark events
	// that have been removed.
	if ((src->flags & EV_ERROR) && src->data == 0) {
		return 0;
	}

	ev->id = (int)src->ident;
	switch (src->filter) {
	case EVFILT_READ: ev->type = XPOLL_IN; break;
	case EVFILT_WRITE: ev->type = XPOLL_OUT; break;
	case EVFILT_SIGNAL: ev->type = XPOLL_SIG; break;
	default:
		return 0;
	}

	if (src->flags & EV_ERROR) {
		ev->type |= XPOLL_ERR;
		ev->errcode = XESYS((int)src->data);
	}

	if (src->flags & EV_EOF) {
		ev->type |= XPOLL_EOF;
	}

	return 1;
}

static int
kq_wait(struct xpoll *poll, struct timespec *ts)
{
	struct kevent *events = poll->events, *changes = events + poll->rlen;
	int nevents = xlen(poll->events), nchanges = poll->wpos - poll->rlen;

	int rc = kevent(poll->fd, changes, nchanges, events, nevents, ts);
	if (rc < 0) { return XERRNO; }

	poll->rpos = 0;
	poll->rlen = rc;
	poll->wpos = poll->rlen;
	return rc;
}

#elif HAS_EPOLL

#define c_init ep_init
#define c_final ep_final
#define c_ctl_io ep_ctl_io
#define c_ctl_sig ep_ctl_sig
#define c_next ep_next
#define c_wait ep_wait

#include <sys/signalfd.h>

static int
ep_init(struct xpoll *poll)
{
	int rc;

    if ((poll->fd = epoll_create1(EPOLL_CLOEXEC)) < 0) {
		goto error;
	}

	if ((poll->sigfd = signalfd(-1, &poll->sigset, SFD_NONBLOCK|SFD_CLOEXEC)) < 0) {
		goto error;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN|EPOLLET;
	ev.data.fd = poll->sigfd;

	if (epoll_ctl(poll->fd, EPOLL_CTL_ADD, poll->sigfd, &ev) < 0) {
		goto error;
	}

	return 0;

error:
	rc = XERRNO;
	xretry(close(poll->sigfd));
	xretry(close(poll->fd));
	return rc;
}

static void
ep_final(struct xpoll *poll)
{
	xretry(close(poll->sigfd));
	xretry(close(poll->fd));
}

static int
ep_ctl_io(struct xpoll *poll, int id, int oldtype, int newtype)
{
	if (id == poll->sigfd) { return XESYS(EINVAL); }

	struct epoll_event ev = { EPOLLET, { .fd = id } };
	int op;

	if (newtype) {
		if (newtype & XPOLL_IN) { ev.events |= EPOLLIN; }
		if (newtype & XPOLL_OUT) { ev.events |= EPOLLOUT; }
		op = oldtype ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
	}
	else {
		op = EPOLL_CTL_DEL;
	}

	if (epoll_ctl(poll->fd, op, id, &ev) < 0) {
		return XERRNO;
	}

	if (op == EPOLL_CTL_DEL) {
		for (uint16_t p = poll->rpos, len = poll->rlen; p < len; p++) {
			if (poll->events[p].data.fd == id) {
				poll->events[p].events = 0;
			}
		}
	}

	return 0;
}

static int
ep_ctl_sig(struct xpoll *poll, int id, int oldtype, int newtype)
{
	(void)id;
	(void)oldtype;
	(void)newtype;

	int rc = signalfd(poll->sigfd, &poll->sigset, SFD_NONBLOCK|SFD_CLOEXEC);
	return rc < 0 ? XERRNO : 0;
}

static int
ep_next(struct xpoll *poll, struct xevent *ev)
{
	struct epoll_event *src = &poll->events[poll->rpos++];

	if (src->data.fd == poll->sigfd) {
		struct signalfd_siginfo info;
		ssize_t n = read(poll->sigfd, &info, sizeof(info));
		if (n == sizeof(info)) {
			ev->id = info.ssi_signo;
			ev->type = XPOLL_SIG;
			poll->rpos--;
			return 1;
		}
		return 0;
	}

	if (src->events == 0) {
		return 0;
	}

	ev->id = src->data.fd;
	ev->type = 0;

	if (src->events & EPOLLIN) {
		ev->type |= XPOLL_IN;
	}

	if (src->events & EPOLLOUT) {
		ev->type |= XPOLL_OUT;
	}

	if (src->events & EPOLLERR) {
		int ev;
		socklen_t l = sizeof(ev);
		getsockopt(src->data.fd, SOL_SOCKET, SO_ERROR, (void *)&ev, &l);

		ev->type |= XPOLL_ERR;
		ev->errcode = XESYS(errcode);
	}

	if (src->events & EPOLLHUP) {
		ev->type |= XPOLL_EOF;
	}

	return 1;
}

static int
ep_wait(struct xpoll *poll, struct timespec *ts)
{
	int ms = -1;
	if (ts) {
		int64_t t = X_NSEC_TO_MSEC(ts->tv_nsec) + X_SEC_TO_MSEC(ts->tv_sec);
		if (t >= 0) {
			ms = t > INT_MAX ? INT_MAX : (int)t;
		}
	}

	int rc = epoll_wait(poll->fd, poll->events, xlen(poll->events), ms);
	if (rc < 0) { return XERRNO; }

	poll->rpos = 0;
	poll->rlen = rc;
	return rc;
}

#endif

int
xpoll_new(struct xpoll **pollp)
{
	struct xpoll *poll = malloc(sizeof(*poll));
	if (poll == NULL) {
		return XERRNO;
	}

	int rc = xpoll_init(poll);
	if (rc < 0) {
		free(poll);
		return rc;
	}

	*pollp = poll;
	return 0;
}

int
xpoll_init(struct xpoll *poll)
{
	int rc;
	
	if ((rc = xclock_mono(&poll->clock)) < 0) {
		return rc;
	}

	sigemptyset(&poll->sigset);
	poll->rpos = 0;
	poll->rlen = 0;

	return c_init(poll);
}

void
xpoll_free(struct xpoll **pollp)
{
	struct xpoll *poll = *pollp;
	if (poll != NULL) {
		*pollp = NULL;
		xpoll_final(poll);
		free(poll);
	}
}

void
xpoll_final(struct xpoll *poll)
{
	if (poll != NULL) { 
		c_final(poll);
	}
}

int
xpoll_ctl(struct xpoll *poll, int id, int oldtype, int newtype)
{
	assert(poll != NULL);

	if (xunlikely(XPOLL_TYPE(oldtype ^ newtype) == 0)) {
		return 0;
	}

	int io = (oldtype|newtype) & XPOLL_INOUT;
	int sig = (oldtype|newtype) & XPOLL_SIG;

	if (xlikely(io && !sig)) {
		if (xunlikely(id < 0)) { return XESYS(EBADF); }
		return c_ctl_io(poll, id, oldtype, newtype);
	}

	if (sig && !io) {
		if (id < 1 || id > 31) { return XESYS(EINVAL); }

		sigset_t prev = poll->sigset;

		if (newtype & XPOLL_SIG) {
			sigaddset(&poll->sigset, id);
		}
		else {
			sigdelset(&poll->sigset, id);
		}

		if (sigprocmask(SIG_SETMASK, &poll->sigset, NULL) < 0) {
			poll->sigset = prev;
			return XERRNO;
		}

		int rc = c_ctl_sig(poll, id, oldtype, newtype);
		if (rc < 0) {
			sigprocmask(SIG_SETMASK, &prev, NULL);
			poll->sigset = prev;
			return rc;
		}

		return 0;
	}

	return XESYS(EINVAL);
}

int
xpoll_wait(struct xpoll *poll, int64_t ms, struct xevent *ev)
{
	int rc;
	struct timespec c;
	struct timespec *tsp = ms < 0 ? NULL : &c;
	struct xtimeout timeo;

	ev->type = 0;
	ev->id = -1;
	ev->errcode = 0;

	xclock_mono(&poll->clock);
	xtimeout_start(&timeo, ms, &poll->clock);

next:
	while (poll->rpos < poll->rlen) {
		rc = c_next(poll, ev);
		if (rc != 0) { goto done; }
	}

wait:
	xclock_mono(&poll->clock);
	XCLOCK_SET_MSEC(&c, xtimeout(&timeo, &poll->clock));

	rc = c_wait(poll, tsp);
	if (rc > 0)             { goto next; }
	if (rc == XESYS(EINTR)) { goto wait; }

done:
	xclock_mono(&poll->clock);
	return rc;
}

const struct timespec *
xpoll_clock(const struct xpoll *poll)
{
	return &poll->clock;
}

