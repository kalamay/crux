#ifndef CRUX_FILTER_H
#define CRUX_FILTER_H

#include "def.h"

struct xfilter;

enum xfilter_mode
{
	XFILTER_ACCEPT,
	XFILTER_REJECT,
};

XEXTERN int
xfilter_new(struct xfilter **fp,
		const char **m,
		size_t count,
		enum xfilter_mode mode);

XEXTERN void
xfilter_free(struct xfilter **fp);

XEXTERN int
xfilter_key(const struct xfilter *f, const char *data, size_t len);


#endif

