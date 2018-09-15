#ifndef CRUX_HTTP_H
#define CRUX_HTTP_H

#include "def.h"
#include "range.h"
#include "buf.h"

#include <stdio.h>
#include <sys/uio.h>

#define XHTTP_MAX_METHOD 32
#define XHTTP_MAX_URI 8192
#define XHTTP_MAX_REASON 256
#define XHTTP_MAX_FIELD 256
#define XHTTP_MAX_VALUE 1024

struct xhttp_request
{
	struct xrange8 method;
	struct xrange16 uri;
	uint8_t version;
};

struct xhttp_response
{
	struct xrange16 reason;
	uint16_t status;
	uint8_t version;
};

struct xhttp_field
{
	struct xrange16 name;
	struct xrange16 value;
};

union xhttp_value
{
	struct xhttp_request request; // request line values
	struct xhttp_response response; // response line values
	struct xhttp_field field; // header field name and value

	// beginning of body
	struct {
		size_t content_length;
		bool chunked;
	} body_start;

	// size of next chunk
	struct {
		size_t length;
	} body_chunk;
};

enum xhttp_type {
	XHTTP_NONE = -1,
	XHTTP_REQUEST,        // complete request line
	XHTTP_RESPONSE,       // complete response line
	XHTTP_FIELD,          // header or trailer field name and value
	XHTTP_BODY_START,     // start of the body
	XHTTP_BODY_CHUNK,     // size for chunked body
	XHTTP_BODY_END,       // end of the body chunks
	XHTTP_TRAILER_END     // complete request or response
};

struct xhttp_map;

#define XHTTP_FKEEPALIVE (1<<0)
#define XHTTP_FCHUNKED   (1<<1)

#define XHTTP_ACCEPT 1
#define XHTTP_REJECT 2

struct xhttp {
	// public
	uint16_t max_method;  // max size for a request method
	uint16_t max_uri;     // max size for a request uri
	uint16_t max_reason;  // max size for a response status message
	uint16_t max_field;   // max size for a header field
	uint16_t max_value;   // max size for a header value

	// readonly
	uint16_t scans;       // number of passes through the scanner
	uint8_t cscans;       // number of scans in the current rule
	bool response;        // true if response, false if request
	uint8_t flags;        // set by field scanner
	bool trailers;        // parsing trailers
	union xhttp_value as; // captured value
	enum xhttp_type type; // type of the captured value
	unsigned cs;          // current scanner state
	size_t off;           // internal offset mark
	size_t body_len;      // content length or current chunk size
	struct xhttp_map *map;// optional map to collect headers and then trailers
	const struct xbuf *buf;// buffer used in next call
	void *block;          // block list
	int blocklen;         // number of elements in block list
	int blocktype;        // block type (XHTTP_ACCEPT or XHTTP_REJECT)
};



XEXTERN int
xhttp_init_request(struct xhttp *p, struct xhttp_map *map);

XEXTERN int
xhttp_init_response(struct xhttp *p, struct xhttp_map *map);

XEXTERN void
xhttp_final(struct xhttp *p);

XEXTERN void
xhttp_reset(struct xhttp *p);

XEXTERN int
xhttp_block(struct xhttp *p, const char **names, size_t count, int type);

XEXTERN ssize_t
xhttp_next(struct xhttp *p, const struct xbuf *buf);

XEXTERN bool
xhttp_is_done(const struct xhttp *p);

XEXTERN void
xhttp_print(const struct xhttp *p, const struct xbuf *buf, FILE *out);


XEXTERN int
xhttp_map_new(struct xhttp_map **mapp);

XEXTERN void
xhttp_map_free(struct xhttp_map **mapp);

XEXTERN void
xhttp_map_reset(struct xhttp_map *map);

XEXTERN int
xhttp_map_add(struct xhttp_map *map,
		const char *name, size_t namelen, 
		const char *value, size_t valuelen);

XEXTERN int
xhttp_map_addstr(struct xhttp_map *map, const char *name, const char *value);

XEXTERN size_t
xhttp_map_get(struct xhttp_map *map,
		const char *name, size_t namelen,
		struct iovec *iov, size_t iovlen);

XEXTERN size_t
xhttp_map_full(const struct xhttp_map *map, struct iovec *iov, size_t iovlen);

XEXTERN void
xhttp_map_print(const struct xhttp_map *map, FILE *out);

#endif

