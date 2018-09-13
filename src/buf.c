#include "buf.h"
#include "../include/crux/vm.h"

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

int
xbuf_new(struct xbuf **bufp, size_t cap, bool ring)
{
	struct xbuf *buf = malloc(sizeof(*buf));
	if (buf == NULL) { return xerrno; }
	int rc = xbuf_init(buf, cap, ring ? XBUF_RING : XBUF_LINE);
	if (rc < 0) { free(buf); }
	else { *bufp = buf; }
	return rc;
}

int
xbuf_copy(struct xbuf **bufp, const void *ptr, size_t len, bool ring)
{
	int rc = xbuf_new(bufp, len, ring);
	if (rc < 0) { return rc; }
	rc = xbuf_add(*bufp, ptr, len);
	if (rc < 0) {
		xbuf_free(bufp);
		return rc;
	}
	return 0;
}

int
xbuf_open(struct xbuf **bufp, const char *path, off_t off, size_t len)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) { return xerrno; }

	int rc = 0;
	size_t sz = 0;
	uint8_t *p = MAP_FAILED;

	if (len == 0) {
		struct stat sbuf;
		if (fstat(fd, &sbuf) < 0) { goto error; }
		if (off > sbuf.st_size) {
			errno = EINVAL;
			goto error;
		}
		len = (size_t)(sbuf.st_size - off);
	}

	sz = XBUF_FILE_SIZE(len);
	p = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, off);
	if (p == MAP_FAILED) { goto error; }

	madvise(p, sz, MADV_SEQUENTIAL);

	struct xbuf *buf = malloc(sizeof(*buf));
	if (buf == NULL) { goto error; }
	*buf = (struct xbuf){ p, 0, len, len, sz, XBUF_FILE };
	*bufp = buf;
	goto done;

error:
	rc = xerrno;
	if (p != MAP_FAILED) { munmap(p, sz); }

done:
	xretry(close(fd));
	return rc;
}

int
xbuf_init(struct xbuf *buf, size_t cap, int mode)
{
	*buf = XBUF_INIT(mode);
	return xbuf_ensure(buf, cap);
}

void
xbuf_free(struct xbuf **bufp)
{
	struct xbuf *buf = *bufp;
	if (buf == NULL) { return; }
	*bufp = NULL;

	if (buf->map) {
		if (buf->mode == XBUF_RING) {
			xvm_dealloc_ring(buf->map, buf->sz);
		}
		else {
			munmap(buf->map, buf->sz);
		}
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
	return buf->mode == XBUF_RING ? buf->cap : XBUF_WSIZE(buf);
}

static int
ensure_line(struct xbuf *buf, size_t unused)
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
	size_t sz = XBUF_LINE_SIZE(full + off);
	ssize_t rc = xvm_reallocsub((void **)&buf->map, buf->sz, sz, off, len);

	if (rc < 0) {
		return (int)rc;
	}

	buf->cap = sz - XBUF_REDZONE;
	buf->sz = sz;
	buf->r = rc;
	buf->w = rc + len;

	return 0;
}

static int
ensure_ring(struct xbuf *buf, size_t unused)
{
	if (unused <= buf->cap) {
		size_t wr = buf->cap - XBUF_RSIZE(buf);
		if (wr < unused) {
			XBUF_RBUMP(buf, unused - wr);
		}
		return 0;
	}

	size_t sz = XBUF_RING_SIZE(unused);

	uint8_t *map;
	int rc = xvm_alloc_ring((void **)&map, sz);
	if (rc < 0) { return rc; }

	size_t len = XBUF_RSIZE(buf);
	memcpy(map, XBUF_RDATA(buf), len);

	if (buf->map) {
		xvm_dealloc_ring(buf->map, buf->sz);
	}

	buf->map = map;
	buf->cap = buf->sz = sz;
	XBUF_COMPACT(buf, len);

	return 0;
}

int
xbuf_ensure(struct xbuf *buf, size_t unused)
{
	if (buf->mode == XBUF_LINE) {
		return ensure_line(buf, unused);
	}
	if (buf->mode == XBUF_RING) {
		return ensure_ring(buf, unused);
	}
	return xerr_sys(ENOTSUP);
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
	XBUF_COMPACT(buf, buf->mode == XBUF_FILE ? buf->w : 0);
}

int
xbuf_trim(struct xbuf *buf, size_t len)
{
	size_t max = XBUF_RSIZE(buf);
	if (len > max) { return xerr_sys(ERANGE); }

	if (len == max) {
		xbuf_reset(buf);
	}
	else {
		XBUF_RBUMP(buf, len);
	}
	return 0;
}

int
xbuf_bump(struct xbuf *buf, size_t len)
{
	size_t max = XBUF_WSIZE(buf);
	if (len > max) { return xerr_sys(ERANGE); }
	XBUF_WBUMP(buf, len);
	return 0;
}

int
xbuf_compact(struct xbuf *buf)
{
	if (buf->mode == XBUF_FILE) { return xerr_sys(ENOTSUP); }
	size_t len = XBUF_RSIZE(buf);
	if (len > 0) {
		memmove(buf->map, XBUF_RDATA(buf), len);
	}
	XBUF_COMPACT(buf, len);
	return 0;
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

