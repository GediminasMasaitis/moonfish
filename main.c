/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

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
	long int wtime, btime, *xtime;
	int score;
	int depth;
	
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
			depth = -1;
			
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
				else if (!strcmp(arg, "depth"))
				{
					arg = strtok(NULL, "\r\n\t ");
					
					if (arg == NULL || sscanf(arg, "%d", &depth) != 1 || depth < 0)
					{
						free(ctx);
						fprintf(stderr, "%s: malformed 'go depth' command\n", argv[0]);
						return 1;
					}
				}
			}
			
			if (wtime < 0) wtime = 0;
			if (btime < 0) btime = 0;
			
			if (depth >= 0)
				score = moonfish_best_move_depth(ctx, &move, depth);
			else if (ctx->chess.white)
				score = moonfish_best_move_time(ctx, &move, wtime, btime);
			else
				score = moonfish_best_move_time(ctx, &move, btime, wtime);
			
			printf("info depth 1 ");
			if (score >= moonfish_omega || score <= -moonfish_omega)
				printf("score mate %d\n", moonfish_countdown(score));
			else
				printf("score cp %d\n", score);
			moonfish_to_uci(name, &move);
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
#ifndef moonfish_mini
		else if (!strcmp(arg, "debug") || !strcmp(arg, "setoption") || !strcmp(arg, "ucinewgame") || !strcmp(arg, "stop"))
		{
		}
		else
		{
			fprintf(stderr, "%s: unknown command '%s'\n", argv[0], arg);
		}
#endif
		
		fflush(stdout);
	}
	
	free(ctx);
	return 0;
}
