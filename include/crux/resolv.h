#ifndef XRESOLV_H
#define XRESOLV_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "dns.h"

struct xresolv_config {
	int timeout;
	int attempts;
	struct {
		uint16_t udpmax;
		bool dnssec;
		bool enabled;
	} edns0;
	bool rotate;
	bool debug;
	struct sockaddr_in *hosts; // TODO: change sockaddr type
	size_t nhosts;
};

struct xresolv_result {
	union {
		struct sockaddr_in in;
		struct sockaddr_in6 in6;
	};
	enum xdns_type type;
	int32_t ttl;
	uint16_t priority;
	uint16_t weight;
};

struct xresolv;

XEXTERN int
xresolv_new(struct xresolv **rp, const struct xresolv_config *cfg);

XEXTERN void
xresolv_free(struct xresolv **rp);

XEXTERN int
xresolv(struct xresolv *r, const char *name, struct xresolv_result *rr, int count);

#endif

