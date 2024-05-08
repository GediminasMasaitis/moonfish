/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../moonfish.h"
#include "ugi.h"

/* UGI GUI, UCI bot */

static char *moonfish_argv0;
static struct moonfish_chess moonfish_ugi_chess;

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		if (argc > 0) fprintf(stderr, "usage: %s <command> <args>...\n", argv[0]);
		return 1;
	}
	
	moonfish_chess(&moonfish_ugi_chess);
	moonfish_argv0 = argv[0];
	moonfish_convert_ugi(argv[0], argv + 1, "ugi", "uci");
	return 0;
}

/* read from GUI, write to bot */
void *moonfish_convert_in(void *data)
{
	FILE *in;
	char line[2048], line2[2048], *arg;
	char *save;
	struct moonfish_move move;
	
	in = data;
	
	for (;;)
	{
		if (fgets(line, sizeof line, stdin) == NULL) return NULL;
		save = NULL;
		strcpy(line2, line);
		
		arg = strtok_r(line, "\r\n\t ", &save);
		
		if (arg == NULL) continue;
		
		if (!strcmp(arg, "query"))
		{
			arg = strtok_r(NULL, "\r\n\t ", &save);
			if (arg == NULL)
			{
				fprintf(stderr, "%s: malformed 'query' command\n", moonfish_argv0);
				exit(1);
			}
			
			if (!strcmp(arg, "gameover"))
			{
				if (moonfish_finished(&moonfish_ugi_chess))
					printf("response true\n");
				else
					printf("response false\n");
				continue;
			}
			
			if (!strcmp(arg, "p1turn"))
			{
				if (moonfish_ugi_chess.white)
					printf("response true\n");
				else
					printf("response false\n");
				continue;
			}
			
			if (!strcmp(arg, "result"))
			{
				if (!moonfish_finished(&moonfish_ugi_chess))
					printf("response none\n");
				else if (!moonfish_checkmate(&moonfish_ugi_chess))
					printf("response draw\n");
				else if (moonfish_ugi_chess.white)
					printf("response p2win\n");
				else
					printf("response p1win\n");
				continue;
			}
			
			fprintf(stderr, "%s: unknown 'query %s' command\n", moonfish_argv0, arg);
			exit(1);
		}
		
		if (!strcmp(arg, "go"))
		{
			fprintf(in, "go");
			for (;;)
			{
				arg = strtok_r(NULL, "\r\n\t ", &save);
				if (arg == NULL) break;
				fprintf(in, " ");
				if (!strcmp(arg, "p1time")) fprintf(in, "wtime");
				else if (!strcmp(arg, "p2time")) fprintf(in, "btime");
				else if (!strcmp(arg, "p1inc")) fprintf(in, "winc");
				else if (!strcmp(arg, "p2inc")) fprintf(in, "binc");
				else fputs(arg, in);
			}
			fprintf(in, "\n");
			continue;
		}
		
		if (!strcmp(arg, "position"))
		{
			arg = strtok_r(NULL, "\r\n\t ", &save);
			if (arg == NULL)
			{
				fprintf(stderr, "%s: malformed 'position' command\n", moonfish_argv0);
				exit(1);
			}
			
			if (!strcmp(arg, "startpos"))
			{
				moonfish_chess(&moonfish_ugi_chess);
			}
			else if (!strcmp(arg, "fen"))
			{
				arg = strtok_r(NULL, "\r\n", &save);
				if (arg == NULL)
				{
					fprintf(stderr, "%s: malformed 'position fen' command\n", moonfish_argv0);
					exit(1);
				}
				moonfish_from_fen(&moonfish_ugi_chess, arg);
				
				arg = strstr(arg, "moves");
				if (arg != NULL)
				{
					do arg--;
					while (*arg == '\t' || *arg == ' ');
					save = NULL;
					strcpy(line, arg);
					strtok_r(line, "\r\n", &save);
				}
				else
				{
					save = NULL;
					strtok_r("", "\r\n", &save);
				}
			}
			else
			{
				fprintf(stderr, "%s: unknown 'position %s' command\n", moonfish_argv0, arg);
				exit(1);
			}
			
			arg = strtok_r(NULL, "\r\n\t ", &save);
			if (arg != NULL)
			{
				if (strcmp(arg, "moves"))
				{
					fprintf(stderr, "%s: unknown '%s' in 'position' command\n", moonfish_argv0, arg);
					exit(1);
				}
				
				for (;;)
				{
					arg = strtok_r(NULL, " ", &save);
					if (arg == NULL) break;
					if (moonfish_from_uci(&moonfish_ugi_chess, &move, arg))
					{
						fprintf(stderr, "%s: invalid move '%s' from bot\n", moonfish_argv0, arg);
						exit(1);
					}
					moonfish_ugi_chess = move.chess;
				}
			}
		}
		
		fputs(line2, in);
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
		
		if (!strcmp(arg, "uciok"))
		{
			printf("ugiok\n");
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
