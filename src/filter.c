#include "../include/crux/filter.h"
#include "../include/crux/err.h"

#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

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
	atomic_int ref;
	struct xfilter *clone;
	hs_scratch_t *key_scratch;
	hs_scratch_t *value_scratch;
	hs_database_t *key;
	union {
		hs_database_t *values[0];
		struct xfilter *chain[0];
	};
};

static inline struct xfilter *
ref(struct xfilter *f)
{
	atomic_fetch_add(&f->ref, 1);
	return f;
}

static inline bool
unref(struct xfilter *f)
{
	return atomic_fetch_sub(&f->ref, 1) == 1;
}

#define ALLOC(_mode, _count) __extension__ ({ \
	struct xfilter *f = calloc(1, sizeof(*f) + sizeof(f->values[0])*_count); \
	if (f == NULL) { return xerrno; } \
	f->mode = _mode; \
	f->count = _count; \
	f->ref = 1; \
	f; \
})

#define ISBASE(_mode) \
	(_mode == XFILTER_ACCEPT || _mode == XFILTER_REJECT)

#define ISCHAIN(_mode) \
	(_mode == XFILTER_CHAIN_AND || _mode == XFILTER_CHAIN_OR)

int
xfilter_new(struct xfilter **fp,
		const struct xfilter_expr *exp, size_t count,
		enum xfilter_mode mode,
		struct xfilter_err *err)
{
	if (count == 0 || count > MAX || !ISBASE(mode)) {
		return xerr_sys(EINVAL);
	}

	struct xfilter *f = ALLOC(mode, count);

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
	enum xfilter_mode mode = src->mode;
	struct xfilter *f = ALLOC(mode, count);
	int rc = xerr_sys(EINVAL);

	if (ISCHAIN(mode)) {
		for (int i = 0; i < count; i++) {
			rc = xfilter_clone(&f->chain[i], src->chain[i]);
			if (rc < 0) { goto error; }
		}
	}
	else {
		f->clone = ref(src);

		hs_error_t ec;

		ec = hs_clone_scratch(src->key_scratch, &f->key_scratch);
		if (ec != HS_SUCCESS) { goto error; }

		if (src->value_scratch) {
			ec = hs_clone_scratch(src->value_scratch, &f->value_scratch);
			if (ec != HS_SUCCESS) { goto error; }
		}

		f->key = src->key;
		for (int i = 0; i < count; i++) {
			f->values[i] = src->values[i];
		}
	}

	*fp = f;
	return 0;

error:
	xfilter_free(&f);
	return rc;
}

int
xfilter_chain(struct xfilter **fp,
		struct xfilter **src, size_t srclen,
		enum xfilter_mode mode)
{
	if (srclen == 0 || srclen > MAX || !ISCHAIN(mode)) {
		return xerr_sys(EINVAL);
	}

	struct xfilter *f = ALLOC(mode, srclen);
	for (size_t i = 0; i < srclen; i++) {
		f->chain[i] = ref(src[i]);
	}

	*fp = f;
	return 0;
}

void
xfilter_free(struct xfilter **fp)
{
	struct xfilter *f = *fp;
	if (f == NULL) { return; }

	*fp = NULL;

	if (unref(f)) {
		if (f->key_scratch) { hs_free_scratch(f->key_scratch); }
		if (f->value_scratch) { hs_free_scratch(f->value_scratch); }
		if (ISCHAIN(f->mode)) {
			for (int i = 0; i < f->count; i++) {
				xfilter_free(&f->chain[i]);
			}
		}
		else if (f->clone) {
			xfilter_free(&f->clone);
		}
		else {
			if (f->key) { hs_free_database(f->key); }
			for (int i = 0; i < f->count; i++) {
				if (f->values[i]) { hs_free_database(f->values[i]); }
			}
		}
		free(f);
	}
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
	enum xfilter_mode mode = f->mode;

	if (xunlikely(ISCHAIN(mode))) {
		bool or = mode == XFILTER_CHAIN_OR;
		int count = f->count;
		for (int i = 0; i < count; i++) {
			int rc = xfilter_key(f->chain[i], key, keylen);
			if (or == (rc >= 0)) { return or - 1; }
		}
		return !or - 1;
	}

	int id = -1;
	hs_scan(f->key, key, keylen, 0, f->key_scratch, match, &id);
	if (id == -1) {
		return mode == XFILTER_ACCEPT ? -1 : 0;
	}
	return mode == XFILTER_ACCEPT ? id : -1;
}

int
xfilter(const struct xfilter *f,
		const char *key, size_t keylen,
		const char *value, size_t valuelen)
{
	enum xfilter_mode mode = f->mode;

	if (xunlikely(ISCHAIN(mode))) {
		bool or = mode == XFILTER_CHAIN_OR;
		int count = f->count;
		for (int i = 0; i < count; i++) {
			int rc = xfilter(f->chain[i], key, keylen, value, valuelen);
			if (or == (rc >= 0)) { return or - 1; }
		}
		return !or - 1;
	}

	struct xmatch m = { f, value, valuelen, -1 };
	hs_scan(f->key, key, keylen, 0, f->key_scratch, match_full, &m);
	if (m.id == -1) {
		return mode == XFILTER_ACCEPT ? -1 : 0;
	}
	return mode == XFILTER_ACCEPT ? m.id : -1;
}

static void
print(const struct xfilter *f, FILE *out, int depth)
{
	for (int i = 0; i < depth; i++) {
		fwrite("  ", 1, 2, out);
	}

	if (f == NULL) {
		fprintf(out, "<crux:filter:(null)>\n");
		return;
	}

	const char *end = "INVALID>\n";
	switch (f->mode) {
	case XFILTER_ACCEPT: end = "ACCEPT"; break;
	case XFILTER_REJECT: end = "REJECT"; break;
	case XFILTER_CHAIN_AND: end = "CHAIN AND"; break;
	case XFILTER_CHAIN_OR: end = "CHAIN OR"; break;
	}

	fprintf(out, "<crux:filter:%p count=%d ref=%d %s%s>",
			(void *)f, f->count, f->ref, f->clone ? "CLONE " : "", end);

	struct xfilter *const *chld;
	int nchld = 0;
	if (ISCHAIN(f->mode)) {
		chld = f->chain;
		nchld = f->count;
	}
	else if (f->clone) {
		chld = &f->clone;
		nchld = 1;
	}

	if (nchld > 0) {
		fprintf(out, " {\n");
		for (int i = 0; i < nchld; i++) {
			print(chld[i], out, depth+1);
		}
		for (int i = 0; i < depth; i++) {
			fwrite("  ", 1, 2, out);
		}
		fprintf(out, "}\n");
	}
	else {
		fprintf(out, "\n");
	}
}

void
xfilter_print(const struct xfilter *f, FILE *out)
{
	if (out == NULL) { out = stdout; }
	print(f, out, 0);
}

