#include "../include/crux/vm.h"
#include "../include/crux/err.h"

#include "config.h"

#include <string.h>
#include <sys/mman.h>

#if HAS_VM_MAP

#include <mach/mach.h>
#include <mach/vm_map.h>

#define CHECK(expr) do { \
	if ((rc = (expr)) != KERN_SUCCESS) { goto error; } \
} while(0)

int
xvm_alloc_ring(void **const ptr, size_t sz)
{
	const mach_port_t port = mach_task_self();
	vm_address_t p = 0, half;
	memory_object_size_t len = sz;
	mem_entry_name_port_t map_port = 0;
	kern_return_t rc;

	CHECK(vm_allocate(port, &p, 2*len, VM_FLAGS_ANYWHERE));
	CHECK(vm_allocate(port, &p, len, VM_FLAGS_FIXED|VM_FLAGS_OVERWRITE));
	CHECK(mach_make_memory_entry_64(port, &len, p, VM_PROT_DEFAULT, &map_port, 0));

	half = p + len;
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

	*ptr = (void *)p;
	return 0;

error:
	if (p) { vm_deallocate(port, p, 2*len); }
	return XEKERN(rc);
}

int
xvm_dealloc_ring(void *ptr, size_t sz)
{
	kern_return_t rc = vm_deallocate(mach_task_self(), (vm_address_t)ptr, 2*sz);
	return rc == KERN_SUCCESS ? 0 : XEKERN(rc);
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

int
xvm_alloc_ring(void **const ptr, size_t sz)
{
	void *p = NULL;
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
	if (p != MAP_FAILED) { munmap(p, 2*sz); }
	if (fd > -1) { close(fd); }
	return ec;
}

int
xvm_dealloc_ring(void *ptr, size_t sz)
{
	return xvm_dealloc(ptr, 2*sz);
}

#endif

#define MAP(addr, size, flags) \
	mmap((addr), (size), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|(flags), -1, 0)

int
xvm_alloc(void **const ptr, size_t sz)
{
	void *p = MAP(NULL, sz, 0);
	if (p == MAP_FAILED) {
		return XERRNO;
	}
	*ptr = p;
	return 0;
}

int
xvm_realloc(void **const ptr, size_t oldsz, size_t newsz)
{
	if (xunlikely(*ptr == NULL)) {
		return xvm_alloc(ptr, newsz);
	}

	uint8_t *old = *ptr, *p;

	if (xunlikely(oldsz >= newsz)) {
		if (oldsz != newsz) {
			int rc = munmap(old + newsz, oldsz - newsz);
			if (rc < 0) { return XERRNO; }
		}
		return 0;
	}

#if HAS_MREMAP
# if HAS_MREMAP4
	p = mremap(old, oldsz, newsz, MREMAP_MAYMOVE);
# elif HAS_MREMAP5
	p = mremap(old, oldsz, NULL, newsz, 0);
# endif
	if (p == MAP_FAILED) { return XERRNO; }
#else
	p = MAP(NULL, newsz, 0);
	if (p == MAP_FAILED) { return XERRNO; }

	memcpy(p, old, oldsz);
	munmap(old, oldsz);
#endif

	*ptr = p;
	return 0;
}

ssize_t
xvm_reallocsub(void **const ptr, size_t oldsz, size_t newsz, size_t off, size_t len)
{
	if (xunlikely(*ptr == NULL)) {
		return xvm_alloc(ptr, newsz);
	}

	uint8_t *old = *ptr, *p;

	if (xunlikely(oldsz >= newsz)) {
		size_t trunc = xpagetrunc(oldsz - newsz);
		memmove(old, old + off, len);
		if (trunc) {
			munmap(old + newsz, trunc);
		}
		return 0;
	}

#if HAS_MREMAP
	size_t trunc = xpagetrunc(off);
# if HAS_MREMAP4
	p = mremap(old, oldsz, newsz + trunc, MREMAP_MAYMOVE);
# elif HAS_MREMAP5
	p = mremap(old, oldsz, NULL, newsz + trunc, 0);
# endif
	if (p == MAP_FAILED) { return XERRNO; }
	if (trunc && munmap(p, trunc) < 0) {
		trunc = 0;
	}
	*ptr = p + trunc;
	return (ssize_t)(off - trunc);
#else
	p = MAP(NULL, newsz, 0);
	if (p == MAP_FAILED) { return XERRNO; }

	memcpy(p, old + off, len);
	munmap(old, oldsz);

	*ptr = p;
	return 0;
#endif
}

int
xvm_dealloc(void *ptr, size_t sz)
{
	int rc = munmap(ptr, sz);
	return rc == 0 ? 0 : XERRNO;
}

