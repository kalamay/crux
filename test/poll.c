#include "mu.h"

#include "../src/poll.h"

static void
test_signal (void)
{
	struct xpoll *p;
	struct xevent ev;

	mu_assert_int_eq (xpoll_new (&p), 0);
	mu_assert_int_eq (xpoll_wait (p, 0, &ev), 0);
	mu_assert_int_eq (xpoll_ctl (p, XPOLL_ADD, XPOLL_SIG, SIGHUP, "test"), 0);

	kill (getpid (), SIGHUP);

	mu_assert_int_eq (xpoll_wait (p, -1, &ev), 1);
	mu_assert_int_eq (ev.type, XPOLL_SIG);
	mu_assert_int_eq (ev.id, SIGHUP);
	mu_assert_str_eq (ev.ptr, "test");

	xpoll_free (&p);
}

static void
test_io (void)
{
	struct xpoll *p;
	struct xevent ev;
	int fd[2];

	mu_assert_call (pipe (fd));

	mu_assert_int_eq (xpoll_new (&p), 0);
	mu_assert_int_eq (xpoll_ctl (p, XPOLL_ADD, XPOLL_IN, fd[0], "in"), 0);
	mu_assert_int_eq (xpoll_ctl (p, XPOLL_ADD, XPOLL_OUT, fd[1], "out"), 0);

	mu_assert_int_eq (xpoll_wait (p, 0, &ev), 1);
	mu_assert_int_eq (ev.type & XPOLL_OUT, XPOLL_OUT);
	mu_assert_str_eq (ev.ptr, "out");

	mu_assert_int_eq (write (ev.id, "test", 4), 4);

	mu_assert_int_eq (xpoll_wait (p, 0, &ev), 1);
	mu_assert_int_eq (ev.type & XPOLL_IN, XPOLL_IN);
	mu_assert_str_eq (ev.ptr, "in");

	char buf[64];
	memset (buf, 0, sizeof buf);

	mu_assert_int_eq (read (ev.id, buf, sizeof buf), 4);
	mu_assert_str_eq (buf, "test");

	xpoll_free (&p);
}

int
main (void)
{
	mu_init ("poll");
	test_signal();
	test_io();
}

