/* moonfish's license: 0BSD */
/* copyright 2025 zamfofex */

#ifndef MOONFISH_TOOLS
#define MOONFISH_TOOLS

#include <stdio.h>

struct moonfish_arg {
	char *letter;
	char *name;
	char *format;
	char *value;
	char *description;
};

struct moonfish_env {
	char *name;
	char *format;
	char *description;
};

struct moonfish_example {
	char *args;
	char *description;
};

struct moonfish_command {
	char *description;
	char *rest;
	struct moonfish_arg args[16];
	struct moonfish_example examples[16];
	struct moonfish_env env[16];
	char *notes[16];
};

struct moonfish_chess;
struct moonfish_move;

void moonfish_spawn(char **argv, FILE **in, FILE **out, char *directory);

char *moonfish_wait(FILE *file, char *name);
char **moonfish_args(struct moonfish_command *cmd, int argc, char **argv);
void moonfish_usage(struct moonfish_command *cmd, char *argv0);

int moonfish_int(char *arg, int *result);

int moonfish_pgn(FILE *file, struct moonfish_chess *chess, struct moonfish_move *move, int allow_tag);

#endif
