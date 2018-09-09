#include "../../include/crux/hashmap.h"

#include <sys/signalfd.h>

static uint64_t
fdmap_hash(int k, size_t kn)
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
fdmap_has_key(const struct fdent *ent, int k, size_t kn)
{
	(void)kn;
	return ent->fd == k;
}

XHASHMAP_STATIC(fdmap, struct fdmap, int, struct fdent)

int
xpoll__init(struct xpoll *poll)
{
	int ec;

	poll->fd = poll->sigfd = -1;

	ec = fdmap_init(&poll->fdmap, 0.9, 512);
	if (ec < 0) { return ec; }

    poll->fd = epoll_create1(EPOLL_CLOEXEC);
	if (poll->fd < 0) { goto error; }

	poll->sigfd = signalfd(-1, &poll->sigset, SFD_NONBLOCK|SFD_CLOEXEC);
	if (poll->sigfd < 0) { goto error; }

	struct epoll_event ev;
	ev.events = EPOLLIN|EPOLLET;
	ev.data.fd = poll->sigfd;

	if (epoll_ctl(poll->fd, EPOLL_CTL_ADD, poll->sigfd, &ev) < 0) {
		goto error;
	}

	for (size_t i = 0; i < xlen(poll->sig); i++) {
		poll->sig[i] = NULL;
	}
	poll->rpos = 0;
	poll->rlen = 0;
	return 0;

error:
	ec = XERRNO;
	xpoll__final(poll);
	return ec;
}

void
xpoll__final(struct xpoll *poll)
{
	fdmap_final(&poll->fdmap);
	if (poll->fd >= 0) {
		xretry(close(poll->fd));
		poll->fd = -1;
	}
	if (poll->sigfd >= 0) {
		xretry(close(poll->sigfd));
		poll->sigfd = -1;
	}
}

bool
xpoll__has_more(struct xpoll *poll)
{
	return poll->rpos < poll->rlen;
}

int
xpoll__update(struct xpoll *poll, const struct timespec *ts)
{
	assert(!xpoll__has_more(poll));

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

int
xpoll__next(struct xpoll *poll, struct xevent *dst)
{
	assert(xpoll__has_more(poll));

	struct epoll_event *src = &poll->events[poll->rpos];

	if (!(src->events & (EPOLLIN|EPOLLOUT))) {
		poll->rpos++;
		return 0;
	}

	if (src->data.fd == poll->sigfd) {
		struct signalfd_siginfo info;
		ssize_t n = read(poll->sigfd, &info, sizeof(info));
		if (n != sizeof(info)) {
			if (errno == EAGAIN) {
				poll->rpos++;
				return 0;
			}
		}
		if (!sigismember(&poll->sigset, info.ssi_signo)) {
			return 0;
		}
		dst->ptr = poll->sig[info.ssi_signo-1];
		dst->id = info.ssi_signo;
		dst->type = XPOLL_SIG;
		return 1;
	}

	struct fdent *ent = fdmap_get(&poll->fdmap, src->data.fd, 0);

	dst->id = src->data.fd;

	if (src->events & EPOLLIN) {
		dst->ptr = ent ? ent->ptr[0] : NULL;
		dst->type = XPOLL_IN;
		if (src->events & EPOLLOUT) {
			src->events &= ~EPOLLIN;
		}
		else {
			poll->rpos++;
		}
	}
	else if (src->events & EPOLLOUT) {
		dst->ptr = ent ? ent->ptr[1] : NULL;
		dst->type = XPOLL_OUT;
		poll->rpos++;
	}

	if (src->events & EPOLLERR) {
		dst->type |= XPOLL_ERR;
		socklen_t l = sizeof(dst->errcode);
		getsockopt(src->data.fd, SOL_SOCKET, SO_ERROR, (void *)&dst->errcode, &l);
	}

	if (src->events & EPOLLHUP) {
		dst->type |= XPOLL_EOF;
	}

	return 1;
}

static int
ctl_sig(struct xpoll *poll, int id, void *ptr)
{
	// the sigset will be already be updated, so it just needs to be synced
	int rc = signalfd(poll->sigfd, &poll->sigset, SFD_NONBLOCK|SFD_CLOEXEC);
	if (rc < 0) { return XERRNO; }
	poll->sig[id - 1] = ptr;
	return 0;
}

static int
ctl_io_add(struct xpoll *poll, int type, int id, void *ptr)
{
	struct fdent *ent;
	int res = fdmap_reserve(&poll->fdmap, id, 0, &ent);
	if (res < 0) { return XERRNO; }

	struct epoll_event ev;
	int op;

	if (res == XHASHMAP_RESERVE_NEW) {
		ev.events = type == XPOLL_OUT ? EPOLLOUT|EPOLLONESHOT : EPOLLIN|EPOLLONESHOT;
		op = EPOLL_CTL_ADD;
	}
	else {
		ev.events = EPOLLIN|EPOLLOUT|EPOLLONESHOT;
		op = EPOLL_CTL_MOD;
	}

	ev.data.fd = id;

	if (epoll_ctl(poll->fd, op, id, &ev) < 0) {
		if (res == XHASHMAP_RESERVE_NEW) {
			fdmap_remove(&poll->fdmap, ent);
		}
		return XERRNO;
	}

	ent->fd = id;
	ent->events = ev.events;
	ent->ptr[type-1] = ptr;
	return 0;
}

static int
ctl_io_del(struct xpoll *poll, int type, int id)
{
	struct fdent *ent = fdmap_get(&poll->fdmap, id, 0);
	struct epoll_event ev = {0};
	int op = EPOLL_CTL_DEL;

	if (ent) {
		ev.events = ent->events & (type == XPOLL_OUT ? ~EPOLLOUT : ~EPOLLIN);
		if (ev.events & (EPOLLIN|EPOLLOUT)) {
			op = EPOLL_CTL_MOD;
			ev.data.fd = id;
		}
	}

	if (epoll_ctl(poll->fd, op, id, &ev) < 0) {
		return XERRNO;
	}

	// find matching events in the pending list and "disable" them
	struct epoll_event *p = poll->events, *pe = p + poll->rlen;
	for (; p < pe; p++) {
		if (p->data.fd == id) {
			p->events = ev.events;
		}
	}

	if (ent) {
		if (op == EPOLL_CTL_DEL) {
			fdmap_remove(&poll->fdmap, ent);
		}
		else {
			ent->events = ev.events;
		}
	}

	return 0;
}

int
xpoll__ctl(struct xpoll *poll, int op, int type, int id, void *ptr)
{
	if (id == poll->sigfd) { return XESYS(EINVAL); }

	if (type == XPOLL_SIG) {
		return ctl_sig(poll, id, ptr);
	}

	assert(type == XPOLL_IN || type == XPOLL_OUT);

	if (op == XPOLL_ADD) {
		return ctl_io_add(poll, type, id, ptr);
	}
	else {
		return ctl_io_del(poll, type, id);
	}
}

