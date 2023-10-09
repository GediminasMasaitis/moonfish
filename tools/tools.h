#ifndef MOONFISH_TOOLS
#define MOONFISH_TOOLS

#include <stdio.h>

int moonfish_spawn(char *argv0, char **argv, int *in, int *out);
char *moonfish_next(FILE *file);
char *moonfish_wait(FILE *file, char *name);

#endif
