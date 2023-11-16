/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023 zamfofex */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "moonfish.h"

int main(int argc, char **argv)
{
	static char line[2048];
	struct moonfish *ctx;
	char *arg;
	struct moonfish_move move;
	char name[6];
	long int time, wtime, btime, *xtime;
	
	if (argc > 1)
	{
		if (argc > 0) fprintf(stderr, "usage: %s\n", argv[0]);
		return 1;
	}
	
	ctx = malloc(sizeof *ctx);
	if (ctx == NULL)
	{
		perror(argv[0]);
		return 1;
	}
	
	ctx->argv0 = argv[0];
	ctx->cpu_count = -1;
	moonfish_chess(&ctx->chess);
	
	for (;;)
	{
		errno = 0;
		if (fgets(line, sizeof line, stdin) == NULL)
		{
			if (errno)
			{
				free(ctx);
				perror(argv[0]);
				return 1;
			}
			
			break;
		}
		
		arg = strtok(line, "\r\n\t ");
		if (arg == NULL) continue;
		
		if (!strcmp(arg, "go"))
		{
			wtime = -1;
			btime = -1;
			
			for (;;)
			{
				arg = strtok(NULL, "\r\n\t ");
				if (arg == NULL) break;
				if (!strcmp(arg, "wtime") || !strcmp(arg, "btime"))
				{
					if (!strcmp(arg, "wtime")) xtime = &wtime;
					else xtime = &btime;
					
					arg = strtok(NULL, "\r\n\t ");
					
					if (arg == NULL || sscanf(arg, "%ld", xtime) != 1 || *xtime < 0)
					{
						free(ctx);
						fprintf(stderr, "%s: malformed 'go' command\n", argv[0]);
						return 1;
					}
				}
			}
			
			if (wtime < 0 && btime < 0)
			{
				wtime = 3600000;
				btime = 3600000;
			}
			
			if (wtime < 0) wtime = btime;
			if (btime < 0) btime = wtime;
			
			if (!ctx->chess.white)
			{
				time = wtime;
				wtime = btime;
				btime = time;
			}
			
			moonfish_best_move(ctx, &move, wtime, btime);
			moonfish_to_uci(name, &move, ctx->chess.white);
			printf("bestmove %s\n", name);
		}
		else if (!strcmp(arg, "quit"))
		{
			break;
		}
		else if (!strcmp(arg, "position"))
		{
			arg = strtok(NULL, "\r\n\t ");
			if (arg == NULL)
			{
				fprintf(stderr, "incomplete 'position' command\n");
				free(ctx);
				return 1;
			}
			
			if (!strcmp(arg, "fen"))
			{
				arg = strtok(NULL, "\r\n");
				moonfish_fen(&ctx->chess, arg);
				
				arg = strstr(arg, "moves");
				if (arg != NULL)
				{
					do arg--;
					while (*arg == '\t' || *arg == ' ');
					strcpy(line, arg);
					strtok(line, "\r\n\t ");
				}
				else
				{
					strtok("", "\r\n\t ");
				}
			}
			else if (!strcmp(arg, "startpos"))
			{
				moonfish_chess(&ctx->chess);
			}
			else
			{
				fprintf(stderr, "malformed 'position' command\n");
				free(ctx);
				return 1;
			}
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg != NULL && !strcmp(arg, "moves"))
			{
				while ((arg = strtok(NULL, "\r\n\t ")) != NULL)
				{
					moonfish_from_uci(&ctx->chess, &move, arg);
					moonfish_play(&ctx->chess, &move);
				}
			}
		}
		else if (!strcmp(arg, "uci"))
		{
			printf("id name moonfish\n");
			printf("id author zamfofex\n");
			printf("uciok\n");
		}
		else if (!strcmp(arg, "isready"))
		{
			printf("readyok\n");
		}
		else if (!strcmp(arg, "debug") || !strcmp(arg, "setoption") || !strcmp(arg, "ucinewgame") || !strcmp(arg, "stop"))
		{
		}
		else
		{
			fprintf(stderr, "%s: unknown command '%s'\n", argv[0], arg);
		}
		
		fflush(stdout);
	}
	
	free(ctx);
	return 0;
}
