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
	int id;
	const char *name;
};

static void
print_thing(void *value, FILE *out)
{
	struct thing *t = value;
	fprintf(out, "{ id=%d, name=\"%s\" }", t->id, t->name);
}

static void
test_struct(void)
{
	struct thing stuff[5] = {
		{  8, "peanuts" },
		{ 12, "beans" },
		{  3, "carrots" },
		{  7, "pears" },
		{ 19, "mushrooms" },
	};

	struct xmap *map;
	mu_assert_int_eq(xmap_new(&map, 0.0, xlen(stuff)), 0);
	xmap_set_print(map, print_thing);

	for (size_t i = 0; i < xlen(stuff); i++) {
		struct thing *t = &stuff[i];
		mu_assert_int_eq(xmap_put(map, &stuff[i].id, sizeof(int), (void **)&t), XMAP_NEW_ENTRY);
	}

	for (size_t i = 0; i < xlen(stuff); i++) {
		struct thing *t = xmap_get(map, &stuff[i].id, sizeof(int));
		mu_assert_ptr_eq(t, &stuff[i]);
	}

	xmap_free(&map);
}

int
main(void)
{
	mu_init("map");

	test_basic();
	test_struct();
}

