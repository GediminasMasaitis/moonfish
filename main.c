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
	int time;
	
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
	
	printf("   (moonfish by zamfofex)\n");
	printf("   (inspired by sunfish by tahle)\n");
	printf("   (simple UCI interface)\n");
	
	printf("\n");
	printf(
		"        .-, *  z     *\n"
		"  *   /  /    z___ *\n"
		"     |  | *   /-  \\/(\n"
		"   * |  \\     \\___/\\(\n"
		" *    \\  \\  *          *\n"
		"        `--    *\n\n"
	);
	
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
			if (ctx->chess.white) arg = strstr(arg, " wtime ");
			else arg = strstr(arg, " btime ");
			
			if (arg == NULL)
			{
				time = 120000;
			}
			else
			{
				if (sscanf(arg + 6, "%d", &time) != 1)
				{
					free(ctx);
					fprintf(stderr, "%s: malformed 'go' command\n", argv[0]);
					return 1;
				}
			}
			
			moonfish_best_move(ctx, &move, time / 1000);
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
	
	printf("   (good night)\n");
	return 0;
}
