#include "poll.h"
#include "../include/crux/def.h"
#include "../include/crux/err.h"
#include "../include/crux/clock.h"

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

static uint64_t
xfdmap_hash(int k, size_t kn)
{
	(void)kn;
	uint32_t a = (uint32_t)k;
	a = (a+0x7ed55d16) + (a<<12);
	a = (a^0xc761c23c) ^ (a>>19);
	a = (a+0x165667b1) + (a<<5);
	a = (a+0xd3a2646c) ^ (a<<9);
	a = (a+0xfd7046c5) + (a<<3);
	a = (a^0xb55a4f09) ^ (a>>16);
	return a;
}

static bool
xfdmap_has_key(const struct xfdent *ent, int k, size_t kn)
{
	(void)kn;
	return ent->fd == k;
}

XHASHMAP_STATIC(xfdmap, struct xfdmap, int, struct xfdent)

#if HAS_KQUEUE

#define c_init kq_init
#define c_final kq_final
#define c_ctl_io kq_ctl_io
#define c_ctl_sig kq_ctl_sig
#define c_next kq_next
#define c_wait kq_wait

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
kq_ctl_io(struct xpoll *poll, int op, bool ein, bool eout, int type, int id)
{
	struct kevent ev[2];
	size_t i = 0;

	if (op == XPOLL_ADD) {
		if ((type & XPOLL_IN) && !ein) {
			EV_SET(&ev[i++], id, EVFILT_READ, EV_ADD|EV_CLEAR, 0, 0, NULL);
		}
		if ((type & XPOLL_OUT) && !eout) {
			EV_SET(&ev[i++], id, EVFILT_WRITE, EV_ADD|EV_CLEAR, 0, 0, NULL);
		}
	}
	else {
		if ((type & XPOLL_IN) && ein) {
			EV_SET(&ev[i++], id, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		}
		if ((type & XPOLL_OUT) && !eout) {
			EV_SET(&ev[i++], id, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		}
	}

	if (i < xlen(poll->events) - poll->wpos) {
		ev[0].flags |= EV_RECEIPT;
		ev[1].flags |= EV_RECEIPT;
		memcpy(poll->events+poll->wpos, ev, sizeof(ev[0]) * i);
		poll->wpos += i;
		return 0;
	}

	if (kevent(poll->fd, ev, i, NULL, 0, NULL) < 0) {
		return XERRNO;
	}
	return 0;
}

static int
kq_ctl_sig(struct xpoll *poll, int op, int id)
{
	// signal events must be registered immediately
	struct kevent ev;
	EV_SET(&ev, id, EVFILT_SIGNAL, op|EV_CLEAR, 0, 0, NULL);
	int rc = kevent(poll->fd, &ev, 1, NULL, 0, NULL);
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
		ev->errcode = (int)src->data;
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
ep_ctl_io(struct xpoll *poll, int op, bool ein, bool eout, int type, int id)
{
	if (id == poll->sigfd) { return XESYS(EINVAL); }

	struct epoll_event ev = { EPOLLET, { .fd = id } };

	if (op == XPOLL_ADD) {
		if ((type & XPOLL_IN) || ein) { ev.events |= EPOLLIN; }
		if ((type & XPOLL_OUT) || eout) { ev.events |= EPOLLOUT; }
		op = ein || eout ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
	}
	else {
		if (!(type & XPOLL_IN) && ein) { ev.events |= EPOLLIN; }
		if (!(type & XPOLL_OUT) && eout) { ev.events |= EPOLLOUT; }
		op = ev.events & (EPOLLIN|EPOLLOUT) ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
	}

	if (epoll_ctl(poll->fd, op, id, &ev) < 0) {
		return XERRNO;
	}

	return 0;
}

static int
ep_ctl_sig(struct xpoll *poll, int op, int id)
{
	(void)op;
	(void)id;
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

	// if there is a HUP/ERR event without IN or OUT readiness, sumulate them based
	// on the fdmap entry for the fd
	if ((src->events & (EPOLLHUP|EPOLLERR)) && !(src->events & (EPOLLIN|EPOLLOUT))) {
		struct xfdent *ent = xfdmap_get(&poll->fdmap, src->data.fd, 0);
		if (ent && ent->ein) { src->events |= EPOLLIN; }
		if (ent && ent->eout) { src->events |= EPOLLOUT; }
	}

	ev->id = src->data.fd;

	if (src->events & EPOLLIN) {
		ev->type = XPOLL_IN;
		if (src->events & EPOLLOUT) {
			src->events &= ~EPOLLIN;
			poll->rpos--;
		}
	}
	else if (src->events & EPOLLOUT) {
		ev->type = XPOLL_OUT;
	}

	if (src->events & EPOLLERR) {
		ev->type |= XPOLL_ERR;
		socklen_t l = sizeof(ev->errcode);
		getsockopt(src->data.fd, SOL_SOCKET, SO_ERROR, (void *)&ev->errcode, &l);
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

	if ((rc = xfdmap_init(&poll->fdmap, 0.9, 0)) < 0) {
		return rc;
	}

	for (size_t i = 0; i < xlen(poll->sig); i++) {
		poll->sig[i] = NULL;
	}

	sigemptyset(&poll->sigset);

	if ((rc = c_init(poll)) < 0) {
		xfdmap_final(&poll->fdmap);
	}

	poll->rpos = 0;
	poll->rlen = 0;

	return rc;
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
		xfdmap_final(&poll->fdmap);
		c_final(poll);
	}
}

static int
ctl_io(struct xpoll *poll, int op, int type, int id, void *ptr)
{
	struct xfdent *ent;
	int res, rc = 0;
	bool ein, eout;

	if ((res = xfdmap_reserve(&poll->fdmap, id, 0, &ent)) < 0) {
		return res;
	}

	// if we get a new map entry it needs to be initialized
	if (res == XHASHMAP_RESERVE_NEW) {
		ent->fd = id;
		ent->ein = false;
		ent->eout = false;
		ent->ptr[0] = ent->ptr[1] = NULL;
	}

	ein = ent->ein;
	eout = ent->eout;

	// figure out the desired ein and eout flags
	if (op == XPOLL_ADD) {
		if (type & XPOLL_IN) { ein = true; }
		if (type & XPOLL_OUT) { eout = true; }
	}
	else {
		if (type & XPOLL_IN) { ein = false; }
		if (type & XPOLL_OUT) { eout = false; }
	}

	// if the flags differ, we need to update the underlying poll
	if (ein != ent->ein || eout != ent->eout) {
		rc = c_ctl_io(poll, op, ent->ein, ent->eout, type, id);
		if (rc == 0) {
			ent->ein = ein;
			ent->eout = eout;
		}
	}

	// if either flag is set we will keep the record
	if (ent->ein || ent->eout) {
		// only save the additions if no error occurred
		if (rc == 0 && op == XPOLL_ADD) {
			if (type & XPOLL_IN) {
				ent->din = false;
				ent->ptr[0] = ptr;
			}
			if (type & XPOLL_OUT) {
				ent->dout = false;
				ent->ptr[1] = ptr;
			}
		}
	}
	// no flag is set so remove the entry
	else {
		xfdmap_remove(&poll->fdmap, ent);
	}

	return rc;
}

static int
ctl_sig(struct xpoll *poll, int op, int id, void *ptr)
{
	sigset_t prev = poll->sigset;
	int rc = 0;
	bool change = false;

	if (op == XPOLL_ADD) {
		if (!sigismember(&prev, id)) {
			sigaddset(&poll->sigset, id);
			change = true;
		}
	}
	else {
		if (sigismember(&prev, id)) {
			sigdelset(&poll->sigset, id);
			change = true;
		}
	}

	if (change) {
		if (sigprocmask(SIG_SETMASK, &poll->sigset, NULL) < 0) {
			rc = XERRNO;
			poll->sigset = prev;
		}
		else if ((rc = c_ctl_sig(poll, op, id)) < 0) {
			sigprocmask(SIG_SETMASK, &prev, NULL);
			poll->sigset = prev;
		}
	}

	if (rc == 0) {
		poll->sig[id-1] = ptr;
	}

	return rc;
}

int
xpoll_ctl(struct xpoll *poll, int op, int type, int id, void *ptr)
{
	assert(poll != NULL);

	if (xunlikely(op != XPOLL_ADD && op != XPOLL_DEL)) {
		return XESYS(EINVAL);
	}

	if (xlikely(type & XPOLL_INOUT)) {
		if (id < 0) { return XESYS(EBADF); }
		return ctl_io(poll, op, type, id, ptr);
	}

	if (type & XPOLL_SIG) {
		if (id < 1 || id > 31) { return XESYS(EINVAL); }
		return ctl_sig(poll, op, id, ptr);
	}

	return XESYS(EINVAL);
}

void *
xpoll_get(struct xpoll *poll, int type, int id)
{
	assert(poll != NULL);

	if (type == XPOLL_SIG) {
		if (id >= 1 && id <= 31) {
			return poll->sig[id-1];
		}
	}
	else if (type & (XPOLL_IN|XPOLL_OUT)) {
		struct xfdent *ent = xfdmap_get(&poll->fdmap, id, 0);
		if (ent) {
			if (type == XPOLL_IN) { return ent->ein ? ent->ptr[0] : NULL; }
			if (type == XPOLL_OUT) { return ent->eout ? ent->ptr[1] : NULL; }
		}
	}
	return NULL;
}

static int
next(struct xpoll *poll, struct xevent *ev)
{
	int rc = c_next(poll, ev);
	if (rc != 1) { return rc; }

	if (ev->type & XPOLL_SIG) {
		if (!sigismember(&poll->sigset, ev->id)) {
			return 0;
		}
		ev->ptr = poll->sig[ev->id-1];
		return 1;
	}

	struct xfdent *ent = xfdmap_get(&poll->fdmap, ev->id, 0);
	if (ent == NULL) {
		return 0;
	}

	rc = 0;

	if ((ev->type & XPOLL_IN) && ent->ein) {
		if (!ent->din) {
			ent->din = true;
			rc = 1;
		}
		if (ev->type & (XPOLL_ERR|XPOLL_EOF)) {
			ent->ein = false;
			rc = 1;
		}
		ev->ptr = ent->ptr[0];
	}

	if ((ev->type & XPOLL_OUT) && ent->eout) {
		if (!ent->dout) {
			ent->dout = true;
			rc = 1;
		}
		if (ev->type & (XPOLL_ERR|XPOLL_EOF)) {
			ent->eout = false;
			rc = 1;
		}
		ev->ptr = ent->ptr[1];
	}

	if (!ent->ein && !ent->eout) {
		xfdmap_remove(&poll->fdmap, ent);
	}

	return rc;
}

int
xpoll_wait(struct xpoll *poll, int64_t ms, struct xevent *ev)
{
	int rc;
	struct xclock c;
	struct timespec *tsp = ms < 0 ? NULL : &c.ts;
	struct xtimeout timeo;

	ev->poll = poll;
	ev->type = 0;
	ev->id = -1;
	ev->ptr = NULL;
	ev->errcode = -1;

	xclock_mono(&poll->clock);
	xtimeout_start(&timeo, ms, &poll->clock);

next:
	while (poll->rpos < poll->rlen) {
		rc = next(poll, ev);
		if (rc != 0) { goto done; }
	}

wait:
	xclock_mono(&poll->clock);
	XCLOCK_SET_MSEC(&c, xtimeout(&timeo, &poll->clock));

	xfdmap_condense(&poll->fdmap, 64);
	rc = c_wait(poll, tsp);
	if (rc > 0)             { goto next; }
	if (rc == XESYS(EINTR)) { goto wait; }

done:
	xclock_mono(&poll->clock);
	return rc;
}

const struct xclock *
xpoll_clock(const struct xpoll *poll)
{
	return &poll->clock;
}

