/*
 * Prototype hex printer
 */

#include <stdio.h>

#define PML4ENTRY(addr) (((unsigned short)((addr & 0x0000ff8000000000) >> 39)))
#define PDPTENTRY(addr) (((unsigned short)((addr & 0x0000007fc0000000) >> 30)))
#define PDENTRY(addr) (((unsigned short)((addr & 0x000000003fe00000) >> 21)))
#define PTENTRY(addr) (((unsigned short)((addr & 0x00000000001ff000) >> 12)))

static unsigned long addrs[] = {
        // 0xFFFFFFFF80000000,
        0x0000feffffffffff,
        0x0000ff0000000000,
        0x0000ff7fffffffff,
        0x0000ff8000000000,
        // 0xFFFFFFFF7fffffff,
        // 0xFE00000080000000,
        // 0xFE01000080000000,
        // 0x0000000000100000,
        // 0x00000000001fffff,
        // 0x0000000000200000,
        0,
};

int main() {
    unsigned long *current = addrs;

    while (*current) {
        printf("0x%016lx:\n", *current);
        printf("  PML4: 0x%04x\n", PML4ENTRY(*current));
        printf("  PDPT: 0x%04x\n", PDPTENTRY(*current));
        printf("  PD  : 0x%04x\n", PDENTRY(*current));
        printf("  PT  : 0x%04x\n", PTENTRY(*current));

        current++;
    }
}