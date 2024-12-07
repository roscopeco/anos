/*
 * Prototype hex printer
 */

#include <stdint.h>
#include <stdio.h>

#define ZERO 48
#define EX 120

typedef void(char_handler)(char chr);

void printchar(char chr) { printf("%c", chr); }

static inline void preamble(char_handler printfunc) {
    printfunc(ZERO);
    printfunc(EX);
}

static inline void digitprint(uint8_t digit, char_handler printfunc) {
    if (digit < 10) {
        digit += 48;
    } else {
        digit += 87;
    }

    printfunc(digit);
}

void printhex64(uint64_t num, char_handler printfunc) {
    preamble(printfunc);

    for (int i = 0; i < 64; i += 4) {
        char digit = (num & 0xF000000000000000) >> 60;
        num <<= 4;
        digitprint(digit, printfunc);
    }
}

void printhex32(uint64_t num, char_handler printfunc) {
    preamble(printfunc);

    for (int i = 0; i < 32; i += 4) {
        char digit = (num & 0xF0000000) >> 28;
        num <<= 4;
        digitprint(digit, printfunc);
    }
}

void printhex16(uint64_t num, char_handler printfunc) {
    preamble(printfunc);

    for (int i = 0; i < 16; i += 4) {
        char digit = (num & 0xF000) >> 12;
        num <<= 4;
        digitprint(digit, printfunc);
    }
}

void printhex8(uint64_t num, char_handler printfunc) {
    preamble(printfunc);

    for (int i = 0; i < 8; i += 4) {
        char digit = (num & 0xF0) >> 4;
        num <<= 4;
        digitprint(digit, printfunc);
    }
}

int main() {
    printhex64(0x123456789ABCDEF0, printchar);
    printf("\n");
    printhex32(0x12345678, printchar);
    printf("\n");
    printhex16(0x1234, printchar);
    printf("\n");
    printhex8(0x1F, printchar);
    printf("\n");
    printhex16(0x11, printchar);
    printf("\n");
}