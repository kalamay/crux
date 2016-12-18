#include "../include/crux/hub.h"
#include "../include/crux/err.h"
#include "../include/crux/list.h"
#include "../include/crux/common.h"

#include "task.h"
#include "heap.h"
#include "poll.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <assert.h>

struct xhub {
	struct xmgr mgr;
	struct xpoll poll;
	struct xheap timeout;
	struct xlist immediate;
	struct xlist pending;
	bool running;
};

struct xhub_entry {
	struct xheap_entry hent;
	struct xlist lent;
	struct xtask *t;
	void *data;
	struct xhub *hub;
	void (*fn)(struct xhub *, void *);
	int poll_id, poll_type;
};

static bool
is_scheduled (struct xhub_entry *ent)
{
	return (ent->poll_type != 0 ||
			ent->hent.key != XHEAP_NONE ||
			xlist_is_added (&ent->lent));
}

static int
schedule_immediate (struct xhub_entry *ent)
{
	xlist_add (&ent->hub->immediate, &ent->lent, X_ASCENDING);
	ent->poll_type = 0;
	return 0;
}

static int
schedule_timeout (struct xhub_entry *ent, int ms)
{
	struct xhub *h = ent->hub;
	ent->hent.prio = X_MSEC_TO_NSEC (ms) + XCLOCK_NSEC (&h->poll.clock);
	int rc = xheap_add (&h->timeout, &ent->hent);
	return rc < 0 ? rc : 0;
}

static int
schedule_poll (struct xhub_entry *ent, int id, int type, int timeoutms)
{
	int rc;
	if (timeoutms < 0) {
		ent->hent.key = XHEAP_NONE;
	}
	else {
		rc = schedule_timeout (ent, timeoutms);
		if (rc < 0) { return rc; }
	}

	struct xhub *h = ent->hub;
	rc = xpoll_ctl (&h->poll, XPOLL_ADD, type, id, ent);
	if (rc < 0) {
		xheap_remove (&h->timeout, &ent->hent);
		return rc;
	}

	xlist_add (&ent->hub->pending, &ent->lent, X_ASCENDING);
	ent->poll_id = id;
	ent->poll_type = type;
	return 0;
}

static void
unschedule (struct xhub_entry *ent)
{
	if (ent->poll_type) {
		xpoll_ctl (&ent->hub->poll, XPOLL_DEL, ent->poll_type, ent->poll_id, NULL);
	}

	if (xlist_is_added (&ent->lent)) {
		xlist_del (&ent->lent);
	}

	if (ent->hent.key != XHEAP_NONE) {
		xheap_remove (&ent->hub->timeout, &ent->hent);
	}
}

int
xhub_new (struct xhub **hubp)
{
	assert (hubp != NULL);

	struct xhub *hub = malloc (sizeof *hub);
	if (hub == NULL) {
		return XERRNO;
	}

	int rc;

	rc = xmgr_init (&hub->mgr, sizeof (struct xhub_entry), XTASK_STACK_DEFAULT, XTASK_FDEFAULT);
	if (rc < 0) {
		goto err_mgr;
	}

	rc = xpoll_init (&hub->poll);
	if (rc < 0) {
		goto err_poll;
	}

	rc = xheap_init (&hub->timeout);
	if (rc < 0) {
		goto err_heap;
	}

	xlist_init (&hub->immediate);
	xlist_init (&hub->pending);
	hub->running = false;

	*hubp = hub;
	return 0;

err_heap:
	xpoll_final (&hub->poll);
err_poll:
	xmgr_final (&hub->mgr);
err_mgr:
	free (hub);
	return rc;
}

static void
free_hent (struct xheap_entry *hent, void *data)
{
	(void)data;
	struct xhub_entry *ent = xcontainer (hent, struct xhub_entry, hent);
	unschedule (ent);
	xtask_free (&ent->t);
}

void
xhub_free (struct xhub **hubp)
{
	assert (hubp != NULL);

	struct xhub *hub = *hubp;
	if (hub != NULL) {
		*hubp = NULL;

		struct xhub_entry *ent;
		struct xlist *lent;

		xlist_each (&hub->immediate, lent, X_ASCENDING) {
			ent = xcontainer (lent, struct xhub_entry, lent);
			unschedule (ent);
			xtask_free (&ent->t);
		}

		xlist_each (&hub->pending, lent, X_ASCENDING) {
			ent = xcontainer (lent, struct xhub_entry, lent);
			unschedule (ent);
			xtask_free (&ent->t);
		}

		xheap_clear (&hub->timeout, free_hent, NULL);
		xheap_final (&hub->timeout);
		xpoll_final (&hub->poll);
		xmgr_final (&hub->mgr);
		free (hub);
	}
}

static int
run_once (struct xhub *hub)
{
	int rc, val = 0;
	int64_t ms;
	struct xlist *immediate;
	struct xheap_entry *wait;
	struct xevent ev;
	struct xhub_entry *ent;

	immediate = xlist_first (&hub->immediate, X_ASCENDING);
	if (immediate != NULL) {
		ent = xcontainer (immediate, struct xhub_entry, lent);
		goto invoke;
	}

	wait = xheap_get (&hub->timeout, XHEAP_ROOT);
	if (wait != NULL) {
		ms = X_NSEC_TO_MSEC (wait->prio - XCLOCK_NSEC (&hub->poll.clock));
		if (ms < 0) {
			ent = xcontainer (wait, struct xhub_entry, hent);
			goto timeout;
		}
	}
	else {
		if (xlist_is_empty (&hub->pending)) {
			return 0;
		}
		ms = -1;
	}

	rc = xpoll_wait (&hub->poll, ms, &ev);
	switch (rc) {
	case 0:
		ent = xcontainer (wait, struct xhub_entry, hent);
		if (ent->poll_type) {
			val = -ETIMEDOUT;
		}
		goto timeout;
	case 1:
		ent = ev.ptr;
		ent->poll_type = 0;
		goto invoke;
	default:
		return rc;
	}

timeout:
	xheap_remove (&hub->timeout, wait);

invoke:
	unschedule (ent);
	rc = xresume (ent->t, XINT (val)).i;
	if (!is_scheduled (ent)) {
		xtask_free (&ent->t);
		if (rc < 0) {
			return rc;
		}
	}
	return 1;
}

int
xhub_run (struct xhub *hub)
{
	if (hub->running) {
		return -EPERM;
	}

	hub->running = true;

	int rc;
	do {
		rc = run_once (hub);
	} while (hub->running && rc == 1);

	hub->running = false;

	return rc;
}

void
xhub_stop (struct xhub *hub)
{
	hub->running = false;
}

static union xvalue
spawn_fn (void *tls, union xvalue val)
{
	(void)val;
	struct xhub_entry *ent = tls;
	ent->fn (ent->hub, ent->data);
	return XZERO;
}

int
xspawn_at (struct xhub *hub, const char *file, int line,
		void (*fn)(struct xhub *, void *), void *data)
{
	struct xtask *t;
	struct xhub_entry *ent;
	int rc = xtask_newf (&t, &hub->mgr, NULL, file, line, spawn_fn);
	if (rc < 0) {
		return rc;
	}

	ent = xtask_local (t);
	ent->t = t;
	ent->data = data;
	ent->hub = hub;
	ent->fn = fn;

	return schedule_immediate (ent);
}

#if defined (__BLOCKS__)

#include <Block.h>

static void
spawn_block (struct xhub *h, void *data)
{
	(void)h;
	void (^block)(void) = data;
	block ();
	Block_release (block);
}

int
xspawn_b (struct xhub *hub, void (^block)(void))
{
	void (^copy)(void) = Block_copy (block);
	int rc = xspawn_at (hub, NULL, 0, spawn_block, copy);
	if (rc < 0) {
		Block_release (copy);
	}
	return rc;
}

#endif

const struct xclock *
xclock (void)
{
	struct xtask *t = xtask_self ();
	if (t == NULL) { return NULL; }

	struct xhub_entry *ent = xtask_local (t);
	if (ent == NULL) { return NULL; }

	return &ent->hub->poll.clock;
}

int
xsleep (unsigned ms)
{
	struct xtask *t = xtask_self ();
	if (t == NULL) {
		struct xclock c = XCLOCK_MAKE_MSEC (ms);
		int rc;
		do {
			rc = nanosleep (&c.ts, &c.ts);
			if (rc < 0) { rc = XERRNO; }
		} while (rc == -EINTR);
		return rc;
	}

	int rc = schedule_timeout (xtask_local (t), ms);
	if (rc == 0) {
		xyield (XZERO);
	}
	return rc;
}

void
xexit (int ec)
{
	struct xtask *t = xtask_self ();
	if (t == NULL) { exit (ec); }

	unschedule (xtask_local (t));
	xtask_exit (t, ec);
}

int
xsignal (int signum, int timeoutms)
{
	struct xtask *t = xtask_self ();
	if (t == NULL) { return -EPERM; }

	int rc = schedule_poll (xtask_local (t), signum, XPOLL_SIG, timeoutms);
	if (rc == 0) {
		int val = xyield (XZERO).i;
		return val ? val : signum;
	}
	return rc;
}

#define RECV(fd, ms, fn, ...) do { \
	ssize_t rc; \
again: \
	rc = fn (fd, __VA_ARGS__); \
	if (rc >= 0) { return rc; } \
	rc = XERRNO; \
	if (rc != -EAGAIN) { return rc; } \
	struct xtask *t = xtask_self (); \
	if (t == NULL) { return rc; } \
	rc = schedule_poll (xtask_local (t), fd, XPOLL_IN, ms); \
	if (rc < 0) { return rc; } \
	int val = xyield (XZERO).i; \
	if (val < 0) { return (ssize_t)val; } \
	goto again; \
} while (0)

#define SEND(fd, ms, fn, ...) do { \
	ssize_t rc; \
again: \
	rc = fn (fd, __VA_ARGS__); \
	if (rc >= 0) { return rc; } \
	rc = XERRNO; \
	if (rc != -EAGAIN) { return rc; } \
	struct xtask *t = xtask_self (); \
	if (t == NULL) { return rc; } \
	rc = schedule_poll (xtask_local (t), fd, XPOLL_OUT, ms); \
	if (rc < 0) { return rc; } \
	int val = xyield (XZERO).i; \
	if (val < 0) { return (ssize_t)val; } \
	goto again; \
} while (0)

ssize_t
xread (int fd, void *buf, size_t len, int timeoutms)
{
	RECV (fd, timeoutms, read, buf, len);
}

extern ssize_t
xreadv (int fd, struct iovec *iov, int iovcnt, int timeoutms)
{
	RECV (fd, timeoutms, readv, iov, iovcnt);
}

ssize_t
xreadn (int fd, void *buf, size_t len, int timeoutms)
{
	return xio (fd, buf, len, timeoutms, xread);
}

ssize_t
xwrite (int fd, const void *buf, size_t len, int timeoutms)
{
	SEND (fd, timeoutms, write, buf, len);
}

ssize_t
xwritev (int fd, const struct iovec *iov, int iovcnt, int timeoutms)
{
	SEND (fd, timeoutms, writev, iov, iovcnt);
}

ssize_t
xwriten (int fd, const void *buf, size_t len, int timeoutms)
{
	return xio (fd, (void *)buf, len, timeoutms, (xio_fn)xwrite);
}

extern ssize_t
xrecvfrom (int s, void *buf, size_t len, int flags,
	 struct sockaddr *src_addr, socklen_t *src_len, int timeoutms)
{
	RECV (s, timeoutms, recvfrom, buf, len, flags, src_addr, src_len);
}

ssize_t
xsendto (int s, const void *buf, size_t len, int flags,
	 const struct sockaddr *dest_addr, socklen_t dest_len, int timeoutms)
{
	SEND (s, timeoutms, sendto, buf, len, flags, dest_addr, dest_len);
}

ssize_t
xio (int fd, void *buf, size_t len, int timeoutms,
		ssize_t (*fn) (int fd, void *buf, size_t len, int timeoutms))
{
	if (len > SSIZE_MAX) {
		return -EINVAL;
	}

	struct xclock now;
	int64_t abs;
	if (timeoutms > 0) {
		xclock_mono (&now);
		abs = X_MSEC_TO_NSEC (timeoutms) + XCLOCK_NSEC (&now);
	}

	size_t total = 0;
	ssize_t rc;

again:
	rc = fn (fd, (uint8_t *)buf+total, len-total, timeoutms);
	if (rc < 0) { return rc; }
	total += (size_t)rc;
	if (total < len) {
		if (timeoutms > 0) {
			xclock_mono (&now);
			timeoutms = X_NSEC_TO_MSEC (abs - XCLOCK_NSEC (&now));
			if (timeoutms < 0) { timeoutms = 0; }
		}
		goto again;
	}
	return (ssize_t)total;
}

int
xpipe (int fds[static 2])
{
	// TODO: improve feature detection
#if defined(__linux__)
	int rc = pipe2 (fds, O_NONBLOCK|O_CLOEXEC);
	if (rc < 0) { return XERRNO; }
	return 0;
#else
	int rc = pipe (fds);
	if (rc < 0) { return XERRNO; }
	rc = xcloexec (xunblock (fds[0]));
	if (rc < 0) { goto error; }
	rc = xcloexec (xunblock (fds[1]));
	if (rc < 0) { goto error; }
	return 0;
error:
	close (fds[0]);
	close (fds[1]);
	return rc;
#endif
}

int
xsocket (int domain, int type, int protocol)
{
	int s, rc;

#if defined (SOCK_NONBLOCK) && defined (SOCK_CLOEXEC)
	s = socket (domain, type|SOCK_NONBLOCK|SOCK_CLOEXEC, protocol);
	if (s < 0) { return XERRNO; }
#else
	s = socket (domain, type, protocol);
	if (s < 0) { return XERRNO; }
	rc = xcloexec (xunblock (s));
	if (rc < 0) { goto error; }
#endif

	rc = setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof (int));
	if (rc < 0) {
		rc = XERRNO;
		goto error;
	}

	return s;

error:
	close (s);
	return rc;
}

int
xaccept (int s, struct sockaddr *addr, socklen_t *addrlen, int timeoutms)
{
	int fd;

again:
#if defined (SOCK_NONBLOCK) && defined (SOCK_CLOEXEC)
	fd = accept4 (s, addr, addrlen, SOCK_NONBLOCK|SOCK_CLOEXEC);
	if (fd >= 0) {
		return fd;
	}
#else
	fd = accept (s, addr, addrlen);
	if (fd >= 0) {
		int rc = xcloexec (xunblock (fd));
		if (rc < 0) {
			close (fd);
			return rc;
		}
		return fd;
	}
#endif

	fd = XERRNO;
	if (fd == -EAGAIN) {
		struct xtask *t = xtask_self ();
		if (t != NULL) {
			fd = schedule_poll (xtask_local (t), s, XPOLL_IN, timeoutms);
			if (fd == 0) {
				int val = xyield (XZERO).i;
				if (val < 0) { return (ssize_t)val; }
				goto again;
			}
		}
	}

	return fd;
}

int
xunblock (int fd)
{
	if (fd < 0) { return fd; }
	int flags = fcntl (fd, F_GETFL, 0);
	if (flags < 0) { return XERRNO; }
	if (flags & O_NONBLOCK) { return fd; }
	return fcntl (fd, F_SETFL, flags|O_NONBLOCK) < 0 ? XERRNO : fd;

}

int
xcloexec (int fd)
{
	if (fd < 0) { return fd; }
	int flags = fcntl (fd, F_GETFD, 0);
	if (flags < 0) { return XERRNO; }
	if (flags & FD_CLOEXEC) { return fd; }
	return fcntl (fd, F_SETFD, flags|FD_CLOEXEC) < 0 ? XERRNO : fd;
}

