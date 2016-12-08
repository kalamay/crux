#include "mu.h"

#include "../include/crux.h"

#include <signal.h>
#include <math.h>
#include <arpa/inet.h>

static void
doclose (void *data)
{
	close ((int)(intptr_t)data);
}

static void
dosleep (struct xhub *h, void *data)
{
	(void)h;
	xsleep ((uintptr_t)data);
}

static void
test_concurrent_sleep (void)
{
	struct xhub *hub;
	struct xclock clock;

	mu_assert_int_eq (xhub_new (&hub), 0);

	mu_assert_int_eq (xspawn (hub, dosleep, (void *)100), 0);
	mu_assert_int_eq (xspawn (hub, dosleep, (void *)200), 0);
	mu_assert_int_eq (xspawn (hub, dosleep, (void *)100), 0);

	xclock_mono (&clock);

	mu_assert_int_eq (xhub_run (hub), 0);

	int ms = (int)round (xclock_step (&clock) * 1000);
	mu_assert_int_ge (ms, 200);
	mu_assert_int_le (ms, 210);

	xhub_free (&hub);
}

static void
dosignal (struct xhub *h, void *data)
{
	(void)h;
	int signum = (intptr_t)data;
	int rc = xsignal (signum, -1);
	mu_assert_int_eq (rc, signum);
}

static void
dokill (struct xhub *h, void *data)
{
	(void)h;
	xsleep (10);
	kill (getpid (), (intptr_t)data);
}

static void
test_signal (void)
{
	struct xhub *hub;
	mu_assert_int_eq (xhub_new (&hub), 0);
	mu_assert_int_eq (xspawn (hub, dosignal, (void *)SIGHUP), 0);
	mu_assert_int_eq (xspawn (hub, dokill, (void *)SIGHUP), 0);
	mu_assert_int_eq (xhub_run (hub), 0);
	xhub_free (&hub);
}

static void
dowrite (struct xhub *h, void *data)
{
	(void)h;
	int fd = (intptr_t)data;
	for (int i = 0; i < 5; i++) {
		xwrite (fd, "test", 4, -1);
		xsleep (10);
	}
	close (fd);
}

static void
doread (struct xhub *h, void *data)
{
	(void)h;
	int fd = (intptr_t)data;

	char buf[5];
	memset (buf, 0, sizeof buf);
	int n = 0;
	while (xread (fd, buf, 4, -1) > 0) {
		mu_assert_str_eq (buf, "test");
		n++;
	}
	mu_assert_int_eq (n, 5);
	close (fd);
}

static void
test_pipe (void)
{
	int fds[2];
	mu_assert_call (xpipe (fds));

	struct xhub *hub;
	mu_assert_int_eq (xhub_new (&hub), 0);
	mu_assert_int_eq (xspawn (hub, dowrite, (void *)(intptr_t)fds[1]), 0);
	mu_assert_int_eq (xspawn (hub, doread, (void *)(intptr_t)fds[0]), 0);
	mu_assert_int_eq (xhub_run (hub), 0);
	xhub_free (&hub);
}

static void
dorecv (struct xhub *h, void *data)
{
	(void)h;
	const struct sockaddr_in *dest = data;

	int s = xsocket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	mu_assert_call (s);
	mu_assert_call (bind (s, (const struct sockaddr *)dest, sizeof *dest));
	xdefer (doclose, (void *)(intptr_t)s);

	struct sockaddr_in src;
	socklen_t len = sizeof src;

	char buf[5];
	memset (buf, 0, sizeof buf);

	xrecvfrom (s, buf, 4, 0, (struct sockaddr *)&src, &len, -1);

	mu_assert_str_eq (buf, "test");
}

static void
dosend (struct xhub *h, void *data)
{
	(void)h;
	const struct sockaddr_in *dest = data;

	int s = xsocket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	mu_assert_call (s);
	xdefer (doclose, (void *)(intptr_t)s);

	xsendto (s, "test", 4, 0, (struct sockaddr *)dest, sizeof *dest, -1);
}

static void
test_udp (void)
{
	struct xhub *hub;
	mu_assert_int_eq (xhub_new (&hub), 0);

	struct sockaddr_in dest = {
		.sin_family = AF_INET,
		.sin_port = htons (3333),
		.sin_addr.s_addr = inet_addr ("0.0.0.0")
	};

	xspawn (hub, dorecv, &dest);
	xspawn (hub, dosend, &dest);

	mu_assert_int_eq (xhub_run (hub), 0);
	xhub_free (&hub);
}

static void
dorecv_timeout (struct xhub *h, void *data)
{
	(void)h;
	const struct sockaddr_in *dest = data;

	int s = xsocket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	mu_assert_call (s);
	mu_assert_call (bind (s, (const struct sockaddr *)dest, sizeof *dest));
	xdefer (doclose, (void *)(intptr_t)s);

	struct sockaddr_in src;
	socklen_t len = sizeof src;

	char buf[5];
	memset (buf, 0, sizeof buf);

	struct xclock clock;
	xclock_mono (&clock);

	int rc, ms;

	rc = xrecvfrom (s, buf, 4, 0, (struct sockaddr *)&src, &len, 100);
	ms = (int)round (xclock_step (&clock) * 1000);
	mu_assert_int_eq (rc, -ETIMEDOUT);
	mu_assert_int_ge (ms, 100);
	mu_assert_int_le (ms, 110);

	rc = xrecvfrom (s, buf, 4, 0, (struct sockaddr *)&src, &len, 200);
	mu_assert_int_eq (rc, 4);
	mu_assert_str_eq (buf, "test");
}

static void
dosend_timeout (struct xhub *h, void *data)
{
	(void)h;
	const struct sockaddr_in *dest = data;

	int s = xsocket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	mu_assert_call (s);
	xdefer (doclose, (void *)(intptr_t)s);

	xsleep (200);
	int rc = xsendto (s, "test", 4, 0, (struct sockaddr *)dest, sizeof *dest, -1);
	mu_assert_int_eq (rc, 4);
}

static void
test_udp_timeout (void)
{
	struct xhub *hub;
	mu_assert_int_eq (xhub_new (&hub), 0);

	struct sockaddr_in dest = {
		.sin_family = AF_INET,
		.sin_port = htons (3334),
		.sin_addr.s_addr = inet_addr ("0.0.0.0")
	};

	xspawn (hub, dorecv_timeout, &dest);
	xspawn (hub, dosend_timeout, &dest);

	mu_assert_int_eq (xhub_run (hub), 0);
	xhub_free (&hub);
}

int
main (void)
{
	mu_init ("hub");
	mu_run (test_concurrent_sleep);
	mu_run (test_signal);
	mu_run (test_pipe);
	mu_run (test_udp);
	mu_run (test_udp_timeout);
}

