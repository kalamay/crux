#ifndef CRUX_LIST_H
#define CRUX_LIST_H

#include <stdbool.h>
#include "def.h"

struct xlist {
	struct xlist *link[2];
};

enum xorder {
	X_DESCENDING = 0,
	X_ASCENDING = 1
};

__attribute__((unused)) static inline void
xlist_init (struct xlist *list)
{
	list->link[0] = list->link[1] = list;
}

__attribute__((unused)) static inline void
xlist_clear (struct xlist *entry)
{
	entry->link[0] = NULL;
	entry->link[1] = NULL;
}

__attribute__((unused)) static inline bool
xlist_is_empty (const struct xlist *list)
{
	return list->link[1] == list;
}

__attribute__((unused)) static inline bool
xlist_is_singular (const struct xlist *list)
{
	return !xlist_is_empty (list) && (list->link[0] == list->link[1]);
}

__attribute__((unused)) static inline bool
xlist_is_added (const struct xlist *entry)
{
	return entry->link[0] != NULL;
}

__attribute__((unused)) static inline bool
xlist_is_first (const struct xlist *list, const struct xlist *entry, enum xorder dir)
{
	return list->link[dir] == entry;
}

__attribute__((unused)) static inline struct xlist *
xlist_first (const struct xlist *list, enum xorder dir)
{
	return list->link[dir] == list ? NULL : list->link[dir];
}

__attribute__((unused)) static inline struct xlist *
xlist_next (const struct xlist *list, struct xlist *entry, enum xorder dir)
{
	return entry->link[dir] == list ? NULL : entry->link[dir];
}

__attribute__((unused)) static inline struct xlist *
xlist_get (const struct xlist *list, int idx, enum xorder dir)
{
	struct xlist *entry = xlist_first (list, dir);
	for (; idx > 0 && entry != NULL; idx--) {
		entry = xlist_next (list, entry, dir);
	}
	return entry;
}

__attribute__((unused)) static inline bool
xlist_has_next (const struct xlist *list, struct xlist *entry, enum xorder dir)
{
	return entry->link[dir] != list;
}

__attribute__((unused)) static inline void
xlist_add (struct xlist *list, struct xlist *entry, enum xorder dir)
{
	struct xlist *link[2];
	link[dir] = list;
	link[!dir] = list->link[!dir];
	link[1]->link[0] = entry;
	entry->link[1] = link[1];
	entry->link[0] = link[0];
	link[0]->link[1] = entry;
}

__attribute__((unused)) static inline void
xlist_del (struct xlist *entry)
{
	struct xlist *link[2] = { entry->link[0], entry->link[1] };
	link[0]->link[1] = link[1];
	link[1]->link[0] = link[0];
	xlist_clear (entry);
}

__attribute__((unused)) static inline struct xlist *
xlist_pop (struct xlist *list, enum xorder dir)
{
	struct xlist *entry = xlist_first (list, dir);
	if (entry != NULL) { xlist_del (entry); }
	return entry;
}

__attribute__((unused)) static inline void
xlist_copy_head (struct xlist *dst, struct xlist *src)
{
	dst->link[0] = src->link[0];
	dst->link[0]->link[1] = dst;
	dst->link[1] = src->link[1];
	dst->link[1]->link[0] = dst;
}

__attribute__((unused)) static inline void
xlist_replace (struct xlist *dst, struct xlist *src)
{
	xlist_copy_head (dst, src);
	xlist_init (src);
}

__attribute__((unused)) static inline void
xlist_swap (struct xlist *a, struct xlist *b)
{
	struct xlist tmp;
	xlist_copy_head (&tmp, a);
	xlist_copy_head (a, b);
	xlist_copy_head (b, &tmp);
}

__attribute__((unused)) static inline void
xlist_splice (struct xlist *list, struct xlist *other, enum xorder dir)
{
	if (!xlist_is_empty (other)) {
		struct xlist *link[2];
		link[!dir] = list->link[!dir];
		link[dir] = list;
		link[!dir]->link[dir] = other->link[dir];
		other->link[dir]->link[!dir] = link[!dir];
		other->link[!dir]->link[dir] = link[dir];
		link[dir]->link[!dir] = other->link[!dir];
		xlist_init (other);
	}
}

__attribute__((unused)) static inline void
xlist_insert (struct xlist *entry, struct xlist *before)
{
	before->link[0]->link[1] = entry;
	entry->link[0] = before->link[0];
	entry->link[1] = before;
	before->link[0] = entry;
}

#define xlist_each(list, var, dir) \
	for (struct xlist *__n = (var = (list)->link[(dir)], var->link[(dir)]); \
	     var != (list); \
	     var = __n, __n = __n->link[(dir)])

#endif

