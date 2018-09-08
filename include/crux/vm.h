#ifndef CRUX_VM_H
#define CRUX_VM_H

#include "def.h"

XEXTERN int
xvm_map(void **const ptr, size_t sz);

XEXTERN int
xvm_remap(void **const ptr, size_t oldsz, size_t newsz);

XEXTERN int
xvm_unmap(void *ptr, size_t sz);

XEXTERN int
xvm_map_ring(void **const ptr, size_t sz);

XEXTERN int
xvm_unmap_ring(void *ptr, size_t sz);

#endif

