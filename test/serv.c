#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <crux.h>
#include <crux/net.h>
#include <crux/http.h>
#include <crux/filter.h>

struct server {
	const char *net;
	struct xfilter *filter;
	union xaddr addr;
	int fd;
};

struct client {
	struct server *server;
	union xaddr addr;
	int fd;
};

static void
respond(int fd, struct xhttp_map *map)
{
	static char resp[] =
		"HTTP/1.1 200 OK\r\n"
		;

	static char body[] =
		"Content-Length: 6\r\n"
		"\r\n"
		"Hello\n"
		;

	struct iovec iov[3];
	iov[0].iov_base = resp;
	iov[0].iov_len = sizeof(resp) - 1;
	xhttp_map_full(map, &iov[1], 1);
	iov[2].iov_base = body;
	iov[2].iov_len = sizeof(body) - 1;

	xwritev(fd, iov, 3, -1);
}

static void
connection(struct xhub *h, union xvalue val)
{
	(void)h;

	struct client *cli = val.ptr;
	int fd = cli->fd;

	xdefer_close(fd);
	xdefer_free(cli);

	struct xhttp_map *map;
	xhttp_map_new(&map);

	struct xhttp http;
	xhttp_init_request(&http, map, cli->server->filter);

	struct xbuf *buf = xbuf(8000, true);
	while (xbuf_read(buf, fd, 4096, 2000) > 0) {
		for (;;) {
			ssize_t n = xcheck(xhttp_next(&http, buf));
			if (n == 0) { break; }

			xbuf_trim(buf, n);
			if (xhttp_is_done(&http)) {
				xhttp_map_addstr(map, "Server", "crux");
				respond(fd, map);
				xhttp_reset(&http);
			}
		}
	}
}

static void
intr(struct xhub *h, union xvalue val)
{
	struct server *srv = val.ptr;

	xsignal(SIGINT, XTIMEOUT_DETACH);
	printf("[%d] stopping %s\n", getpid(), xaddrstr(&srv->addr));
	xhub_stop(h);
}

static void
term(struct xhub *h, union xvalue val)
{
	(void)h;

	struct server *srv = val.ptr;

	xsignal(SIGTERM, XTIMEOUT_DETACH);
	printf("[%d] terminating %s\n", getpid(), xaddrstr(&srv->addr));
	xclose(val.i);
}

static void
server(struct xhub *h, union xvalue val)
{
	struct server *srv = val.ptr;
	int s = xcheck(xbind(srv->net, SOCK_STREAM, XREUSEADDR, &srv->addr));
	srv->fd = s;

	xspawn(h, intr, xptr(srv));
	xspawn(h, term, xptr(srv));

	printf("[%d] listening on %s\n", getpid(), xaddrstr(&srv->addr));

	for (;;) {
		struct client *cli = malloc(sizeof(*cli));
		cli->fd = xcheck(xaccept(s, 0, -1, &cli->addr));
		cli->server = srv;
		printf("[%d] accepted from %s\n", getpid(), xaddrstr(&cli->addr));
		xspawn(h, connection, xptr(cli));
	}
}

int
main(int argc, char *const *argv)
{
	struct server srv = {
		.fd = -1,
		.net = argc > 1 ? argv[1] : ":3333",
	};

	static const struct xfilter_expr filt[] = {
		{ "^[^X]", NULL, XFILTER_HTTP },
		{ "^X-Bar$", "^baz", XFILTER_HTTP },
		{ "^User-Agent$", "curl", XFILTER_HTTP },
	};

	struct xfilter_err err;
	int rc = xfilter_new(&srv.filter, filt, xlen(filt), XFILTER_ACCEPT, &err);
	if (rc < 0) {
		xerr_fabort(rc, "%s", err.message);
	}

	struct xhub *hub;
	xcheck(xhub_new(&hub));
	xspawn(hub, server, xptr(&srv));
	xhub_run(hub);
	xhub_free(&hub);
	return 0;
}

