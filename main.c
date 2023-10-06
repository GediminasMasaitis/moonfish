#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "moonfish.h"

static char *moonfish_strdup(char *str)
{
	char *result;
	result = malloc(strlen(str) + 1);
	strcpy(result, str);
	return result;
}

int main(int argc, char **argv)
{
	FILE *file;
	struct moonfish *ctx;
	char line[512], *arg;
	struct moonfish_move move;
	char name[6];
	int score;
	
	if (argc != 2)
	{
		if (argc > 0) fprintf(stderr, "usage: %s <file-name>\n", argv[0]);
		return 1;
	}
	
	ctx = malloc(sizeof *ctx);
	if (ctx == NULL)
	{
		perror(argv[0]);
		return 1;
	}
	
	file = fopen(argv[1], "rb");
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
	printf("   (try typing 'help')\n");
	
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
		
		if (!strcmp(arg, "play"))
		{
			while ((arg = strtok(NULL, "\r\n\t ")) != NULL)
				moonfish_play_uci(ctx, arg);
		}
		else if (!strcmp(arg, "go"))
		{
			moonfish_best_move(ctx, &move);
			moonfish_to_uci(name, &move, ctx->white);
			printf("bestmove %s\n", name);
		}
		else if (!strcmp(arg, "eval"))
		{
			score = moonfish_best_move(ctx, &move);
			printf("score %d\n", score / 127);
		}
		else if (!strcmp(arg, "show"))
		{
			moonfish_show(ctx);
		}
		else if (!strcmp(arg, "restart"))
		{
			moonfish_chess(ctx);
		}
		else if (!strcmp(arg, "fen"))
		{
			arg = strtok(NULL, "\r\n");
			if (arg == NULL)
			{
				fprintf(stderr, "malformed 'fen' command\n");
				free(ctx);
				return 1;
			}
			moonfish_fen(ctx, arg);
		}
		else if (!strcmp(arg, "echo"))
		{
			arg = strtok(NULL, "\r\n");
			if (arg == NULL) arg = "";
			printf("%s\n", arg);
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
					arg = moonfish_strdup(arg);
					strtok(arg, "\r\n\t ");
					free(arg);
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
		else if (!strcmp(arg, "help"))
		{
			printf("   'show'          - shows the current position as ASCII\n");
			printf("   'play <moves>'  - plays the given moves on the current position\n");
			printf("   'eval'          - shows the evaluation of the current position\n");
			printf("   'go'            - finds the best move in the current position\n");
			printf("   'restart'       - resets the board to the initial position\n");
			printf("   'fen <FEN>'     - sets the board to the given FEN\n");
			printf("   'echo <message> - shows the given message (useful for scripts)\n");
			printf("   'quit'          - self-explanatory\n");
			printf("   for UCI compatibility:\n");
			printf("   'debug', 'setoption' - ignored\n");
			printf("   'ucinewgame', 'stop' - ignored\n");
			printf("   'uci', 'isready'     - handled per UCI\n");
			printf("   'position startpos [moves <moves>]'  - equivalent to 'restart' then 'play <moves>'\n");
			printf("   'position fen <FEN> [moves <moves>]' - equivalent to 'fen <FEN>' then 'play <moves>'\n");
		}
		else
		{
			fprintf(stderr, "%s: unknown command '%s'\n", argv[0], arg);
			free(ctx);
			return 1;
		}
		
		fflush(stdout);
	}
	
	free(ctx);
	
	printf("   (good night)\n");
	return 0;
}
