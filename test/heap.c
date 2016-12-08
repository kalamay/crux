#include "mu.h"
#include "../include/crux/heap.h"

typedef struct {
	struct xheap_entry entry;
	const char *value;
} Task;

static void
test_basic (void)
{
	unsigned i;

	Task tasks[] = {
		{ { 2, 0, 0 }, "Tax return" },
		{ { 1, 0, 0 }, "Solve RC tasks" },
		{ { 3, 0, 0 }, "Clear drains" },
		{ { 2, 0, 0 }, "Other 2" },
		{ { 4, 0, 0 }, "Feed cat" },
		{ { 5, 0, 0 }, "Make tea" },
	};

	Task *expect[] = {
		&tasks[1],
		&tasks[0],
		&tasks[3],
		&tasks[2],
		&tasks[4],
		&tasks[5],
	};

	struct xheap *heap;
	struct xheap_entry *e;

	mu_assert_int_eq (xheap_new (&heap), 0);
	mu_assert_ptr_eq (xheap_get (heap, XHEAP_ROOT), NULL);

	for (i = 0; i < 6; i++) {
		mu_assert_uint_eq (xheap_count (heap), i);
		mu_assert_int_eq (xheap_add (heap, &tasks[i].entry), 0);
	}
	mu_assert_uint_eq (xheap_count (heap), 6);

	i = 0;
	while ((e = xheap_get (heap, XHEAP_ROOT)) != NULL) {
		Task *task = (Task *)e;
		mu_assert_ptr_eq (task, expect[i]);
		mu_assert_uint_eq (e->key, XHEAP_ROOT);
		xheap_remove (heap, e);
		i++;
	}
	mu_assert_uint_eq (xheap_count (heap), 0);

	xheap_free (&heap);
}

static void
free_entry (struct xheap_entry *val, void *data)
{
	(void)data;
	free (val);
}

static void
test_large (void)
{
	unsigned i;
	int prev;
	struct xheap *heap;
	struct xheap_entry *e;

	mu_assert_int_eq (xheap_new (&heap), 0);

	for (i = 0; i < 1 << 18; i++) {
		e = malloc (sizeof *e);
		e->prio = rand ();
		mu_assert_int_eq (xheap_add (heap, e), 0);
	}

	mu_assert_uint_eq (xheap_count (heap), 1 << 18);
	
	prev = -1;
	while ((e = xheap_get (heap, XHEAP_ROOT)) != NULL) {
		mu_assert_int_le (prev, e->prio);
		prev = e->prio;
		mu_assert_int_eq (xheap_remove (heap, e), 0);
		free (e);
	}

	for (i = 0; i < 1 << 18; i++) {
		e = malloc (sizeof *e);
		e->prio = rand ();
		mu_assert_int_eq (xheap_add (heap, e), 0);
	}

	xheap_clear (heap, free_entry, NULL);
	mu_assert_uint_eq (xheap_count (heap), 0);
	xheap_free (&heap);
}

int
main (void)
{
	mu_init ("heap");
	mu_run (test_basic);
	mu_run (test_large);
}

