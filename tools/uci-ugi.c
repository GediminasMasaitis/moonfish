/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ugi.h"

/* UCI GUI, UGI bot */

char *moonfish_argv0;

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		if (argc > 0) fprintf(stderr, "usage: %s <command> <args>...\n", argv[0]);
		return 1;
	}
	
	moonfish_argv0 = argv[0];
	moonfish_convert_ugi(argv[0], argv + 1, "ugi", "uci");
	return 0;
}

/* read from GUI, write to bot */
void *moonfish_convert_in(void *data)
{
	FILE *in;
	char line[2048], *arg;
	char *save;
	
	in = data;
	
	for (;;)
	{
		if (fgets(line, sizeof line, stdin) == NULL) return NULL;
		save = NULL;
		
		arg = strtok_r(line, "\r\n\t ", &save);
		
		if (arg == NULL) continue;
		
		if (!strcmp(arg, "go"))
		{
			fprintf(in, "go");
			for (;;)
			{
				arg = strtok_r(NULL, "\r\n\t ", &save);
				if (arg == NULL) break;
				fprintf(in, " ");
				if (!strcmp(arg, "wtime")) fprintf(in, "p1time");
				else if (!strcmp(arg, "btime")) fprintf(in, "p2time");
				else if (!strcmp(arg, "winc")) fprintf(in, "p1inc");
				else if (!strcmp(arg, "binc")) fprintf(in, "p2inc");
				else fputs(arg, in);
			}
			fprintf(in, "\n");
		}
		
		fputs(arg, in);
		for (;;)
		{
			arg = strtok_r(NULL, "\r\n\t ", &save);
			if (arg == NULL) break;
			fprintf(in, " %s", arg);
		}
		fprintf(in, "\n");
	}
}


/* read from bot, write to GUI */
void *moonfish_convert_out(void *data)
{
	FILE *out;
	char line[2048], *arg;
	char *save;
	
	out = data;
	
	for (;;)
	{
		if (fgets(line, sizeof line, out) == NULL) exit(0);
		save = NULL;
		
		arg = strtok_r(line, "\r\n\t ", &save);
		
		if (arg == NULL) continue;
		
		if (!strcmp(arg, "ugiok"))
		{
			printf("uciok\n");
			fflush(stdout);
			continue;
		}
		
		fputs(arg, stdout);
		for (;;)
		{
			arg = strtok_r(NULL, "\r\n\t ", &save);
			if (arg == NULL) break;
			printf(" %s", arg);
		}
		printf("\n");
		fflush(stdout);
	}
}
