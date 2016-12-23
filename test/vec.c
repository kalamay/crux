#include "mu.h"
#include "../include/crux/vec.h"

struct intvec {
	XVEC (int);
};

static int
cmp (const int *a, const int *b)
{
	return *a - *b;
}

XVEC_STATIC (intvec, struct intvec, int)

struct str {
	XVEC (char);
};

XVEC_STATIC_EXT (str, struct str, char, 1)

static void
test_resize (void)
{
	struct intvec vec = XVEC_INIT;

	mu_assert_int_eq (vec.count, 0);
	mu_assert_int_eq (vec.size, 0);
	mu_assert_int_eq (intvec_resize (&vec, 5), 1);
	mu_assert_int_eq (vec.count, 0);
	mu_assert_int_ge (vec.size, 5);

	intvec_final (&vec);
}

static void
test_push (void)
{
	struct intvec vec = XVEC_INIT;
	mu_assert_uint_eq (vec.count, 0);

	intvec_push (&vec, 1);
	mu_assert_uint_eq (vec.count, 1);

	intvec_push (&vec, 2);
	mu_assert_uint_eq (vec.count, 2);

	intvec_push (&vec, 3);
	mu_assert_uint_eq (vec.count, 3);

	int vals[] = { 10, 11, 12 };
	intvec_pushn (&vec, vals, xlen (vals));
	mu_assert_uint_eq (vec.count, 6);

	mu_assert_int_eq (vec.arr[0], 1);
	mu_assert_int_eq (vec.arr[1], 2);
	mu_assert_int_eq (vec.arr[2], 3);
	mu_assert_int_eq (vec.arr[3], 10);
	mu_assert_int_eq (vec.arr[4], 11);
	mu_assert_int_eq (vec.arr[5], 12);

	intvec_final (&vec);
	mu_assert_uint_eq (vec.count, 0);
}

static void
test_grow (void)
{
	struct intvec vec = XVEC_INIT;

	int grows = 0;
	for (int i = 1; i < 100; i++) {
		size_t before = vec.size;
		intvec_push (&vec, i);
		size_t after = vec.size;
		if (after > before) grows++;
	}

	mu_assert_uint_eq (vec.count, 99);
	mu_assert_int_eq (grows, 5);

	for (int i = 1; i < 100; i++) {
		mu_assert_int_eq (vec.arr[i-1], i);
	}

	intvec_final (&vec);
	mu_assert_uint_eq (vec.count, 0);
}

static void
test_pop (void)
{
	struct intvec vec = XVEC_INIT;

	int vals[] = { 1, 2, 3 };
	intvec_pushn (&vec, vals, xlen (vals));

	mu_assert_int_eq (intvec_pop (&vec, -1), 3);
	mu_assert_int_eq (intvec_pop (&vec, -1), 2);
	mu_assert_int_eq (intvec_pop (&vec, -1), 1);
	mu_assert_int_eq (intvec_pop (&vec, -1), -1);

	mu_assert_uint_eq (vec.count, 0);
	intvec_final (&vec);
	mu_assert_uint_eq (vec.count, 0);
}

static void
test_popn (void)
{
	struct intvec vec = XVEC_INIT;

	int vals[] = { 1, 2, 3, 4 };
	intvec_pushn (&vec, vals, xlen (vals));

	int out[3], rc;

	rc = intvec_popn (&vec, out, xlen (out));
	mu_assert_int_eq (rc, 0);
	mu_assert_int_eq (out[0], 2);
	mu_assert_int_eq (out[1], 3);
	mu_assert_int_eq (out[2], 4);
	mu_assert_uint_eq (vec.count, 1);

	rc = intvec_popn (&vec, out, xlen (out));
	mu_assert_int_eq (rc, -ERANGE);

	intvec_final (&vec);
	mu_assert_uint_eq (vec.count, 0);
}

static void
test_shift (void)
{
	struct intvec vec = XVEC_INIT;

	int vals[] = { 1, 2, 3 };
	intvec_pushn (&vec, vals, xlen (vals));

	int val;

	val = intvec_shift (&vec, -1);
	mu_assert_int_eq (val, 1);
	mu_assert_int_eq (vec.arr[0], 2);
	mu_assert_int_eq (vec.arr[1], 3);

	val = intvec_shift (&vec, -1);
	mu_assert_int_eq (val, 2);
	mu_assert_int_eq (vec.arr[0], 3);

	val = intvec_shift (&vec, -1);
	mu_assert_int_eq (val, 3);

	val = intvec_shift (&vec, -1);
	mu_assert_int_eq (val, -1);

	mu_assert_uint_eq (vec.count, 0);
	intvec_final (&vec);
	mu_assert_uint_eq (vec.count, 0);
}

static void
test_shiftn (void)
{
	struct intvec vec = XVEC_INIT;

	int vals[] = { 1, 2, 3, 4 };
	intvec_pushn (&vec, vals, xlen (vals));

	int out[3], rc;

	rc = intvec_shiftn (&vec, out, xlen (out));
	mu_assert_int_eq (rc, 0);
	mu_assert_int_eq (out[0], 1);
	mu_assert_int_eq (out[1], 2);
	mu_assert_int_eq (out[2], 3);
	mu_assert_uint_eq (vec.count, 1);

	rc = intvec_shiftn (&vec, out, xlen (out));
	mu_assert_int_eq (rc, -ERANGE);

	intvec_final (&vec);
	mu_assert_uint_eq (vec.count, 0);
}

static void
test_splice (void)
{
	int vals[] = { 1, 2, 3 };
	struct intvec vec = XVEC_INIT;

	intvec_splice (&vec, 0, 0, vals, xlen (vals));

	mu_assert_int_eq (vec.count, 3);
	mu_assert_int_eq (vec.arr[0], 1);
	mu_assert_int_eq (vec.arr[1], 2);
	mu_assert_int_eq (vec.arr[2], 3);

	intvec_final (&vec);
}

static void
test_splice_offset (void)
{
	int vals[] = { 1, 2, 3 };
	struct intvec vec = XVEC_INIT;

	int rc = intvec_splice (&vec, 2, 0, vals, xlen (vals));

	mu_assert_int_eq (rc, -ERANGE);

	intvec_final (&vec);
}

static void
test_splice_remove (void)
{
	int first[] = { 1, 2, 3, 4 };
	int second[] = { 10, 20, 30 };
	struct intvec vec = XVEC_INIT;

	int rc = intvec_splice (&vec, 0, -1, NULL, 0);
	mu_assert_int_eq (vec.count, 0);

	intvec_splice (&vec, 0, 0, first, xlen (first));
	intvec_splice (&vec, 0, 2, second, xlen (second));

	mu_assert_int_eq (vec.arr[0], 10);
	mu_assert_int_eq (vec.arr[1], 20);
	mu_assert_int_eq (vec.arr[2], 30);
	mu_assert_int_eq (vec.arr[3], 3);
	mu_assert_int_eq (vec.arr[4], 4);

	rc = intvec_splice (&vec, -3, -1, first, 0);
	mu_assert_int_eq (rc, 0);
	mu_assert_uint_eq (vec.count, 2);
	mu_assert_int_eq (vec.arr[0], 10);
	mu_assert_int_eq (vec.arr[1], 20);

	rc = intvec_splice (&vec, 0, -1, first, 0);
	mu_assert_int_eq (rc, 0);
	mu_assert_uint_eq (vec.count, 0);

	intvec_final (&vec);
}

static void
test_splice_remove_offset (void)
{
	int first[] = { 1, 2, 3, 4 };
	int second[] = { 10, 20, 30 };
	struct intvec vec = XVEC_INIT;

	intvec_splice (&vec, 0, 0, first, xlen (first));
	intvec_splice (&vec, 1, 2, second, xlen (second));

	mu_assert_int_eq (vec.arr[0], 1);
	mu_assert_int_eq (vec.arr[1], 10);
	mu_assert_int_eq (vec.arr[2], 20);
	mu_assert_int_eq (vec.arr[3], 30);
	mu_assert_int_eq (vec.arr[4], 4);

	intvec_final (&vec);
}

static void
test_splice_remove_end (void)
{
	int first[] = { 1, 2, 3, 4 };
	int second[] = { 10, 20, 30 };
	struct intvec vec = XVEC_INIT;

	intvec_splice (&vec, 0, 0, first, xlen (first));
	intvec_splice (&vec, -2, -1, second, xlen (second));

	mu_assert_int_eq (vec.arr[0], 1);
	mu_assert_int_eq (vec.arr[1], 2);
	mu_assert_int_eq (vec.arr[2], 10);
	mu_assert_int_eq (vec.arr[3], 20);
	mu_assert_int_eq (vec.arr[4], 30);

	intvec_final (&vec);
}

static void
test_splice_after (void)
{
	int first[] = { 1, 2, 3, 4 };
	int second[] = { 10, 20, 30 };
	struct intvec vec = XVEC_INIT;

	intvec_splice (&vec, 0, 0, first, xlen (first));
	int rc = intvec_splice (&vec, 8, 2, second, xlen (second));

	mu_assert_int_eq (rc, -ERANGE);

	intvec_final (&vec);
}

static void
test_reverse (void)
{
	struct intvec vec = XVEC_INIT;

	intvec_reverse (&vec);
	mu_assert_uint_eq (vec.count, 0);

	int vals[] = { 1, 2, 3, 4, 5 };
	intvec_pushn (&vec, vals, xlen (vals));
	mu_assert_uint_eq (vec.count, 5);

	intvec_reverse (&vec);
	mu_assert_int_eq (vec.arr[0], 5);
	mu_assert_int_eq (vec.arr[1], 4);
	mu_assert_int_eq (vec.arr[2], 3);
	mu_assert_int_eq (vec.arr[3], 2);
	mu_assert_int_eq (vec.arr[4], 1);
	mu_assert_uint_eq (vec.count, 5);

	intvec_final (&vec);
}

static void
test_bsearch (void)
{
	struct intvec vec = XVEC_INIT;

	int vals[] = { 1, 2, 3, 4, 5 };
	intvec_pushn (&vec, vals, xlen (vals));
	mu_assert_uint_eq (vec.count, 5);

	int find, *match;

	find = 1;
	match = intvec_bsearch (&vec, &find, cmp);
	mu_assert_ptr_eq (match, vec.arr + 0);
	mu_assert_int_eq (intvec_index (&vec, match), 0);

	find = 3;
	match = intvec_bsearch (&vec, &find, cmp);
	mu_assert_ptr_eq (match, vec.arr + 2);
	mu_assert_int_eq (intvec_index (&vec, match), 2);

	find = 5;
	match = intvec_bsearch (&vec, &find, cmp);
	mu_assert_ptr_eq (match, vec.arr + 4);
	mu_assert_int_eq (intvec_index (&vec, match), 4);

	find = 0;
	match = intvec_bsearch (&vec, &find, cmp);
	mu_assert_ptr_eq (match, NULL);
	mu_assert_int_eq (intvec_index (&vec, match), -1);

	find = 6;
	match = intvec_bsearch (&vec, &find, cmp);
	mu_assert_ptr_eq (match, NULL);
	mu_assert_int_eq (intvec_index (&vec, match), -1);

	intvec_final (&vec);
}

static void
test_search (void)
{
	struct intvec vec = XVEC_INIT;

	int vals[] = { 2, 5, 1, 4, 3 };
	intvec_pushn (&vec, vals, xlen (vals));
	mu_assert_uint_eq (vec.count, 5);

	int find, *match;

	find = 1;
	match = intvec_search (&vec, &find, cmp);
	mu_assert_ptr_eq (match, vec.arr + 2);
	mu_assert_int_eq (intvec_index (&vec, match), 2);

	find = 3;
	match = intvec_search (&vec, &find, cmp);
	mu_assert_ptr_eq (match, vec.arr + 4);
	mu_assert_int_eq (intvec_index (&vec, match), 4);

	find = 5;
	match = intvec_search (&vec, &find, cmp);
	mu_assert_ptr_eq (match, vec.arr + 1);
	mu_assert_int_eq (intvec_index (&vec, match), 1);

	find = 0;
	match = intvec_search (&vec, &find, cmp);
	mu_assert_ptr_eq (match, NULL);
	mu_assert_int_eq (intvec_index (&vec, match), -1);

	find = 6;
	match = intvec_search (&vec, &find, cmp);
	mu_assert_ptr_eq (match, NULL);
	mu_assert_int_eq (intvec_index (&vec, match), -1);

	intvec_final (&vec);
}

static void
test_sort (void)
{
	struct intvec vec = XVEC_INIT;

	int vals[] = { 2, 5, 1, 4, 3 };
	intvec_pushn (&vec, vals, xlen (vals));
	mu_assert_uint_eq (vec.count, 5);

	intvec_sort (&vec, cmp);

	mu_assert_int_eq (vec.count, 5);
	mu_assert_int_eq (vec.arr[0], 1);
	mu_assert_int_eq (vec.arr[1], 2);
	mu_assert_int_eq (vec.arr[2], 3);
	mu_assert_int_eq (vec.arr[3], 4);
	mu_assert_int_eq (vec.arr[4], 5);

	intvec_final (&vec);
}

static void
test_str (void)
{
	char buf[16];
	struct str str = XVEC_INIT;
	str_resize (&str, 4);
	memset (str.arr+1, 0xFF, str.size-1);

	str_push (&str, 't');
	mu_assert_int_eq (str.count, 1);
	mu_assert_int_eq (str.arr[1], '\0');
	mu_assert_str_eq (str.arr, "t");

	str_pushn (&str, "est", 3);
	mu_assert_int_eq (str.count, 4);
	mu_assert_int_eq (str.arr[4], '\0');
	mu_assert_str_eq (str.arr, "test");

	str_splice (&str, 2, 1, "ar", 2);
	mu_assert_int_eq (str.count, 5);
	mu_assert_int_eq (str.arr[5], '\0');
	mu_assert_str_eq (str.arr, "teart");

	str_shiftn (&str, buf, 2);
	mu_assert_int_eq (str.count, 3);
	mu_assert_int_eq (str.arr[3], '\0');
	mu_assert_str_eq (str.arr, "art");
	mu_assert_int_eq (buf[0], 't');
	mu_assert_int_eq (buf[1], 'e');

	str_insert (&str, 0, "ch", 2);
	mu_assert_int_eq (str.count, 5);
	mu_assert_int_eq (str.arr[5], '\0');
	mu_assert_str_eq (str.arr, "chart");

	str_reverse (&str);
	mu_assert_int_eq (str.count, 5);
	mu_assert_int_eq (str.arr[5], '\0');
	mu_assert_str_eq (str.arr, "trahc");

	str_popn (&str, buf, 2);
	mu_assert_int_eq (str.count, 3);
	mu_assert_int_eq (str.arr[3], '\0');
	mu_assert_str_eq (str.arr, "tra");
	mu_assert_int_eq (buf[0], 'h');
	mu_assert_int_eq (buf[1], 'c');

	str_pushn (&str, "ctors", 5);
	mu_assert_int_eq (str.count, 8);
	mu_assert_int_gt (str.size, 8);
	mu_assert_int_eq (str.arr[8], '\0');
	mu_assert_str_eq (str.arr, "tractors");
}

int
main (void)
{
	mu_init ("vec");

	test_resize ();
	test_push ();
	test_grow ();
	test_pop ();
	test_popn ();
	test_shift ();
	test_shiftn ();
	test_splice ();
	test_splice_offset ();
	test_splice_remove ();
	test_splice_remove_offset ();
	test_splice_remove_end ();
	test_splice_after ();
	test_reverse ();
	test_bsearch ();
	test_search ();
	test_sort ();
	test_str ();
}

