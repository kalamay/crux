#include "mu.h"

#include <sys/types.h>
#include <sys/socket.h>

#include "../include/crux/poll.h"

static void
test_signal(void)
{
	struct xpoll *p;
	struct xevent ev;

	mu_assert_int_eq(xpoll_new(&p), 0);
	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 0);
	mu_assert_int_eq(xpoll_ctl(p, SIGHUP, 0, XPOLL_SIG), 0);

	kill(getpid(), SIGHUP);

	mu_assert_int_eq(xpoll_wait(p, -1, &ev), 1);
	mu_assert_int_eq(ev.type, XPOLL_SIG);
	mu_assert_int_eq(ev.id, SIGHUP);

	xpoll_free(&p);
}

static void
test_io(void)
{
	struct xpoll *p;
	struct xevent ev;
	int fd[2];

	mu_assert_call(pipe(fd));

	mu_assert_int_eq(xpoll_new(&p), 0);
	mu_assert_int_eq(xpoll_ctl(p, fd[0], XPOLL_NONE, XPOLL_IN), 0);
	mu_assert_int_eq(xpoll_ctl(p, fd[1], XPOLL_NONE, XPOLL_OUT), 0);

	mu_assert_int_eq(xpoll_wait(p, 100, &ev), 1);
	mu_assert_int_eq(ev.type & XPOLL_OUT, XPOLL_OUT);
	mu_assert_int_eq(xpoll_wait(p, 100, &ev), 0);

	mu_assert_int_eq(write(fd[1], "test", 4), 4);
	mu_assert_int_eq(xpoll_ctl(p, fd[1], XPOLL_OUT, XPOLL_NONE), 0);

	mu_assert_int_eq(xpoll_wait(p, 100, &ev), 1);
	mu_assert_int_eq(ev.type & XPOLL_IN, XPOLL_IN);

	char buf[64];
	memset(buf, 0, sizeof(buf));

	mu_assert_int_eq(read(ev.id, buf, sizeof(buf)), 4);
	mu_assert_str_eq(buf, "test");

	xpoll_free(&p);
}

static void
test_remove(void)
{
	struct xpoll *p;
	struct xevent ev;
	int a[2], b[2];

	mu_assert_call(pipe(a));
	mu_assert_call(pipe(b));

	mu_assert_int_eq(xpoll_new(&p), 0);
	mu_assert_int_eq(xpoll_ctl(p, a[0], 0, XPOLL_IN), 0);
	mu_assert_int_eq(xpoll_ctl(p, b[0], 0, XPOLL_IN), 0);

	mu_assert_int_eq(write(a[1], "test", 4), 4);
	mu_assert_int_eq(write(b[1], "test", 4), 4);

	mu_assert_int_eq(xpoll_wait(p, 100, &ev), 1);
	mu_assert_int_eq(ev.type & XPOLL_IN, XPOLL_IN);

	if (ev.id == a[0]) {
		mu_assert_int_eq(xpoll_ctl(p, b[0], XPOLL_IN, 0), 0);
	}
	else {
		mu_assert_int_eq(xpoll_ctl(p, a[0], XPOLL_IN, 0), 0);
	}

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 0);

	xpoll_free(&p);
}

static void
test_remove2(void)
{
	struct xpoll *p;
	struct xevent ev;
	int fd[2];
	int match[32] = {0};

	mu_assert_call(socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
	mu_assert_int_lt(fd[0], 32);
	mu_assert_int_lt(fd[1], 32);

	mu_assert_int_eq(xpoll_new(&p), 0);

	mu_assert_int_eq(xpoll_ctl(p, fd[0], 0, XPOLL_IN|XPOLL_OUT), 0);
	mu_assert_int_eq(xpoll_ctl(p, fd[1], 0, XPOLL_IN|XPOLL_OUT), 0);

	mu_assert_int_eq(write(fd[0], "test", 4), 4);
	mu_assert_int_eq(write(fd[1], "test", 4), 4);

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);
	match[ev.id]++;

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);
	match[ev.id]++;

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);
	match[ev.id]++;

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);
	match[ev.id]++;

	mu_assert_int_eq(match[fd[0]], 2);
	mu_assert_int_eq(match[fd[1]], 2);

	mu_assert_int_eq(xpoll_ctl(p, fd[0], XPOLL_IN|XPOLL_OUT, 0), 0);
	mu_assert_int_eq(xpoll_ctl(p, fd[1], XPOLL_IN|XPOLL_OUT, 0), 0);

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 0);

	mu_assert_int_eq(xpoll_ctl(p, fd[0], 0, XPOLL_IN|XPOLL_OUT), 0);
	mu_assert_int_eq(xpoll_ctl(p, fd[1], 0, XPOLL_IN|XPOLL_OUT), 0);

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);

	mu_assert_int_eq(xpoll_ctl(p, fd[0], XPOLL_IN|XPOLL_OUT, 0), 0);
	mu_assert_int_eq(xpoll_ctl(p, fd[1], XPOLL_IN|XPOLL_OUT, 0), 0);

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 0);

	xpoll_free(&p);
}

int
main(void)
{
	mu_init("poll");
	mu_run(test_signal);
	mu_run(test_io);
	mu_run(test_remove);
	mu_run(test_remove2);
}

