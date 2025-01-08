/*
 * libanos version string
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

const char *libanos_version() { return VERSION; }