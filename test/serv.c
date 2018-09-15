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
				//xhttp_map_addstr(map, "Server", "crux");
				xhttp_map_print(map, stdout);
				respond(xint(fd));
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

	static const char *keys[] = {
		"Accept",
		"User-Agent",
	};

	xfilter_new(&srv.filter, keys, xlen(keys), XFILTER_REJECT);

	struct xhub *hub;
	xcheck(xhub_new(&hub));
	xspawn(hub, server, xptr(&srv));
	xhub_run(hub);
	xhub_free(&hub);
	return 0;
}

