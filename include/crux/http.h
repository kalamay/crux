#ifndef CRUX_HTTP_H
#define CRUX_HTTP_H

#include "def.h"
#include "range.h"

#include <stdio.h>

#define XHTTP_MAX_METHOD 32
#define XHTTP_MAX_URI 8192
#define XHTTP_MAX_REASON 256
#define XHTTP_MAX_FIELD 256
#define XHTTP_MAX_VALUE 1024

union xhttp_value {
	// request line values
	struct {
		struct xrange8 method;
		struct xrange16 uri;
		uint8_t version;
	} request;

	// response line values
	struct {
		struct xrange16 reason;
		uint16_t status;
		uint8_t version;
	} response;

	// header field name and value
	struct {
		struct xrange16 name;
		struct xrange16 value;
	} field;

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

#define XHTTP_FKEEPALIVE (1<<0)
#define XHTTP_FCHUNKED   (1<<1)

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
};



XEXTERN int
xhttp_init_request (struct xhttp *p);

XEXTERN int
xhttp_init_response (struct xhttp *p);

XEXTERN void
xhttp_final (struct xhttp *p);

XEXTERN void
xhttp_reset (struct xhttp *p);

XEXTERN ssize_t
xhttp_next (struct xhttp *p, const void *restrict buf, size_t len);

XEXTERN bool
xhttp_is_done (const struct xhttp *p);

XEXTERN void
xhttp_print (const struct xhttp *p, const void *restrict buf, FILE *out);

#endif

