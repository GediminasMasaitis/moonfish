/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../moonfish.h"
#include "tools.h"

struct moonfish_bot
{
	char *name;
	FILE *in, *out;
	long int time, increment;
	int fixed_time;
	int ugi;
};

struct moonfish_battle
{
	struct moonfish_chess chess;
	struct moonfish_bot white, black;
	char *fen;
	int ugi;
	char **moves;
	int move_count;
};

static void moonfish_usage0(char *argv0)
{
	fprintf(stderr, "usage: %s <options>... '[' <options>... <cmd> <args>... ']' '[' <options>... <cmd> <args>... ']'\n", argv0);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "   --fen=<FEN>   the starting position of the game\n");
	fprintf(stderr, "bot options:\n");
	fprintf(stderr, "   --protocol=<protocol>   either 'uci' or 'ugi' (default: 'uci')\n");
	fprintf(stderr, "   --time=<time-control>   the time and increment for the bot in milliseconds\n");
	fprintf(stderr, "   --name=<name>           name to be reported in the logs and in the output PGN\n");
	exit(1);
}

static char *moonfish_option(char *name, char ***argv)
{
	size_t length;
	
	if (**argv == NULL) return NULL;
	
	if (!strcmp(**argv, name))
	{
		(*argv)++;
		return *(*argv)++;
	}
	
	length = strlen(name);
	if (!strncmp(**argv, name, length) && (**argv)[length] == '=')
		return *(*argv)++ + length + 1;
	
	return NULL;
}

static void moonfish_battle_options(struct moonfish_battle *battle, char ***argv)
{
	char *arg;
	
	for (;;)
	{
		arg = moonfish_option("--fen", argv);
		if (arg != NULL)
		{
			battle->fen = arg;
			continue;
		}
		
		break;
	}
}

static void moonfish_bot_options(char *argv0, struct moonfish_bot *bot, char ***argv)
{
	char *arg;
	
	for (;;)
	{
		arg = moonfish_option("--time", argv);
		if (arg != NULL)
		{
			bot->fixed_time = 0;
			if (arg[0] == 'x')
			{
				bot->fixed_time = 1;
				arg++;
			}
			
			if (sscanf(arg, "%ld%ld", &bot->time, &bot->increment) != 2)
				moonfish_usage0(argv0);
			
			continue;
		}
		
		arg = moonfish_option("--protocol", argv);
		if (arg != NULL)
		{
			if (!strcmp(arg, "uci"))
				bot->ugi = 0;
			else if (!strcmp(arg, "ugi"))
				bot->ugi = 1;
			else
				moonfish_usage0(argv0);
			
			continue;
		}
		
		arg = moonfish_option("--name", argv);
		if (arg != NULL)
		{
			bot->name = arg;
			continue;
		}
		
		break;
	}
}

static void moonfish_bot_arguments(char *argv0, struct moonfish_bot *bot, char ***argv)
{
	size_t i, j;
	char **options, **command, *value;
	char *line, *arg;
	char *buffer;
	
	if (**argv == NULL) moonfish_usage0(argv0);
	
	for (i = 0 ; (**argv)[i] != '\0' ; i++)
		if ((**argv)[i] != '[') moonfish_usage0(argv0);
	
	if (i < 1) moonfish_usage0(argv0);
	
	bot->name = NULL;
	
	(*argv)++;
	moonfish_bot_options(argv0, bot, argv);
	
	if (**argv != NULL)
	{
		if (!strcmp(**argv, "--"))
			(*argv)++;
		else if (***argv == '-')
			moonfish_usage0(argv0);
	}
	
	options = NULL;
	
	j = 0;
	for (;;)
	{
		options = realloc(options, (j + 2) * sizeof *options);
		if (options == NULL)
		{
			perror(argv0);
			exit(1);
		}
		
		if (**argv == NULL) moonfish_usage0(argv0);
		options[j++] = *(*argv)++;
		
		if (strlen(**argv) != i) continue;
		if (strspn(**argv, "]") != i) continue;
		
		if (**argv == NULL) moonfish_usage0(argv0);
		(*argv)++;
		break;
	}
	
	if (j < 1) moonfish_usage0(argv0);
	
	options[j] = NULL;
	
	command = options;
	
	while (strchr(*command, '=') != NULL)
		command++;
	if (*command != NULL && !strcmp(*command, "--"))
		command++;
	
	moonfish_spawn(command, &bot->in, &bot->out, NULL);
	
	if (bot->ugi) fprintf(bot->in, "ugi\n");
	else fprintf(bot->in, "uci\n");
	
	for (;;)
	{
		line = moonfish_next(bot->out);
		if (line == NULL) exit(1);
		
		arg = strtok_r(line, "\r\n\t ", &buffer);
		if (arg == NULL) continue;
		
		if (!strcmp(arg, bot->ugi ? "ugiok" : "uciok")) break;
		
		if (bot->name != NULL) continue;
		
		if (strcmp(arg, "id")) continue;
		
		arg = strtok_r(NULL, "\r\n\t ", &buffer);
		if (arg == NULL) continue;
		if (strcmp(arg, "name")) continue;
		
		arg = strtok_r(NULL, "\r\n", &buffer);
		if (arg == NULL) continue;
		
		bot->name = strdup(arg);
		if (bot->name == NULL)
		{
			perror(argv0);
			exit(1);
		}
	}
	
	if (bot->name == NULL) bot->name = command[0];
	
	j = 0;
	for (;;)
	{
		value = strchr(options[j], '=');
		if (value == NULL) break;
		fprintf(bot->in, "setoption name %.*s value %s\n", (int) (value - options[j]), options[j], value + 1);
		j++;
	}
	
	free(options);
	
	fprintf(bot->in, "isready\n");
	moonfish_wait(bot->out, "readyok");
	
	if (bot->ugi) fprintf(bot->in, "uginewgame\n");
	else fprintf(bot->in, "ucinewgame\n");
}

static void moonfish_bot_play(char *argv0, struct moonfish_battle *battle, struct moonfish_bot *bot)
{
	static char name[16];
	
	char *white, *black;
	int i;
	char *arg;
	char *buffer;
	struct moonfish_move move;
	struct timespec t0, t1;
	
	if (!bot->fixed_time && clock_gettime(CLOCK_MONOTONIC, &t0))
	{
		perror(argv0);
		exit(1);
	}
	
	fprintf(bot->in, "isready\n");
	moonfish_wait(bot->out, "readyok");
	
	if (battle->fen)
		fprintf(bot->in, "position fen %s", battle->fen);
	else
		fprintf(bot->in, "position startpos");
	
	if (battle->move_count > 0)
	{
		fprintf(bot->in, " moves");
		for (i = 0 ; i < battle->move_count ; i++)
			fprintf(bot->in, " %s", battle->moves[i]);
	}
	
	fprintf(bot->in, "\n");
	
	if (bot->ugi)
		white = "p1",
		black = "p2";
	else
		white = "w",
		black = "b";
	
	fprintf(bot->in, "go");
	fprintf(bot->in, " %stime %ld", white, battle->white.time);
	if (battle->white.increment > 0) fprintf(bot->in, " %sinc %ld", white, battle->white.increment);
	fprintf(bot->in, " %stime %ld", black, battle->black.time);
	if (battle->black.increment > 0) fprintf(bot->in, " %sinc %ld", black, battle->black.increment);
	fprintf(bot->in, "\n");
	
	arg = moonfish_wait(bot->out, "bestmove");
	arg = strtok_r(arg, "\r\n\t ", &buffer);
	if (arg == NULL)
	{
		fprintf(stderr, "%s: invalid 'bestmove'\n", argv0);
		exit(1);
	}
	arg = strdup(arg);
	if (arg == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	battle->moves = realloc(battle->moves, (battle->move_count + 1) * sizeof *battle->moves);
	if (battle->moves == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	battle->moves[battle->move_count++] = arg;
	if (!battle->ugi)
	{
		if (moonfish_from_uci(&battle->chess, &move, arg))
		{
			fprintf(stderr, "%s: invalid move '%s' from the bot\n", argv0, arg);
			exit(1);
		}
		
		moonfish_to_san(&battle->chess, &move, name);
		printf("%s ", name);
		
		battle->chess = move.chess;
	}
	else
	{
		printf("%s ", arg);
	}
	
	if (!bot->fixed_time)
	{
		if (clock_gettime(CLOCK_MONOTONIC, &t1))
		{
			perror(argv0);
			exit(1);
		}
		
		bot->time += bot->increment;
		bot->time -= (t1.tv_sec - t0.tv_sec) * 1000;
		bot->time -= (t1.tv_nsec - t0.tv_nsec) / 1000000;
	}
}

static int moonfish_battle_play(char *argv0, struct moonfish_battle *battle)
{
	int white;
	char *arg;
	char *buffer;
	
	if (battle->ugi)
	{
		fprintf(battle->white.in, "isready\n");
		moonfish_wait(battle->white.out, "readyok");
		
		fprintf(battle->white.in, "query result\n");
		arg = moonfish_wait(battle->white.out, "response");
		arg = strtok_r(arg, "\r\n\t ", &buffer);
		if (arg == NULL)
		{
			fprintf(stderr, "%s: invalid 'response'\n", argv0);
			exit(1);
		}
		
		if (!strcmp(arg, "p1win"))
		{
			printf("1-0\n");
			return 0;
		}
		if (!strcmp(arg, "p2win"))
		{
			printf("0-1\n");
			return 0;
		}
		if (!strcmp(arg, "draw"))
		{
			printf("1/2-1/2\n");
			return 0;
		}
		
		fprintf(battle->white.in, "isready\n");
		moonfish_wait(battle->white.out, "readyok");
		
		fprintf(battle->white.in, "query p1turn\n");
		arg = moonfish_wait(battle->white.out, "response");
		arg = strtok_r(arg, "\r\n\t ", &buffer);
		if (arg == NULL)
		{
			fprintf(stderr, "%s: invalid 'response'\n", argv0);
			exit(1);
		}
		
		if (!strcmp(arg, "true"))
			white = 1;
		else
			white = 0;
	}
	else
	{
		white = battle->chess.white;
		if (moonfish_finished(&battle->chess))
		{
			if (moonfish_check(&battle->chess))
			{
				if (white) printf("0-1\n");
				else printf("1-0\n");
			}
			else
			{
				printf("1/2-1/2\n");
			}
			
			return 0;
		}
	}
	
	if (white)
	{
		moonfish_bot_play(argv0, battle, &battle->white);
		if (battle->white.time < -125)
		{
			printf("0-1 {timeout}\n");
			return 0;
		}
		if (battle->white.time < 10) battle->white.time = 10;
	}
	else
	{
		moonfish_bot_play(argv0, battle, &battle->black);
		if (battle->black.time < -125)
		{
			printf("1-0 {timeout}\n");
			return 0;
		}
		if (battle->black.time < 10) battle->black.time = 10;
	}
	
	return 1;
}

int main(int argc, char **argv)
{
	static struct moonfish_battle battle;
	
	char *argv0;
	
	(void) argc;
	
	moonfish_spawner(argv[0]);
	
	battle.fen = NULL;
	
	battle.white.time = 15 * 60000;
	battle.white.increment = 10000;
	battle.white.fixed_time = 0;
	battle.white.ugi = 0;
	
	battle.black.time = 15 * 60000;
	battle.black.increment = 10000;
	battle.black.fixed_time = 0;
	battle.black.ugi = 0;
	
	argv0 = *argv++;
	moonfish_battle_options(&battle, &argv);
	
	moonfish_bot_arguments(argv0, &battle.white, &argv);
	moonfish_bot_arguments(argv0, &battle.black, &argv);
	
	if (*argv != NULL) moonfish_usage0(argv0);
	
	battle.ugi = 0;
	if (battle.white.ugi && battle.black.ugi)
		battle.ugi = 1;
	
	if (!battle.ugi)
	{
		moonfish_chess(&battle.chess);
		if (battle.fen && moonfish_from_fen(&battle.chess, battle.fen)) moonfish_usage0(argv0);
	}
	
	battle.moves = NULL;
	battle.move_count = 0;
	
	fprintf(stderr, "starting %s vs %s\n", battle.white.name, battle.black.name);
	
	printf("[White \"%s\"]\n", battle.white.name);
	printf("[Black \"%s\"]\n", battle.black.name);
	if (battle.fen != NULL) printf("[FEN \"%s\"]\n", battle.fen);
	while (moonfish_battle_play(argv0, &battle)) { }
	printf("\n");
	
	fprintf(stderr, "finished %s vs %s\n", battle.white.name, battle.black.name);
	return 0;
}
