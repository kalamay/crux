#include "http.h"
#include "parser.h"
#include "../include/crux/hash.h"
#include "../include/crux/rand.h"
#if WITH_FILTER
# include "../include/crux/filter.h"
#endif

#include <string.h>
#include <assert.h>
#include <ctype.h>

// TODO: add support for strings and lws within header field values

static const uint8_t version_start[] = "HTTP/1.";
static const uint8_t sep[] = ": ";
static const uint8_t crlf[] = "\r\n";

#define START    0x000000

#define REQ      0x00000F
#define REQ_METH 0x000001
#define REQ_URI  0x000002
#define REQ_VER  0x000003
#define REQ_EOL  0x000004

#define RES      0x0000F0
#define RES_VER  0x000010
#define RES_SEP  0x000020
#define RES_CODE 0x000030
#define RES_MSG  0x000040

#define FLD      0x000F00
#define FLD_KEY  0x000100
#define FLD_LWS  0x000200
#define FLD_VAL  0x000300

#define CHK      0x00F000
#define CHK_NUM  0x001000
#define CHK_EOL1 0x002000
#define CHK_EOL2 0x003000

static bool
accept_field(struct xhttp *restrict p, const char *restrict m)
{
#if WITH_FILTER
	struct xfilter *f = p->filter;
	return f == NULL || xfilter(f,
			m + p->as.field.name.off, p->as.field.name.len,
			m + p->as.field.value.off, p->as.field.value.len) >= 0;
#else
	(void)p;
	(void)m;
	return true;
#endif
}

static int
scrape_field(struct xhttp *restrict p, const uint8_t *restrict m)
{
	if (p->map != NULL) {
		int rc = xhttp_map_put(p->map, p->buf, p->as.field);
		if (rc < 0) {
			return rc;
		}
	}

	if (p->trailers || p->body_len) {
		return 0;
	}

	if (LEQ16("content-length", m + p->as.field.name.off, p->as.field.name.len)) {
		if (p->as.field.value.len == 0) {
			YIELD_ERROR(xerr_http(XESYNTAX));
		}
		size_t num = 0;
		const uint8_t *s = m + p->as.field.value.off;
		const uint8_t *e = s + p->as.field.value.len;
		while (s < e) {
			if (!isdigit(*s)) {
				YIELD_ERROR(xerr_http(XESYNTAX));
			}
			num = num * 10 + (*s - '0');
			s++;
		}
		p->body_len = num;
		return 0;
	}

	if (LEQ16("connection", m + p->as.field.name.off, p->as.field.name.len)) {
		if (LEQ16("keep-alive", m + p->as.field.value.off, p->as.field.value.len)) {
			p->flags |= XHTTP_FKEEPALIVE;
		}
		else if (LEQ16("close", m + p->as.field.value.off, p->as.field.value.len)) {
			p->flags &= ~XHTTP_FKEEPALIVE;
		}
		return 0;
	}

	if (LEQ("transfer-encoding", m + p->as.field.name.off, p->as.field.name.len) &&
			LEQ16("chunked", m + p->as.field.value.off, p->as.field.value.len)) {
		p->flags |= XHTTP_FCHUNKED;
		return 0;
	}

	return 0;
}

static ssize_t
parse_request_line(struct xhttp *restrict p,
		const uint8_t *const restrict m, const size_t len)
{
	static const uint8_t method_sep[] = "\0@[`{\xff"; // must match ' '
	static const uint8_t uri_sep[] = "\0 \x7f\xff"; // must match ' '

	const uint8_t *end = m + p->off;

	p->type = XHTTP_NONE;

	switch (p->cs) {
	case REQ:
		p->cs = REQ_METH;
		p->as.request.method.off = (uint8_t)p->off;

	case REQ_METH:
		EXPECT_RANGE_THEN_CHAR(method_sep, ' ', p->max_method, false,
				xerr_http(XESYNTAX), xerr_http(XESIZE));
		p->as.request.method.len = (uint8_t)(p->off - 1);
		p->cs = REQ_URI;
		p->as.request.uri.off = p->off;

	case REQ_URI:
		EXPECT_RANGE_THEN_CHAR(uri_sep, ' ', p->max_uri, false,
				xerr_http(XESYNTAX), xerr_http(XESIZE));
		p->as.request.uri.len = (uint16_t)(p->off - 1 - p->as.request.uri.off);
		p->cs = REQ_VER;

	case REQ_VER:
		EXPECT_PREFIX(version_start, 1, false, xerr_http(XESYNTAX));
		if (!isdigit(*end)) {
			YIELD_ERROR(xerr_http(XESYNTAX));
		}
		p->as.request.version = (uint8_t)(*end - '0');
		p->cs = REQ_EOL;
		if (p->as.request.version == 1) {
			p->flags |= XHTTP_FKEEPALIVE;
		}
		end++;

	case REQ_EOL:
		EXPECT_PREFIX(crlf, 0, false, xerr_http(XESYNTAX));
		YIELD(XHTTP_REQUEST, FLD);

	default:
		YIELD_ERROR(xerr_http(XESTATE));
	}
}

static ssize_t
parse_response_line(struct xhttp *restrict p,
		const uint8_t *const restrict m, const size_t len)
{
	const uint8_t *end = m + p->off;

	p->type = XHTTP_NONE;

	switch (p->cs) {
	case RES:
		p->cs = RES_VER;

	case RES_VER:
		EXPECT_PREFIX(version_start, 1, false, xerr_http(XESYNTAX));
		if (!isdigit(*end)) {
			YIELD_ERROR(xerr_http(XESYNTAX));
		}
		p->as.response.version = (uint8_t)(*end - '0');
		p->cs = RES_SEP;
		end++;
	
	case RES_SEP:
		EXPECT_CHAR(' ', false, xerr_http(XESYNTAX));
		p->cs = RES_CODE;
		p->as.response.status = 0;

	case RES_CODE:
		do {
			if (p->off == len) return 0;
			if (*end == ' ') {
				p->off++;
				end++;
				break;
			}
			if (isdigit(*end)) {
				p->as.response.status = p->as.response.status * 10 + (*end - '0');
				p->off++;
				end++;
			}
			else {
				YIELD_ERROR(xerr_http(XESYNTAX));
			}
		} while (true);
		p->as.response.reason.off = p->off;
		p->cs = RES_MSG;

	case RES_MSG:
		EXPECT_CRLF(p->max_reason + p->as.response.reason.off, false,
				xerr_http(XESYNTAX), xerr_http(XESIZE));
		p->as.response.reason.len = (uint16_t)(p->off - p->as.response.reason.off - (sizeof(crlf) - 1));
		YIELD(XHTTP_RESPONSE, FLD);

	default:
		YIELD_ERROR(xerr_http(XESTATE));
	}
}

static ssize_t
parse_field(struct xhttp *restrict p,
		const uint8_t *const restrict m, const size_t len)
{
	static const uint8_t field_sep[] = ":@\0 \"\"()[]//{{}}"; // must match ':', allows commas
	static const uint8_t field_lws[] = "\0\x08\x0A\x1f!\xff";

	const uint8_t *end = m + p->off;
	size_t scan = 0;

#undef SCAN
#define SCAN scan

again:
	p->type = XHTTP_NONE;

	switch (p->cs) {
	case FLD:
		if (REMAIN < sizeof(crlf) - 1) {
			return SCAN;
		}
		if (end[0] == crlf[0] && end[1] == crlf[1]) {
			end += 2;
			p->as.body_start.chunked = (p->flags & XHTTP_FCHUNKED) == XHTTP_FCHUNKED;
			p->as.body_start.content_length = p->body_len;
			if (p->trailers) {
				YIELD(XHTTP_TRAILER_END, DONE);
			}
			else {
				YIELD(XHTTP_BODY_START, (p->flags & XHTTP_FCHUNKED) ? CHK : DONE);
			}
		}
		p->cs = FLD_KEY;
		p->as.field.name.off = 0;

	case FLD_KEY:
		EXPECT_RANGE_THEN_CHAR(field_sep, ':', p->max_field, false,
				xerr_http(XESYNTAX), xerr_http(XESIZE));
		p->as.field.name.len = (uint16_t)(p->off - 1);
		p->cs = FLD_LWS;

	case FLD_LWS:
		EXPECT_RANGE(field_lws, p->max_value + p->as.field.value.off, false,
				xerr_http(XESYNTAX), xerr_http(XESIZE));
		p->as.field.value.off = (uint16_t)p->off;
		p->cs = FLD_VAL;

	case FLD_VAL:
		EXPECT_CRLF(p->max_value + p->as.field.value.off, false,
				xerr_http(XESYNTAX), xerr_http(XESIZE));
		p->as.field.name.off = SCAN;
		p->as.field.value.off += SCAN;
		p->as.field.value.len = (uint16_t)(p->off + SCAN - p->as.field.value.off - (sizeof(crlf) - 1));
		if (accept_field(p, (const char *)m)) {
			CHECK_ERROR(scrape_field(p, m));
			if (p->map == NULL) {
				YIELD(XHTTP_FIELD, FLD);
			}
		}
		SCAN = end - m;
		p->cs = FLD;
		p->off = 0;
		goto again;

	default:
		YIELD_ERROR(xerr_http(XESTATE));
	}

#undef SCAN
#define SCAN 0
}

static ssize_t
parse_chunk(struct xhttp *restrict p,
		const uint8_t *const restrict m, const size_t len)
{
	static const uint8_t hex[] = {
		['0'] = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
		['A'] = 10, 11, 12, 13, 14, 15,
		['a'] = 10, 11, 12, 13, 14, 15
	};

	const uint8_t *end = m + p->off;

again:
	p->type = XHTTP_NONE;

	switch (p->cs) {
	case CHK:
		p->off = 0;
		p->body_len = 0;
		p->cs = CHK_NUM;
	
	case CHK_NUM:
		do {
			if (p->off == len) return 0;
			if (isxdigit(*end)) {
				p->body_len = (p->body_len << 4) | hex[*end];
				p->off++;
				end++;
			}
			else {
				break;
			}
		} while (true);
		p->cs = CHK_EOL1;
	
	case CHK_EOL1:
		EXPECT_PREFIX(crlf, 0, false, xerr_http(XESYNTAX));
		if (p->body_len == 0) {
			p->trailers = true;
			YIELD(XHTTP_BODY_END, FLD);
		}
		else {
			p->as.body_chunk.length = p->body_len;
			p->body_len = 0;
			YIELD(XHTTP_BODY_CHUNK, CHK_EOL2);
		}

	case CHK_EOL2:
		EXPECT_PREFIX(crlf, 0, false, xerr_http(XESYNTAX));
		p->cs = CHK_NUM;
		goto again;

	default:
		YIELD_ERROR(xerr_http(XESTATE));
	}
}

static void
init_sizes(struct xhttp *p)
{
	p->max_method = XHTTP_MAX_METHOD;
	p->max_uri = XHTTP_MAX_URI;
	p->max_reason = XHTTP_MAX_REASON;
	p->max_field = XHTTP_MAX_FIELD;
	p->max_value = XHTTP_MAX_VALUE;
}

int
xhttp_init_request(struct xhttp *p, struct xhttp_map *map, struct xfilter *filter)
{
	assert(p != NULL);

	memset(p, 0, sizeof(*p));
	init_sizes(p);
	p->cs = REQ;
	p->map = map;
#if WITH_FILTER
	p->filter = filter;
#else
	(void)filter;
	p->filter = NULL;
#endif
	return 0;
}

int
xhttp_init_response(struct xhttp *p, struct xhttp_map *map, struct xfilter *filter)
{
	assert(p != NULL);

	memset(p, 0, sizeof(*p));
	init_sizes(p);
	p->cs = RES;
	p->response = true;
	p->map = map;
#if WITH_FILTER
	p->filter = filter;
#else
	(void)filter;
	p->filter = NULL;
#endif
	return 0;
}

void
xhttp_final(struct xhttp *p)
{
	assert(p != NULL);
	(void)p;
}

void
xhttp_reset(struct xhttp *p)
{
	assert(p != NULL);

	uint16_t max_method = p->max_method;
	uint16_t max_uri = p->max_uri;
	uint16_t max_reason = p->max_reason;
	uint16_t max_field = p->max_field;
	uint16_t max_value = p->max_value;

	if (p->response) {
		xhttp_init_response(p, p->map, p->filter);
	}
	else {
		xhttp_init_request(p, p->map, p->filter);
	}

	p->max_method = max_method;
	p->max_uri = max_uri;
	p->max_reason = max_reason;
	p->max_field = max_field;
	p->max_value = max_value;

	if (p->map) {
		xhttp_map_reset(p->map);
	}
}

ssize_t
xhttp_next(struct xhttp *p, const struct xbuf *buf)
{
	assert(p != NULL);

	if (IS_DONE(p->cs)) {
		return xerr_http(XESTATE);
	}

	const uint8_t *ptr = xbuf_data(buf);
	size_t len = xbuf_length(buf);
	if (len == 0) {
		return 0;
	}

	ssize_t rc;
	p->scans++;
	p->cscans++;
	p->buf = buf;

	     if (p->cs & REQ) rc = parse_request_line(p, ptr, len);
	else if (p->cs & RES) rc = parse_response_line(p, ptr, len);
	else if (p->cs & FLD) rc = parse_field(p, ptr, len);
	else if (p->cs & CHK) rc = parse_chunk(p, ptr, len);
	else { YIELD_ERROR(xerr_http(XESTATE)); }
	if (rc > 0) {
		p->cscans = 0;
	}
	else if (rc == 0 && p->cscans > 64) {
		YIELD_ERROR(xerr_http(XETOOSHORT));
	}
	return rc;
}

bool
xhttp_is_done(const struct xhttp *p)
{
	assert(p != NULL);

	return IS_DONE(p->cs);
}

void
xhttp_print(const struct xhttp *p, const struct xbuf *buf, FILE *out)
{
	assert(p != NULL);

	if (out == NULL) {
		out = stderr;
	}

	const uint8_t *ptr = xbuf_data(buf);

	switch (p->type) {
	case XHTTP_REQUEST:
		fprintf(out, "> %.*s %.*s HTTP/1.%u\n",
				(int)p->as.request.method.len, ptr+p->as.request.method.off,
				(int)p->as.request.uri.len, ptr+p->as.request.uri.off,
				p->as.request.version);
		break;
	case XHTTP_RESPONSE:
		fprintf(out, "< HTTP/1.%u %u %.*s\n",
				p->as.response.version,
				p->as.response.status,
				(int)p->as.response.reason.len, ptr+p->as.response.reason.off);
		break;
	case XHTTP_FIELD:
		fprintf(out, "%c %.*s: %.*s\n",
				p->response ? '<' : '>',
				(int)p->as.field.name.len, ptr+p->as.field.name.off,
				(int)p->as.field.value.len, ptr+p->as.field.value.off);
		break;
	case XHTTP_BODY_START:
	case XHTTP_TRAILER_END:
		fprintf(out, "%c\n", p->response ? '<' : '>');
		break;
	default: break;
	}
}

XVEC_STATIC(xhttp_vec, struct xhttp_vec, struct xhttp_field)

static uint64_t
xhttp_tab_hash(struct xhttp_map *map, const char *k, size_t kn)
{
	return xhash_sipcase(k, kn, &map->seed);
}

static bool
xhttp_tab_has_key(struct xhttp_map *map, struct xhttp_vec *ent, const char *k, size_t kn)
{
	if (ent->count == 0) { return false; }
	return (size_t)ent->arr[0].name.len == kn &&
		strncasecmp((char *)map->buf.map + ent->arr[0].name.off, k, kn) == 0;
}

XHASHMAP_STATIC(xhttp_tab, struct xhttp_map, const char *, struct xhttp_vec)

int
xhttp_map_new(struct xhttp_map **mapp)
{
	return xnew(xhttp_map_init, mapp);
}

int
xhttp_map_init(struct xhttp_map *map)
{
	int rc = xbuf_init(&map->buf, 4000, XBUF_LINE);
	if (rc < 0) {
		return rc;
	}

	rc = xhttp_tab_init(map, 0.9, 0);
	if (rc < 0) {
		goto err_tab;
	}

	rc = xrand(&map->seed, sizeof(map->seed));
	if (rc < 0) {
		goto err_rand;
	}

	return 0;

err_rand:
	xhttp_tab_final(map);
err_tab:
	xbuf_final(&map->buf);
	return rc;
}

void
xhttp_map_free(struct xhttp_map **mapp)
{
	assert(mapp != NULL);

	xfree(xhttp_map_final, mapp);
}

void
xhttp_map_final(struct xhttp_map *map)
{
	xbuf_final(&map->buf);
	xhttp_tab_final(map);
}

void
xhttp_map_reset(struct xhttp_map *map)
{
	xbuf_reset(&map->buf);
	xhttp_tab_clear(map);
}

int
xhttp_map_add(struct xhttp_map *map,
		const char *name, size_t namelen, 
		const char *value, size_t valuelen)
{
	size_t len = namelen + (sizeof(sep)-1) + valuelen + (sizeof(crlf)-1);
	struct xhttp_vec *ent;
	bool new;
	int rc;

	if (xbuf_length(&map->buf) + len > (size_t)UINT16_MAX) {
		return xerr_http(XESIZE);
	}

	rc = xbuf_ensure(&map->buf, len + 1);
	if (rc < 0) {
		return rc;
	}

	rc = xhttp_tab_reserve(map, name, namelen, &ent);
	if (rc < 0) {
		return rc;
	}

	new = rc == XHASHMAP_RESERVE_NEW;
	if (new) {
		*ent = (struct xhttp_vec)XVEC_INIT;
	}

	struct xhttp_field fld;
	fld.name.off = xbuf_length(&map->buf);
	fld.name.len = namelen;
	fld.value.off = fld.name.off + namelen + (sizeof(sep)-1);
	fld.value.len = valuelen;

	rc = xhttp_vec_push(ent, fld);
	if (rc < 0) {
		if (new) {
			xhttp_tab_remove(map, ent);
		}
		return rc;
	}

	char *p = xbuf_tail(&map->buf);
	p = stpncpy(p, name, namelen);
	p = stpncpy(p, (char *)sep, sizeof(sep) - 1);
	p = stpncpy(p, value, valuelen);
	p = stpncpy(p, (char *)crlf, sizeof(crlf) - 1);
	xbuf_bump(&map->buf, len);

	return 0;
}

int
xhttp_map_addstr(struct xhttp_map *map, const char *name, const char *value)
{
	return xhttp_map_add(map, name, strlen(name), value, strlen(value));
}

int
xhttp_map_put(struct xhttp_map *map, const struct xbuf *src, struct xhttp_field fld)
{
	const char *p = xbuf_data(src);
	return xhttp_map_add(map,
			p + fld.name.off, fld.name.len,
			p + fld.value.off, fld.value.len);
}

size_t
xhttp_map_get(struct xhttp_map *map,
		const char *name, size_t namelen,
		struct iovec *iov, size_t iovlen)
{
	struct xhttp_vec *vec = xhttp_tab_get(map, name, namelen);
	if (vec == NULL) {
		return 0;
	}

	if (iovlen > vec->count) {
		iovlen = vec->count;
	}

	char *p = (char *)xbuf_data(&map->buf);
	struct xhttp_field *fld = vec->arr + vec->count - 1;
	for (size_t i = 0; i < iovlen; i++, fld--, iov++) {
		iov->iov_base = p + fld->value.off;
		iov->iov_len = fld->value.len;
	}
	return iovlen;
}

size_t
xhttp_map_full(const struct xhttp_map *map, struct iovec *iov, size_t iovlen)
{
	if (iovlen == 0) {
		return 0;
	}

	iov->iov_base = (void *)xbuf_data(&map->buf);
	iov->iov_len = xbuf_length(&map->buf);
	return 1;
}

static void
entry_print(const struct xhttp_map *map, struct xhttp_vec *ent, FILE *out)
{
	const char *p = xbuf_data(&map->buf);
	fprintf(out, "\"%.*s\" = ", (int)ent->arr[0].name.len, p + ent->arr[0].name.off);
	if (ent->count == 1) {
			fprintf(out, "\"%.*s\"", (int)ent->arr[0].value.len, p + ent->arr[0].value.off);
	}
	else {
		fprintf(out, "{\n");
		for (size_t i = 0; i < ent->count; i++) {
			fprintf(out, "      \"%.*s\"\n", (int)ent->arr[i].value.len, p + ent->arr[i].value.off);
		}
		fprintf(out, "    }");
	}
}

void
xhttp_map_print(const struct xhttp_map *map, FILE *out)
{
	xhttp_tab_print(map, out, entry_print);
}

