/*
 * Just a (temporary) smoke test, calling out to C from stage2.
 *
 * Doesn't prove a great deal to be fair. Just that I've set 
 * up the long-mode segments and stack well enough for (basic)
 * C to work...
 */

static char *vram = (char*)0xb8000;
static char *MSG = "Made it to C";

void stage2_ctest() {
    char *msgp = MSG;
    int vrptr = 160;

    while (*msgp) {
        vram[vrptr++] = *msgp++;
        vram[vrptr++] = 0x1b;
    }
}
