#include "mu.h"
#include "../include/crux/buf.h"
#include "../include/crux/http.h"
#include "../include/crux/err.h"

#include <stdlib.h>
#include <ctype.h>

typedef struct {
	union {
		struct {
			char method[8];
			char uri[32];
			uint8_t version;
		} request;
		struct {
			uint8_t version;
			uint16_t status;
			char reason[32];
		} response;
	} as;
	struct {
		char name[32];
		char value[64];
	} fields[16];
	size_t field_count;
	char body[256];
} Message;

static ssize_t
next(struct xhttp *p, const struct xbuf *buf)
{
	return xhttp_next(p, xbuf_value(buf), xbuf_length(buf));
}

static bool
parse(struct xhttp *p, Message *msg, const uint8_t *in, size_t inlen, size_t speed)
{
	memset(msg, 0, sizeof(*msg));

	struct xbuf *buf;
	size_t body = 0;
	ssize_t rc;
	bool ok = true;

	mu_assert_int_eq(xbuf_new(&buf, inlen), 0);

	while (body > 0 || !xhttp_is_done(p)) {
		if (inlen > 0) {
			size_t n = speed && speed < inlen ? speed : inlen;
			mu_assert_int_ge(xbuf_add(buf, in, inlen), 0);
			in += n;
			inlen -= n;
		}

		const char *val = xbuf_value(buf);
		size_t len = xbuf_length(buf);

		if (body > 0) {
			rc = body < len ? body : len;
			strncat(msg->body, val, rc);
			body -= rc;
		}
		else {
			rc = xhttp_next(p, val, len);

			// normally rc could equal 0 if a full scan couldn't be completed
			mu_assert_int_ge(rc, 0);

			if (p->type == XHTTP_REQUEST) {
				strncat(msg->as.request.method,
						val + p->as.request.method.off,
						p->as.request.method.len);
				strncat(msg->as.request.uri,
						val + p->as.request.uri.off,
						p->as.request.uri.len);
				msg->as.request.version = p->as.request.version;
			}
			else if (p->type == XHTTP_RESPONSE) {
				msg->as.response.version = p->as.response.version;
				msg->as.response.status = p->as.response.status;
				strncat(msg->as.response.reason,
						val + p->as.response.reason.off,
						p->as.response.reason.len);
			}
			else if (p->type == XHTTP_FIELD) {
				strncat(msg->fields[msg->field_count].name,
						val + p->as.field.name.off,
						p->as.field.name.len);
				strncat(msg->fields[msg->field_count].value,
						val + p->as.field.value.off,
						p->as.field.value.len);
				msg->field_count++;
			}
			else if (p->type == XHTTP_BODY_START) {
				if (!p->as.body_start.chunked) {
					body = p->as.body_start.content_length;
				}
			}
			else if (p->type == XHTTP_BODY_CHUNK) {
				body = p->as.body_chunk.length;
			}
		}

		xbuf_trim(buf, rc);
	}

	xbuf_free(&buf);
	return ok;
}

static void
test_request(size_t speed)
{
	struct xhttp p;
	xhttp_init_request(&p);

	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Content-Length: 12\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"Hello World!"
		;

	Message msg;
	mu_assert(parse(&p, &msg, request, sizeof(request) - 1, speed));

	mu_assert_str_eq("GET", msg.as.request.method);
	mu_assert_str_eq("/some/path", msg.as.request.uri);
	mu_assert_uint_eq(1, msg.as.request.version);
	mu_assert_uint_eq(7, msg.field_count);
	mu_assert_str_eq("Empty", msg.fields[0].name);
	mu_assert_str_eq("", msg.fields[0].value);
	mu_assert_str_eq("Empty-Space", msg.fields[1].name);
	mu_assert_str_eq("", msg.fields[1].value);
	mu_assert_str_eq("Space", msg.fields[2].name);
	mu_assert_str_eq("value", msg.fields[2].value);
	mu_assert_str_eq("No-Space", msg.fields[3].name);
	mu_assert_str_eq("value", msg.fields[3].value);
	mu_assert_str_eq("Spaces", msg.fields[4].name);
	mu_assert_str_eq("value with spaces", msg.fields[4].value);
	mu_assert_str_eq("Pre-Spaces", msg.fields[5].name);
	mu_assert_str_eq("value with prefix spaces", msg.fields[5].value);
	mu_assert_str_eq("Content-Length", msg.fields[6].name);
	mu_assert_str_eq("12", msg.fields[6].value);
	mu_assert_str_eq("Hello World!", msg.body);

	xhttp_final(&p);
}

/*
static void
test_request_capture(size_t speed)
{
	struct xhttp p;
	xhttp_init_request(&p);

	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Content-Length: 12\r\n"
		"Test: value 1\r\n"
		"TEST: value 2\r\n"
		"test: value 3\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"Hello World!"
		;

	Message msg;
	mu_assert(parse(&p, &msg, request, sizeof(request) - 1, speed));

	mu_assert_str_eq("GET", msg.as.request.method);
	mu_assert_str_eq("/some/path", msg.as.request.uri);
	mu_assert_uint_eq(1, msg.as.request.version);
	mu_assert_uint_eq(0, msg.field_count);
	mu_assert_str_eq("Hello World!", msg.body);

	struct iovec iov = { NULL, 0 };
	const SpHttpEntry *e = xhttp_map_get(p.headers, "TeSt", 4);
	mu_assert_ptr_ne(e, NULL);

	xhttp_entry_name(e, &iov);
	mu_assert_str_eq("Test", iov.iov_base);

	mu_assert_uint_eq(3, xhttp_entry_count(e));

	mu_assert(xhttp_entry_value(e, 0, &iov));
	mu_assert_str_eq("value 1", iov.iov_base);
	mu_assert(xhttp_entry_value(e, 1, &iov));
	mu_assert_str_eq("value 2", iov.iov_base);
	mu_assert(xhttp_entry_value(e, 2, &iov));
	mu_assert_str_eq("value 3", iov.iov_base);

	char buf[1024];

	mu_assert_uint_eq(xhttp_map_encode_size(p.headers), 185);
	mu_assert_uint_eq(xhttp_map_scatter_count(p.headers), 40);
	memset(buf, 0, sizeof(buf));
	xhttp_map_encode(p.headers, buf);
	mu_assert_uint_eq(strlen(buf), 185);

	xhttp_map_del(p.headers, "test", 4);

	mu_assert_uint_eq(xhttp_map_encode_size(p.headers), 140);
	mu_assert_uint_eq(xhttp_map_scatter_count(p.headers), 28);
	memset(buf, 0, sizeof(buf));
	xhttp_map_encode(p.headers, buf);
	mu_assert_uint_eq(strlen(buf), 140);

	xhttp_final(&p);
}
*/

static void
test_chunked_request(size_t speed)
{
	struct xhttp p;
	xhttp_init_request(&p);

	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Transfer-Encoding: chunked\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"5\r\n"
		"Hello\r\n"
		"7\r\n"
		" World!\r\n"
		"0\r\n"
		"Trailer: trailer value\r\n"
		"\r\n"
		;

	Message msg;
	mu_assert(parse(&p, &msg, request, sizeof(request) - 1, speed));

	mu_assert_str_eq("GET", msg.as.request.method);
	mu_assert_str_eq("/some/path", msg.as.request.uri);
	mu_assert_uint_eq(1, msg.as.request.version);
	mu_assert_uint_eq(8, msg.field_count);
	mu_assert_str_eq("Empty", msg.fields[0].name);
	mu_assert_str_eq("", msg.fields[0].value);
	mu_assert_str_eq("Empty-Space", msg.fields[1].name);
	mu_assert_str_eq("", msg.fields[1].value);
	mu_assert_str_eq("Space", msg.fields[2].name);
	mu_assert_str_eq("value", msg.fields[2].value);
	mu_assert_str_eq("No-Space", msg.fields[3].name);
	mu_assert_str_eq("value", msg.fields[3].value);
	mu_assert_str_eq("Spaces", msg.fields[4].name);
	mu_assert_str_eq("value with spaces", msg.fields[4].value);
	mu_assert_str_eq("Pre-Spaces", msg.fields[5].name);
	mu_assert_str_eq("value with prefix spaces", msg.fields[5].value);
	mu_assert_str_eq("Transfer-Encoding", msg.fields[6].name);
	mu_assert_str_eq("chunked", msg.fields[6].value);
	mu_assert_str_eq("Trailer", msg.fields[7].name);
	mu_assert_str_eq("trailer value", msg.fields[7].value);
	mu_assert_str_eq("Hello World!", msg.body);
}

/*
static void
test_chunked_request_capture(size_t speed)
{
	struct xhttp p;
	xhttp_init_request(&p);

	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Test: value 1\r\n"
		"TEST: value 2\r\n"
		"test: value 3\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"5\r\n"
		"Hello\r\n"
		"7\r\n"
		" World!\r\n"
		"0\r\n"
		"Trailer: trailer value\r\n"
		"\r\n"
		;

	Message msg;
	mu_assert(parse(&p, &msg, request, sizeof(request) - 1, speed));

	mu_assert_str_eq("GET", msg.as.request.method);
	mu_assert_str_eq("/some/path", msg.as.request.uri);
	mu_assert_uint_eq(1, msg.as.request.version);
	mu_assert_uint_eq(0, msg.field_count);
	mu_assert_str_eq("Hello World!", msg.body);

	struct iovec iov = { NULL, 0 };
	const SpHttpEntry *e = xhttp_map_get(p.headers, "TeSt", 4);
	mu_assert_ptr_ne(e, NULL);

	xhttp_entry_name(e, &iov);
	mu_assert_str_eq("Test", iov.iov_base);

	mu_assert_uint_eq(3, xhttp_entry_count(e));

	mu_assert(xhttp_entry_value(e, 0, &iov));
	mu_assert_str_eq("value 1", iov.iov_base);
	mu_assert(xhttp_entry_value(e, 1, &iov));
	mu_assert_str_eq("value 2", iov.iov_base);
	mu_assert(xhttp_entry_value(e, 2, &iov));
	mu_assert_str_eq("value 3", iov.iov_base);

	char buf[1024];

	mu_assert_uint_eq(xhttp_map_encode_size(p.headers), 217);
	mu_assert_uint_eq(xhttp_map_scatter_count(p.headers), 44);
	memset(buf, 0, sizeof(buf));
	xhttp_map_encode(p.headers, buf);
	mu_assert_uint_eq(strlen(buf), 217);

	xhttp_map_del(p.headers, "test", 4);

	mu_assert_uint_eq(xhttp_map_encode_size(p.headers), 172);
	mu_assert_uint_eq(xhttp_map_scatter_count(p.headers), 32);
	memset(buf, 0, sizeof(buf));
	xhttp_map_encode(p.headers, buf);
	mu_assert_uint_eq(strlen(buf), 172);

	xhttp_final(&p);
}
*/

static void
test_response(size_t speed)
{
	struct xhttp p;
	xhttp_init_response(&p);

	static const uint8_t response[] = 
		"HTTP/1.1 200 OK\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Content-Length: 12\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"Hello World!"
		;

	Message msg;
	mu_assert(parse(&p, &msg, response, sizeof(response) - 1, speed));

	mu_assert_uint_eq(1, msg.as.response.version);
	mu_assert_uint_eq(200, msg.as.response.status);
	mu_assert_str_eq("OK", msg.as.response.reason);
	mu_assert_uint_eq(7, msg.field_count);
	mu_assert_str_eq("Empty", msg.fields[0].name);
	mu_assert_str_eq("", msg.fields[0].value);
	mu_assert_str_eq("Empty-Space", msg.fields[1].name);
	mu_assert_str_eq("", msg.fields[1].value);
	mu_assert_str_eq("Space", msg.fields[2].name);
	mu_assert_str_eq("value", msg.fields[2].value);
	mu_assert_str_eq("No-Space", msg.fields[3].name);
	mu_assert_str_eq("value", msg.fields[3].value);
	mu_assert_str_eq("Spaces", msg.fields[4].name);
	mu_assert_str_eq("value with spaces", msg.fields[4].value);
	mu_assert_str_eq("Pre-Spaces", msg.fields[5].name);
	mu_assert_str_eq("value with prefix spaces", msg.fields[5].value);
	mu_assert_str_eq("Content-Length", msg.fields[6].name);
	mu_assert_str_eq("12", msg.fields[6].value);
	mu_assert_str_eq("Hello World!", msg.body);

	xhttp_final(&p);
}

static void
test_chunked_response(size_t speed)
{
	struct xhttp p;
	xhttp_init_response(&p);

	static const uint8_t response[] = 
		"HTTP/1.1 200 OK\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Transfer-Encoding: chunked\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"5\r\n"
		"Hello\r\n"
		"7\r\n"
		" World!\r\n"
		"0\r\n"
		"Trailer: trailer value\r\n"
		"\r\n"
		;

	Message msg;
	mu_assert(parse(&p, &msg, response, sizeof(response) - 1, speed));

	mu_assert_uint_eq(1, msg.as.response.version);
	mu_assert_uint_eq(200, msg.as.response.status);
	mu_assert_str_eq("OK", msg.as.response.reason);
	mu_assert_uint_eq(8, msg.field_count);
	mu_assert_str_eq("Empty", msg.fields[0].name);
	mu_assert_str_eq("", msg.fields[0].value);
	mu_assert_str_eq("Empty-Space", msg.fields[1].name);
	mu_assert_str_eq("", msg.fields[1].value);
	mu_assert_str_eq("Space", msg.fields[2].name);
	mu_assert_str_eq("value", msg.fields[2].value);
	mu_assert_str_eq("No-Space", msg.fields[3].name);
	mu_assert_str_eq("value", msg.fields[3].value);
	mu_assert_str_eq("Spaces", msg.fields[4].name);
	mu_assert_str_eq("value with spaces", msg.fields[4].value);
	mu_assert_str_eq("Pre-Spaces", msg.fields[5].name);
	mu_assert_str_eq("value with prefix spaces", msg.fields[5].value);
	mu_assert_str_eq("Transfer-Encoding", msg.fields[6].name);
	mu_assert_str_eq("chunked", msg.fields[6].value);
	mu_assert_str_eq("Trailer", msg.fields[7].name);
	mu_assert_str_eq("trailer value", msg.fields[7].value);
	mu_assert_str_eq("Hello World!", msg.body);

	xhttp_final(&p);
}

static void
test_invalid_header(void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Header\r\n"
		"\r\n"
		;

	struct xhttp p;
	struct xbuf *buf;
	ssize_t rc;

	mu_assert_int_eq(xbuf_copy(&buf, request, sizeof(request) - 1), 0);

	xhttp_init_request(&p);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 25);
	mu_assert_int_eq(p.type, XHTTP_REQUEST);
	mu_assert_int_ge(xbuf_trim(buf, rc), 0);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, XEHTTPSYNTAX);
}

static void
test_limit_method_size(void)
{
	static const uint8_t request[] = 
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX /some/path HTTP/1.1\r\n"
		"\r\n"
		"\r\n"
		;

	struct xhttp p;
	struct xbuf *buf;
	ssize_t rc;

	mu_assert_int_eq(xbuf_copy(&buf, request, sizeof(request) - 1), 0);

	xhttp_init_request(&p);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 54);
}

static void
test_exceed_method_size(void)
{
	static const uint8_t request[] = 
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX /some/path HTTP/1.1\r\n"
		"\r\n"
		"\r\n"
		;

	struct xhttp p;
	struct xbuf *buf;
	ssize_t rc;

	mu_assert_int_eq(xbuf_copy(&buf, request, sizeof(request) - 1), 0);

	xhttp_init_request(&p);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, XEHTTPSIZE);
}

static void
test_limit_name_size(void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX: value\r\n"
		"\r\n"
		;

	struct xhttp p;
	struct xbuf *buf;
	ssize_t rc;

	mu_assert_int_eq(xbuf_copy(&buf, request, sizeof(request) - 1), 0);

	xhttp_init_request(&p);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 25);
	mu_assert_int_eq(p.type, XHTTP_REQUEST);
	mu_assert_int_ge(xbuf_trim(buf, rc), 0);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 265);
}

static void
test_exceed_name_size(void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX: value\r\n"
		"\r\n"
		;

	struct xhttp p;
	struct xbuf *buf;
	ssize_t rc;

	mu_assert_int_eq(xbuf_copy(&buf, request, sizeof(request) - 1), 0);

	xhttp_init_request(&p);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 25);
	mu_assert_int_eq(p.type, XHTTP_REQUEST);
	mu_assert_int_ge(xbuf_trim(buf, rc), 0);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, XEHTTPSIZE);
}

static void
test_limit_value_size(void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Name:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n"
		"\r\n"
		;

	struct xhttp p;
	struct xbuf *buf;
	ssize_t rc;

	mu_assert_int_eq(xbuf_copy(&buf, request, sizeof(request) - 1), 0);

	xhttp_init_request(&p);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 25);
	mu_assert_int_eq(p.type, XHTTP_REQUEST);
	mu_assert_int_ge(xbuf_trim(buf, rc), 0);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 1031);
}

static void
test_exceed_value_size(void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Name:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n"
		"\r\n"
		;

	struct xhttp p;
	struct xbuf *buf;
	ssize_t rc;

	mu_assert_int_eq(xbuf_copy(&buf, request, sizeof(request) - 1), 0);

	xhttp_init_request(&p);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 25);
	mu_assert_int_eq(p.type, XHTTP_REQUEST);
	mu_assert_int_ge(xbuf_trim(buf, rc), 0);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, XEHTTPSIZE);
}

static void
test_increase_value_size(void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Name:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n"
		"\r\n"
		;

	struct xhttp p;
	struct xbuf *buf;
	ssize_t rc;

	mu_assert_int_eq(xbuf_copy(&buf, request, sizeof(request) - 1), 0);

	xhttp_init_request(&p);
	p.max_value = 2048;
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 25);
	mu_assert_int_eq(p.type, XHTTP_REQUEST);
	mu_assert_int_ge(xbuf_trim(buf, rc), 0);
	rc = next(&p, buf);
	mu_assert_int_eq(rc, 1032);
}

/*
static void
test_cc_max_stale(void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf1[] = "max-stale";
	const char buf2[] = "max-stale=123";

	n = xcache_control_parse(&cc, buf1, sizeof(buf1) - 1);
	mu_assert_int_eq(n, sizeof(buf1) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf1 + (-1 - n));
		return;
	}
	mu_assert_int_eq(cc.type & XCACHE_MAX_STALE, XCACHE_MAX_STALE);
	mu_assert_int_ne(cc.type & XCACHE_MAX_STALE_TIME, XCACHE_MAX_STALE_TIME);

	n = xcache_control_parse(&cc, buf2, sizeof(buf2) - 1);
	mu_assert_int_eq(n, sizeof(buf2) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf2 + (-1 - n));
		return;
	}
	mu_assert_int_eq(cc.type & XCACHE_MAX_STALE, XCACHE_MAX_STALE);
	mu_assert_int_eq(cc.type & XCACHE_MAX_STALE_TIME, XCACHE_MAX_STALE_TIME);
	mu_assert_int_eq(cc.max_stale, 123);
}

static void
test_cc_private(void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf1[] = "private";
	const char buf2[] = "private=\"string\"";
	const char buf3[] = "private=token"; // non-standard
	char val[16];

	n = xcache_control_parse(&cc, buf1, sizeof(buf1) - 1);
	mu_assert_int_eq(n, sizeof(buf1) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf1 + (-1 - n));
		return;
	}
	mu_assert_int_eq(cc.type & XCACHE_PRIVATE, XCACHE_PRIVATE);
	mu_assert_uint_eq(cc.private.len, 0);

	n = xcache_control_parse(&cc, buf2, sizeof(buf2) - 1);
	mu_assert_int_eq(n, sizeof(buf2) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf2 + (-1 - n));
		return;
	}
	mu_assert_int_eq(cc.type & XCACHE_PRIVATE, XCACHE_PRIVATE);
	mu_assert_uint_eq(cc.private.len, 6);
	memcpy(val, buf2+cc.private.off, cc.private.len);
	val[cc.private.len] = '\0';
	mu_assert_str_eq(val, "string");

	n = xcache_control_parse(&cc, buf3, sizeof(buf3) - 1);
	mu_assert_int_eq(n, sizeof(buf3) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf3 + (-1 - n));
		return;
	}
	mu_assert_int_eq(cc.type & XCACHE_PRIVATE, XCACHE_PRIVATE);
	mu_assert_uint_eq(cc.private.len, 5);
	memcpy(val, buf3+cc.private.off, cc.private.len);
	val[cc.private.len] = '\0';
	mu_assert_str_eq(val, "token");
}

static void
test_cc_no_cache(void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf1[] = "no-cache";
	const char buf2[] = "no-cache=\"string\"";
	const char buf3[] = "no-cache=token"; // non-standard
	char val[16];

	n = xcache_control_parse(&cc, buf1, sizeof(buf1) - 1);
	mu_assert_int_eq(n, sizeof(buf1) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf1 + (-1 - n));
		return;
	}
	mu_assert_int_eq(cc.type & XCACHE_NO_CACHE, XCACHE_NO_CACHE);
	mu_assert_uint_eq(cc.no_cache.len, 0);

	n = xcache_control_parse(&cc, buf2, sizeof(buf2) - 1);
	mu_assert_int_eq(n, sizeof(buf2) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf2 + (-1 - n));
		return;
	}
	mu_assert_int_eq(cc.type & XCACHE_NO_CACHE, XCACHE_NO_CACHE);
	mu_assert_uint_eq(cc.no_cache.len, 6);
	memcpy(val, buf2+cc.no_cache.off, cc.no_cache.len);
	val[cc.no_cache.len] = '\0';
	mu_assert_str_eq(val, "string");

	n = xcache_control_parse(&cc, buf3, sizeof(buf3) - 1);
	mu_assert_int_eq(n, sizeof(buf3) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf3 + (-1 - n));
		return;
	}
	mu_assert_int_eq(cc.type & XCACHE_NO_CACHE, XCACHE_NO_CACHE);
	mu_assert_uint_eq(cc.no_cache.len, 5);
	memcpy(val, buf3+cc.no_cache.off, cc.no_cache.len);
	val[cc.no_cache.len] = '\0';
	mu_assert_str_eq(val, "token");
}

static void
test_cc_group(void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf[] = "max-age=0, no-cache, no-store";
	n = xcache_control_parse(&cc, buf, sizeof(buf) - 1);
	mu_assert_int_eq(n, sizeof(buf) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf + (-1 - n));
		return;
	}

	mu_assert_int_eq(cc.type & XCACHE_MAX_AGE, XCACHE_MAX_AGE);
	mu_assert_int_eq(cc.max_age, 0);

	mu_assert_int_eq(cc.type & XCACHE_NO_CACHE, XCACHE_NO_CACHE);

	mu_assert_int_eq(cc.type & XCACHE_NO_STORE, XCACHE_NO_STORE);

	mu_assert_int_eq(cc.type, XCACHE_MAX_AGE | XCACHE_NO_CACHE | XCACHE_NO_STORE);
}

static void
test_cc_group_semi(void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf[] = "max-age=0; no-cache; no-store";
	n = xcache_control_parse(&cc, buf, sizeof(buf) - 1);
	mu_assert_int_eq(n, sizeof(buf) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf + (-1 - n));
		return;
	}

	mu_assert_int_eq(cc.type & XCACHE_MAX_AGE, XCACHE_MAX_AGE);
	mu_assert_int_eq(cc.max_age, 0);

	mu_assert_int_eq(cc.type & XCACHE_NO_CACHE, XCACHE_NO_CACHE);

	mu_assert_int_eq(cc.type & XCACHE_NO_STORE, XCACHE_NO_STORE);

	mu_assert_int_eq(cc.type, XCACHE_MAX_AGE | XCACHE_NO_CACHE | XCACHE_NO_STORE);
}

static void
test_cc_all(void)
{
	const char buf[] =
		"public,"
		"private,"
		"private=\"stuff\","
		"no-cache,"
		"no-cache=\"thing\","
		"no-store,"
		"max-age=12,"
		"s-maxage=34,"
		"max-stale,"
		"max-stale=56,"
		"min-fresh=78,"
		"no-transform,"
		"only-if-cached,"
		"must-revalidate,"
		"proxy-revalidate,"
		"ext1,"
		"ext2=something,"
		"ext3=\"other\""
		;

	SpCacheControl cc;
	ssize_t n = xcache_control_parse(&cc, buf, sizeof(buf) - 1);
	mu_assert_int_eq(n, sizeof(buf) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf + (-1 - n));
		return;
	}

	mu_assert_int_eq(cc.type & XCACHE_PUBLIC, XCACHE_PUBLIC);
	mu_assert_int_eq(cc.type & XCACHE_PRIVATE, XCACHE_PRIVATE);
	mu_assert_int_eq(cc.type & XCACHE_NO_CACHE, XCACHE_NO_CACHE);
	mu_assert_int_eq(cc.type & XCACHE_NO_STORE, XCACHE_NO_STORE);
	mu_assert_int_eq(cc.type & XCACHE_MAX_AGE, XCACHE_MAX_AGE);
	mu_assert_int_eq(cc.type & XCACHE_S_MAXAGE, XCACHE_S_MAXAGE);
	mu_assert_int_eq(cc.type & XCACHE_MAX_STALE, XCACHE_MAX_STALE);
	mu_assert_int_eq(cc.type & XCACHE_MAX_STALE_TIME, XCACHE_MAX_STALE_TIME);
	mu_assert_int_eq(cc.type & XCACHE_MIN_FRESH, XCACHE_MIN_FRESH);
	mu_assert_int_eq(cc.type & XCACHE_NO_TRANSFORM, XCACHE_NO_TRANSFORM);
	mu_assert_int_eq(cc.type & XCACHE_ONLY_IF_CACHED, XCACHE_ONLY_IF_CACHED);
	mu_assert_int_eq(cc.type & XCACHE_MUST_REVALIDATE, XCACHE_MUST_REVALIDATE);
	mu_assert_int_eq(cc.type & XCACHE_PROXY_REVALIDATE, XCACHE_PROXY_REVALIDATE);

	mu_assert_int_eq(cc.max_age, 12);
	mu_assert_int_eq(cc.s_maxage, 34);
	mu_assert_int_eq(cc.max_stale, 56);
	mu_assert_int_eq(cc.min_fresh, 78);

	char val[64];

	memcpy(val, buf+cc.private.off, cc.private.len);
	val[cc.private.len] = '\0';
	mu_assert_str_eq(val, "stuff");

	memcpy(val, buf+cc.no_cache.off, cc.no_cache.len);
	val[cc.no_cache.len] = '\0';
	mu_assert_str_eq(val, "thing");
}

static void
test_cc_all_semi(void)
{
	const char buf[] =
		"public;"
		"private;"
		"private=\"stuff\";"
		"no-cache;"
		"no-cache=\"thing\","
		"no-store;"
		"max-age=12;"
		"s-maxage=34;"
		"max-stale;"
		"max-stale=56;"
		"min-fresh=78;"
		"no-transform;"
		"only-if-cached;"
		"must-revalidate;"
		"proxy-revalidate;"
		"ext1;"
		"ext2=something;"
		"ext3=\"other\""
		;

	SpCacheControl cc;
	ssize_t n = xcache_control_parse(&cc, buf, sizeof(buf) - 1);
	mu_assert_int_eq(n, sizeof(buf) - 1);
	if (n < 0) {
		printf("ERROR: %s\n", buf + (-1 - n));
		return;
	}

	mu_assert_int_eq(cc.type & XCACHE_PUBLIC, XCACHE_PUBLIC);
	mu_assert_int_eq(cc.type & XCACHE_PRIVATE, XCACHE_PRIVATE);
	mu_assert_int_eq(cc.type & XCACHE_NO_CACHE, XCACHE_NO_CACHE);
	mu_assert_int_eq(cc.type & XCACHE_NO_STORE, XCACHE_NO_STORE);
	mu_assert_int_eq(cc.type & XCACHE_MAX_AGE, XCACHE_MAX_AGE);
	mu_assert_int_eq(cc.type & XCACHE_S_MAXAGE, XCACHE_S_MAXAGE);
	mu_assert_int_eq(cc.type & XCACHE_MAX_STALE, XCACHE_MAX_STALE);
	mu_assert_int_eq(cc.type & XCACHE_MAX_STALE_TIME, XCACHE_MAX_STALE_TIME);
	mu_assert_int_eq(cc.type & XCACHE_MIN_FRESH, XCACHE_MIN_FRESH);
	mu_assert_int_eq(cc.type & XCACHE_NO_TRANSFORM, XCACHE_NO_TRANSFORM);
	mu_assert_int_eq(cc.type & XCACHE_ONLY_IF_CACHED, XCACHE_ONLY_IF_CACHED);
	mu_assert_int_eq(cc.type & XCACHE_MUST_REVALIDATE, XCACHE_MUST_REVALIDATE);
	mu_assert_int_eq(cc.type & XCACHE_PROXY_REVALIDATE, XCACHE_PROXY_REVALIDATE);

	mu_assert_int_eq(cc.max_age, 12);
	mu_assert_int_eq(cc.s_maxage, 34);
	mu_assert_int_eq(cc.max_stale, 56);
	mu_assert_int_eq(cc.min_fresh, 78);

	char val[64];

	memcpy(val, buf+cc.private.off, cc.private.len);
	val[cc.private.len] = '\0';
	mu_assert_str_eq(val, "stuff");

	memcpy(val, buf+cc.no_cache.off, cc.no_cache.len);
	val[cc.no_cache.len] = '\0';
	mu_assert_str_eq(val, "thing");
}
*/


int
main(void)
{
	mu_init("http");

	/**
	 * Parse the input at varying "speeds". The speed is the number
	 * of bytes to emulate reading at each pass of the parser.
	 * 0 indicates that all bytes should be available at the start
	 * of the parser.
	 */
	for (ssize_t i = 0; i <= 250; i++) {
		test_request(i);
		test_chunked_request(i);
		//test_request_capture(i);
		//test_chunked_request_capture(i);
		test_response(i);
		test_chunked_response(i);
	}

	test_invalid_header();

	test_limit_method_size();
	test_exceed_method_size();
	test_limit_name_size();
	test_exceed_name_size();
	test_limit_value_size();
	test_exceed_value_size();
	test_increase_value_size();

	/*
	test_cc_max_stale();
	test_cc_private();
	test_cc_no_cache();
	test_cc_group();
	test_cc_group_semi();
	test_cc_all();
	test_cc_all_semi();
	*/
}

