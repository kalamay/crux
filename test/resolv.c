#include "mu.h"
#include "../include/crux/resolv.h"
#include "../include/crux/hub.h"
#include "../src/resolv.h"

static bool done = false;

static void
doresolv(struct xhub *h, void *res)
{
	(void)h;

	struct xresolv_result rr[4];
	int rc = xresolv(res, "www.google.com", rr, xlen(rr));
	//int rc = xresolv(res, "_http._tcp.jeremy.imgix.com", rr, xlen(rr));
	printf("rc: %d\n", rc);
	if (rc < 0) {
		printf("error: %s\n", strerror(-rc));
	}
	for (int i = 0; i < rc; i++) {
		char buf[64];
		switch (rr[i].type) {
		case XDNS_A:
			printf("IPv4: %s:%d, ttl=%d, priority=%u, weight=%u\n",
				inet_ntop(AF_INET, &rr[i].in.sin_addr, buf, sizeof(buf)),
				ntohs(rr[i].in.sin_port),
				rr[i].ttl, rr[i].priority, rr[i].weight);
			break;
		case XDNS_AAAA:
			printf("IPv6: %s:%d, ttl=%d, priority=%u, weight=%u\n",
				inet_ntop(AF_INET6, &rr[i].in6.sin6_addr, buf, sizeof(buf)),
				ntohs(rr[i].in6.sin6_port),
				rr[i].ttl, rr[i].priority, rr[i].weight);
			break;
		default:
			break;
		}
	}
	done = true;
}

static void
dostuff(struct xhub *h, void *data)
{
	(void)h;

	int *stuff = data;
	while (!done) {
		stuff[0]++;
		xsleep(1);
	}
}

int
main(void)
{
	mu_init("resolv");

	struct sockaddr_in hosts[1];
	memset(hosts, 0, sizeof(hosts));
	hosts[0].sin_family = AF_INET;
	hosts[0].sin_port = htons(53);
	inet_aton("8.8.8.8", &hosts[0].sin_addr);

	struct xresolv_config cfg = {
		.timeout = 1000,
		.attempts = 5,
		.hosts = hosts,
		.nhosts = 1
	};

	struct xresolv *r;
	mu_assert_int_eq(xresolv_new(&r, &cfg), 0);

	int stuff = 0;

	struct xhub *hub;
	mu_assert_int_eq(xhub_new(&hub), 0);
	mu_assert_int_eq(xspawn(hub, doresolv, r), 0);
	mu_assert_int_eq(xspawn(hub, dostuff, &stuff), 0);
	mu_assert_int_eq(xhub_run(hub), 0);

	mu_assert_int_gt(stuff, 0);

	xhub_free(&hub);
	xresolv_free(&r);
}

