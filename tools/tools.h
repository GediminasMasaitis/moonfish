/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#ifndef MOONFISH_TOOLS
#define MOONFISH_TOOLS

#include <stdio.h>

void moonfish_spawn(char *argv0, char **argv, FILE **in, FILE **out);
char *moonfish_next(FILE *file);
char *moonfish_wait(FILE *file, char *name);

#endif
