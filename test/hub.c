#include "mu.h"

#include "../include/crux.h"
#include "../include/crux/net.h"

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
	struct xclock start, end;

	mu_assert_int_eq(xhub_new(&hub), 0);

	mu_assert_int_eq(xspawn(hub, dosleep, XINT(10)), 0);
	mu_assert_int_eq(xspawn(hub, dosleep, XINT(20)), 0);
	mu_assert_int_eq(xspawn(hub, dosleep, XINT(10)), 0);

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
	int signum = val.i;
	int rc = xsignal(signum, -1);
	mu_assert_int_eq(rc, signum);
}

static void
dokill(struct xhub *h, union xvalue val)
{
	(void)h;
	xsleep(10);
	mu_assert_call(kill(getpid(), val.i));
}

static void
test_signal(void)
{
	struct xhub *hub;
	mu_assert_int_eq(xhub_new(&hub), 0);
	mu_assert_int_eq(xspawn(hub, dosignal, XINT(SIGHUP)), 0);
	mu_assert_int_eq(xspawn(hub, dokill, XINT(SIGHUP)), 0);
	mu_assert_int_eq(xhub_run(hub), 0);
	xhub_free(&hub);
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
	int n = 0;
	while (xread(fd, buf, 4, -1) > 0) {
		mu_assert_str_eq(buf, "test");
		n++;
	}
	mu_assert_int_eq(n, 5);
}

static void
test_pipe(void)
{
	int fds[2];
	mu_assert_call(xpipe(fds));

	struct xhub *hub;
	mu_assert_int_eq(xhub_new(&hub), 0);
	mu_assert_int_eq(xspawn(hub, dowrite, XINT(fds[1])), 0);
	mu_assert_int_eq(xspawn(hub, doread, XINT(fds[0])), 0);
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
	xdefer(doclose, XINT(s));

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
	xdefer(doclose, XINT(s));

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

	xspawn(hub, dorecv, XPTR(&addr));
	xspawn(hub, dosend, XPTR(&addr));

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
	xdefer(doclose, XINT(s));

	struct sockaddr_in src;
	socklen_t len = sizeof(src);

	char buf[5];
	memset(buf, 0, sizeof(buf));

	struct xclock start, end;

	xclock_mono(&start);
	int rc = xrecvfrom(s, buf, 4, 0, (struct sockaddr *)&src, &len, 20);
	xclock_mono(&end);

	int64_t nsec = XCLOCK_NSEC(&end) - XCLOCK_NSEC(&start);
	int ms = round((double)nsec / X_NSEC_PER_MSEC);

	mu_assert_int_eq(rc, XESYS(ETIMEDOUT));
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
	xdefer(doclose, XINT(s));

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

	xspawn(hub, dorecv_timeout, XPTR(&dest));
	xspawn(hub, dosend_timeout, XPTR(&dest));

	mu_assert_int_eq(xhub_run(hub), 0);
	xhub_free(&hub);
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
}

