#include "mu.h"
#include "../include/crux/rand.h"

struct round
{
	uint64_t values[6];
	char coins[65];
	char rolls[33];
	char cards[52][3];
};

const struct round rounds32[5] = {
	{
		{ 0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e },
		"HHTTTHTHHHTHTTTHHHHHTTTHHHTHTHTHTTHTTTHHHHHHTTTTHHTTTTTHTTTTTTTHT",
		{3,4,1,1,2,2,3,2,4,3,2,4,3,3,5,2,3,1,3,1,5,1,4,1,5,6,4,6,6,2,6,3,3},
		{
			"Qd","Ks","6d","3s","3d","4c","3h","Td","Kc","5c","Jh","Kd","Jd",
			"As","4s","4h","Ad","Th","Ac","Jc","7s","Qs","2s","7h","Kh","2d",
			"6c","Ah","4d","Qh","9h","6s","5s","2c","9c","Ts","8d","9s","3c",
			"8c","Js","5d","2h","6h","7d","8s","9d","5h","8h","Qc","7c","Tc",
		}
	},
	{
		{ 0x74ab93ad, 0x1c1da000, 0x494ff896, 0x34462f2f, 0xd308a3e5, 0x0fa83bab },
		"HHHHHHHHHHTHHHTHTHTHTHTTTTHHTTTHHTHHTHTTHHTTTHHHHHHTHTTHTHTTTTTTT",
		{5,1,1,3,3,2,4,5,3,2,2,6,4,3,2,4,2,4,3,2,3,6,3,2,3,4,2,4,1,1,5,4,4},
		{
			"7d","2s","7h","Td","8s","3c","3d","Js","2d","Tc","4h","Qs","5c",
			"9c","Th","2c","Jc","Qd","9d","Qc","7s","3s","5s","6h","4d","Jh",
			"4c","Ac","4s","5h","5d","Kc","8h","8d","Jd","9s","Ad","6s","6c",
			"Kd","2h","3h","Kh","Ts","Qh","9h","6d","As","7c","Ks","Ah","8c",
		}
	},
	{
		{ 0x39af5f9f, 0x04196b18, 0xc3c3eb28, 0xc076c60c, 0xc693e135, 0xf8f63932 },
		"HTTHHTTTTTHTTHHHTHTTHHTTHTHHTHTHTTTTHHTTTHHTHHTTHTTHHHTHHHTHTTTHT",
		{5,1,5,3,2,2,4,5,3,3,1,3,4,6,3,2,3,4,2,2,3,1,5,2,4,6,6,4,2,4,3,3,6},
		{
			"Kd","Jh","Kc","Qh","4d","Qc","4h","9d","3c","Kh","Qs","8h","5c",
			"Jd","7d","8d","3h","7c","8s","3s","2h","Ks","9c","9h","2c","8c",
			"Ad","7s","4s","2s","5h","6s","4c","Ah","7h","5s","Ac","3d","5d",
			"Qd","As","Tc","6h","9s","2d","6c","6d","Td","Jc","Ts","Th","Js",
		}
	},
	{
		{ 0x55ce6851, 0x97a7726d, 0x17e10815, 0x58007d43, 0x962fb148, 0xb9bb55bd },
		"HHTHHTTTTHTHHHHHTTHHHTTTHHTHTHTHTHHTTHTHHHHHHTHHTHHTHHTTTTHHTHHTT",
		{6,6,3,2,3,4,2,6,4,2,6,3,2,3,5,5,3,4,4,6,6,2,6,5,4,4,6,1,6,1,3,6,5},
		{
			"Qd","8h","5d","8s","8d","Ts","7h","Th","Qs","Js","7s","Kc","6h",
			"5s","4d","Ac","Jd","7d","7c","Td","2c","6s","5h","6d","3s","Kd",
			"9s","Jh","Kh","As","Ah","9h","3c","Qh","9c","2d","Tc","9d","2s",
			"3d","Ks","4h","Qc","Ad","Jc","8c","2h","3h","4s","4c","5c","6c",
		}
	},
	{
		{ 0xfcef7cd6, 0x1b488b5a, 0xd0daf7ea, 0x1d9a70f7, 0x241a37cf, 0x9a3857b7 },
		"HHHHTHHTTHTTHHHTTTHHTHTHTTTTHTTHTHTTTHHHTHTHTTHTTHTHHTHTHHHTHTHTT",
		{5,4,1,2,6,1,3,1,5,6,3,6,2,1,4,4,5,2,1,5,6,5,6,4,4,4,5,2,6,4,3,5,6},
		{
			"4d","9s","Qc","9h","As","Qs","7s","4c","Kd","6h","6s","2c","8c",
			"5d","7h","5h","Jc","3s","7c","Jh","Js","Ks","Tc","Jd","Kc","Th",
			"3h","Ts","Qh","Ad","Td","3c","Ah","2d","3d","5c","Ac","8s","5s",
			"9c","2h","6c","6d","Kh","Qd","8d","7d","2s","8h","4h","9d","4s",
		}
	},
};

const struct round rounds64[5] = {
	{
		{ 0x86b1da1d72062b68, 0x1304aa46c9853d39, 0xa3670e9e0dd50358,
			0xf9090e529a7dae00, 0xc85b9fd837996f2c, 0x606121f8e3919196 },
		"TTTHHHTTTHHHTTTTHHTTHHTHTHTTHHTHTTTTHHTTTHTHHTHTTTTHHTTTHHHTTTHTT",
		{6,4,1,5,1,5,5,3,6,3,4,6,2,3,6,5,5,5,1,5,3,6,2,6,1,4,4,3,5,2,6,3,2},
		{
			"3d","7d","3h","Qd","9d","8c","Ts","Ad","9s","6c","Jh","Ac","5s",
			"4c","2c","7s","Kh","Kd","7h","Qh","6d","Qc","8d","Qs","6s","Js",
			"4d","Kc","9h","3c","2h","Td","5d","5h","9c","4s","5c","7c","3s",
			"4h","As","Th","6h","Jc","2s","Jd","Tc","Ah","2d","Ks","8h","8s",
		}
	}, {
		{ 0x1773ba241e7a792a, 0xe41aed7117b0bc10, 0x36bac8d9432af525,
			0xe0c78e2f3c850a38, 0xe3ad939c1c7ce70d, 0xa302fdced8c79e93 },
		"TTTTHTHTHHTHTHTTTTTHHTTHHHHTHTHHHHHHHTHHHTHHTHTTTHHHHTTHHTTTHTHTH",
		{6,1,1,5,4,1,5,6,3,2,4,2,2,4,6,2,1,5,2,6,2,3,1,5,1,1,5,4,4,2,3,6,3},
		{
			"As","2h","4d","7d","Ad","Qc","9s","7h","Kh","Jc","7c","3d","8c",
			"Th","9c","Qd","9h","Td","6d","8d","Qs","5c","6s","8s","Ac","Kd",
			"2d","3h","Qh","Tc","Jh","Ah","3s","4h","9d","8h","Jd","4s","2s",
			"Ts","5s","Kc","4c","5d","3c","6h","2c","6c","7s","Js","5h","Ks",
		},
	}, {
		{ 0xc96006593aed3b62, 0xf04d5afa3f197bf1, 0xce6f729cc913a50f,
			0x98b5fc4fbb1e4aea, 0x802dce1b410fc8c3, 0xe3bac0a14f6e5033, },
		"HTTHTHTTTTTHTTTHHTHTHHTHHHHHHHHHTTTHTHTHTHHTTTTTTHHHHTHTTTTHHHHHH",
		{5,6,4,3,3,1,4,5,2,3,2,1,1,3,2,3,4,5,4,6,4,3,6,2,2,6,3,2,2,4,5,2,5},
		{
			"5c","5d","9d","4s","Qs","Kh","2c","3h","Ac","2s","7s","4c","6s",
			"8h","9c","6d","2h","4d","3c","5h","6h","Ad","7c","Js","Jd","6c",
			"2d","3d","4h","Kd","9s","Th","Kc","7h","8s","Tc","Qc","Qd","Jh",
			"Ks","8d","Ts","Ah","Jc","5s","As","Qh","8c","3s","Td","7d","9h",
		}
	}, {
		{ 0x68da679de81de48a, 0x7ee3c031fa0aa440, 0x6eb1663983530403,
			0xfec4d7a9a7aec823, 0xbce221c255ee9467, 0x460a42a962b8a2f9 },
		"HHHTTTTHHHHHTTTTTTTHHHTHHHHTTHTTTHTTTTHTHHHHTHHTTTHHHTHHTTHHHTHTH",
		{3,5,6,3,6,4,5,6,5,6,1,1,6,6,5,5,5,1,6,4,6,4,5,1,1,4,4,4,3,5,6,1,6},
		{
			"7c","Kh","2d","Qc","Jh","Js","Kc","Ks","Kd","3d","8d","4s","Jc",
			"8c","9d","5c","9c","Qh","As","Qd","3s","Ac","3h","3c","Ad","9h",
			"6h","Th","Jd","5s","6s","7h","7d","7s","2c","2h","2s","6d","8h",
			"4d","Ts","Tc","4h","5h","4c","Ah","9s","Td","8s","5d","6c","Qs",
		}
	}, {
		{ 0x9e0d084cff42fe2f, 0x63cd8347aae338ea, 0x112aae00540d3fa1,
			0x53968bc829afd6ec, 0x1b9900eb6c5b6d90, 0xe89ed17ea33cb420 },
		"HTTTTTHTHTHHHTHTTTHTHHTHHTHTTTHHTTHHHTTTTHTTHHTHHTHHHTTHHTHTHHHHH",
		{6,6,5,1,1,4,5,5,3,1,2,6,5,2,4,6,4,2,6,4,4,3,2,5,3,3,6,5,3,4,5,1,2},
		{
			"Jd","Qh","8s","9h","Kh","3c","Ts","Th","Kc","Kd","4s","Ah","5h",
			"4d","Jc","7d","9c","Ac","8c","Ks","6s","2d","Td","Qc","2s","8h",
			"Tc","6c","3d","3h","4h","6h","7s","Qs","As","5d","3s","5c","6d",
			"4c","Js","5s","8d","9d","2c","9s","7h","Qd","Jh","Ad","2h","7c",
		}
	}
};

static void
test_32(void)
{
	struct xrand32 rng;
	xrand32_seed(&rng, 42, 54);
	for (size_t round = 0; round < xlen(rounds32); round++) {
		for (size_t i = 0; i < xlen(rounds32[i].values); i++) {
			mu_assert_uint_eq(xrand32(&rng), rounds32[round].values[i]);
		}
		for (size_t i = 0; i < xlen(rounds32[i].coins); i++) {
			char c = xrand32_bound(&rng, 2) ? 'H' : 'T';
			mu_assert_char_eq(c, rounds32[round].coins[i]);
		}
		for (size_t i = 0; i < xlen(rounds32[i].rolls); i++) {
			mu_assert_uint_eq(xrand32_bound(&rng, 6)+1, rounds32[round].rolls[i]);
		}

		enum { SUITS = 4, NUMBERS = 13, CARDS = 52 };
		char cards[CARDS];

		for (int i = 0; i < CARDS; ++i) {
			cards[i] = i;
		}

		for (int i = CARDS; i > 1; --i) {
			int chosen = xrand32_bound(&rng, i);
			char card     = cards[chosen];
			cards[chosen] = cards[i-1];
			cards[i-1]  = card;
		}

		static const char number[] = {'A', '2', '3', '4', '5', '6', '7',
			'8', '9', 'T', 'J', 'Q', 'K'};
		static const char suit[] = {'h', 'c', 'd', 's'};
		for (int i = 0; i < CARDS; ++i) {
			char card[4];
			snprintf(card, sizeof(card), "%c%c", number[cards[i] / SUITS], suit[cards[i] % SUITS]);
			mu_assert_str_eq(card, rounds32[round].cards[i]);
		}
	}
}

static void
test_64(void)
{
	struct xrand64 rng;
	xrand64_seed(&rng, 42, 54);
	for (size_t round = 0; round < xlen(rounds64); round++) {
		for (size_t i = 0; i < xlen(rounds64[i].values); i++) {
			mu_assert_uint_eq(xrand64(&rng), rounds64[round].values[i]);
		}
		for (size_t i = 0; i < xlen(rounds64[i].coins); i++) {
			char c = xrand64_bound(&rng, 2) ? 'H' : 'T';
			mu_assert_char_eq(c, rounds64[round].coins[i]);
		}
		for (size_t i = 0; i < xlen(rounds64[i].rolls); i++) {
			mu_assert_uint_eq(xrand64_bound(&rng, 6)+1, rounds64[round].rolls[i]);
		}

		enum { SUITS = 4, NUMBERS = 13, CARDS = 52 };
		char cards[CARDS];

		for (int i = 0; i < CARDS; ++i) {
			cards[i] = i;
		}

		for (int i = CARDS; i > 1; --i) {
			int chosen = xrand64_bound(&rng, i);
			char card     = cards[chosen];
			cards[chosen] = cards[i-1];
			cards[i-1]  = card;
		}

		static const char number[] = {'A', '2', '3', '4', '5', '6', '7',
			'8', '9', 'T', 'J', 'Q', 'K'};
		static const char suit[] = {'h', 'c', 'd', 's'};
		for (int i = 0; i < CARDS; ++i) {
			char card[4];
			snprintf(card, sizeof(card), "%c%c", number[cards[i] / SUITS], suit[cards[i] % SUITS]);
			mu_assert_str_eq(card, rounds64[round].cards[i]);
		}
	}
}

static void
test_real(void)
{
	for (int i = 0; i < 1<<20; i++) {
		double d = xrand_real_global();
		mu_assert(d >= 0.0);
		mu_assert(d < 1.0);
	}
}

int
main(void)
{
	mu_init("rand");

	mu_run(test_32);
	mu_run(test_64);
	mu_run(test_real);
}
