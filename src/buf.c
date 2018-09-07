#include "buf.h"
#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/mman.h>

int
xbuf_new(struct xbuf **bufp, size_t cap)
{
	struct xbuf *buf = malloc(sizeof(*buf));
	if (buf == NULL) { return XERRNO; }
	int rc = xbuf_init(buf, cap);
	if (rc < 0) { free(buf); }
	else { *bufp = buf; }
	return rc;
}

int
xbuf_copy(struct xbuf **bufp, const void *ptr, size_t len)
{
	int rc = xbuf_new(bufp, len);
	if (rc < 0) { return rc; }
	rc = xbuf_add(*bufp, ptr, len);
	if (rc < 0) {
		xbuf_free(bufp);
		return rc;
	}
	return 0;
}

int
xbuf_init(struct xbuf *buf, size_t cap)
{
	*buf = XBUF_INIT;
	return xbuf_ensure(buf, cap);
}

void
xbuf_free(struct xbuf **bufp)
{
	struct xbuf *buf = *bufp;
	if (buf == NULL) { return; }
	*bufp = NULL;
	if (buf->map) {
		munmap(buf->map, XBUF_MSIZE(buf));
	}
	free(buf);
}

bool
xbuf_empty(const struct xbuf *buf)
{
	return XBUF_EMPTY(buf);
}

const void *
xbuf_data(const struct xbuf *buf)
{
	return XBUF_RDATA(buf);
}

size_t
xbuf_length(const struct xbuf *buf)
{
	return XBUF_RSIZE(buf);
}

void *
xbuf_tail(const struct xbuf *buf)
{
	return XBUF_WDATA(buf);
}

size_t
xbuf_unused(const struct xbuf *buf)
{
	return XBUF_WSIZE(buf);
}

int
xbuf_ensure(struct xbuf *buf, size_t unused)
{
	size_t len = XBUF_RSIZE(buf);
	size_t full = unused + len;

	if (full <= buf->cap) {
		if (len == 0) {
			xbuf_reset(buf);
			return 0;
		}
		if (XBUF_WSIZE(buf) >= full) {
			return 0;
		}
		if (len <= XBUF_MAX_COMPACT) {
			xbuf_compact(buf);
			return 0;
		}
	}

	size_t off = XBUF_ROFFSET(buf);
	size_t size = XBUF_HINT(full + off);

	uint8_t *old = buf->map;
	uint8_t *map = MAP_FAILED;
	if (old != NULL) {
#ifdef MREMAP_MAYMOVE
		map = mremap(old, XBUF_MSIZE(buf), size, MREMAP_MAYMOVE);
#else
		map = mmap(old + XBUF_MSIZE(buf), size - XBUF_MSIZE(buf),
				PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_FIXED, -1, 0);
#endif
	}
	if (map == MAP_FAILED) {
		size = XBUF_HINT(full);
		map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
		if (map == MAP_FAILED) { return XERRNO; }
		memcpy(map, XBUF_RDATA(buf), len);
		if (old) {
			munmap(old, XBUF_MSIZE(buf));
		}
		buf->map = map;
		XBUF_COMPACT(buf, len);
	}

	buf->cap = size - XBUF_REDZONE;
	return 0;
}

int
xbuf_add(struct xbuf *buf, const void *ptr, size_t len)
{
	int rc = xbuf_ensure(buf, len);
	if (rc < 0) { return rc; }
	memcpy(XBUF_WDATA(buf), ptr, len);
	XBUF_WBUMP(buf, len);
	return 0;
}

int
xbuf_addch(struct xbuf *buf, char ch, size_t len)
{
	int rc = xbuf_ensure(buf, len);
	if (rc < 0) { return rc; }
	memset(XBUF_WDATA(buf), ch, len);
	XBUF_WBUMP(buf, len);
	return 0;
}

void
xbuf_reset(struct xbuf *buf)
{
	XBUF_COMPACT(buf, 0);
}

int
xbuf_trim(struct xbuf *buf, size_t len)
{
	size_t max = XBUF_RSIZE(buf);
	if (len > max) { return XESYS(ERANGE); }

	if (len == max) {
		xbuf_reset(buf);
	}
	else {
#if XBUF_TRIM_UNMAP
		size_t off = XBUF_ROFFSET(buf) + len;
		XBUF_RSET(buf, off);

		ssize_t trim = (off) / xpagesize * xpagesize;
		if (trim > 0) {
			ssize_t size = (ssize_t)XBUF_MSIZE(buf) - trim;
			if (size < (ssize_t)XBUF_MIN_TRIM) {
				trim -= XBUF_MIN_TRIM - size;
			}
			if (trim > 0 && munmap(buf->map, trim) == 0) {
				buf->map += trim;
				buf->cap -= trim;
			}
		}
#else
		XBUF_RBUMP(buf, len);
#endif
	}
	return 0;
}

int
xbuf_bump(struct xbuf *buf, size_t len)
{
	size_t max = XBUF_WSIZE(buf);
	if (len > max) { return XESYS(ERANGE); }
	XBUF_WBUMP(buf, len);
	return 0;
}

void
xbuf_compact(struct xbuf *buf)
{
	size_t len = XBUF_RSIZE(buf);
	if (len > 0) {
		memmove(buf->map, XBUF_RDATA(buf), len);
	}
	XBUF_COMPACT(buf, len);
}

void
xbuf_print(const struct xbuf *buf, FILE *out)
{
	if (out == NULL) { out = stdout; }

	if (buf == NULL) {
		fprintf(out, "<crux:buf:(null)>\n");
		return;
	}

	fprintf(out, "<crux:buf:%p> {\n", (void *)buf);

	char line[256];
	char *lp = line;

#define FMT(...) do { \
	size_t rem = sizeof(line) - (lp - line); \
	int len = snprintf(lp, rem, __VA_ARGS__); \
	if (len < 0 || (size_t)len > rem) { return; } \
	lp += len; \
} while (0)

#define LINE 24
#define GROUP 4
#define TRIM (LINE*2)
#define MOD(n,m) ((TRIM+n)%m)

	static const char space[64] =
		"                                "
		"                                ";
	const uint8_t *p = XBUF_RDATA(buf);
	ssize_t start = -XBUF_ROFFSET(buf);
	ssize_t end = XBUF_RSIZE(buf) - 1;
	ssize_t mark;
	int tty = isatty(fileno(out));

	if (start < -TRIM) { start = -TRIM; }
	if (tty && start < 0) { FMT("\033[0;37m"); }

	for (ssize_t i = start; i <= end; i++) {
		ssize_t mil = MOD(i, LINE);
		if (mil == 0 || i == start) {
			FMT("  %08zd: ", i);
			if (mil != 0) {
				FMT("%.*s", (int)(mil*2 + mil/GROUP), space);
			}
			mark = i;
		}
		FMT("%02x", p[i]);
		if (mil == LINE-1 || i == end) {
			ssize_t pad = LINE - mil;
			FMT("%.*s", (int)(pad*2 + pad/GROUP + MOD(mark, LINE)), space);
			for (; mark <= i; mark++) {
				FMT("%c", isprint(p[mark]) ? p[mark] : '.');
			}
			FMT("\n");
			fwrite(line, 1, lp - line, out);
			lp = line;
			if (tty && i == -1) { FMT("\033[0m"); }
		}
		else if (MOD(i, GROUP) == GROUP-1) {
			FMT(" ");
		}
	}

	fprintf(out, "}\n");
}

