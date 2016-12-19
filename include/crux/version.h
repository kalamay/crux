#ifndef CRUX_VERSION_H
#define CRUX_VERSION_H

#include <stdint.h>

struct xversion {
	uint8_t major;
	uint8_t minor;
	uint16_t patch;
};

struct xversion
xversion (void);

#endif

