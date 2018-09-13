#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <arpa/inet.h>

#include "../include/crux/dns.h"
#include "../include/crux/err.h"

const struct xdns_opt XDNS_OPT_DEFAULT = XDNS_OPT_MAKE(XDNS_MAX_UDP_EDNS, 0);

static uint16_t
u16(const uint8_t buf[static 2])
{
	uint16_t val;
	memcpy(&val, buf, sizeof(val));
#if BYTE_ORDER == LITTLE_ENDIAN
	val = __builtin_bswap16(val);
#endif
	return val;
}

static uint32_t
u32(const uint8_t buf[static 4])
{
	uint32_t val;
	memcpy(&val, buf, sizeof(val));
#if BYTE_ORDER == LITTLE_ENDIAN
	val = __builtin_bswap32(val);
#endif
	return val;
}

static void *
tail(struct xdns *p, size_t need)
{
	return p->len + need > p->max ?
		NULL : p->buf + p->len;
}

ssize_t
xdns_encode_name(uint8_t *enc, ssize_t len, const char *name)
{
	ssize_t nlen = strnlen(name, XDNS_MAX_NAME+1);
	ssize_t rlen = (nlen == 0 || name[nlen-1] != '.') ? nlen+1 : nlen;
	if (rlen == 1) {
		if (len == 0) { return xerr_sys(ENOBUFS); }
		enc[0] = 0;
		return 1;
	}
	if (rlen > XDNS_MAX_NAME) { return xerr_sys(EINVAL); }
	if (rlen >= len) { return xerr_sys(ENOBUFS); }

	memcpy(enc+1, name, nlen);
	enc[rlen] = '.';

	ssize_t mark = 0;
	for (ssize_t i = 1; i <= rlen; i++) {
		if (enc[i] == '.') {
			ssize_t label = i - mark - 1;
			if (label > XDNS_MAX_LABEL) {
				return xerr_sys(EINVAL);
			}
			enc[mark] = (uint8_t)label;
			mark = i;
		}
	}
	enc[mark] = 0;
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
	return xerr_sys(EINVAL);
}

static ssize_t
read_query(const uint8_t *buf, size_t pos, size_t len, struct xdns_query *q)
{
	if (pos + sizeof(*q) > len) { return xerr_sys(EINVAL); }
	const struct xdns_query *qs = (const struct xdns_query *)(buf + pos);
	q->qtype = ntohs(qs->qtype);
	q->qclass = ntohs(qs->qclass);
	return pos + sizeof(*q);
}

static ssize_t
read_a(const uint8_t *buf, size_t pos, size_t len, struct in_addr *a)
{
	if (pos + sizeof(*a) > len) { return xerr_sys(EINVAL); }
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
	if ((size_t)rc + 20 > len) { return xerr_sys(EINVAL); }
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
	if (pos + 2 > len) { return xerr_sys(EINVAL); }
	mx->preference = u16(buf+pos); pos += 2;
	return read_name(buf, pos, len, mx->host);
}

static ssize_t
read_aaaa(const uint8_t *buf, size_t pos, size_t len, struct in6_addr *aaaa)
{
	if (pos + sizeof(*aaaa) > len) { return xerr_sys(EINVAL); }
	memcpy(aaaa, buf+pos, sizeof(*aaaa));
	return pos + sizeof(*aaaa);
}

static ssize_t
read_srv(const uint8_t *buf, size_t pos, size_t len, struct xdns_srv *srv)
{
	if (pos + 6 > len) { return xerr_sys(EINVAL); }
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
	case XDNS_OPT:
		rr->ttl = rrs->ttl;
	default:
		memset(rdata, 0, sizeof(*rdata));
		npos = epos;
		break;
	}

	if (npos < 0) { return npos; }
	if ((size_t)npos != epos) { goto error; }
	return npos;

error:
	return xerr_sys(EINVAL);
}

void
xdns_init(struct xdns *p, uint8_t *buf, size_t len, uint16_t id)
{
	p->buf = buf;
	p->len = sizeof(*p->hdr);
	p->max = len;

	memset(p->hdr, 0, sizeof(*p->hdr));
	p->hdr->id = htons(id);
	p->hdr->rd = 1;
	p->hdr->opcode = XDNS_QUERY;
	p->hdr->qdcount = 0;
}

int
xdns_load(struct xdns *p, const void *packet, size_t len)
{
	p->buf = (uint8_t *)packet;
	p->len = len;
	p->max = len;
	return 0;
}

void
xdns_final(struct xdns *p)
{
	(void)p;
}

uint16_t
xdns_id(const struct xdns *p)
{
	return ntohs(p->hdr->id);
}

ssize_t
xdns_add_query(struct xdns *p, const char *host, enum xdns_type type)
{
	if (p->hdr->ancount || p->hdr->nscount || p->hdr->arcount) {
		return xerr_sys(EPERM);
	}

	struct xdns_query q;

	ssize_t rc = xdns_encode_name(
			p->buf + p->len,
			p->max - p->len - sizeof(q),
			host);
	if (rc < 0) { return rc; }
	p->len += rc;

	q.qtype = htons((uint16_t)type);
	q.qclass = htons((uint16_t)XDNS_IN);

	memcpy(tail(p, sizeof(q)), &q, sizeof(q));
	p->len += sizeof(q);

	p->hdr->qdcount = htons(ntohs(p->hdr->qdcount) + 1);
	return p->len;
}

ssize_t
xdns_add_rr(struct xdns *p, const char *name, struct xdns_rr rr, const void *rdata)
{
	uint16_t rdlength = rr.rdlength;
	ssize_t rc = xdns_encode_name(
			p->buf + p->len,
			p->max - p->len - sizeof(rr) - rdlength,
			name);
	if (rc < 0) { return rc; }
	p->len += rc;

	rr.rtype = htons(rr.rtype);
	rr.rclass = htons(rr.rclass);
	rr.ttl = htonl(rr.ttl);
	rr.rdlength = htons(rr.rdlength);

	memcpy(tail(p, sizeof(rr)), &rr, sizeof(rr));
	p->len += sizeof(rr);
	memcpy(tail(p, rdlength), rdata, rdlength);
	p->len += rdlength;

	p->hdr->arcount = htons(ntohs(p->hdr->arcount) + 1);
	return p->len;
}

ssize_t 
xdns_add_opt(struct xdns *p, struct xdns_opt opt, const void *rdata)
{
	if (opt.rtype != XDNS_OPT || opt.udpmax < 512 || 4096 < opt.udpmax) {
		return xerr_sys(EINVAL);
	}
	
	if (opt.version != 0 || opt.dof) {
		return xerr_sys(ENOTSUP);
	}

	struct xdns_rr rr;
	memcpy(&rr, &opt, sizeof(rr));

	return xdns_add_rr(p, ".", rr, rdata);
}

void
xdns_iter_init(struct xdns_iter *i, const struct xdns *p)
{
	i->p = p;
	i->hdr.id = ntohs(p->hdr->id);
	i->hdr.qr = p->hdr->qr;
	i->hdr.opcode = p->hdr->opcode;
	i->hdr.aa = p->hdr->aa;
	i->hdr.tc = p->hdr->tc;
	i->hdr.rd = p->hdr->rd;
	i->hdr.ra = p->hdr->ra;
	i->hdr.rcode = p->hdr->rcode;
	i->hdr.qdcount = ntohs(p->hdr->qdcount);
	i->hdr.ancount = ntohs(p->hdr->ancount);
	i->hdr.nscount = ntohs(p->hdr->nscount);
	i->hdr.arcount = ntohs(p->hdr->arcount);
	i->pos = sizeof(*p->hdr);
	i->section = XDNS_S_QD;
	i->at = 0;
}

int
xdns_iter_next(struct xdns_iter *i)
{
	ssize_t pos = (ssize_t)i->pos;

	for (; i->section <= XDNS_S_AR; i->at = 0, i->section++) {
		if (i->at < i->hdr.counts[i->section]) {
			size_t old = pos;
			pos = read_name(i->p->buf, pos, i->p->len, i->name);
			if (pos < 0) { return (int)pos; }
			i->namelen = pos - old;
			if (i->section == XDNS_S_QD) {
				pos = read_query(i->p->buf, pos, i->p->len, &i->query);
			}
			else {
				pos = read_rr(i->p->buf, pos, i->p->len, &i->res.rr, &i->res.rdata);
			}
			if (pos < 0) { return (int)pos; }
			i->pos = pos;
			i->at++;
			return XDNS_MORE;
		}
	}

	return XDNS_DONE;
}

enum xdns_type
xdns_type(const char *name, bool other)
{
#define XX(n, code, desc) \
	if (strncmp(name, #n, sizeof(#n)) == 0) { \
		return code; \
	}
	XDNS_TYPE_MAP(XX)
	if (other) {
		XDNS_TYPE_OTHER_MAP(XX)
	}
#undef XX
	return 0;
}

#define XX_NAME(name, code, desc) case code: return #name;
#define XX_DESC(name, code, desc) case code: return desc;

const char *
xdns_section_name(enum xdns_section s)
{
	switch (s) { XDNS_SECTION_MAP(XX_NAME) }
	return NULL;
}

const char *
xdns_section_desc(enum xdns_section s)
{
	switch (s) { XDNS_SECTION_MAP(XX_DESC) }
	return NULL;
}


const char *
xdns_type_name(enum xdns_type t)
{
	switch (t) { XDNS_TYPE_MAP(XX_NAME) }
	switch ((enum xdns_type_other)t) { XDNS_TYPE_OTHER_MAP(XX_NAME) }
	return NULL;
}

const char *
xdns_class_name(enum xdns_class c)
{
	switch (c) { XDNS_CLASS_MAP(XX_NAME) }
	return NULL;
}

const char *
xdns_opcode_name(enum xdns_opcode oc)
{
	switch (oc) { XDNS_OPCODE_MAP(XX_NAME) }
	return NULL;
}

const char *
xdns_rcode_name(enum xdns_rcode rc)
{
	switch (rc) { XDNS_RCODE_MAP(XX_NAME) }
	return NULL;
}

#undef XX_NAME
#undef XX_DESC

static const char *bools[] = { "false", "true" };

void
xdns_print(const struct xdns *p, FILE *out)
{
	char buf[8192];
	ssize_t n = xdns_json(p, buf, sizeof(buf));
	if (n > 0) {
		if (out == NULL) { out = stdout; }
		fwrite(buf, 1, n, out);
	}
}

#define fmt(...) do { \
	int n = snprintf(buf, len, __VA_ARGS__); \
	if (n < 0) { return -errno; } \
	if ((size_t)n > len) { return xerr_sys(ENOBUFS); } \
	buf += n; \
	len -= (size_t)n; \
} while (0)

ssize_t
xdns_json(const struct xdns *p, char *buf, size_t len)
{
	if (p == NULL || buf == NULL) { return xerr_sys(EINVAL); }

	char *start = buf;

	fmt("{\n"
		"  \"header\": {\n"
		"    \"id\": %u,\n"
		"    \"opcode\": \"%s\",\n"
		"    \"status\": \"%s\",\n"
		"    \"qr\": %u,\n"
		"    \"aa\": %s,\n"
		"    \"tc\": %s,\n"
		"    \"rd\": %s,\n"
		"    \"ra\": %s,\n"
		"    \"qdcount\": %u,\n"
		"    \"nscount\": %u,\n"
		"    \"ancount\": %u,\n"
		"    \"arcount\": %u\n"
		"  },\n",
		ntohs(p->hdr->id),
		xdns_opcode_name(p->hdr->opcode),
		xdns_rcode_name(p->hdr->rcode),
		p->hdr->qr,
		bools[p->hdr->aa],
		bools[p->hdr->tc],
		bools[p->hdr->rd],
		bools[p->hdr->ra],
		ntohs(p->hdr->qdcount),
		ntohs(p->hdr->ancount),
		ntohs(p->hdr->nscount),
		ntohs(p->hdr->arcount));

	struct xdns_iter iter;
	xdns_iter_init(&iter, p);

	int section = 0;
	int index;

	while (xdns_iter_next(&iter) == 0) {
		if (iter.at == 1) {
			if (section++ > 0) {
				fmt("    }\n  ],\n");
			}
			fmt("  \"%s\": [\n    {\n", xdns_section_desc(iter.section));
			index = 0;
		}
		if (index++ > 0) {
			fmt("    },\n    {\n");
		}
		if (iter.section == XDNS_S_QD) {
			fmt("      \"qname\": \"%s\",\n"
				"      \"qtype\": \"%s\",\n"
				"      \"qclass\": \"%s\"\n",
				iter.name,
				xdns_type_name(iter.query.qtype),
				xdns_class_name(iter.query.qclass));
		}
		else if (iter.res.rr.rtype == XDNS_OPT) {
			fmt("      \"name\": \".\",\n"
				"      \"type\": \"OPT\",\n"
				"      \"udpmax\": %u,\n"
				"      \"version\": %u,\n"
				"      \"do\": %s\n",
				iter.res.opt.udpmax,
				iter.res.opt.version,
				bools[iter.res.opt.dof]);
		}
		else {
			fmt("      \"name\": \"%s\",", iter.name);
			ssize_t n = xdns_res_json(&iter.res, buf, len);
			if (n < 0) { return n; }
			buf += n;
			buf -= n;
		}
	}

	fmt("    }\n  ]\n}\n");

	return buf - start;
}

ssize_t
xdns_res_json(const struct xdns_res *res, char *buf, size_t len)
{
	char *start = buf;
	char tmp[256];

	fmt("      \"type\": \"%s\",\n"
		"      \"class\": \"%s\",\n"
		"      \"ttl\": %d",
			xdns_type_name(res->rr.rtype),
			xdns_class_name(res->rr.rclass),
			res->rr.ttl);
	switch (res->rr.rtype) {
	case XDNS_A:
		fmt(",\n      \"a\": \"%s\"\n",
				inet_ntop(AF_INET, &res->rdata.a, tmp, sizeof(tmp)));
		break;
	case XDNS_NS:
		fmt(",\n      \"ns\": \"%s\"\n",
				res->rdata.ns);
		break;
	case XDNS_CNAME:
		fmt(",\n      \"cname\": \"%s\"\n",
				res->rdata.cname);
		break;
	case XDNS_SOA:
		fmt(",\n"
			"      \"mname\": \"%s\",\n"
			"      \"rname\": \"%s\",\n"
			"      \"serial\": %u,\n"
			"      \"refresh\": %u,\n"
			"      \"retry\": %u,\n"
			"      \"expire\": %u,\n"
			"      \"minimum\": %u\n",
			res->rdata.soa.mname,
			res->rdata.soa.rname,
			res->rdata.soa.serial,
			res->rdata.soa.refresh,
			res->rdata.soa.retry,
			res->rdata.soa.expire,
			res->rdata.soa.minimum);
		break;
	case XDNS_PTR:
		fmt(",\n      \"ptr\": \"%s\"\n",
			res->rdata.ptr);
		break;
	case XDNS_MX:
		fmt(",\n"
			"      \"preference\": %u,\n"
			"      \"host\": \"%s\"\n",
			res->rdata.mx.preference,
			res->rdata.mx.host);
		break;
	case XDNS_TXT:
		fmt(",\n      \"txt\": \"%.*s\"\n",
			(int)res->rdata.txt.length, (char *)res->rdata.txt.data);
		break;
	case XDNS_AAAA:
		fmt(",\n      \"aaaa\": \"%s\"\n",
			inet_ntop(AF_INET6, &res->rdata.aaaa, tmp, sizeof(tmp)));
		break;
	case XDNS_SRV:
		fmt(",\n"
			"      \"priority\": %u,\n"
			"      \"weight\": %u,\n"
			"      \"port\": %u,\n"
			"      \"target\": \"%s\"\n",
			res->rdata.srv.priority,
			res->rdata.srv.weight,
			res->rdata.srv.port,
			res->rdata.srv.target);
		break;
	case XDNS_OPT:
	case XDNS_ANY:
		break;
	}
	fmt("\n");

	return buf - start;
}

#define RDATA_SIZE(f) \
	(sizeof(((union xdns_rdata *)0)->f))

static int
copy_field(struct xdns_res **dst, const struct xdns_res *src, size_t field_size)
{
	struct xdns_res *r = malloc(offsetof(struct xdns_res, rdata) + field_size);
	if (r == NULL) { return xerrno; }
	memcpy(&r->rr, &src->rr, sizeof(src->rr));
	memcpy(&r->rdata, &src->rdata, field_size);
	*dst = r;
	return 0;
}

static int
copy_txt(struct xdns_res **dst, const struct xdns_res *src)
{
	size_t base = offsetof(struct xdns_res, rdata) + RDATA_SIZE(txt);
	struct xdns_res *r = malloc(base + src->rdata.txt.length + 1);
	if (r == NULL) { return xerrno; }
	uint8_t *data = ((uint8_t *)r) + base;
	memcpy(&r->rr, &src->rr, sizeof(src->rr));
	memcpy(data, src->rdata.txt.data, src->rdata.txt.length);
	data[src->rdata.txt.length] = 0;
	r->rdata.txt.data = data;
	r->rdata.txt.length = src->rdata.txt.length;
	*dst = r;
	return 0;
}

int
xdns_res_copy(struct xdns_res **dst, const struct xdns_res *src)
{
	switch (src->rr.rtype) {
	case XDNS_A:     return copy_field(dst, src, RDATA_SIZE(a));
	case XDNS_NS:    return copy_field(dst, src, RDATA_SIZE(ns));
	case XDNS_CNAME: return copy_field(dst, src, RDATA_SIZE(cname));
	case XDNS_SOA:   return copy_field(dst, src, RDATA_SIZE(soa));
	case XDNS_PTR:   return copy_field(dst, src, RDATA_SIZE(ptr));
	case XDNS_MX:    return copy_field(dst, src, RDATA_SIZE(mx));
	case XDNS_AAAA:  return copy_field(dst, src, RDATA_SIZE(aaaa));
	case XDNS_SRV:   return copy_field(dst, src, RDATA_SIZE(srv));
	case XDNS_TXT:   return copy_txt(dst, src);
	default:         return xerr_sys(ENOTSUP);
	}
}

void xdns_res_free(struct xdns_res **res)
{
	struct xdns_res *r = *res;
	if (r != NULL) {
		*res = NULL;
		free(r);
	}
}

