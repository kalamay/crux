#include "../include/crux/filter.h"
#include "../include/crux/err.h"

#include <stdlib.h>
#include <string.h>

#include <hs/hs.h>

#define MAX 256

struct xmatch
{
	const struct xfilter *filter;
	const char *value;
	size_t valuelen;
	int id;
};

struct xfilter
{
	enum xfilter_mode mode;
	int count;
	bool clone;
	hs_scratch_t *key_scratch;
	hs_scratch_t *value_scratch;
	hs_database_t *key;
	hs_database_t *values[0];
};

int
xfilter_new(struct xfilter **fp,
		const struct xfilter_expr *exp,
		size_t count,
		enum xfilter_mode mode,
		struct xfilter_err *err)
{
	if (count == 0 || count > MAX) { return xerr_sys(EINVAL); }

	struct xfilter *f = calloc(1, sizeof(*f) + sizeof(f->values[0])*count);
	if (f == NULL) { return xerrno; }

	f->mode = mode;
	f->count = count;

	const char *expr[MAX];
	unsigned flags[MAX];
	unsigned ids[MAX];
	hs_compile_error_t *e = NULL;
	hs_error_t ec;

	for (size_t i = 0; i < count; i++) {
		expr[i] = exp[i].key;
		flags[i] = exp[i].flags | HS_FLAG_SINGLEMATCH;
		ids[i] = (unsigned)i + 1;
	}

	ec = hs_compile_multi(expr, flags, ids, count, HS_MODE_BLOCK, NULL, &f->key, &e);
	if (ec != HS_SUCCESS) { goto error; }

	ec = hs_alloc_scratch(f->key, &f->key_scratch);
	if (ec != HS_SUCCESS) { goto error; }

	for (size_t i = 0; i < count; i++) {
		if (exp[i].value == NULL) { continue; }

		ec = hs_compile(exp[i].value, exp[i].flags, HS_MODE_BLOCK, NULL, &f->values[i], &e);
		if (ec != HS_SUCCESS) { goto error; }

		ec = hs_alloc_scratch(f->values[i], &f->value_scratch);
		if (ec != HS_SUCCESS) { goto error; }
	}

	*fp = f;
	return 0;

error:
	err->message = strdup(e->message);
	err->index = e->expression;
	hs_free_compile_error(e);
	xfilter_free(&f);
	return xerr_sys(EINVAL);
}

int
xfilter_clone(struct xfilter **fp, struct xfilter *src)
{
	int count = src->count;
	struct xfilter *f = calloc(1, sizeof(*f) + sizeof(f->values[0])*count);
	if (f == NULL) { return xerrno; }

	hs_error_t ec;

	f->mode = src->mode;
	f->count = src->count;
	f->clone = true;

	ec = hs_clone_scratch(src->key_scratch, &f->key_scratch);
	if (ec != HS_SUCCESS) { goto error; }

	ec = hs_clone_scratch(src->value_scratch, &f->value_scratch);
	if (ec != HS_SUCCESS) { goto error; }

	f->key = src->key;
	for (int i = 0; i < count; i++) {
		f->values[i] = src->values[i];
	}

	*fp = f;
	return 0;

error:
	xfilter_free(&f);
	return xerr_sys(EINVAL);
}

void
xfilter_free(struct xfilter **fp)
{
	struct xfilter *f = *fp;
	if (f == NULL) { return; }

	*fp = NULL;

	if (f->key_scratch) { hs_free_scratch(f->key_scratch); }
	if (f->value_scratch) { hs_free_scratch(f->value_scratch); }
	if (!f->clone) {
		if (f->key) { hs_free_database(f->key); }
		for (int i = 0; i < f->count; i++) {
			if (f->values[i]) { hs_free_database(f->values[i]); }
		}
	}

	free(f);
}

static int
match(unsigned int id,
		unsigned long long from,
		unsigned long long to,
		unsigned int flags,
		void *context)
{
	(void)from;
	(void)to;
	(void)flags;

	*(int *)context = (int)id;

	return 0;
}

static int
match_full(unsigned int id,
		unsigned long long from,
		unsigned long long to,
		unsigned int flags,
		void *context)
{
	(void)from;
	(void)to;
	(void)flags;

	struct xmatch *m = context;
	const struct xfilter *f = m->filter;

	hs_database_t *db = f->values[id-1];
	if (db) {
		int sub = -1;
		hs_scan(db, m->value, m->valuelen, 0, f->value_scratch, match, &sub);
		m->id = sub == 0 ? (int)id : -1;
	}
	else {
		m->id = (int)id;
	}
	return 0;
}

int
xfilter_key(const struct xfilter *f, const char *key, size_t keylen)
{
	int id = -1;
	hs_scan(f->key, key, keylen, 0, f->key_scratch, match, &id);
	if (id == -1) {
		return f->mode == XFILTER_ACCEPT ? -1 : 0;
	}
	return f->mode == XFILTER_ACCEPT ? id : -1;
}

int
xfilter(const struct xfilter *f,
		const char *key, size_t keylen,
		const char *value, size_t valuelen)
{
	struct xmatch m = { f, value, valuelen, -1 };
	hs_scan(f->key, key, keylen, 0, f->key_scratch, match_full, &m);
	if (m.id == -1) {
		return f->mode == XFILTER_ACCEPT ? -1 : 0;
	}
	return f->mode == XFILTER_ACCEPT ? m.id : -1;
}

