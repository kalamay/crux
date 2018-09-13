#include "../include/crux/hub.h"
#include "../include/crux/err.h"
#include "../include/crux/list.h"

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
#if HAS_EXECINFO
# include <execinfo.h>
#endif

struct xhub {
	struct xmgr mgr;
	struct xpoll poll;
	struct xheap timeout;
	struct xlist immediate;
	struct xlist polled;
	struct xlist closed;
	unsigned npolled;
	unsigned ndetached;
	bool running;
};

struct xhub_entry {
	uint64_t magic;
#define MAGIC UINT64_C(0x989b369eac2205a3)
	struct xheap_entry hent;
	struct xlist lent;
	struct xtask *t;
	union xvalue val;
	struct xhub *hub;
	void (*fn)(struct xhub *, union xvalue val);
	int poll_id, poll_type;
	bool detached;
};

static thread_local struct xhub *active_hub = NULL;
static thread_local struct xhub_entry *active_entry = NULL;

static bool
is_scheduled(struct xhub_entry *ent)
{
	return (ent->poll_type != 0 ||
			ent->hent.key != XHEAP_NONE ||
			xlist_is_added(&ent->lent));
}

static int
schedule_immediate(struct xhub_entry *ent)
{
	xlist_add(&ent->hub->immediate, &ent->lent, X_ASCENDING);
	ent->poll_type = 0;
	return 0;
}

static int
schedule_timeout(struct xhub_entry *ent, int ms)
{
	struct xhub *h = ent->hub;
	ent->hent.prio = X_MSEC_TO_NSEC((int64_t)ms) + XCLOCK_NSEC(&h->poll.clock);
	ent->detached = false;
	int rc = xheap_add(&h->timeout, &ent->hent);
	return rc < 0 ? rc : 0;
}

static int
schedule_poll(struct xhub_entry *ent, int id, int type, int timeoutms)
{
	int rc;
	if (timeoutms < 0) {
		ent->hent.key = XHEAP_NONE;
		ent->detached = timeoutms == XTIMEOUT_DETACH;
	}
	else {
		rc = schedule_timeout(ent, timeoutms);
		if (rc < 0) { return rc; }
	}

	struct xhub *h = ent->hub;
	rc = xpoll_ctl(&h->poll, XPOLL_ADD, type, id, ent);
	if (rc < 0) {
		xheap_remove(&h->timeout, &ent->hent);
		return rc;
	}

	xlist_add(&ent->hub->polled, &ent->lent, X_ASCENDING);
	ent->hub->npolled++;
	if (ent->detached) {
		ent->hub->ndetached++;
	}
	ent->poll_id = id;
	ent->poll_type = type;
	return 0;
}

static void
uncount_poll(struct xhub_entry *ent)
{
	assert(ent->hub->npolled > 0);
	ent->hub->npolled--;
	if (ent->detached) {
		assert(ent->hub->ndetached > 0);
		ent->hub->ndetached--;
	}
}

static void
unschedule(struct xhub_entry *ent)
{
	if (ent->poll_type) {
		xpoll_ctl(&ent->hub->poll, XPOLL_DEL, ent->poll_type, ent->poll_id, NULL);
		uncount_poll(ent);
		ent->poll_type = 0;
	}

	if (xlist_is_added(&ent->lent)) {
		xlist_del(&ent->lent);
	}

	if (ent->hent.key != XHEAP_NONE) {
		xheap_remove(&ent->hub->timeout, &ent->hent);
	}
}

static void
mark_closed(struct xhub_entry *ent)
{
	unschedule(ent);
	xlist_add(&ent->hub->closed, &ent->lent, X_ASCENDING);
}

int
xhub_new(struct xhub **hubp)
{
	assert(hubp != NULL);

	struct xhub *hub = malloc(sizeof(*hub));
	if (hub == NULL) {
		return XERRNO;
	}

	int rc;

	rc = xmgr_init(&hub->mgr, sizeof(struct xhub_entry), XSTACK_DEFAULT, XTASK_FDEFAULT);
	if (rc < 0) {
		goto err_mgr;
	}

	rc = xpoll_init(&hub->poll);
	if (rc < 0) {
		goto err_poll;
	}

	rc = xheap_init(&hub->timeout);
	if (rc < 0) {
		goto err_heap;
	}

	xlist_init(&hub->immediate);
	xlist_init(&hub->polled);
	xlist_init(&hub->closed);
	hub->ndetached = 0;
	hub->npolled = 0;
	hub->running = false;

	*hubp = hub;
	return 0;

err_heap:
	xpoll_final(&hub->poll);
err_poll:
	xmgr_final(&hub->mgr);
err_mgr:
	free(hub);
	return rc;
}

static void
free_hent(struct xheap_entry *hent, void *data)
{
	(void)data;
	struct xhub_entry *ent = xcontainer(hent, struct xhub_entry, hent);
	unschedule(ent);
	xtask_free(&ent->t);
}

void
xhub_free(struct xhub **hubp)
{
	assert(hubp != NULL);

	struct xhub *hub = *hubp;
	if (hub != NULL) {
		struct xhub_entry *ent = active_entry;
		if (ent && ent->hub == hub) { return; }

		*hubp = NULL;

		struct xlist *lent;

		xlist_each(&hub->immediate, lent, X_ASCENDING) {
			ent = xcontainer(lent, struct xhub_entry, lent);
			unschedule(ent);
			xtask_free(&ent->t);
		}

		xlist_each(&hub->polled, lent, X_ASCENDING) {
			ent = xcontainer(lent, struct xhub_entry, lent);
			unschedule(ent);
			xtask_free(&ent->t);
		}

		xheap_clear(&hub->timeout, free_hent, NULL);
		xheap_final(&hub->timeout);
		xpoll_final(&hub->poll);
		xmgr_final(&hub->mgr);
		free(hub);
	}
}

static int
run_once(struct xhub *hub)
{
	int rc, val = 0;
	int64_t ms = -1;
	struct xlist *lent;
	struct xheap_entry *wait;
	struct xevent ev;
	struct xhub_entry *ent, *tmp;

	// first clear all immediate tasks
	if ((lent = xlist_first(&hub->immediate, X_ASCENDING))) {
		ent = xcontainer(lent, struct xhub_entry, lent);
		goto invoke;
	}

	// special "closed" events are scheduled when xclose affects a scheduled task
	if ((lent = xlist_first(&hub->closed, X_ASCENDING))) {
		ent = xcontainer(lent, struct xhub_entry, lent);
		val = XESYS(ECONNABORTED);
		goto invoke;
	}

	// then check for a timeout period
	if ((wait = xheap_get(&hub->timeout, XHEAP_ROOT))) {
		// there is some task scheduled with a timeout to get its value to pass to the poll
		ms = X_NSEC_TO_MSEC(wait->prio - XCLOCK_NSEC(&hub->poll.clock));
		// if the timeout has already expired, immediately invoke it as a timeout
		if (ms < 0) {
			// TODO: should we give the task a 0-timeout poll if its scheduled for polling?
			ent = xcontainer(wait, struct xhub_entry, hent);
			goto invoke;
		}
	}
	// if there are no polled tasks then there is nothing to do
	else if (xlist_is_empty(&hub->polled)) {
		return 0;
	}
	// if all polled tasks are deteched then invoke them as timed out
	else if (hub->npolled == hub->ndetached) {
		ent = xcontainer(xlist_first(&hub->polled, X_ASCENDING), struct xhub_entry, lent);
		assert(ent->detached);
		val = XESYS(ETIMEDOUT);
		goto invoke;
	}

	// we have some pollable tasks
	rc = xpoll_wait(&hub->poll, ms, &ev);
	switch (rc) {
	case 0:
		// there was a timeout so get the task with the earliest scheduled timeout
		ent = xcontainer(wait, struct xhub_entry, hent);
		if (ent->poll_type) {
			val = XESYS(ETIMEDOUT);
		}
		break;
	case 1:
		// a pollable event occurred so extract the task to invoke
		ent = ev.ptr;
		ent->poll_type = 0;
		uncount_poll(ent);
		break;
	default:
		return rc;
	}

invoke:
	unschedule(ent);
	tmp = active_entry;
	active_entry = ent;
	xresume(ent->t, XINT(val));
	active_entry = tmp;
	if (!is_scheduled(ent) || !xtask_alive(ent->t)) {
		xtask_free(&ent->t);
	}
	return 1;
}

int
xhub_run(struct xhub *hub)
{
	if (hub->running) {
		return XESYS(EPERM);
	}

	active_hub = hub;
	hub->running = true;

	int rc;
	do {
		rc = run_once(hub);
	} while (hub->running && rc == 1);

	hub->running = false;
	active_hub = NULL;

	return rc;
}

void
xhub_stop(struct xhub *hub)
{
	hub->running = false;
}

void
xhub_remove_io(struct xhub *hub, int fd)
{
	struct xlist *lent;
	struct xhub_entry *ent;
	xlist_each(&hub->polled, lent, X_ASCENDING) {
		ent = xcontainer(lent, struct xhub_entry, lent);
		if ((ent->poll_type == XPOLL_IN || ent->poll_type == XPOLL_OUT)
				&& ent->poll_id == fd) {
			mark_closed(ent);
		}
	}
	xpoll_ctl(&hub->poll, XPOLL_DEL, XPOLL_IN|XPOLL_OUT, fd, NULL);
}

static union xvalue
spawn_fn(void *tls, union xvalue val)
{
	(void)val;
	struct xhub_entry *ent = tls;
	ent->fn(ent->hub, ent->val);
	return XZERO;
}

int
xspawnf(struct xhub *hub, const char *file, int line,
		void (*fn)(struct xhub *, union xvalue), union xvalue val)
{
	struct xtask *t;
	struct xhub_entry *ent;
	int rc = xtask_newf(&t, &hub->mgr, NULL, file, line, spawn_fn);
	if (rc < 0) {
		return rc;
	}

	ent = xtask_local(t);
	ent->magic = MAGIC;
	ent->t = t;
	ent->val = val;
	ent->hub = hub;
	ent->fn = fn;

	return schedule_immediate(ent);
}

#if defined(__BLOCKS__)

#include <Block.h>

static void
spawn_block(struct xhub *h, union xvalue val)
{
	(void)h;
	void (^block)(void) = val.ptr;
	block();
	Block_release(block);
}

int
xspawn_b(struct xhub *hub, void (^block)(void))
{
	void (^copy)(void) = Block_copy(block);
	int rc = xspawnf(hub, NULL, 0, spawn_block, XPTR(copy));
	if (rc < 0) {
		Block_release(copy);
	}
	return rc;
}

#endif

const struct timespec *
xclock(void)
{
	return active_hub ? &active_hub->poll.clock : NULL;
}

int
xwait(int fd, int polltype, int timeoutms)
{
	struct xhub_entry *ent = active_entry;
	if (ent == NULL) { return XESYS(EAGAIN); }

	int rc;
	if (polltype > 0) {
		rc = schedule_poll(ent, fd, polltype, timeoutms);
	}
	else if (timeoutms >= 0) {
		rc = schedule_timeout(ent, timeoutms);
	}
	else {
		rc = schedule_immediate(ent);
	}

	if (rc == 0) {
		rc = xyield(XZERO).i;
	}
	return rc;
}

int
xsleep(unsigned ms)
{
	struct xhub_entry *ent = active_entry;
	if (ent == NULL) {
		struct timespec c = XCLOCK_MAKE_MSEC(ms);
		int rc;
		do {
			rc = nanosleep(&c, &c);
			if (rc < 0) { rc = XERRNO; }
		} while (rc == XESYS(EINTR));
		return rc;
	}

	int rc = schedule_timeout(ent, ms);
	if (rc == 0) {
		xyield(XZERO);
	}
	return rc;
}

void
xexit(int ec)
{
	struct xtask *t = xtask_self();
	if (t == NULL) { exit(ec); }
	struct xhub_entry *ent = xtask_local(t);
	if (ent && ent->magic == MAGIC) { unschedule(ent); }
	xtask_exit(t, ec);
}

void
xabort(void)
{
	fflush(stderr);
#if HAS_EXECINFO
	void *calls[32];
	int frames = backtrace(calls, xlen(calls));
	backtrace_symbols_fd(calls, frames, STDERR_FILENO);
#endif
	struct xtask *t = xtask_self();
	if (t == NULL) { abort(); }
	struct xhub_entry *ent = xtask_local(t);
	if (ent && ent->magic == MAGIC) { unschedule(ent); }
	xtask_print(t, stderr);
	xtask_exit(t, SIGABRT);
}

int
xsignal(int signum, int timeoutms)
{
	struct xhub_entry *ent = active_entry;
	if (ent == NULL) { return XESYS(EPERM); }
	int rc = schedule_poll(ent, signum, XPOLL_SIG, timeoutms);
	if (rc == 0) {
		int val = xyield(XZERO).i;
		return val ? val : signum;
	}
	return rc;
}

#define RECV_LOOP(fd, ms, fn, ...) for (;;) { \
	ssize_t rc = fn(fd, __VA_ARGS__); \
	if (rc >= 0) { return rc; } \
	rc = XERRNO; \
	if (rc != XESYS(EAGAIN)) { return rc; } \
	struct xhub_entry *ent = active_entry; \
	if (ent == NULL) { return rc; } \
	rc = schedule_poll(ent, fd, XPOLL_IN, ms); \
	if (rc < 0) { return rc; } \
	int val = xyield(XZERO).i; \
	if (val < 0) { return (ssize_t)val; } \
}

#define SEND_LOOP(fd, ms, fn, ...) for (;;) { \
	ssize_t rc = fn(fd, __VA_ARGS__); \
	if (rc >= 0) { return rc; } \
	rc = XERRNO; \
	if (rc != XESYS(EAGAIN)) { return rc; } \
	struct xhub_entry *ent = active_entry; \
	if (ent == NULL) { return rc; } \
	rc = schedule_poll(ent, fd, XPOLL_OUT, ms); \
	if (rc < 0) { return rc; } \
	int val = xyield(XZERO).i; \
	if (val < 0) { return (ssize_t)val; } \
}

ssize_t
xread(int fd, void *buf, size_t len, int timeoutms)
{
	RECV_LOOP(fd, timeoutms, read, buf, len);
}

extern ssize_t
xreadv(int fd, struct iovec *iov, int iovcnt, int timeoutms)
{
	RECV_LOOP(fd, timeoutms, readv, iov, iovcnt);
}

ssize_t
xrecv(int fd, void *buf, size_t len, int flags, int timeoutms)
{
	RECV_LOOP(fd, timeoutms, recv, buf, flags, len);
}

extern ssize_t
xrecvfrom(int s, void *buf, size_t len, int flags,
	 struct sockaddr *src_addr, socklen_t *src_len, int timeoutms)
{
	RECV_LOOP(s, timeoutms, recvfrom, buf, len, flags, src_addr, src_len);
}

ssize_t
xreadn(int fd, void *buf, size_t len, int timeoutms)
{
	return xio(fd, buf, len, timeoutms, xread);
}

ssize_t
xwrite(int fd, const void *buf, size_t len, int timeoutms)
{
	SEND_LOOP(fd, timeoutms, write, buf, len);
}

ssize_t
xwritev(int fd, const struct iovec *iov, int iovcnt, int timeoutms)
{
	SEND_LOOP(fd, timeoutms, writev, iov, iovcnt);
}

ssize_t
xsend(int fd, const void *buf, size_t len, int flags, int timeoutms)
{
	SEND_LOOP(fd, timeoutms, send, buf, len, flags);
}

ssize_t
xsendto(int s, const void *buf, size_t len, int flags,
	 const struct sockaddr *dest_addr, socklen_t dest_len, int timeoutms)
{
	SEND_LOOP(s, timeoutms, sendto, buf, len, flags, dest_addr, dest_len);
}

ssize_t
xwriten(int fd, const void *buf, size_t len, int timeoutms)
{
	return xio(fd, (void *)buf, len, timeoutms, (xio_fn)xwrite);
}

ssize_t
xio(int fd, void *buf, size_t len, int timeoutms,
		ssize_t (*fn) (int fd, void *buf, size_t len, int timeoutms))
{
	if (len > SSIZE_MAX) {
		return XESYS(EINVAL);
	}

	struct timespec now;
	int64_t abs;
	if (timeoutms > 0) {
		xclock_mono(&now);
		abs = X_MSEC_TO_NSEC(timeoutms) + XCLOCK_NSEC(&now);
	}

	size_t total = 0;
	ssize_t rc;

again:
	rc = fn(fd, (uint8_t *)buf+total, len-total, timeoutms);
	if (rc < 0) { return rc; }
	total += (size_t)rc;
	if (total < len) {
		if (timeoutms > 0) {
			xclock_mono(&now);
			timeoutms = X_NSEC_TO_MSEC(abs - XCLOCK_NSEC(&now));
			if (timeoutms < 0) { timeoutms = 0; }
		}
		goto again;
	}
	return (ssize_t)total;
}

int
xpipe(int fds[static 2])
{
#if HAS_PIPE2
	int rc = pipe2(fds, O_NONBLOCK|O_CLOEXEC);
	if (rc < 0) { return XERRNO; }
	return 0;
#else
	int rc = pipe(fds);
	if (rc < 0) { return XERRNO; }
	rc = xcloexec(xunblock (fds[0]));
	if (rc < 0) { goto error; }
	rc = xcloexec(xunblock (fds[1]));
	if (rc < 0) { goto error; }
	return 0;
error:
	close(fds[0]);
	close(fds[1]);
	return rc;
#endif
}

int
xclose(int fd)
{
	if (fd < 0) { return 0; }
	struct xhub_entry *ent = active_entry;
	if (ent != NULL) {
		xhub_remove_io(ent->hub, fd);
	}
	return xerr(xretry(close(fd)));
}

int
xunblock(int fd)
{
	if (fd < 0) { return fd; }
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) { return XERRNO; }
	if (flags & O_NONBLOCK) { return fd; }
	return fcntl(fd, F_SETFL, flags|O_NONBLOCK) < 0 ? XERRNO : fd;
}

int
xcloexec(int fd)
{
	if (fd < 0) { return fd; }
	int flags = fcntl(fd, F_GETFD, 0);
	if (flags < 0) { return XERRNO; }
	if (flags & FD_CLOEXEC) { return fd; }
	return fcntl(fd, F_SETFD, flags|FD_CLOEXEC) < 0 ? XERRNO : fd;
}

ssize_t
xbuf_read(struct xbuf *buf, int fd, size_t len, int timeoutms)
{
	ssize_t rc = xbuf_ensure(buf, len);
	if (rc < 0) { return rc; }
	rc = xread(fd, xbuf_tail(buf), len, timeoutms);
	if (rc > 0) { xbuf_bump(buf, rc); }
	return rc;
}

ssize_t
xbuf_write(struct xbuf *buf, int fd, size_t len, int timeoutms)
{
	size_t w = xbuf_length(buf);
	if (len < w) { w = len; }
	ssize_t rc = xwrite(fd, xbuf_data(buf), w, timeoutms);
	if (rc > 0) { xbuf_trim(buf, rc); }
	return rc;
}

