/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#ifndef MOONFISH_TOOLS
#define MOONFISH_TOOLS

#include <stdio.h>

struct moonfish_arg
{
	char *letter;
	char *name;
	char *format;
	char *value;
	char *description;
};

void moonfish_spawner(char *argv0);
void moonfish_spawn(char **argv, FILE **in, FILE **out, char *directory);

char *moonfish_next(FILE *file);
char *moonfish_wait(FILE *file, char *name);
char **moonfish_args(struct moonfish_arg *args, char *rest_format, int argc, char **argv);
void moonfish_usage(struct moonfish_arg *args, char *rest_format, char *argv0);

int moonfish_int(char *arg, int *result);

#endif
