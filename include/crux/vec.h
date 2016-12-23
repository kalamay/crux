#ifndef CRUX_VEC_H
#define CRUX_VEC_H

#include "def.h"
#include "err.h"

#include <stdlib.h>
#include <string.h>

#define XVEC(TEnt) \
	size_t size; \
	size_t count; \
	TEnt *arr

#define XVEC_INIT { 0, 0, NULL }

#define XVEC_EXTERN(pref, TVec, TEnt) \
	XVEC_PROTO (XEXTERN, pref, TVec, TEnt)

#define XVEC_STATIC(pref, TVec, TEnt) \
	XVEC_PROTO (XSTATIC, pref, TVec, TEnt) \
	XVEC_GEN (pref, TVec, TEnt)

#define XVEC_PROTO(attr, pref, TVec, TEnt) \
	attr void \
	pref##_final (TVec *vec); \
	attr int \
	pref##_resize (TVec *vec, size_t hint); \
	attr int \
	pref##_push (TVec *vec, TEnt ent); \
	attr int \
	pref##_pushn (TVec *vec, const TEnt *ent, size_t nent); \
	attr TEnt \
	pref##_pop (TVec *vec, TEnt def); \
	attr int \
	pref##_popn (TVec *vec, TEnt *ent, size_t nent); \
	attr TEnt \
	pref##_shift (TVec *vec, TEnt def); \
	attr int \
	pref##_shiftn (TVec *vec, TEnt *ent, size_t nent); \
	attr int \
	pref##_splice (TVec *vec, ssize_t pos, ssize_t len, TEnt *ent, size_t nent); \
	attr int \
	pref##_insert (TVec *vec, size_t pos, TEnt *ent, size_t nent); \
	attr int \
	pref##_remove (TVec *vec, size_t pos, size_t len); \
	attr void \
	pref##_reverse (TVec *vec); \
	attr void \
	pref##_sort (TVec *vec, int (*compar)(const TEnt *, const TEnt *)); \
	attr TEnt * \
	pref##_search (TVec *vec, const TEnt *e, int (*compar)(const TEnt *, const TEnt *)); \
	attr TEnt * \
	pref##_bsearch (TVec *vec, const TEnt *e, int (*compar)(const TEnt *, const TEnt *)); \
	attr ssize_t \
	pref##_index (const TVec *vec, const TEnt *e); \
	attr void \
	pref##_print (const TVec *vec, FILE *out, void (*fn)(TEnt *, FILE *out)); \

#define XVEC_GEN(pref, TVec, TEnt) \
	void \
	pref##_final (TVec *vec) \
	{ \
		free (vec->arr); \
		*vec = (TVec)XVEC_INIT; \
	} \
	int \
	pref##_resize (TVec *vec, size_t hint) \
	{ \
		if (hint < vec->count) { return -EPERM; } \
		if (hint < 8) { hint = 8; } \
		if (hint <= vec->size && hint >= vec->size>>2) { return 0; } \
		size_t b = sizeof (vec->arr[0]) * hint; \
		b = b < 16276 ? xpower2 (b) : xquantum (b, 4096); \
		TEnt *arr = malloc (b); \
		if (arr == NULL) { return XERRNO; } \
		memcpy (arr, vec->arr, sizeof (vec->arr[0]) * vec->count); \
		vec->size = b / sizeof (vec->arr[0]); \
		vec->arr = arr; \
		return 1; \
	} \
	int \
	pref##_push (TVec *vec, TEnt ent) \
	{ \
		int rc = pref##_resize (vec, vec->count + 1); \
		if (rc >= 0) { vec->arr[vec->count++] = ent; } \
		return rc; \
	} \
	int \
	pref##_pushn (TVec *vec, const TEnt *ent, size_t nent) \
	{ \
		int rc = pref##_resize (vec, vec->count + nent); \
		if (rc >= 0) { \
			memcpy (vec->arr + vec->count, ent, sizeof (*ent) * nent); \
			vec->count += nent; \
		} \
		return rc; \
	} \
	TEnt \
	pref##_pop (TVec *vec, TEnt def) \
	{ \
		return vec->count ? vec->arr[--vec->count] : def; \
	} \
	int \
	pref##_popn (TVec *vec, TEnt *ent, size_t nent) \
	{ \
		ssize_t diff = (ssize_t)vec->count - (ssize_t)nent; \
		if (diff < 0) { return -ERANGE; } \
		memcpy (ent, vec->arr+diff, sizeof (*ent) * nent); \
		vec->count -= nent; \
		return pref##_resize (vec, vec->count); \
	} \
	TEnt \
	pref##_shift (TVec *vec, TEnt def) \
	{ \
		if (vec->count) { \
			def = vec->arr[0]; \
			vec->count--; \
			memmove (vec->arr, vec->arr+1, sizeof (def) * vec->count); \
		} \
		return def; \
	} \
	int \
	pref##_shiftn (TVec *vec, TEnt *ent, size_t nent) \
	{ \
		if (nent > vec->count) { return -ERANGE; } \
		vec->count -= nent; \
		memcpy (ent, vec->arr, sizeof (*ent) * nent); \
		memmove (vec->arr, vec->arr+nent, sizeof (*ent) * vec->count); \
		return pref##_resize (vec, vec->count); \
	} \
	int \
	pref##_splice (TVec *vec, ssize_t pos, ssize_t len, TEnt *ent, size_t nent) \
	{ \
		ssize_t count = (ssize_t)vec->count; \
		if (pos < 0) { \
			pos += count; \
			if (pos < 0) { return -ERANGE; }; \
		} \
		if (len < 0) { \
			len = (len + count + 1) - pos; \
			if (len < 0) { return -ERANGE; } \
		} \
		if (pos+len > count) { return -ERANGE; } \
		ssize_t next = count + nent - len; \
		int rc = 0; \
		if (next > count) { \
			rc = pref##_resize (vec, next); \
			if (rc < 0) { return rc; } \
		} \
		memmove (vec->arr + pos + nent, vec->arr + pos + len, \
				sizeof (*ent) * count - pos - len); \
		memcpy (vec->arr + pos, ent, sizeof (*ent) * nent); \
		vec->count = next; \
		return rc; \
	} \
	int \
	pref##_insert (TVec *vec, size_t pos, TEnt *ent, size_t nent) \
	{ \
		return pref##_splice (vec, pos, 0, ent, nent); \
	} \
	int \
	pref##_remove (TVec *vec, size_t pos, size_t len) \
	{ \
		return pref##_splice (vec, pos, len, NULL, 0); \
	} \
	void \
	pref##_reverse (TVec *vec) \
	{ \
		TEnt *arr = vec->arr; \
		size_t count = vec->count, n = count/2; \
		for (size_t a = 0, b = count-1; a < n; a++, b--) { \
			TEnt tmp = arr[a]; \
			arr[a] = arr[b]; \
			arr[b] = tmp; \
		} \
	} \
	void \
	pref##_sort (TVec *vec, int (*compar)(const TEnt *, const TEnt *)) \
	{ \
		qsort (vec->arr, vec->count, sizeof (vec->arr[0]), \
			(int (*)(const void *, const void *))compar); \
	} \
	TEnt * \
	pref##_search (TVec *vec, const TEnt *e, int (*compar)(const TEnt *, const TEnt *)) \
	{ \
		TEnt *p = vec->arr, *pe = p + vec->count; \
		for (; p < pe; p++) { \
			if (compar (p, e) == 0) { return p; } \
		} \
		return NULL; \
	} \
	TEnt * \
	pref##_bsearch (TVec *vec, const TEnt *e, int (*compar)(const TEnt *, const TEnt *)) \
	{ \
		TEnt *p = vec->arr; \
		size_t n = vec->count; \
		while (n > 0) { \
			TEnt *at = p + n/2; \
			int sign = compar (e, at); \
			if (sign == 0)     { return at; } \
			else if (n == 1)   { break; } \
			else if (sign < 0) { n /= 2; } \
			else               { p = at; n -= n/2; } \
		} \
		return NULL; \
	} \
	ssize_t \
	pref##_index (const TVec *vec, const TEnt *e) \
	{ \
		TEnt *p = vec->arr; \
		if (e >= p && e < p + vec->count) { return e - p; } \
		return -1; \
	} \
	void \
	pref##_print (const TVec *vec, FILE *out, void (*fn)(TEnt *, FILE *out)) \
	{ \
		if (out == NULL) { out == stdout; } \
		fprintf (out, "<crux:vec:" #pref "(" #TEnt "):"); \
		if (vec == NULL) { fprintf (out, "(null)>\n"); } \
		else { \
			fprintf (out, "%p count=%zu size=%zu>", \
					(void *)vec, vec->count, vec->size); \
			if (fn) { \
				fprintf (out, " {\n"); \
				size_t n = vec->count; \
				for (size_t i = 0; i < n; i++) { \
					fprintf (out, "    %zu = ", i); \
					fn (&vec->arr[i], out); \
					fprintf (out, "\n"); \
				} \
				fprintf (out, "}\n"); \
			} \
			else { \
				fprintf (out, "\n"); \
			} \
		} \
	} \

#endif
