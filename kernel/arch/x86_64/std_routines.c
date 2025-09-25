#include <stddef.h>
#include <stdint.h>

__attribute__((no_sanitize("alignment"))) // Can't align both src and dest in the general case..
#ifdef UNIT_TESTS
void *anos_std_memcpy(void *restrict dest, const void *restrict src, size_t count)
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

    // For large copies, use non-temporal stores
    if (blocks >= 32) {        // 256 bytes or more
        while (blocks >= 16) { // Process 128 bytes at a time
            // TODO prefetch...

            // Non-temporal stores - bypass cache
            __asm__ volatile("movq (%0), %%r8\n"
                             "movq 8(%0), %%r9\n"
                             "movq 16(%0), %%r10\n"
                             "movq 24(%0), %%r11\n"
                             "movnti %%r8, (%1)\n"
                             "movnti %%r9, 8(%1)\n"
                             "movnti %%r10, 16(%1)\n"
                             "movnti %%r11, 24(%1)\n"
                             "movq 32(%0), %%r8\n"
                             "movq 40(%0), %%r9\n"
                             "movq 48(%0), %%r10\n"
                             "movq 56(%0), %%r11\n"
                             "movnti %%r8, 32(%1)\n"
                             "movnti %%r9, 40(%1)\n"
                             "movnti %%r10, 48(%1)\n"
                             "movnti %%r11, 56(%1)\n"
                             "movq 64(%0), %%r8\n"
                             "movq 72(%0), %%r9\n"
                             "movq 80(%0), %%r10\n"
                             "movq 88(%0), %%r11\n"
                             "movnti %%r8, 64(%1)\n"
                             "movnti %%r9, 72(%1)\n"
                             "movnti %%r10, 80(%1)\n"
                             "movnti %%r11, 88(%1)\n"
                             "movq 96(%0), %%r8\n"
                             "movq 104(%0), %%r9\n"
                             "movq 112(%0), %%r10\n"
                             "movq 120(%0), %%r11\n"
                             "movnti %%r8, 96(%1)\n"
                             "movnti %%r9, 104(%1)\n"
                             "movnti %%r10, 112(%1)\n"
                             "movnti %%r11, 120(%1)\n"
                             :
                             : "r"(s64), "r"(d64)
                             : "r8", "r9", "r10", "r11", "memory");
            d64 += 16;
            s64 += 16;
            blocks -= 16;
        }
        // fence after non-temporal stores
        __asm__ volatile("sfence" : : : "memory");
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

__attribute__((no_sanitize("alignment"))) // Can't align both src and dest in the general case..
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
        while (((uintptr_t)d & 7) && count) {
            *d++ = *s++;
            count--;
        }

        uint64_t *d64 = (uint64_t *)d;
        const uint64_t *s64 = (const uint64_t *)s;
        while (count >= 64) {
            __asm__ volatile("movq (%1), %%r8;\n"
                             "movq 8(%1), %%r9;\n"
                             "movq 16(%1), %%r10;\n"
                             "movq 24(%1), %%r11;\n"
                             "movnti %%r8, (%0);\n"
                             "movnti %%r9, 8(%0);\n"
                             "movnti %%r10, 16(%0);\n"
                             "movnti %%r11, 24(%0);\n"
                             "movq 32(%1), %%r8;\n"
                             "movq 40(%1), %%r9;\n"
                             "movq 48(%1), %%r10;\n"
                             "movq 56(%1), %%r11;\n"
                             "movnti %%r8, 32(%0);\n"
                             "movnti %%r9, 40(%0);\n"
                             "movnti %%r10, 48(%0);\n"
                             "movnti %%r11, 56(%0);\n"
                             : "=r"(d64), "=r"(s64)
                             : "0"(d64), "1"(s64)
                             : "r8", "r9", "r10", "r11", "memory");
            d64 += 8;
            s64 += 8;
            count -= 64;
        }
        __asm__ volatile("sfence" ::: "memory");

        d = (unsigned char *)d64;
        s = (const unsigned char *)s64;
        while (count--) {
            *d++ = *s++;
        }
    } else {
        d += count;
        s += count;

        while ((uintptr_t)d & 7 && count) {
            *--d = *--s;
            count--;
        }

        uint64_t *d64 = (uint64_t *)d;
        const uint64_t *s64 = (const uint64_t *)s;
        while (count >= 64) {
            __asm__ volatile("movq -8(%1), %%r8;\n"
                             "movq -16(%1), %%r9;\n"
                             "movq -24(%1), %%r10;\n"
                             "movq -32(%1), %%r11;\n"
                             "movq %%r8, -8(%0);\n"
                             "movq %%r9, -16(%0);\n"
                             "movq %%r10, -24(%0);\n"
                             "movq %%r11, -32(%0);\n"
                             "movq -40(%1), %%r8;\n"
                             "movq -48(%1), %%r9;\n"
                             "movq -56(%1), %%r10;\n"
                             "movq -64(%1), %%r11;\n"
                             "movq %%r8, -40(%0);\n"
                             "movq %%r9, -48(%0);\n"
                             "movq %%r10, -56(%0);\n"
                             "movq %%r11, -64(%0);\n"
                             : "=r"(d64), "=r"(s64)
                             : "0"(d64), "1"(s64)
                             : "r8", "r9", "r10", "r11", "memory");
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
    while (count >= 64) {
        __asm__ volatile("movnti %1, (%0);\n"
                         "movnti %1, 8(%0);\n"
                         "movnti %1, 16(%0);\n"
                         "movnti %1, 24(%0);\n"
                         "movnti %1, 32(%0);\n"
                         "movnti %1, 40(%0);\n"
                         "movnti %1, 48(%0);\n"
                         "movnti %1, 56(%0);\n"
                         : "=r"(d64)
                         : "r"(fill), "0"(d64)
                         : "memory");
        d64 += 8;
        count -= 64;
    }
    __asm__ volatile("sfence" ::: "memory");

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
