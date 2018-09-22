#include "../include/crux/hash.h"
#include "../include/crux/endian.h"

#include <ctype.h>
#include <string.h>

inline static uint64_t
rotr64(uint64_t v, unsigned k)
{
    return (v >> k) | (v << (64 - k));
}

inline static uint64_t
rotl64(uint64_t v, unsigned k)
{
    return (v << k) | (v >> (64 - k));
}

inline static uint64_t
read64(const void *const ptr)
{
    uint64_t val;
    memcpy(&val, ptr, sizeof(val));
    return val;
}

inline static uint64_t
read32(const void *const ptr)
{
    uint32_t val;
    memcpy(&val, ptr, sizeof(val));
    return val;
}

inline static uint64_t
read16(const void * const ptr)
{
    uint16_t val;
    memcpy(&val, ptr, sizeof(val));
    return val;
}

inline static uint64_t
read8(const void * const ptr)
{
	return (uint64_t)*(const uint8_t *)ptr;
}

// MetroHash Copyright (c) 2015 J. Andrew Rogers
// https://github.com/jandrewrogers/MetroHash
uint64_t
xhash_metro64(const void *s, size_t len, const union xseed *seed)
{
	static const uint64_t k0 = 0xC83A91E1;
	static const uint64_t k1 = 0x8648DBDB;
	static const uint64_t k2 = 0x7BDEC03B;
	static const uint64_t k3 = 0x2F5870A5;

	const uint8_t *ptr = s;
	const uint8_t *const end = ptr + len;

	uint64_t hash = (((uint64_t)seed->u32 + k2) * k0) + len;

	if (len >= 32) {
		uint64_t v[4];
		v[0] = hash;
		v[1] = hash;
		v[2] = hash;
		v[3] = hash;

		do {
			v[0] += read64(ptr) * k0; ptr += 8; v[0] = rotr64(v[0],29) + v[2];
			v[1] += read64(ptr) * k1; ptr += 8; v[1] = rotr64(v[1],29) + v[3];
			v[2] += read64(ptr) * k2; ptr += 8; v[2] = rotr64(v[2],29) + v[0];
			v[3] += read64(ptr) * k3; ptr += 8; v[3] = rotr64(v[3],29) + v[1];
		}
		while (ptr <= (end - 32));

		v[2] ^= rotr64(((v[0] + v[3]) * k0) + v[1], 33) * k1;
		v[3] ^= rotr64(((v[1] + v[2]) * k1) + v[0], 33) * k0;
		v[0] ^= rotr64(((v[0] + v[2]) * k0) + v[3], 33) * k1;
		v[1] ^= rotr64(((v[1] + v[3]) * k1) + v[2], 33) * k0;
		hash += v[0] ^ v[1];
	}

	if ((end - ptr) >= 16) {
		uint64_t v0 = hash + (read64(ptr) * k0); ptr += 8; v0 = rotr64(v0,33) * k1;
		uint64_t v1 = hash + (read64(ptr) * k1); ptr += 8; v1 = rotr64(v1,33) * k2;
		v0 ^= rotr64(v0 * k0, 35) + v1;
		v1 ^= rotr64(v1 * k3, 35) + v0;
		hash += v1;
	}

	if ((end - ptr) >= 8) {
		hash += read64(ptr) * k3; ptr += 8;
		hash ^= rotr64(hash, 33) * k1;
	}

	if ((end - ptr) >= 4) {
		hash += read32(ptr) * k3; ptr += 4;
		hash ^= rotr64(hash, 15) * k1;
	}

	if ((end - ptr) >= 2) {
		hash += read16(ptr) * k3; ptr += 2;
		hash ^= rotr64(hash, 13) * k1;
	}

	if ((end - ptr) >= 1) {
		hash += read8(ptr) * k3;
		hash ^= rotr64(hash, 25) * k1;
	}

	hash ^= rotr64(hash, 33);
	hash *= k0;
	hash ^= rotr64(hash, 33);

	return hash;
}

#define SIPROUND(n, a, b, c, d) do { \
	for (int i = 0; i < n; i++) { \
		a += b; b=rotl64(b,13); b ^= a; a=rotl64(a,32); \
		c += d; d=rotl64(d,16); d ^= c; \
		a += d; d=rotl64(d,21); d ^= a; \
		c += b; b=rotl64(b,17); b ^= c; c=rotl64(c,32); \
	} \
} while (0)

uint64_t
xhash_sip(const void *s, size_t len, const union xseed *seed)
{
	uint64_t b = ((uint64_t)len) << 56;
	uint64_t k0 = xle64toh(seed->u128.low);
	uint64_t k1 = xle64toh(seed->u128.high);
	uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
	uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
	uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
	uint64_t v3 = 0x7465646279746573ULL ^ k1;
	const uint8_t *end = (uint8_t *)s + len - (len % 8);

	for (; s != end; s = (uint8_t*)s + 8) {
		uint64_t m = xle64toh(*(uint64_t *)s);
		v3 ^= m;
		SIPROUND(2, v0, v1, v2, v3);
		v0 ^= m;
	}

	switch (len & 7) {
		case 7: b |= ((uint64_t)((uint8_t *)s)[6])  << 48;
		case 6: b |= ((uint64_t)((uint8_t *)s)[5])  << 40;
		case 5: b |= ((uint64_t)((uint8_t *)s)[4])  << 32;
		case 4: b |= ((uint64_t)((uint8_t *)s)[3])  << 24;
		case 3: b |= ((uint64_t)((uint8_t *)s)[2])  << 16;
		case 2: b |= ((uint64_t)((uint8_t *)s)[1])  <<  8;
		case 1: b |= ((uint64_t)((uint8_t *)s)[0]); break;
		case 0: break;
	}

	v3 ^= b;
	SIPROUND(2, v0, v1, v2, v3);
	v0 ^= b;
	v2 ^= 0xff;
	SIPROUND(4, v0, v1, v2, v3);
	return v0 ^ v1 ^ v2  ^ v3;
}

uint64_t
xhash_sipcase(const void *s, size_t len, const union xseed *seed)
{
	uint64_t b = ((uint64_t)len) << 56;
	uint64_t k0 = xle64toh(seed->u128.low);
	uint64_t k1 = xle64toh(seed->u128.high);
	uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
	uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
	uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
	uint64_t v3 = 0x7465646279746573ULL ^ k1;
	const uint8_t *end = (uint8_t *)s + len - (len % 8);

	for (; s != end; s = (uint8_t *)s + 8) {
		uint64_t m = *(uint64_t *)s;
		for (int i = 0; i < 8; i++) {
			((uint8_t *)&m)[i] = (((uint8_t *)&m)[i] | 1<<5);
		}
		m = xle64toh(m);
		v3 ^= m;
		SIPROUND(2, v0, v1, v2, v3);
		v0 ^= m;
	}

	switch (len & 7) {
		case 7: b |= (uint64_t)(((uint8_t *)s)[6] | 1<<5)  << 48;
		case 6: b |= (uint64_t)(((uint8_t *)s)[5] | 1<<5)  << 40;
		case 5: b |= (uint64_t)(((uint8_t *)s)[4] | 1<<5)  << 32;
		case 4: b |= (uint64_t)(((uint8_t *)s)[3] | 1<<5)  << 24;
		case 3: b |= (uint64_t)(((uint8_t *)s)[2] | 1<<5)  << 16;
		case 2: b |= (uint64_t)(((uint8_t *)s)[1] | 1<<5)  <<  8;
		case 1: b |= (uint64_t)(((uint8_t *)s)[0] | 1<<5); break;
		case 0: break;
	}

	v3 ^= b;
	SIPROUND(2, v0, v1, v2, v3);
	v0 ^= b;
	v2 ^= 0xff;
	SIPROUND(4, v0, v1, v2, v3);
	return v0 ^ v1 ^ v2  ^ v3;
}


// xxHash Copyright (c) 2012-2015, Yann Collet
// https://github.com/Cyan4973/xxHash

#define XX64_PRIME_1 11400714785074694791ULL
#define XX64_PRIME_2 14029467366897019727ULL
#define XX64_PRIME_3  1609587929392839161ULL
#define XX64_PRIME_4  9650029242287828579ULL
#define XX64_PRIME_5  2870177450012600261ULL

uint64_t
xhash_xx64(const void *input, size_t len, const union xseed *seed)
{
	const uint8_t *p = input;
	const uint8_t *pe = p + len;
	uint64_t h;

	if (len >= 32) {
		const uint8_t *limit = pe - 32;
		uint64_t v1 = seed->u64 + XX64_PRIME_1 + XX64_PRIME_2;
		uint64_t v2 = seed->u64 + XX64_PRIME_2;
		uint64_t v3 = seed->u64 + 0;
		uint64_t v4 = seed->u64 - XX64_PRIME_1;

		do {
			v1 += xle64toh(read64(p)) * XX64_PRIME_2;
			p+=8;
			v1 = rotl64(v1, 31);
			v1 *= XX64_PRIME_1;
			v2 += xle64toh(read64(p)) * XX64_PRIME_2;
			p+=8;
			v2 = rotl64(v2, 31);
			v2 *= XX64_PRIME_1;
			v3 += xle64toh(read64(p)) * XX64_PRIME_2;
			p+=8;
			v3 = rotl64(v3, 31);
			v3 *= XX64_PRIME_1;
			v4 += xle64toh(read64(p)) * XX64_PRIME_2;
			p+=8;
			v4 = rotl64(v4, 31);
			v4 *= XX64_PRIME_1;
		} while (p <= limit);

		h = rotl64(v1, 1) + rotl64(v2, 7) + rotl64(v3, 12) + rotl64(v4, 18);

		v1 *= XX64_PRIME_2;
		v1 = rotl64(v1, 31);
		v1 *= XX64_PRIME_1;
		h ^= v1;
		h = h * XX64_PRIME_1 + XX64_PRIME_4;

		v2 *= XX64_PRIME_2;
		v2 = rotl64(v2, 31);
		v2 *= XX64_PRIME_1;
		h ^= v2;
		h = h * XX64_PRIME_1 + XX64_PRIME_4;

		v3 *= XX64_PRIME_2;
		v3 = rotl64(v3, 31);
		v3 *= XX64_PRIME_1;
		h ^= v3;
		h = h * XX64_PRIME_1 + XX64_PRIME_4;

		v4 *= XX64_PRIME_2;
		v4 = rotl64(v4, 31);
		v4 *= XX64_PRIME_1;
		h ^= v4;
		h = h * XX64_PRIME_1 + XX64_PRIME_4;
	}
	else {
		h = seed->u64 + XX64_PRIME_5;
	}

	h += (uint64_t)len;

	while (p+8 <= pe) {
		uint64_t k1 = xle64toh(read64(p));
		k1 *= XX64_PRIME_2;
		k1 = rotl64(k1,31);
		k1 *= XX64_PRIME_1;
		h ^= k1;
		h = rotl64(h,27) * XX64_PRIME_1 + XX64_PRIME_4;
		p+=8;
	}

	if (p+4 <= pe) {
		h ^= xle64toh(read32(p)) * XX64_PRIME_1;
		h = rotl64(h, 23) * XX64_PRIME_2 + XX64_PRIME_3;
		p+=4;
	}

	while (p < pe) {
		h ^= (*p) * XX64_PRIME_5;
		h = rotl64(h, 11) * XX64_PRIME_1;
		p++;
	}

	h ^= h >> 33;
	h *= XX64_PRIME_2;
	h ^= h >> 29;
	h *= XX64_PRIME_3;
	h ^= h >> 32;

	return h;
}

