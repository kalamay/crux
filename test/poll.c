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
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_SIG, SIGHUP, "test"), 0);

	kill(getpid(), SIGHUP);

	mu_assert_int_eq(xpoll_wait(p, -1, &ev), 1);
	mu_assert_int_eq(ev.type, XPOLL_SIG);
	mu_assert_int_eq(ev.id, SIGHUP);
	mu_assert_str_eq(ev.ptr, "test");

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
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_IN, fd[0], "in"), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_OUT, fd[1], "out"), 0);

	mu_assert_int_eq(xpoll_wait(p, 100, &ev), 1);
	mu_assert_int_eq(ev.type & XPOLL_OUT, XPOLL_OUT);
	mu_assert_str_eq(ev.ptr, "out");

	mu_assert_int_eq(write(ev.id, "test", 4), 4);

	mu_assert_int_eq(xpoll_wait(p, 100, &ev), 1);
	mu_assert_int_eq(ev.type & XPOLL_IN, XPOLL_IN);
	mu_assert_str_eq(ev.ptr, "in");

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
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_IN, a[0], "a"), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_IN, b[0], "b"), 0);

	mu_assert_int_eq(write(a[1], "test", 4), 4);
	mu_assert_int_eq(write(b[1], "test", 4), 4);

	mu_assert_int_eq(xpoll_wait(p, 100, &ev), 1);
	mu_assert_int_eq(ev.type & XPOLL_IN, XPOLL_IN);
	if (*(const char *)ev.ptr == 'a') {
		mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_IN, b[0], NULL), 0);
	}
	else {
		mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_IN, a[0], NULL), 0);
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
	int match[4] = { 0, 0, 0, 0 };

	mu_assert_call(socketpair(AF_UNIX, SOCK_STREAM, 0, fd));

	mu_assert_int_eq(xpoll_new(&p), 0);

	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_IN, fd[0], &match[0]), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_OUT, fd[0], &match[1]), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_IN, fd[1], &match[2]), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_OUT, fd[1], &match[3]), 0);

	mu_assert_int_eq(write(fd[0], "test", 4), 4);
	mu_assert_int_eq(write(fd[1], "test", 4), 4);

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);
	mu_assert_int_eq(*(int *)ev.ptr, 0);
	(*(int *)ev.ptr)++;

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);
	mu_assert_int_eq(*(int *)ev.ptr, 0);
	(*(int *)ev.ptr)++;

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);
	mu_assert_int_eq(*(int *)ev.ptr, 0);
	(*(int *)ev.ptr)++;

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);
	mu_assert_int_eq(*(int *)ev.ptr, 0);
	(*(int *)ev.ptr)++;

	mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_IN, fd[0], NULL), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_OUT, fd[0], NULL), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_IN, fd[1], NULL), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_OUT, fd[1], NULL), 0);

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 0);

	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_IN, fd[0], &match[0]), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_OUT, fd[0], &match[1]), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_IN, fd[1], &match[2]), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_ADD, XPOLL_OUT, fd[1], &match[3]), 0);

	mu_assert_int_eq(xpoll_wait(p, 0, &ev), 1);

	mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_IN, fd[0], NULL), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_OUT, fd[0], NULL), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_IN, fd[1], NULL), 0);
	mu_assert_int_eq(xpoll_ctl(p, XPOLL_DEL, XPOLL_OUT, fd[1], NULL), 0);

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

