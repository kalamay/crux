#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <crux.h>
#include <crux/net.h>
#include <crux/http.h>

static void
respond(union xvalue val)
{
	static const char resp[] =
		"HTTP/1.1 200 OK\r\n"
		"Server: crux\r\n"
		"Content-Length: 6\r\n"
		"\r\n"
		"Hello\n"
		;

	int fd = val.i;
	xwriten(fd, resp, sizeof(resp) - 1, -1);
}

static void
connection(struct xhub *h, union xvalue val)
{
	(void)h;
	int fd = val.i;
	xdefer_close(fd);

	struct xhttp http;
	xhttp_init_request(&http);

	struct xbuf *buf = xbuf(8000, true);
	while (xbuf_read(buf, fd, 4096, 2000) > 0) {
		for (;;) {
			ssize_t n = xcheck(xhttp_next(&http, buf));
			if (n == 0) { break; }

			xbuf_trim(buf, n);
			if (xhttp_is_done(&http)) {
				respond(XINT(fd));
				xhttp_reset(&http);
			}
		}
	}
}

static void
intr(struct xhub *h, union xvalue val)
{
	(void)h;
	(void)val;
	xsignal(SIGINT, XTIMEOUT_DETACH);
	printf("[%d] stopping\n", getpid());
	xhub_stop(h);
}

static void
term(struct xhub *h, union xvalue val)
{
	(void)h;
	(void)val;
	xsignal(SIGTERM, XTIMEOUT_DETACH);
	printf("[%d] terminating\n", getpid());
	xclose(val.i);
}

static void
server(struct xhub *h, union xvalue val)
{
	(void)val;
	union xaddr addr;
	int s = xcheck(xbind(":3333", SOCK_STREAM, XREUSEADDR, &addr));

	xspawn(h, intr, XINT(s));
	xspawn(h, term, XINT(s));

	printf("[%d] listening on %s\n", getpid(), xaddrstr(&addr));

	for (;;) {
		union xaddr addr;
		int fd = xcheck(xaccept(s, 0, -1, &addr));
		printf("[%d] accepted from %s\n", getpid(), xaddrstr(&addr));
		xspawn(h, connection, XINT(fd));
	}
}

int
main(void)
{
	struct xhub *hub;
	xcheck(xhub_new(&hub));
	xspawn(hub, server, XNULL);
	xhub_run(hub);
	xhub_free(&hub);
	return 0;
}
