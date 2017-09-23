#!/usr/bin/env python

import os, math, platform, resource, ctypes
from subprocess import Popen, PIPE

PAGESIZE = resource.getpagesize()
PTRSIZE = ctypes.sizeof(ctypes.c_void_p)
PAGEPTR = PAGESIZE / PTRSIZE
ARCH = {
	'x86_64': 'X86_64',
	'AMD64': 'X86_64',
	'i386': 'X86_32',
	'x86': 'X86_32',
}
CC = ['cc', '-D_GNU_SOURCE', '-ldl', '-x', 'c', '-o', '/dev/null', '-']
DEVNULL = open(os.devnull, 'w')

def memoize(f):
	class memodict(dict):
		def __missing__(self, key):
			ret = self[key] = f(key)
			return ret 
	return memodict().__getitem__

def arch(machine=platform.machine()):
	return ARCH.get(machine) or machine.upper()

@memoize
def compiles(c):
	p = Popen(CC, stdin=PIPE, stdout=DEVNULL, stderr=DEVNULL)
	p.communicate(input=c)
	return p.returncode == 0

def has_clock_gettime():
	return compiles("""
		#include <time.h>
		struct timespec tp;
		int main(void) { clock_gettime (CLOCK_REALTIME, &tp); }
	""")

def has_mach_time():
	return compiles("""
		#include <mach/mach_time.h>
		int main(void) { mach_absolute_time (); }
	""")

def has_dlsym():
	return compiles("""
		#include <dlfcn.h>
		Dl_info info;
		int main(void) { dladdr (main, &info); }
	""")

def has_kqueue():
	return compiles("""
		#include <sys/event.h>
		int main(void) { kqueue (); }
	""")

def has_epoll():
	return compiles("""
		#include <sys/epoll.h>
		int main(void) { epoll_create (10); }
	""")

def has_getrandom():
	return compiles("""
		#include <sys/random.h>
		int main(void) { char buf[8]; getrandom(buf, 8, 0); }
	""")

def has_arc4():
	return compiles("""
		#include <stdlib.h>
		int main(void) { char buf[8]; arc4random_buf(buf, 8); }
	""")

def has_mremap4():
	return compiles("""
		#include <sys/mman.h>
		int main(void) { mremap(0, 0, 0, 0); }
	""")

def has_mremap5():
	return compiles("""
		#include <sys/mman.h>
		int main(void) { mremap(0, 0, 0, 0, 0); }
	""")

print("#define XHEAP_PAGECOUNT %d" % PAGEPTR)
print("#define XHEAP_PAGEMASK %d" % (PAGEPTR - 1))
print("#define XHEAP_PAGESHIFT %d" % math.log(PAGEPTR, 2))
print("#define PAGESIZE %d" % PAGESIZE)

if has_clock_gettime():
	print("#define HAS_CLOCK_GETTIME 1")
if has_mach_time():
	print("#define HAS_MACH_TIME 1")
if has_dlsym():
	print("#define HAS_DLADDR 1")
if has_kqueue():
	print("#define HAS_KQUEUE 1")
elif has_epoll():
	print("#define HAS_EPOLL 1")
if has_getrandom():
	print("#define HAS_GETRANDOM 1")
if has_arc4():
	print("#define HAS_ARC4 1")
if has_mremap4() or has_mremap5():
	print("#define HAS_MREMAP 1")

print(("""
#if defined (__aarch64__)
# define HAS_ARM_64 1
#elif defined (__arm__)
# define HAS_ARM_32 1
#elif defined (__amd64__) || defined (__x86_64__) || defined (_M_X64) || defined (_M_AMD64)
# define HAS_X86_64 1
#elif defined (__i386__) || defined (_M_IX86) || defined (_X86_)
# define HAS_X86_32 1
#else
""").strip())
print("# define HAS_%s 1" % arch())
print("#endif")

print(("""
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
# include <unistd.h>
# define HAS_POSIX _POSIX_VERSION
#endif
""").strip())
