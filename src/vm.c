#include "../include/crux/vm.h"
#include "../include/crux/err.h"

#include "config.h"

#include <sys/mman.h>

#if HAS_VM_MAP

#include <mach/mach.h>
#include <mach/vm_map.h>

#define CHECK(expr) do { \
	if ((rc = (expr)) != KERN_SUCCESS) { goto error; } \
} while(0)

int
xvm_map_ring(void **const ptr, size_t sz)
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
xvm_map_ring(void **const ptr, size_t sz)
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

#endif

int
xvm_map(void **const ptr, size_t sz)
{
	void *p = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
	if (p == MAP_FAILED) {
		return XERRNO;
	}
	*ptr = p;
	return 0;
}

int
xvm_remap(void **const ptr, size_t oldsz, size_t newsz)
{
	if (*ptr == NULL) {
		return xvm_map(ptr, newsz);
	}

#if HAS_MREMAP4
	void *p = mremap(*ptr, oldsz, newsz, MREMAP_MAYMOVE);
#elif HAS_MREMAP5
	void *p = mremap(*ptr, oldsz, NULL, newsz, 0);
#else
	if (oldsz > newsz) {
		int rc = munmap((char *)*ptr + newsz, oldsz - newsz);
		return rc == 0 ? 0 : XERRNO;
	}
	void *p = MAP_FAILED;
#endif
	if (p == MAP_FAILED) {
		p = mmap(NULL, newsz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
		if (p == MAP_FAILED) {
			return XERRNO;
		}
		memcpy(p, *ptr, oldsz);
		munmap(*ptr, oldsz);
	}
	*ptr = p;
	return 0;
}

int
xvm_unmap(void *ptr, size_t sz)
{
	int rc = munmap(ptr, sz);
	return rc == 0 ? 0 : XERRNO;
}

int
xvm_unmap_ring(void *ptr, size_t sz)
{
	return xvm_unmap(ptr, 2*sz);
}

