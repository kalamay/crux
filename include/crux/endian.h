#ifndef CRUX_ENDIAN_H
#define CRUX_ENDIAN_H

#if BYTE_ORDER == LITTLE_ENDIAN
# define xhtobe16(v) __builtin_bswap16(v)
# define xhtole16(v) (v)
# define xbe16toh(v) __builtin_bswap16(v)
# define xle16toh(v) (v)
# define xhtobe32(v) __builtin_bswap32(v)
# define xhtole32(v) (v)
# define xbe32toh(v) __builtin_bswap32(v)
# define xle32toh(v) (v)
# define xhtobe64(v) __builtin_bswap64(v)
# define xhtole64(v) (v)
# define xbe64toh(v) __builtin_bswap64(v)
# define xle64toh(v) (v)
#elif BYTE_ORDER == BIG_ENDIAN
# define xhtobe16(v) (v)
# define xhtole16(v) __builtin_bswap16(v)
# define xbe16toh(v) (v)
# define xle16toh(v) __builtin_bswap16(v)
# define xhtobe32(v) (v)
# define xhtole32(v) __builtin_bswap32(v)
# define xbe32toh(v) (v)
# define xle32toh(v) __builtin_bswap32(v)
# define xhtobe64(v) (v)
# define xhtole64(v) __builtin_bswap64(v)
# define xbe64toh(v) (v)
# define xle64toh(v) __builtin_bswap64(v)
#endif

#endif

