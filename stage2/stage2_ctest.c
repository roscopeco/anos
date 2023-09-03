/*
 * Just a (temporary) smoke test, calling out to C from stage2.
 *
 * Doesn't prove a great deal to be fair. Just that I've set 
 * up the long-mode segments and stack well enough for (basic)
 * C to work...
 */

// these will end up in data
static char *vram = (char*)0xb8000;
static char *MSG = "Made it to C";

// this will end up in bss
static char *msgp;

void stage2_ctest() {
    msgp = MSG;
    int vrptr = 160;

    while (*msgp) {
        vram[vrptr++] = *msgp++;
        vram[vrptr++] = 0x1b;
    }
}
