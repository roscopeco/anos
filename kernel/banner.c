/*
 * stage3 - Banner printing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "debugprint.h"
#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#ifndef ARCH
#error Architecture not defined (-DARCH); How did you even get here?
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

#define XARCH(arch) #arch
#define STRARCH(xstrarch) XARCH(xstrarch)
#define ARCH_STR STRARCH(ARCH)

void banner() {
#ifndef NO_BANNER
    debugattr(0x0B);
    debugstr("STAGE");
    debugattr(0x03);
    debugchar('3');
    debugattr(0x08);
    debugstr(" #");
    debugattr(0x0F);
    debugstr(VERSION);
    debugattr(0x08);
    debugstr(" [");
    debugattr(0x07);
    debugstr(ARCH_STR);
    debugattr(0x08);
    debugstr("]\n");
    debugattr(0x07);
#endif
}
