#ifndef CRUX_FILTER_H
#define CRUX_FILTER_H

#include "def.h"

#include <stdio.h>

struct xfilter;

enum xfilter_mode
{
	XFILTER_ACCEPT,
	XFILTER_REJECT,
	XFILTER_CHAIN_AND,
	XFILTER_CHAIN_OR,
};

enum xfilter_flag
{
	XFILTER_CASELESS    = 1<<0,
	XFILTER_DOTALL      = 1<<1,
	XFILTER_ALLOWEMPTY  = 1<<4,
	XFILTER_UTF8        = 1<<5,
	XFILTER_UCP         = 1<<6,
	XFILTER_COMBINATION = 1<<9,

	XFILTER_HTTP       = (XFILTER_CASELESS|XFILTER_DOTALL),
};

struct xfilter_expr
{
	const char *key;
	const char *value;
	int flags;
};

struct xfilter_err
{
	const char *message;
	ssize_t index;
};

XEXTERN int
xfilter_new(struct xfilter **fp,
		const struct xfilter_expr *exp, size_t count,
		enum xfilter_mode mode,
		struct xfilter_err *err);

XEXTERN void
xfilter_free(struct xfilter **fp);

XEXTERN int
xfilter_clone(struct xfilter **fp, struct xfilter *src);

XEXTERN struct xfilter *
xfilter_ref(struct xfilter *f);

XEXTERN int
xfilter_chain(struct xfilter **fp,
		struct xfilter **src, size_t srclen,
		enum xfilter_mode mode);

XEXTERN int
xfilter_key(const struct xfilter *f, const char *key, size_t keylen);

XEXTERN int
xfilter(const struct xfilter *f,
		const char *key, size_t keylen,
		const char *value, size_t valuelen);

XEXTERN void
xfilter_print(const struct xfilter *f, FILE *out);

#endif

