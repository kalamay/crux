#include "mu.h"

#include "../include/crux.h"
#include "../include/crux/net.h"
#include "../src/poll.h"

#include <signal.h>
#include <math.h>
#include <arpa/inet.h>

static void
doclose(union xvalue val)
{
	close(val.i);
}

static int sleep_count = 0;

static void
dosleep(struct xhub *h, union xvalue val)
{
	(void)h;
	xsleep(val.i);
	sleep_count++;
}

static void
test_concurrent_sleep(void)
{
	struct xhub *hub;
	struct timespec start, end;

	mu_assert_int_eq(xhub_new(&hub), 0);

	mu_assert_int_eq(xspawn(hub, dosleep, xint(10)), 0);
	mu_assert_int_eq(xspawn(hub, dosleep, xint(20)), 0);
	mu_assert_int_eq(xspawn(hub, dosleep, xint(10)), 0);

	xclock_mono(&start);
	mu_assert_int_eq(xhub_run(hub), 0);
	xclock_mono(&end);

	int64_t nsec = XCLOCK_NSEC(&end) - XCLOCK_NSEC(&start);
	int ms = round((double)nsec / X_NSEC_PER_MSEC);
	mu_assert_int_ge(ms, 20-5);
	mu_assert_int_le(ms, 20+5);

	mu_assert_int_eq(sleep_count, 3);

	xhub_free(&hub);
}

static void
dosignal(struct xhub *h, union xvalue val)
{
	(void)h;
	int rc = xsignal(SIGHUP, -1);
	mu_assert_int_eq(rc, SIGHUP);
	int *p = val.ptr;
	(*p)++;
}

static void
dokill(struct xhub *h, union xvalue val)
{
	(void)h;
	(void)val;

	xsleep(10);
	mu_assert_call(kill(getpid(), SIGHUP));
}

static void
test_signal(void)
{
	struct xhub *hub;
	int count = 0;
	mu_assert_int_eq(xhub_new(&hub), 0);
	mu_assert_int_eq(xspawn(hub, dosignal, xptr(&count)), 0);
	mu_assert_int_eq(xspawn(hub, dosignal, xptr(&count)), 0);
	mu_assert_int_eq(xspawn(hub, dokill, xzero), 0);
	mu_assert_int_eq(xhub_run(hub), 0);
	xhub_free(&hub);

	mu_assert_int_eq(count, 2);
}

static void
dowrite(struct xhub *h, union xvalue val)
{
	(void)h;
	int fd = val.i;

	xdefer_close(fd);

	for (int i = 0; i < 5; i++) {
		xwrite(fd, "test", 4, -1);
		xsleep(10);
	}
}

static void
doread(struct xhub *h, union xvalue val)
{
	(void)h;
	int fd = val.i;

	xdefer_close(fd);

	char buf[5];
	memset(buf, 0, sizeof(buf));
	int reads = 0;
	for (;;) {
		ssize_t n = xread(fd, buf, 4, -1);
		mu_assert_int_ge(n, 0);
		if (n <= 0) { break; }
		mu_assert_str_eq(buf, "test");
		reads++;
	}
	//xhub_print(h, stdout);
	mu_assert_int_eq(reads, 5);
}

static void
test_pipe(void)
{
	int fds[2];
	mu_assert_call(xpipe(fds));

	struct xhub *hub;
	mu_assert_int_eq(xhub_new(&hub), 0);
	mu_assert_int_eq(xspawn(hub, dowrite, xint(fds[1])), 0);
	mu_assert_int_eq(xspawn(hub, doread, xint(fds[0])), 0);
	mu_assert_int_eq(xhub_run(hub), 0);
	xhub_free(&hub);
}

static void
dorecv(struct xhub *h, union xvalue val)
{
	(void)h;
	const struct sockaddr_in *addr = val.ptr;

	int s = xsocket(AF_INET, SOCK_DGRAM);
	mu_assert_call(s);
	mu_assert_call(bind(s, (const struct sockaddr *)addr, sizeof(*addr)));
	xdefer(doclose, xint(s));

	struct sockaddr_in src;
	socklen_t len = sizeof(src);

	char buf[5];
	memset(buf, 0, sizeof(buf));

	xrecvfrom(s, buf, 4, 0, (struct sockaddr *)&src, &len, -1);

	mu_assert_str_eq(buf, "test");
}

static void
dosend(struct xhub *h, union xvalue val)
{
	(void)h;
	const struct sockaddr_in *addr = val.ptr;

	int s = xsocket(AF_INET, SOCK_DGRAM);
	mu_assert_call(s);
	xdefer(doclose, xint(s));

	xsendto(s, "test", 4, 0, (struct sockaddr *)addr, sizeof(*addr), -1);
}

static void
test_udp(void)
{
	struct xhub *hub;
	mu_assert_int_eq(xhub_new(&hub), 0);

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(3333),
		.sin_addr.s_addr = inet_addr("0.0.0.0")
	};

	xspawn(hub, dorecv, xptr(&addr));
	xspawn(hub, dosend, xptr(&addr));

	mu_assert_int_eq(xhub_run(hub), 0);
	xhub_free(&hub);
}

static void
dorecv_timeout(struct xhub *h, union xvalue val)
{
	(void)h;
	const struct sockaddr_in *dest = val.ptr;

	int s = xsocket(AF_INET, SOCK_DGRAM);
	mu_assert_call(s);
	mu_assert_call(bind(s, (const struct sockaddr *)dest, sizeof(*dest)));
	xdefer(doclose, xint(s));

	struct sockaddr_in src;
	socklen_t len = sizeof(src);

	char buf[5];
	memset(buf, 0, sizeof(buf));

	struct timespec start, end;

	xclock_mono(&start);
	int rc = xrecvfrom(s, buf, 4, 0, (struct sockaddr *)&src, &len, 20);
	xclock_mono(&end);

	int64_t nsec = XCLOCK_NSEC(&end) - XCLOCK_NSEC(&start);
	int ms = round((double)nsec / X_NSEC_PER_MSEC);

	mu_assert_int_eq(rc, xerr_sys(ETIMEDOUT));
	mu_assert_int_ge(ms, 20-5);
	mu_assert_int_le(ms, 20+5);

	rc = xrecvfrom(s, buf, 4, 0, (struct sockaddr *)&src, &len, 30);
	mu_assert_int_eq(rc, 4);
	mu_assert_str_eq(buf, "test");
}

static void
dosend_timeout(struct xhub *h, union xvalue val)
{
	(void)h;
	const struct sockaddr_in *dest = val.ptr;

	int s = xsocket(AF_INET, SOCK_DGRAM);
	mu_assert_call(s);
	xdefer(doclose, xint(s));

	xsleep(40);
	int rc = xsendto(s, "test", 4, 0, (struct sockaddr *)dest, sizeof(*dest), -1);
	mu_assert_int_eq(rc, 4);
}

static void
test_udp_timeout(void)
{
	struct xhub *hub;
	mu_assert_int_eq(xhub_new(&hub), 0);

	struct sockaddr_in dest = {
		.sin_family = AF_INET,
		.sin_port = htons(3334),
		.sin_addr.s_addr = inet_addr("0.0.0.0")
	};

	xspawn(hub, dorecv_timeout, xptr(&dest));
	xspawn(hub, dosend_timeout, xptr(&dest));

	mu_assert_int_eq(xhub_run(hub), 0);
	xhub_free(&hub);
}

static void
doread2(struct xhub *h, union xvalue val)
{
	(void)h;
	int fd = val.i;

	char buf[5];
	memset(buf, 0, sizeof(buf));
	ssize_t n = xread(fd, buf, 2, -1);
	mu_assert_int_eq(n, 2);
}

static void
dowrite2(struct xhub *h, union xvalue val)
{
	(void)h;
	int fd = val.i;

	mu_assert_int_eq(xwrite(fd, "test", 4, -1), 4);
	xclose(fd);
}

static void
test_read2(void)
{
	int fds[2];
	mu_assert_call(xpipe(fds));

	struct xhub *hub;
	mu_assert_int_eq(xhub_new(&hub), 0);
	mu_assert_int_eq(xspawn(hub, doread2, xint(fds[0])), 0);
	mu_assert_int_eq(xspawn(hub, doread2, xint(fds[0])), 0);
	mu_assert_int_eq(xspawn(hub, dowrite2, xint(fds[1])), 0);
	mu_assert_int_eq(xhub_run(hub), 0);
	xhub_free(&hub);
}

static void
wakeread(struct xhub *h, union xvalue val)
{
	int fd = val.i;

	char buf[5];
	memset(buf, 0, sizeof(buf));
	ssize_t n = xread(fd, buf, 4, -1);
	mu_assert_int_eq(n, 4);
	mu_assert_str_eq(buf, "wake");

	mu_assert_int_eq(xhub_wake(h), 0);
}

static void
wakewrite(struct xhub *h, union xvalue val)
{
	(void)h;
	int fd = val.i;
	mu_assert_int_eq(xwrite(fd, "wake", 4, -1), 4);
	xclose(fd);
}

static void
wake(struct xhub *h, union xvalue val)
{
	(void)h;
	int rc = xwait(-1, XPOLL_WAKE, -1);
	mu_assert_int_eq(rc, 0);

	int *woke = val.ptr;
	*woke = 1;

	rc = xwait(-1, XPOLL_WAKE, 10);
	mu_assert_int_eq(rc, xerr_sys(ETIMEDOUT));
}

static void
test_wake(void)
{
	int fds[2];
	mu_assert_call(xpipe(fds));

	int woke = 0;

	struct xhub *hub;
	mu_assert_int_eq(xhub_new(&hub), 0);
	mu_assert_int_eq(xspawn(hub, wakeread, xint(fds[0])), 0);
	mu_assert_int_eq(xspawn(hub, wakewrite, xint(fds[1])), 0);
	mu_assert_int_eq(xspawn(hub, wake, xptr(&woke)), 0);
	mu_assert_int_eq(xhub_run(hub), 0);
	xhub_free(&hub);

	mu_assert_int_eq(woke, 1);
}

int
main(void)
{
	mu_init("hub");
	mu_run(test_concurrent_sleep);
	mu_run(test_signal);
	mu_run(test_pipe);
	mu_run(test_udp);
	mu_run(test_udp_timeout);
	mu_run(test_read2);
	mu_run(test_wake);
}

