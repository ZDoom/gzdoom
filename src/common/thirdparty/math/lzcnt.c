#include "lzcnt.h"

uint32_t lzcnt32_generic(uint32_t x)
{
    uint32_t n;
    static uint32_t clz_table_4[] = {
        0,
        4,
        3, 3,
        2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1
    };

    if (x == 0) {
        return sizeof(x)*8;
    }

    n = clz_table_4[x >> (sizeof(x)*8 - 4)];
    if (n == 0) {
        if ((x & 0xFFFF0000) == 0) { n  = 16; x <<= 16; }
        if ((x & 0xFF000000) == 0) { n += 8;  x <<= 8;  }
        if ((x & 0xF0000000) == 0) { n += 4;  x <<= 4;  }
        n += clz_table_4[x >> (sizeof(x)*8 - 4)];
    }

    return n - 1;
}

uint32_t lzcnt64_generic(uint64_t x)
{
    uint32_t n;
    static uint32_t clz_table_4[] = {
        0,
        4,
        3, 3,
        2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1
    };

    if (x == 0) {
        return sizeof(x)*8;
    }

    n = clz_table_4[x >> (sizeof(x)*8 - 4)];
    if (n == 0) {
        if ((x & ((uint64_t)0xFFFFFFFF << 32)) == 0) { n  = 32; x <<= 32; }
        if ((x & ((uint64_t)0xFFFF0000 << 32)) == 0) { n += 16; x <<= 16; }
        if ((x & ((uint64_t)0xFF000000 << 32)) == 0) { n += 8;  x <<= 8;  }
        if ((x & ((uint64_t)0xF0000000 << 32)) == 0) { n += 4;  x <<= 4;  }
        n += clz_table_4[x >> (sizeof(x)*8 - 4)];
    }

    return n - 1;
}

