# CoRoUtine eXchange

This code was born out of an idea for a programming language I have been 
working on. The idea was to have a standard library that could be usable
from C. The language has coroutines, so I wanted a way to have those usable
in foreign code. I'm still not sure if that's a good idea or not, but I
figure some of this code could at least be interesting. This is the task-based
concurrency system. I have yet to get around to channels and a multi-event selection system.

The coroutines are currently only supported on x86 processors (32-bit and
64-bit), and they have been tested on Linux, FreeBSD, and Mac OS X. The
library as a whole has been developed against Linux and Mac OS. It probably
needs a little to work to compile on BSD, but I haven't gotten around to
that yet.

There is one build dependency at the moment: Intel's hyperscan library (libhs).
With that installed, it builds with GNU make.

A lot of these ideas were borrowed heavily from [levee](https://github.com/cablehead/levee)

### Example

This is a basic HTTP server.

```C
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

static const char response[] = 
	"HTTP/1.1 200 OK\r\n"
	"Content-Length: 6\r\n"
	"\r\n"
	"Hello\n"
	;

static void server(struct xhub *h, union xvalue val);
static void connection(struct xhub *h, union xvalue val);
static void intr(struct xhub *h, union xvalue val);
static void term(struct xhub *h, union xvalue val);

int
main(int argc, char *const *argv)
{
	// setup the server configuration
	struct server srv = {
		.fd = -1,
		.net = argc > 1 ? argv[1] : ":3333",
	};
	
	// create a filter
	// this is passed to the http parser to supress headers
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

static void
server(struct xhub *h, union xvalue val)
{
	struct server *srv = val.ptr;
	int s = xcheck(xbind(srv->net, SOCK_STREAM, XREUSEADDR, &srv->addr));
	srv->fd = s;

	// spawn signal handling tasks
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

static void
connection(struct xhub *h, union xvalue val)
{
	(void)h;

	struct client *cli = val.ptr;
	int fd = cli->fd;

	// clean up resources when the coroutine exits
	xdefer_close(fd);
	xdefer_free(cli);

	// http header fields may optionally be collected
	// the parser will not yield header fields when doing so
	struct xhttp_map *map;
	xhttp_map_new(&map);

	struct xhttp http;
	xhttp_init_request(&http, map, cli->server->filter);

	// we'll use an auto-managed buffer per connection
	struct xbuf *buf = xbuf(8000, true);
	while (xbuf_read(buf, fd, 4096, 2000) > 0) {
		for (;;) {
			// the http library is a pull parser that does not allocate
			ssize_t n = xcheck(xhttp_next(&http, buf));
			if (n == 0) { break; }

			// once we have a value, we can trim the buffer by the parsed amount
			// only do this once you no longer need the input values
			xbuf_trim(buf, n);
			
			// respond when complete and reset the parser for another request
			if (xhttp_is_done(&http)) {
				xhttp_reset(&http);
				xwrite(fd, response, sizeof(response)-1, -1);
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
```