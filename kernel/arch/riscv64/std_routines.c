#include <stddef.h>
#include <stdint.h>

__attribute__((no_sanitize(
        "alignment"))) // Can't align both src and dest in the general case..
#ifdef UNIT_TESTS
void *anos_std_memcpy(void *restrict dest, const void *restrict src,
                      size_t count)
#else
void *memcpy(void *restrict dest, const void *restrict src, size_t count)
#endif
{
    char *d = (char *)dest;
    const char *s = (const char *)src;

    // Handle small copies with Duff's device
    if (count < 8) {
        switch (count) {
        case 7:
            d[6] = s[6];
        case 6:
            d[5] = s[5];
        case 5:
            d[4] = s[4];
        case 4:
            d[3] = s[3];
        case 3:
            d[2] = s[2];
        case 2:
            d[1] = s[1];
        case 1:
            d[0] = s[0];
        }
        return dest;
    }

    // Align destination to 8 bytes
    size_t align = (8 - ((uintptr_t)d & 7)) & 7;
    if (align) {
        count -= align;
        switch (align) {
        case 7:
            d[6] = s[6];
        case 6:
            d[5] = s[5];
        case 5:
            d[4] = s[4];
        case 4:
            d[3] = s[3];
        case 3:
            d[2] = s[2];
        case 2:
            d[1] = s[1];
        case 1:
            d[0] = s[0];
        }
        d += align;
        s += align;
    }

    // Now we're 8-byte aligned
    uint64_t *d64 = (uint64_t *)d;
    const uint64_t *s64 = (const uint64_t *)s;

    size_t blocks = count >> 3;
    count &= 7;

    // For large copies, use RISC-V optimized approach
    if (blocks >= 32) { // 256 bytes or more
// Use RISC-V vector extension if available
#ifdef __riscv_v
// Vectorized copy for large blocks
// This would use RISC-V vector instructions if available
// For now, we'll use a scalar approach that's still optimized
#endif

        // Process 128 bytes at a time (16 64-bit words)
        while (blocks >= 16) {
            // Load 16 64-bit words
            uint64_t t0 = s64[0];
            uint64_t t1 = s64[1];
            uint64_t t2 = s64[2];
            uint64_t t3 = s64[3];
            uint64_t t4 = s64[4];
            uint64_t t5 = s64[5];
            uint64_t t6 = s64[6];
            uint64_t t7 = s64[7];
            uint64_t t8 = s64[8];
            uint64_t t9 = s64[9];
            uint64_t t10 = s64[10];
            uint64_t t11 = s64[11];
            uint64_t t12 = s64[12];
            uint64_t t13 = s64[13];
            uint64_t t14 = s64[14];
            uint64_t t15 = s64[15];

            // Store 16 64-bit words
            d64[0] = t0;
            d64[1] = t1;
            d64[2] = t2;
            d64[3] = t3;
            d64[4] = t4;
            d64[5] = t5;
            d64[6] = t6;
            d64[7] = t7;
            d64[8] = t8;
            d64[9] = t9;
            d64[10] = t10;
            d64[11] = t11;
            d64[12] = t12;
            d64[13] = t13;
            d64[14] = t14;
            d64[15] = t15;

            d64 += 16;
            s64 += 16;
            blocks -= 16;
        }
    }

    // Handle remaining blocks with regular stores
    switch (blocks) {
    case 15:
        d64[14] = s64[14];
    case 14:
        d64[13] = s64[13];
    case 13:
        d64[12] = s64[12];
    case 12:
        d64[11] = s64[11];
    case 11:
        d64[10] = s64[10];
    case 10:
        d64[9] = s64[9];
    case 9:
        d64[8] = s64[8];
    case 8:
        d64[7] = s64[7];
    case 7:
        d64[6] = s64[6];
    case 6:
        d64[5] = s64[5];
    case 5:
        d64[4] = s64[4];
    case 4:
        d64[3] = s64[3];
    case 3:
        d64[2] = s64[2];
    case 2:
        d64[1] = s64[1];
    case 1:
        d64[0] = s64[0];
    }
    d64 += blocks;
    s64 += blocks;

    // Handle remaining bytes
    d = (char *)d64;
    s = (const char *)s64;
    switch (count) {
    case 7:
        d[6] = s[6];
    case 6:
        d[5] = s[5];
    case 5:
        d[4] = s[4];
    case 4:
        d[3] = s[3];
    case 3:
        d[2] = s[2];
    case 2:
        d[1] = s[1];
    case 1:
        d[0] = s[0];
    }

    return dest;
}

__attribute__((no_sanitize(
        "alignment"))) // Can't align both src and dest in the general case..
#ifdef UNIT_TESTS
void *anos_std_memmove(void *dest, const void *src, size_t count)
#else
void *memmove(void *dest, const void *src, size_t count)
#endif
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s) {
        return dest;
    }

    if (d < s || d >= s + count) {
        // Forward copy
        while (((uintptr_t)d & 7) && count) {
            *d++ = *s++;
            count--;
        }

        uint64_t *d64 = (uint64_t *)d;
        const uint64_t *s64 = (const uint64_t *)s;

        // Process 64 bytes at a time (8 64-bit words)
        while (count >= 64) {
            // Load 8 64-bit words
            uint64_t t0 = s64[0];
            uint64_t t1 = s64[1];
            uint64_t t2 = s64[2];
            uint64_t t3 = s64[3];
            uint64_t t4 = s64[4];
            uint64_t t5 = s64[5];
            uint64_t t6 = s64[6];
            uint64_t t7 = s64[7];

            // Store 8 64-bit words
            d64[0] = t0;
            d64[1] = t1;
            d64[2] = t2;
            d64[3] = t3;
            d64[4] = t4;
            d64[5] = t5;
            d64[6] = t6;
            d64[7] = t7;

            d64 += 8;
            s64 += 8;
            count -= 64;
        }

        d = (unsigned char *)d64;
        s = (const unsigned char *)s64;
        while (count--) {
            *d++ = *s++;
        }
    } else {
        // Backward copy
        d += count;
        s += count;

        while ((uintptr_t)d & 7 && count) {
            *--d = *--s;
            count--;
        }

        uint64_t *d64 = (uint64_t *)d;
        const uint64_t *s64 = (const uint64_t *)s;

        // Process 64 bytes at a time (8 64-bit words)
        while (count >= 64) {
            // Load 8 64-bit words
            uint64_t t0 = s64[-1];
            uint64_t t1 = s64[-2];
            uint64_t t2 = s64[-3];
            uint64_t t3 = s64[-4];
            uint64_t t4 = s64[-5];
            uint64_t t5 = s64[-6];
            uint64_t t6 = s64[-7];
            uint64_t t7 = s64[-8];

            // Store 8 64-bit words
            d64[-1] = t0;
            d64[-2] = t1;
            d64[-3] = t2;
            d64[-4] = t3;
            d64[-5] = t4;
            d64[-5] = t5;
            d64[-6] = t6;
            d64[-7] = t7;

            d64 -= 8;
            s64 -= 8;
            count -= 64;
        }

        d = (unsigned char *)d64;
        s = (const unsigned char *)s64;
        while (count--) {
            *--d = *--s;
        }
    }

    return dest;
}

#ifdef UNIT_TESTS
void *anos_std_memset(void *dest, int val, size_t count)
#else
void *memset(void *dest, int val, size_t count)
#endif
{
    unsigned char *d = (unsigned char *)dest;
    uint64_t fill = (uint8_t)val;
    fill |= fill << 8;
    fill |= fill << 16;
    fill |= fill << 32;

    while (((uintptr_t)d & 7) && count) {
        *d++ = (unsigned char)val;
        count--;
    }

    uint64_t *d64 = (uint64_t *)d;

    // Process 64 bytes at a time (8 64-bit words)
    while (count >= 64) {
        d64[0] = fill;
        d64[1] = fill;
        d64[2] = fill;
        d64[3] = fill;
        d64[4] = fill;
        d64[5] = fill;
        d64[6] = fill;
        d64[7] = fill;

        d64 += 8;
        count -= 64;
    }

    d = (unsigned char *)d64;
    while (count--) {
        *d++ = (unsigned char)val;
    }
    return dest;
}

void *memclr(void *dest, size_t count) {
#ifdef UNIT_TESTS
    return anos_std_memset(dest, 0, count);
#else
    return memset(dest, 0, count);
#endif
}