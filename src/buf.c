#include "buf.h"
#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/mman.h>

#if HAS_VM_MAP

#include <mach/mach.h>
#include <mach/vm_map.h>

static int
map_ring(uint8_t **const ptr, size_t sz)
{
	const mach_port_t port = mach_task_self();
	vm_address_t addr = 0, half;
	memory_object_size_t len = sz;
	mem_entry_name_port_t map_port = 0;
	kern_return_t rc;

#define CHECK(expr) do { \
	if ((rc = (expr)) != KERN_SUCCESS) { goto error; } \
} while(0)

	CHECK(vm_allocate(port, &addr, 2*len, VM_FLAGS_ANYWHERE));
	CHECK(vm_allocate(port, &addr, len, VM_FLAGS_FIXED|VM_FLAGS_OVERWRITE));
	CHECK(mach_make_memory_entry_64(port, &len, addr, VM_PROT_DEFAULT, &map_port, 0));

	half = addr + len;
	CHECK(vm_map(
				port,
				&half,
				len,
				0, // mask
				VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
				map_port,
				0, // offset
				false, // copy
				VM_PROT_DEFAULT,
				VM_PROT_DEFAULT,
				VM_INHERIT_NONE));

#undef CHECK

	*ptr = (uint8_t *)addr;
	return 0;

error:
	if (addr) { vm_deallocate(port, addr, 2*len); }
	return XEKERN(rc);
}

#else

# if HAS_MEMFD
#  include <linux/memfd.h>
#  include <sys/syscall.h>
static int
open_tmp()
{
    return syscall(__NR_memfd_create, "crux", 0);
}
# else
static int
open_tmp()
{
	char path[] = "/tmp/crux.XXXXXX";
	int fd = mkstemp(path);
	if (fd >= 0) { unlink(path); }
	return fd;
}
# endif

static int
map_ring(uint8_t **const ptr, size_t sz)
{
	uint8_t *p = NULL;
	int fd = -1, ec = 0;

	// Create a temporary file descriptor.
	fd = open_tmp();
	if (fd == -1 || ftruncate(fd, sz) != 0) {
		ec = XERRNO;
		goto error;
	}

	// Map the full file into an initial address that will cover the duplicate
	// address range.
	p = mmap(NULL, sz * 2, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
	if (p == MAP_FAILED) {
		ec = XERRNO;
		goto error;
	}

	// Map the two halves of the buffer into adjacent adresses after the header region.
	if (mmap(p, sz, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0) == MAP_FAILED ||
			mmap(p+sz, sz, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0) == MAP_FAILED) {
		ec = XERRNO;
		goto error;
	}

	close(fd);
	*ptr = p;
	return 0;

error:
	if (p != MAP_FAILED) { munmap(p, size * 2); }
	if (fd > -1) { close(fd); }
	return ec;
}

#endif

int
xbuf_new(struct xbuf **bufp, size_t cap, bool ring)
{
	struct xbuf *buf = malloc(sizeof(*buf));
	if (buf == NULL) { return XERRNO; }
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
#if HAS_VM_MAP
		if (buf->mode == XBUF_RING) {
			vm_deallocate(mach_task_self(), (vm_address_t)buf->map, buf->sz);
		}
		else {
			munmap(buf->map, buf->sz);
		}
#else
		munmap(buf->map, buf->sz);
#endif
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
	size_t size = XBUF_LINE_SIZE(full + off);

	uint8_t *old = buf->map;
	uint8_t *map = MAP_FAILED;
	if (old != NULL) {
#ifdef MREMAP_MAYMOVE
		map = mremap(old, buf->sz, size, MREMAP_MAYMOVE);
#else
		map = mmap(old + buf->sz, size - buf->sz,
				PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_FIXED, -1, 0);
#endif
	}
	if (map == MAP_FAILED) {
		size = XBUF_LINE_SIZE(full);
		map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
		if (map == MAP_FAILED) { return XERRNO; }
		memcpy(map, XBUF_RDATA(buf), len);
		if (old) {
			munmap(old, buf->sz);
		}
		buf->map = map;
		XBUF_COMPACT(buf, len);
	}

	buf->cap = size - XBUF_REDZONE;
	buf->sz = size;
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
	int rc = map_ring(&map, sz);
	if (rc < 0) { return rc; }

	size_t len = XBUF_RSIZE(buf);
	memcpy(map, XBUF_RDATA(buf), len);

	if (buf->map) {
#if HAS_VM_MAP
		vm_deallocate(mach_task_self(), (vm_address_t)buf->map, buf->sz);
#else
		munmap(buf->map, buf->sz);
#endif
	}

	buf->map = map;
	buf->cap = sz;
	buf->sz = 2*sz;
	XBUF_COMPACT(buf, len);

	return 0;
}

int
xbuf_ensure(struct xbuf *buf, size_t unused)
{
	switch (buf->mode) {
	case XBUF_LINE: return ensure_line(buf, unused);
	case XBUF_RING: return ensure_ring(buf, unused);
	}
	return XESYS(ENOTSUP);
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
		XBUF_RBUMP(buf, len);
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

