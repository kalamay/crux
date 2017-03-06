#include "mu.h"
#include "../include/crux/hashtier.h"

struct thing {
	int key, value;
};

bool
thing_has_key(struct thing *t, int k, size_t kn)
{
	(void)kn;
	return t->key == k;
}

uint64_t
thing_hash(int k)
{
	return (uint32_t)k * UINT32_C(2654435761);
}

struct thing_tier {
	XHASHTIER(struct thing);
};

XHASHTIER_STATIC(thing, struct thing_tier, int)

static bool
verify_tier(struct thing_tier *tier)
{
	bool ok = true;
	size_t count = 0;
	for (size_t i = 0; i < tier->size; i++) {
		if (tier->arr[i].h == 0) { continue; }
		ssize_t idx = thing_get(tier, tier->arr[i].entry.key, 0, thing_hash(tier->arr[i].entry.key));
		mu_assert_int_eq(idx, i);
		if (idx != (ssize_t)i) {
			ok = false;
		}
		count++;
	}
	mu_assert_uint_eq(count, tier->count);
	return ok;
}

static void
test_tier(void)
{
	int full;
	struct thing_tier *tier = NULL, *next = NULL;
	thing_new(&tier, 10);
	thing_new(&next, 20);

	mu_assert_uint_eq(tier->size, 16);
	mu_assert_uint_eq(tier->mod, 13);

	mu_assert_uint_eq(next->size, 32);
	mu_assert_uint_eq(next->mod, 31);

	srand(7);

	for (int i = 0; i < 15; i++) {
		int key = rand();
		ssize_t idx = thing_reserve(tier, key, 0, thing_hash(key), &full);
		if (idx >= 0) {
			tier->arr[idx].entry.key = key;
			tier->arr[idx].entry.value = rand();
		}
	}

	mu_assert_uint_eq(tier->count, 15);
	mu_assert_uint_eq(next->count, 0);

	mu_assert(verify_tier(tier));
	mu_assert(verify_tier(next));

	mu_assert_uint_eq(thing_nremap(tier, next, 5), 5);

	mu_assert_uint_eq(tier->count, 10);
	mu_assert_uint_eq(next->count, 5);

	mu_assert(verify_tier(tier));
	mu_assert(verify_tier(next));

	mu_assert_uint_eq(thing_nremap(tier, next, 5), 5);

	mu_assert_uint_eq(tier->count, 5);
	mu_assert_uint_eq(next->count, 10);

	mu_assert(verify_tier(tier));
	mu_assert(verify_tier(next));

	mu_assert_uint_eq(thing_remap(tier, next), 5);

	mu_assert_uint_eq(tier->count, 0);
	mu_assert_uint_eq(next->count, 15);

	mu_assert(verify_tier(tier));
	mu_assert(verify_tier(next));

	srand(7);

	for (int i = 0; i < 15; i++) {
		int key = rand();
		ssize_t idx = thing_get(next, key, 0, thing_hash(key));
		mu_assert_int_ge(idx, 0);
		if (idx >= 0) {
			mu_assert_int_eq(next->arr[idx].entry.key, key);
			mu_assert_int_eq(next->arr[idx].entry.value, rand());
		}
	}

	free(tier);
	free(next);
}

int
main(void)
{
	mu_init("hashtier");
	mu_run(test_tier);
	return 0;
}

