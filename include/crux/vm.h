#ifndef CRUX_VM_H
#define CRUX_VM_H

#include "def.h"

XEXTERN int
xvm_alloc(void **const ptr, size_t sz);

XEXTERN int
xvm_realloc(void **const ptr, size_t oldsz, size_t newsz);

XEXTERN ssize_t
xvm_reallocsub(void **const ptr, size_t oldsz, size_t newsz, size_t off, size_t len);

XEXTERN int
xvm_dealloc(void *ptr, size_t sz);

XEXTERN int
xvm_alloc_ring(void **const ptr, size_t sz);

XEXTERN int
xvm_dealloc_ring(void *ptr, size_t sz);

#endif

