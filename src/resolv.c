#include "resolv.h"
#include "../include/crux/hub.h"
#include "../include/crux/err.h"
#include "../include/crux/def.h"
#include "../include/crux/net.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <assert.h>

int
xresolv_new(struct xresolv **rp, const struct xresolv_config *cfg)
{
	assert(rp != NULL);
	assert(cfg != NULL);
	assert(cfg->nhosts > 0);

	struct xresolv *r = malloc(sizeof(*r));
	if (r == NULL) { return xerrno; }
	int rc = xresolv_init(r, cfg);
	if (rc >= 0) { *rp= r; }
	return rc;
}

void
xresolv_free(struct xresolv **rp)
{
	assert(rp != NULL);

	struct xresolv *r = *rp;
	if (r == NULL) { return; }
	*rp = NULL;
	xresolv_final(r);
	free(r);
}

int
xresolv_init(struct xresolv *r, const struct xresolv_config *cfg)
{
	r->cfg = cfg;
	r->fdpos = 0;
	r->hostpos = 0;

	uint32_t a = (uint32_t)clock();
	uint32_t b = (uint32_t)time(NULL);
	uint32_t c = (uint32_t)getpid();
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
	a -= c;  a ^= rot(c, 4);  c += b;
	b -= a;  b ^= rot(a, 6);  a += c;
	c -= b;  c ^= rot(b, 8);  b += a;
	a -= c;  a ^= rot(c,16);  c += b;
	b -= a;  b ^= rot(a,19);
	c -= b;  c ^= rot(b, 4);
#undef rot

	r->seed = (unsigned)c;

	return 0;
}

void
xresolv_final(struct xresolv *r)
{
	for (unsigned char i = 0; i < r->fdpos; i++) {
		close(r->fdpool[i]);
	}
}


static int
rr_cmp(const void *p1, const void *p2)
{
	uint16_t prio1 = ((struct xresolv_result *)p1)->priority;
	uint16_t prio2 = ((struct xresolv_result *)p2)->priority;
	return (int)prio1 - (int)prio2;
}

static int
read_packet(struct xresolv *r, const struct xdns *p, struct xresolv_result *rr, int count)
{
	struct xdns_iter iter;
	xdns_iter_init(&iter, p);
	xdns_print(p, stdout);

	bool sort = false;
	int i = 0;
	while (xdns_iter_next(&iter) == XDNS_MORE) {
		if (iter.section == XDNS_S_AN) {
			if (iter.res.rr.rtype == XDNS_A) {
				memset(&rr[i].in, 0, sizeof(rr[i].in));
				rr[i].in.sin_addr = iter.res.rdata.a;
				rr[i].type = XDNS_A;
				rr[i].ttl = iter.res.rr.ttl;
				rr[i].priority = 0;
				rr[i].weight = 0;
				if (++i >= count) { break; }
			}
			else if (iter.res.rr.rtype == XDNS_AAAA) {
				memset(&rr[i].in6, 0, sizeof(rr[i].in6));
				rr[i].in6.sin6_addr = iter.res.rdata.aaaa;
				rr[i].type = XDNS_AAAA;
				rr[i].ttl = iter.res.rr.ttl;
				rr[i].priority = 0;
				rr[i].weight = 0;
				if (++i >= count) { break; }
			}
			else if (iter.res.rr.rtype == XDNS_CNAME) {
				int srvs = xresolv(r, iter.res.rdata.cname, rr+i, count-i);
				if (srvs < 0) {
					return srvs;
				}
				if ((i += srvs) >= count) { break; }
			}
			else if (iter.res.rr.rtype == XDNS_SRV) {
				int srvs = xresolv(r, iter.res.rdata.srv.target, rr+i, count-i);
				if (srvs < 0) {
					return srvs;
				}
				for (int end = i + srvs; i < end; i++) {
					rr[i].in.sin_port = htons(iter.res.rdata.srv.port);
					rr[i].priority = iter.res.rdata.srv.priority;
					rr[i].weight = iter.res.rdata.srv.weight;
					if (rr[i].ttl > iter.res.rr.ttl) {
						rr[i].ttl = iter.res.rr.ttl;
					}
				}
				sort = true;
			}
		}
	}

	if (sort) {
		qsort(rr, i, sizeof(*rr), rr_cmp);
	}

	return i;
}

int
xresolv(struct xresolv *r, const char *name, struct xresolv_result *rr, int count)
{
	assert(r != NULL);
	assert(name != NULL);
	assert(rr != NULL);

	struct xdns dns;
	uint8_t inbuf[XDNS_MAX_UDP], outbuf[XDNS_MAX_UDP];
	uint16_t id = (uint16_t)rand_r(&r->seed);
	xdns_init(&dns, inbuf, sizeof(inbuf), id);

	ssize_t rc;

	rc = xdns_add_query(&dns, name, XDNS_ANY);
	if (rc < 0) { return (int)rc; }

	int s;
	if (r->fdpos > 0) {
		s = r->fdpool[--r->fdpos];
	}
	else {
		s = xsocket(AF_INET, SOCK_DGRAM);
		if (s < 0) { return s; }
	}

	for (int sends = 0; sends < r->cfg->attempts; sends++) {
		struct sockaddr_in *to = &r->cfg->hosts[r->hostpos];
		struct sockaddr_storage from;
		socklen_t fromlen = sizeof(from);

		if (r->cfg->rotate && ++r->hostpos >= r->cfg->nhosts) {
			r->hostpos = 0;
		}
		rc = xsendto(s, inbuf, sizeof(inbuf), 0, (struct sockaddr *)to, sizeof(*to), -1);
		if (rc < 0) { break; }

		// TODO: verify recieved id
		rc = xrecvfrom(s, outbuf, sizeof(outbuf), 0,
				(struct sockaddr *)&from, &fromlen, r->cfg->timeout);
		if (rc > 0) {
			xdns_load(&dns, outbuf, rc);
			rc = read_packet(r, &dns, rr, count);
			break;
		}
		if (rc != xerr_sys(ETIMEDOUT)) { break; }
	}

	if (r->fdpos < xlen(r->fdpool)) {
		r->fdpool[r->fdpos++] = s;
	}
	else {
		close(s);
	}

	return (int)rc;
}

