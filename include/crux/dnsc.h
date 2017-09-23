#ifndef CRUX_DNSC_H
#define CRUX_DNSC_H

#include "dns.h"

struct xdns_cache;

XEXTERN int
xdns_cache_new(struct xdns_cache **cachep);

XEXTERN void
xdns_cache_free(struct xdns_cache **cachep);

XEXTERN const struct xdns_res *
xdns_cache_get(struct xdns_cache *cache, const char *name, enum xdns_type type);

XEXTERN int
xdns_cache_put(struct xdns_cache *cache, const struct xdns *dns);

XEXTERN void
xdns_cache_print(const struct xdns_cache *cache, FILE *out);

#endif

