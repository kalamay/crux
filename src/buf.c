#include "buf.h"
#include "../include/crux/err.h"

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
xbuf_init(struct xbuf *buf, size_t cap)
{
	*buf = (struct xbuf)XBUF_INIT;
	return xbuf_ensure(buf, cap);
}

void
xbuf_free(struct xbuf **bufp)
{
	struct xbuf *buf = *bufp;
	if (buf == NULL) { return; }
	*bufp = NULL;
	if (buf->map) {
		munmap(buf->map, XBUF_MAPSIZE(buf));
	}
	free(buf);
}

const void *
xbuf_value(const struct xbuf *buf)
{
	return buf->rd;
}

size_t
xbuf_length(const struct xbuf *buf)
{
	return XBUF_LENGTH(buf);
}

void *
xbuf_tail(const struct xbuf *buf)
{
	return buf->wr;
}

size_t
xbuf_capacity(const struct xbuf *buf)
{
	return XBUF_CAPACITY(buf);
}

int
xbuf_ensure(struct xbuf *buf, size_t hint)
{
	size_t len = XBUF_LENGTH(buf);
	hint += len;

	if (hint <= buf->cap) {
		if (len == 0) {
			xbuf_reset(buf);
			return 0;
		}
		if (XBUF_CAPACITY(buf) >= hint) {
			return 0;
		}
		if (len <= XBUF_MAX_COMPACT) {
			xbuf_compact(buf);
			return 0;
		}
	}

	size_t off = XBUF_READ_OFFSET(buf);
	size_t size = XBUF_HINT(hint + off);

	uint8_t *old = buf->map;
	uint8_t *map = MAP_FAILED;
	if (old != NULL) {
#if XBUF_REDZONE == PAGESIZE
		mprotect(old + buf->cap, PAGESIZE, PROT_READ|PROT_WRITE);
#endif
#ifdef MREMAP_MAYMOVE
		map = mremap(old, XBUF_MAPSIZE(buf), size, MREMAP_MAYMOVE);
#else
		map = mmap(old + XBUF_MAPSIZE(buf), size - XBUF_MAPSIZE(buf),
				PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_FIXED, -1, 0);
#endif
	}
	if (map == MAP_FAILED) {
		size = XBUF_HINT(hint);
		map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
		if (map == MAP_FAILED) { return XERRNO; }
		memcpy(map, buf->rd, len);
		if (old) {
			munmap(old, XBUF_MAPSIZE(buf));
		}
		buf->wr = map + len;
		buf->map = buf->rd = map;
	}

	buf->cap = size - XBUF_REDZONE;
#if XBUF_REDZONE == PAGESIZE
	mprotect(buf->map + buf->cap, PAGESIZE, PROT_READ);
#endif
	return 0;
}

int
xbuf_add(struct xbuf *buf, const void *ptr, size_t len)
{
	int rc = xbuf_ensure(buf, len);
	if (rc < 0) { return rc; }
	memcpy(buf->wr, ptr, len);
	buf->wr += len;
	return 0;
}

void
xbuf_reset(struct xbuf *buf)
{
	buf->rd = buf->wr = buf->map;
}

int
xbuf_trim(struct xbuf *buf, size_t len)
{
	size_t max = XBUF_LENGTH(buf);
	if (len > max) { return XESYS(ERANGE); }

	if (len == max) {
		xbuf_reset(buf);
	}
	else {
		size_t off = XBUF_READ_OFFSET(buf) + len;
		buf->rd = buf->map + off;

		ssize_t trim = (off) / PAGESIZE * PAGESIZE;
		if (trim > 0) {
			ssize_t size = (ssize_t)XBUF_MAPSIZE(buf) - trim;
			if (size < XBUF_MIN_TRIM) {
				trim -= XBUF_MIN_TRIM - size;
			}
			if (trim > 0 && munmap(buf->map, trim) == 0) {
				buf->map += trim;
				buf->cap -= trim;
			}
		}
	}
	return 0;
}

int
xbuf_bump(struct xbuf *buf, size_t len)
{
	size_t max = XBUF_CAPACITY(buf);
	if (len > max) { return XESYS(ERANGE); }
	buf->wr += len;
	return 0;
}

void
xbuf_compact(struct xbuf *buf)
{
	size_t len = XBUF_LENGTH(buf);
	if (len > 0) {
		memmove(buf->map, buf->rd, len);
	}
	buf->rd = buf->map;
	buf->wr = buf->map + len;
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
	const uint8_t *p = buf->rd;
	ssize_t start = -XBUF_READ_OFFSET(buf);
	ssize_t end = XBUF_LENGTH(buf) - 1;
	ssize_t mark;
	int tty = isatty(fileno(out));

	if (start < -TRIM) { start = -TRIM; }
	if (tty && start < 0) { FMT("\033[0;37m"); }

	for (ssize_t i = start; i <= end; i++) {
		ssize_t mil = MOD(i, LINE);
		if (mil == 0 || i == start) {
			FMT("    %08zd: ", i);
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

