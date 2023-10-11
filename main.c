#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "moonfish.h"

int main(int argc, char **argv)
{
	static char line[2048];
	FILE *file;
	struct moonfish *ctx;
	char *arg;
	struct moonfish_move move;
	char name[6];
	
	if (argc < 1) return 1;
	
#ifdef MOONFISH_INBUILT_NET
	
	if (argc > 2)
	{
		fprintf(stderr, "usage: %s [<file-name>]\n", argv[0]);
		return 1;
	}
	
	if (argc >= 2) file = fopen(argv[1], "rb");
	else file = fmemopen(moonfish_network, 1139, "rb");
	
#else
	
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <file-name>\n", argv[0]);
		return 1;
	}
	
	file = fopen(argv[1], "rb");
	
#endif
	
	ctx = malloc(sizeof *ctx);
	if (ctx == NULL)
	{
		perror(argv[0]);
		return 1;
	}
	
	if (file == NULL)
	{
		perror(argv[0]);
		free(ctx);
		return 1;
	}
	
	if (moonfish_nnue(ctx, file))
	{
		fprintf(stderr, "%s: could not parse network\n", argv[0]);
		fclose(file);
		free(ctx);
		return 1;
	}
	
	fclose(file);
	
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
	
	moonfish_chess(ctx);
	
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
			moonfish_best_move(ctx, &move);
			moonfish_to_uci(name, &move, ctx->white);
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
				moonfish_fen(ctx, arg);
				
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
				moonfish_chess(ctx);
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
					moonfish_play_uci(ctx, arg);
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
