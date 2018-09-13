#include "../include/crux/dnsc.h"
#include "../include/crux/hashmap.h"
#include "../include/crux/hash.h"

#include <time.h>

struct key {
	const char *name;
	uint8_t namelen;
	enum xdns_type type;
};

struct entry {
	time_t time;
	struct xdns_res *res;
	uint8_t namelen;
	char name[];
};

struct xdns_cache {
	XHASHMAP(dnsc, struct entry *, 2);
};

static bool
dnsc_has_key(struct entry **ep, const struct key *k, size_t kn)
{
	(void)kn;
	struct entry *e = *ep;
	return e->namelen == k->namelen &&
		e->res->rr.rtype == k->type &&
		memcmp(e->name, k, kn) == 0;
}

static uint64_t
dnsc_hash(const struct key *k, size_t kn)
{
	(void)kn;
	union xseed seed = {
		.u128 = {
			k->type * UINT64_C(2654435761),
			XSEED_RANDOM->u128.high
		}
	};
	return xhash_sip(k, kn, &seed);
}

XHASHMAP_STATIC(dnsc, struct xdns_cache, const struct key *, struct entry *)

static bool
entry_expired(const struct entry *e)
{
	return e->time + e->res->rr.ttl < time(NULL);
}

int
xdns_cache_new(struct xdns_cache **cachep)
{
	struct xdns_cache *cache = malloc(sizeof(*cache));
	if (cache == NULL) { return xerrno; }
	dnsc_init(cache, 0.9, 0);
	*cachep = cache;
	return 0;
}

void
xdns_cache_free(struct xdns_cache **cachep)
{
	struct xdns_cache *cache = *cachep;
	if (cache != NULL) {
		*cachep = NULL;

		struct entry **ep;
		XHASHMAP_EACH(cache, ep) {
			struct entry *e = *ep;
			xdns_res_free(&e->res);
			free(e);
		}

		dnsc_final(cache);
		free(cache);
	}
}

const struct xdns_res *
xdns_cache_get(struct xdns_cache *cache, const char *name, enum xdns_type type)
{
	size_t namelen = strnlen(name, XDNS_MAX_NAME+1);
	if (namelen > XDNS_MAX_NAME) { return NULL; }

	struct key key = { name, (uint8_t)namelen, type };

	struct entry **ep = dnsc_get(cache, &key, 0);
	if (ep == NULL) { return NULL; }

	struct entry *e = *ep;
	if (entry_expired(e)) {
		dnsc_remove(cache, ep);
		return NULL;
	}
	return e->res;
}

int
xdns_cache_put(struct xdns_cache *cache, const struct xdns *dns)
{
	time_t now = time(NULL);
	struct xdns_iter iter;
	xdns_iter_init(&iter, dns);

	while (xdns_iter_next(&iter) == XDNS_MORE) {
		struct xdns_res *res;
		int rc = xdns_res_copy(&res, &iter.res);
		if (rc == xerr_sys(ENOTSUP)) { continue; }
		if (rc < 0) { return rc; }

		struct key key = { iter.name, iter.namelen, iter.res.rr.rtype };
		struct entry **ep;
		rc = dnsc_reserve(cache, &key, 0, &ep);
		if (rc < 0) {
			xdns_res_free(&res);
			return rc;
		}

		struct entry *e;
		if (rc == 1) {
			e = *ep;
			xdns_res_free(&e->res);
		}
		else {
			e = malloc(sizeof(*e) + iter.namelen + 1);
			if (e == NULL) {
				rc = xerrno;
				dnsc_remove(cache, ep);
				return rc;
			}
			e->namelen = iter.namelen;
			memcpy(e->name, iter.name, iter.namelen);
		}
		e->res = res;
		e->time = now;
	}

	return 0;
}

void
xdns_cache_print(const struct xdns_cache *cache, FILE *out)
{
	if (out == NULL) { out = stdout; }
	if (cache == NULL) {
		fprintf(out, "<crux:dns:cache:(null)>\n");
		return;
	}

	fprintf(out, "<crux:dns:cache:%p> {\n", (void *)cache);

	struct entry **ep;
	XHASHMAP_EACH(cache, ep) {
		struct entry *e = *ep;
		if (!entry_expired(e)) {
			char buf[4096];
			ssize_t len = snprintf(buf, sizeof(buf), "    %.*s = {", (int)e->namelen, e->name);
			ssize_t n = xdns_res_json(e->res, buf+len, sizeof(buf)-len);
			if (n > 0) {
				len += n;
				n = snprintf(buf+len, sizeof(buf)-len, "    },\n");
				if (n > 0) {
					len += n;
					if (len < (ssize_t)sizeof(buf)) {
						fwrite(buf, 1, len, out);
					}
				}
			}
		}
	}

	fprintf(out, "}\n");
}

