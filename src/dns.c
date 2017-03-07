#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include "../include/crux/dns.h"
#include "../include/crux/err.h"

const struct xdns_opt XDNS_OPT_DEFAULT = XDNS_OPT_MAKE(XDNS_MAX_OPT_UDP, 0);

static uint16_t
u16(const uint8_t buf[static 2])
{
	uint16_t val;
	memcpy(&val, buf, sizeof(val));
	return ntohs(val);
}

static uint32_t
u32(const uint8_t buf[static 4])
{
	uint32_t val;
	memcpy(&val, buf, sizeof(val));
	return ntohl(val);
}

static void *
tail(struct xdns *p)
{
	return p->buf + p->len;
}

/**
 * Encodes a host name at the current buffer position
 *
 * @param  p      dns pointer
 * @param  dec    presentational host name
 * @param  len    length of host name
 * @param  extra  additional space required in the buffer after the name
 * @return  0 on success, <0 error code
 */
static ssize_t
encode_name(struct xdns *p, const char *dec, size_t len, size_t extra)
{
	uint8_t *enc = tail(p);
	size_t encsize = sizeof(p->buf) - p->len - extra;
	if (dec[len-1] != '.') {
		if (len > XDNS_MAX_NAME - 1) { return -EINVAL; }
		if (len > encsize - 1) { return -ENOBUFS; }
		memcpy(enc+1, dec, len);
		enc[++len] = '.';
	}
	else {
		if (len > XDNS_MAX_NAME) { return -EINVAL; }
		if (len > encsize) { return -ENOBUFS; }
		memcpy(enc+1, dec, len);
	}

	// TODO: remove this ugly case
	if (len == 1) {
		p->len++;
		enc[0] = 0;
		return 1;
	}

	size_t mark = 0;
	for (size_t i = 1; i <= len; i++) {
		if (enc[i] == '.') {
			size_t label = i - mark - 1;
			if (label > XDNS_MAX_LABEL) {
				return -EINVAL;
			}
			enc[mark] = (uint8_t)label;
			mark = i;
		}
	}
	enc[mark] = 0;
	p->len += mark + 1;
	return mark + 1;
}

ssize_t
read_name(const uint8_t *buf, size_t pos, size_t len, char name[static 256])
{
	size_t mark = 0;
	size_t namelen = 0;
	uint8_t b;

	for (int limit = 0; limit < 128; limit++) {
		if (pos >= len) { goto error; }
		b = buf[pos++];
		switch (3 & (b >> 6)) {
		case 0: // normal label
			if (b > 63) { goto error; }
			if (pos+b > len || namelen+b+1 > 255) { goto error; }
			memcpy(name+namelen, buf+pos, b);
			pos += b;
			if (b || !namelen) {
				memcpy(name+namelen+b, ".", 2);
				namelen += b + 1;
			}
			if (!b) {
				return mark ? mark : pos;
			}
			break;
		case 3: // compressed, move the buffer position
			if (pos >= len) { goto error; }
			if (!mark) { mark = pos + 1; }
			pos = ((b - 192) << 8) + buf[pos];
			break;
		default:
			goto error;
		}
	}

error:
	return -EINVAL;
}

static ssize_t
read_query(const uint8_t *buf, size_t pos, size_t len, struct xdns_query *q)
{
	if (pos + sizeof(*q) > len) { return -EINVAL; }
	const struct xdns_query *qs = (const struct xdns_query *)(buf + pos);
	q->qtype = ntohs(qs->qtype);
	q->qclass = ntohs(qs->qclass);
	return pos + sizeof(*q);
}

static ssize_t
read_a(const uint8_t *buf, size_t pos, size_t len, struct in_addr *a)
{
	if (pos + sizeof(*a) > len) { return -EINVAL; }
	memcpy(a, buf+pos, sizeof(*a));
	return pos + sizeof(*a);
}

static ssize_t
read_soa(const uint8_t *buf, size_t pos, size_t len, struct xdns_soa *soa)
{
	ssize_t rc;
	rc = read_name(buf, pos, len, soa->mname);
	if (rc < 0) { return rc; }
	rc = read_name(buf, rc, len, soa->rname);
	if (rc < 0) { return rc; }
	if ((size_t)rc + 20 > len) { return -EINVAL; }
	soa->serial = u32(buf+rc); rc += 4;
	soa->refresh = u32(buf+rc); rc += 4;
	soa->retry = u32(buf+rc); rc += 4;
	soa->expire = u32(buf+rc); rc += 4;
	soa->minimum = u32(buf+rc); rc += 4;
	return rc;
}

static ssize_t
read_mx(const uint8_t *buf, size_t pos, size_t len, struct xdns_mx *mx)
{
	if (pos + 2 > len) { return -EINVAL; }
	mx->preference = u16(buf+pos); pos += 2;
	return read_name(buf, pos, len, mx->host);
}

static ssize_t
read_aaaa(const uint8_t *buf, size_t pos, size_t len, struct in6_addr *aaaa)
{
	if (pos + sizeof(*aaaa) > len) { return -EINVAL; }
	memcpy(aaaa, buf+pos, sizeof(*aaaa));
	return pos + sizeof(*aaaa);
}

static ssize_t
read_srv(const uint8_t *buf, size_t pos, size_t len, struct xdns_srv *srv)
{
	if (pos + 6 > len) { return -EINVAL; }
	srv->priority = u16(buf+pos); pos += 2;
	srv->weight = u16(buf+pos); pos += 2;
	srv->port = u16(buf+pos); pos += 2;
	return read_name(buf, pos, len, srv->target);
}

static ssize_t
read_rr(const uint8_t *buf, size_t pos, size_t len,
		struct xdns_rr *rr, union xdns_rdata *rdata)
{
	if (pos + sizeof(*rr) > len) { goto error; }
	const struct xdns_rr *rrs = (const struct xdns_rr *)(buf + pos);
	rr->rtype = ntohs(rrs->rtype);
	rr->rclass = ntohs(rrs->rclass);
	rr->ttl = ntohl(rrs->ttl);
	rr->rdlength = ntohs(rrs->rdlength);
	pos += sizeof(*rr);

	size_t epos = pos + rr->rdlength;
	if (epos > len) { goto error; }

	ssize_t npos;
	switch (rr->rtype) {
	case XDNS_A:
		npos = read_a(buf, pos, len, &rdata->a);
		break;
	case XDNS_NS:
		npos = read_name(buf, pos, len, rdata->ns);
		break;
	case XDNS_CNAME:
		npos = read_name(buf, pos, len, rdata->cname);
		break;
	case XDNS_SOA:
		npos = read_soa(buf, pos, len, &rdata->soa);
		break;
	case XDNS_PTR:
		npos = read_name(buf, pos, len, rdata->ptr);
		break;
	case XDNS_MX:
		npos = read_mx(buf, pos, len, &rdata->mx);
		break;
	case XDNS_TXT:
		rdata->txt.data = buf+pos;
		rdata->txt.length = rr->rdlength;
		npos = epos;
		break;
	case XDNS_AAAA:
		npos = read_aaaa(buf, pos, len, &rdata->aaaa);
		break;
	case XDNS_SRV:
		npos = read_srv(buf, pos, len, &rdata->srv);
		break;
	default:
		npos = epos;
		break;
	}

	if (npos < 0) { return npos; }
	if ((size_t)npos != epos) { goto error; }
	return npos;

error:
	return -EINVAL;
}

void
xdns_init(struct xdns *p, uint16_t id)
{
	memset(p, 0, sizeof(*p));

	p->hdr.id = htons(id);
	p->hdr.rd = 1;
	p->hdr.opcode = XDNS_QUERY;
	p->hdr.qdcount = 0;
	p->len = sizeof(p->hdr);
}

int
xdns_load(struct xdns *p, const void *packet, size_t len)
{
	if (len > sizeof(p->buf)) {
		return -ENOBUFS;
	}

	memcpy(p->buf, packet, len);
	p->len = len;
	return 0;
}

void
xdns_final(struct xdns *p)
{
	(void)p;
}

ssize_t
xdns_add_query(struct xdns *p, const char *host, enum xdns_type type)
{
	if (p->hdr.ancount || p->hdr.nscount || p->hdr.arcount) {
		return -EPERM;
	}

	struct xdns_query q;

	ssize_t rc = encode_name(p, host, strnlen(host, 256), sizeof(q));
	if (rc < 0) { return rc; }

	q.qtype = htons((uint16_t)type);
	q.qclass = htons((uint16_t)XDNS_IN);

	memcpy(tail(p), &q, sizeof(q));
	p->len += sizeof(q);

	p->hdr.qdcount = htons(ntohs(p->hdr.qdcount) + 1);
	return p->len;
}

ssize_t
xdns_add_rr(struct xdns *p, const char *name, struct xdns_rr rr, const void *rdata)
{
	uint16_t rdlength = rr.rdlength;
	ssize_t rc = encode_name(p, name, strnlen(name, 256), sizeof(rr) + rdlength);
	if (rc < 0) { return rc; }

	rr.rtype = htons(rr.rtype);
	rr.rclass = htons(rr.rclass);
	rr.ttl = htonl(rr.ttl);
	rr.rdlength = htons(rr.rdlength);

	memcpy(tail(p), &rr, sizeof(rr));
	p->len += sizeof(rr);
	memcpy(tail(p), rdata, rdlength);
	p->len += rdlength;

	p->hdr.arcount = htons(ntohs(p->hdr.arcount) + 1);
	return p->len;
}

ssize_t 
xdns_add_opt(struct xdns *p, struct xdns_opt opt, const void *rdata)
{
	if (opt.rtype != XDNS_OPT || opt.udpmax < 512 || 4096 < opt.udpmax) {
		return -EINVAL;
	}
	
	if (opt.version != 0 || opt.dof) {
		return -ENOTSUP;
	}

	struct xdns_rr rr;
	memcpy(&rr, &opt, sizeof(rr));

	return xdns_add_rr(p, ".", rr, rdata);
}

ssize_t
xdns_send(const struct xdns *p, int s, struct sockaddr *addr, socklen_t len)
{
	ssize_t rc = sendto(s, p->buf, p->len, 0, addr, len);
	return rc < 0 ? XERRNO : rc;
}

ssize_t
xdns_recv(struct xdns *p, int s, struct sockaddr *addr, socklen_t *len)
{
	ssize_t rc = recvfrom(s, p->buf, sizeof(p->buf), 0, addr, len);
	if (rc < 0) {
		return XERRNO;
	}
	p->len = (size_t)rc;
	return rc;
}

void
xdns_iter_init(struct xdns_iter *i, const struct xdns *p)
{
	i->p = p;
	i->hdr.id = ntohs(p->hdr.id);
	i->hdr.qr = p->hdr.qr;
	i->hdr.opcode = p->hdr.opcode;
	i->hdr.aa = p->hdr.aa;
	i->hdr.tc = p->hdr.tc;
	i->hdr.rd = p->hdr.rd;
	i->hdr.ra = p->hdr.ra;
	i->hdr.rcode = p->hdr.rcode;
	i->hdr.qdcount = ntohs(p->hdr.qdcount);
	i->hdr.ancount = ntohs(p->hdr.ancount);
	i->hdr.nscount = ntohs(p->hdr.nscount);
	i->hdr.arcount = ntohs(p->hdr.arcount);
	i->pos = sizeof(p->hdr);
	i->section = XDNS_S_QD;
	i->at = 0;
}

int
xdns_iter_next(struct xdns_iter *i)
{
	ssize_t pos = i->pos;

	for (; i->section <= XDNS_S_AR; i->at = 0, i->section++) {
		if (i->at < i->hdr.counts[i->section]) {
			pos = read_name(i->p->buf, pos, i->p->len, i->name);
			if (pos < 0) { return (int)pos; }
			if (i->section == XDNS_S_QD) {
				pos = read_query(i->p->buf, pos, i->p->len, &i->query);
			}
			else {
				pos = read_rr(i->p->buf, pos, i->p->len, &i->rr, &i->rdata);
			}
			if (pos < 0) { return (int)pos; }
			i->pos = pos;
			i->at++;
			return XDNS_MORE;
		}
	}

	return XDNS_DONE;
}

