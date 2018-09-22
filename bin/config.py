#!/usr/bin/env python

import os, platform
from subprocess import Popen, PIPE

ARCH = {
	'x86_64': 'X86_64',
	'AMD64': 'X86_64',
	'i386': 'X86_32',
	'x86': 'X86_32',
}
CC = ['cc', '-D_GNU_SOURCE', '-Wno-nonnull', '-x', 'c', '-o', '/dev/null', '-', '-ldl']
DEVNULL = open(os.devnull, 'w')

def memoize(f):
	class memodict(dict):
		def __init__(self, f):
			self.f = f
		def __call__(self, *args):
			return self[args]
		def __missing__(self, key):
			ret = self[key] = self.f(*key)
			return ret
	return memodict(f)

def print_flag(name, val="1"):
	print("""#ifndef HAS_%s
# define HAS_%s %s
#endif""" % (name, name, val))

def arch(machine=platform.machine()):
	return ARCH.get(machine) or machine.upper()

@memoize
def compiles(c):
	p = Popen(CC, stdin=PIPE, stdout=DEVNULL, stderr=DEVNULL)
	p.communicate(input=c)
	return p.returncode == 0

@memoize
def has_function(name, args, *hdrs):
	inc = ["#include <%s>" % h for h in hdrs]
	return compiles("""
		%s
		int main(void) { %s(%s); }
	""" % ("\n".join(inc), name, ",".join(["0"] * args)))

def has_sock_flags():
	return compiles("""
		#include <sys/socket.h>
		int main(void) { socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0); }
	""")

def has_accept4():
	return has_function("accept4", 4, "sys/socket.h")

def has_clock_gettime():
	return has_function("clock_gettime", 2, "time.h")

def has_mach_time():
	return has_function("mach_absolute_time", 0, "mach/mach_time.h")

def has_dladdr():
	return has_function("dladdr", 2, "dlfcn.h")

def has_kqueue():
	return has_function("kqueue", 0, "sys/event.h")

def has_epoll():
	return has_function("epoll_create", 1, "sys/epoll.h")

def has_pipe2():
	return has_function("pipe2", 2, "fcntl.h", "unistd.h")

def has_getrandom():
	return has_function("getrandom", 3, "sys/random.h")

def has_mremap4():
	return has_function("mremap", 4, "sys/mman.h")

def has_mremap5():
	return has_function("mremap", 5, "sys/mman.h")

def has_memfd():
	return compiles("""
		#include <unistd.h>
		#include <linux/memfd.h>
		#include <sys/syscall.h>
		int main(void) { return syscall(__NR_memfd_create, "crux", 0); }
	""")

def has_vm_map():
	return has_function("vm_map", 11, "mach/mach.h", "mach/vm_map.h")

def has_shm_open():
	return has_function("shm_open", 5, "sys/mman.h", "fcntl.h")

print(("""
#if !defined(HAS_X86_64) && !defined(HAS_X86_32) && !defined(HAS_ARM_64) && !defined(HAS_ARM_32)
# if defined (__aarch64__)
#  define HAS_ARM_64 1
# elif defined (__arm__)
#  define HAS_ARM_32 1
# elif defined (__amd64__) || defined (__x86_64__) || defined (_M_X64) || defined (_M_AMD64)
#  define HAS_X86_64 1
# elif defined (__i386__) || defined (_M_IX86) || defined (_X86_)
#  define HAS_X86_32 1
# else
""").strip())
print("#  define HAS_%s 1" % arch())
print("# endif")
print("#endif")

if has_sock_flags():    print_flag("SOCK_FLAGS")
if has_accept4():       print_flag("ACCEPT4")
if has_clock_gettime(): print_flag("CLOCK_GETTIME")
if has_mach_time():     print_flag("MACH_TIME")
if has_dladdr():        print_flag("DLADDR")
if has_kqueue():        print_flag("KQUEUE")
if has_epoll():         print_flag("EPOLL")
if has_pipe2():         print_flag("PIPE2")
if has_getrandom():     print_flag("GETRANDOM")
if has_mremap4():       print_flag("MREMAP4")
elif has_mremap5():     print_flag("MREMAP5")
if has_vm_map():        print_flag("VM_MAP")
if has_memfd():         print_flag("MEMFD")
if has_shm_open():      print_flag("SHM_OPEN")

