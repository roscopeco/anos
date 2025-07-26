#!/bin/sh
#
# extract-includes.sh [COMPILER [LANG]]
#
# Print the compiler's *system include search paths* as -I flags, space-separated.
# Use in Make like:
#   INCLUDES := $(shell ./extract-includes.sh x86_64-elf-anos-gcc c++)
#
# Defaults: COMPILER=gcc  LANG=c
#

CC=${1:-gcc}
LANG=${2:-c}

# Ask compiler to tell us its include search paths.
# Feed it empty stdin, silence normal compile by using -fsyntax-only, and capture stderr.
# -Wp,-v triggers the preprocessor to dump the search list.
"$CC" -Wp,-v -x "$LANG" - -fsyntax-only 2>&1 </dev/null \
| awk '
    /#include <\.\.\.> search starts here:/ { grab=1; next }
    /End of search list\./ && grab { grab=0; exit }
    grab {
        # Trim leading space
        sub(/^[[:space:]]+/, "", $0)
        if ($0 != "") {
            # Print -Ipath tokens space-separated, no newline spam
            printf("-I%s ", $0)
        }
    }
' \
| sed 's/ *$//'
# final newline for good manners
echo

