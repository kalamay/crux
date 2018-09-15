#include "../include/crux/filter.h"
#include "../include/crux/err.h"

#include <stdlib.h>
#include <string.h>

struct xfilter
{
	enum xfilter_mode mode;
	int count;
	char *values[0];
};

int
xfilter_new(struct xfilter **fp,
		const char **m,
		size_t count,
		enum xfilter_mode mode)
{
	if (count > INT_MAX) {
		return xerr_sys(EINVAL);
	}

	struct xfilter *f = malloc(sizeof(*f) + sizeof(f->values[0])*count);
	if (f == NULL) {
		return xerrno;
	}

	f->mode = mode;
	f->count = count;
	for (size_t i = 0; i < count; i++) {
		f->values[i] = strdup(m[i]);
	}

	qsort(f->values, count, sizeof(*f->values),
			(int(*)(const void *, const void *))strcasecmp);

	*fp = f;
	return 0;
}

void
xfilter_free(struct xfilter **fp)
{
	struct xfilter *f = *fp;
	if (f == NULL) {
		return;
	}

	*fp = NULL;
	for (int i = 0; i < f->count; i++) {
		free(f->values[i]);
	}
	free(f);
}

static bool
match_key(const struct xfilter *f, const char *data, size_t len)
{
	char *const *b = f->values;
	size_t n = f->count;
	while (n > 0) {
		char *const *at = b + n/2;
		int sign = strncasecmp(*at, data, len);
		if (sign == 0)     { return true; }
		else if (n == 1)   { break; }
		else if (sign > 0) { n /= 2; }
		else               { b = at; n -= n/2; }
	}
	return false;
}

int
xfilter_key(const struct xfilter *f, const char *data, size_t len)
{
	return (f->mode == (match_key(f, data, len) ? XFILTER_ACCEPT : XFILTER_REJECT)) - 1;
}

