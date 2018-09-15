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

	static const char *filt[] = {
		"Accept",
		"User-Agent",
	};

	struct xhttp_map *map;
	xhttp_map_new(&map);

	struct xhttp http;
	xhttp_init_request(&http, map);

	xhttp_filter(&http, filt, xlen(filt), XHTTP_FILT_REJECT);

	struct xbuf *buf = xbuf(8000, true);
	while (xbuf_read(buf, fd, 4096, 2000) > 0) {
		for (;;) {
			ssize_t n = xcheck(xhttp_next(&http, buf));
			if (n == 0) { break; }

			xbuf_trim(buf, n);
			if (xhttp_is_done(&http)) {
				//xhttp_map_addstr(map, "Server", "crux");
				//xhttp_map_print(map, stdout);
				respond(xint(fd));
				xhttp_reset(&http);
			}
		}
	}
}

static void
intr(struct xhub *h, union xvalue val)
{
	xsignal(SIGINT, XTIMEOUT_DETACH);
	union xaddr addr;
	xsockaddr(val.i, &addr);
	printf("[%d] stopping %s\n", getpid(), xaddrstr(&addr));
	xhub_stop(h);
}

static void
term(struct xhub *h, union xvalue val)
{
	(void)h;

	xsignal(SIGTERM, XTIMEOUT_DETACH);
	union xaddr addr;
	xsockaddr(val.i, &addr);
	printf("[%d] terminating %s\n", getpid(), xaddrstr(&addr));
	xclose(val.i);
}

static void
server(struct xhub *h, union xvalue val)
{
	union xaddr addr;
	int s = xcheck(xbind(val.cptr, SOCK_STREAM, XREUSEADDR, &addr));

	xspawn(h, intr, xint(s));
	xspawn(h, term, xint(s));

	printf("[%d] listening on %s\n", getpid(), xaddrstr(&addr));

	for (;;) {
		union xaddr addr;
		int fd = xcheck(xaccept(s, 0, -1, &addr));
		printf("[%d] accepted from %s\n", getpid(), xaddrstr(&addr));
		xspawn(h, connection, xint(fd));
	}
}

int
main(int argc, char *const *argv)
{
	const char *net = argc > 1 ? argv[1] : ":3333";

	struct xhub *hub;
	xcheck(xhub_new(&hub));
	xspawn(hub, server, xcptr(net));
	xhub_run(hub);
	xhub_free(&hub);
	return 0;
}

