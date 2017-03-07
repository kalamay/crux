#include "mu.h"
#include "../include/crux/map.h"

static void
print_str(void *value, FILE *out)
{
	fprintf(out, "\"%s\"", (char *)value);
}

static void
test_basic(void)
{
	struct xmap *map;
	mu_assert_int_eq(xmap_new(&map, 0.0, 100), 0);
	xmap_set_print(map, print_str);

	char *value;

	value = "old";
	mu_assert_int_eq(xmap_put(map, "x", 1, (void **)&value), XMAP_NEW_ENTRY);
	mu_assert_str_eq(xmap_get(map, "x", 1), "old");

	value = "new";
	mu_assert_int_eq(xmap_put(map, "x", 1, (void **)&value), XMAP_UPDATE_ENTRY);
	mu_assert_str_eq(value, "old");
	mu_assert_str_eq(xmap_get(map, "x", 1), "new");

	//xmap_print(map, stdout);
	mu_assert_int_eq(xmap_count(map), 1);
	mu_assert_int_eq(xmap_resize(map, xmap_count(map)), 1);
	//xmap_print(map, stdout);
	mu_assert_str_eq(xmap_get(map, "x", 1), "new");
	//xmap_print(map, stdout);

	xmap_free(&map);
}

struct thing {
	int key, value;
};

static void
print_thing(void *value, FILE *out)
{
	struct thing *t = value;
	fprintf(out, "<thing key=%d, value=%d>", t->key, t->value);
}

static void
test_struct(void)
{
	struct thing stuff[5] = {
		{  8, 100 },
		{ 12, 101 },
		{  3, 102 },
		{  7, 103 },
		{ 19, 104 },
	};

	struct xmap *map;
	mu_assert_int_eq(xmap_new(&map, 0.0, xlen(stuff)), 0);
	xmap_set_print(map, print_thing);

	for (size_t i = 0; i < xlen(stuff); i++) {
		struct thing *t = &stuff[i];
		mu_assert_int_eq(xmap_put(map, &stuff[i].key, sizeof(int), (void **)&t), XMAP_NEW_ENTRY);
	}

	for (size_t i = 0; i < xlen(stuff); i++) {
		struct thing *t = xmap_get(map, &stuff[i].key, sizeof(int));
		mu_assert_ptr_eq(t, &stuff[i]);
	}

	xmap_free(&map);
}

static void
test_large(void)
{
	struct thing *things;
	unsigned seed = 0;

	things = malloc(sizeof(*things) * 1 << 20);
	mu_assert_ptr_ne(things, NULL);

	for (int i = 0; i < 1 << 20; i++) {
		struct thing *t = &things[i];
		t->key = rand_r(&seed);
		t->value = rand_r(&seed);
	}

	struct xmap *map;
	mu_assert_int_eq(xmap_new(&map, 0.0, 0), 0);
	xmap_set_print(map, print_thing);

	for (int i = 0; i < 1 << 20; i++) {
		struct thing *t = &things[i];
		xmap_put(map, &t->key, sizeof(int), (void **)&t);
	}

	seed = 0;
	for (int i = 0; i < 1 << 20; i++) {
		int k = rand_r(&seed);
		int v = rand_r(&seed);
		struct thing *t = xmap_get(map, &k, sizeof(k));
		mu_assert_ptr_ne(t, NULL);
		mu_assert_int_eq(t->key, k);
		mu_assert_int_eq(t->value, v);
	}

	seed = 0;
	for (int i = 0; i < 1 << 20; i++) {
		int k = rand_r(&seed);
		int v = rand_r(&seed);
		struct thing *t = xmap_get(map, &k, sizeof(k));
		mu_assert_ptr_ne(t, NULL);
		mu_assert_int_eq(t->key, k);
		mu_assert_int_eq(t->value, v);
	}

	mu_assert_int_eq(xmap_count(map), 1 << 20);
}

int
main(void)
{
	mu_init("map");

	test_basic();
	test_struct();
	test_large();
}

