/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <termios.h>
#include <signal.h>
#include <poll.h>

#include "../moonfish.h"
#include "tools.h"

struct moonfish_ply
{
	struct moonfish_chess chess;
	char name[6];
	char best[6];
	char san[16];
	int white, black, draw;
	int score;
	int checkmate;
	int depth;
};

struct moonfish_fancy
{
	struct moonfish_ply plies[256];
	int i, count;
	pthread_mutex_t *mutex;
	char *argv0;
	int x, y;
	FILE *in, *out;
	char *fen;
	int ox, oy;
	int offset;
	char pv[32];
	int time, taken;
	int idle;
	int stop;
};

static void moonfish_fancy_square(struct moonfish_fancy *fancy, int x, int y)
{
	unsigned char piece;
	
	if (x + 1 == fancy->x && y + 1 == fancy->y)
		printf("\x1B[48;5;219m");
	else if (x % 2 == y % 2)
		printf("\x1B[48;5;111m");
	else
		printf("\x1B[48;5;69m");
	
	y = 7 - y;
	
	piece = fancy->plies[fancy->i].chess.board[(x + 1) + (y + 2) * 10];
	
	if (piece == moonfish_empty)
	{
		printf("  ");
		return;
	}
	
	if (piece / 16 == 1)
		printf("\x1B[38;5;253m");
	else
		printf("\x1B[38;5;240m");
	
	switch (piece % 16)
	{
	case 1:
		printf("\xE2\x99\x9F ");
		return;
	case 2:
		printf("\xE2\x99\x9E ");
		return;
	case 3:
		printf("\xE2\x99\x9D ");
		return;
	case 4:
		printf("\xE2\x99\x9C ");
		return;
	case 5:
		printf("\xE2\x99\x9B ");
		return;
	case 6:
		printf("\xE2\x99\x9A ");
		return;
	}
}

static void moonfish_evaluation(struct moonfish_fancy *fancy)
{
	int score;
	int white, black, draw;
	int white_mod, black_mod;
	struct moonfish_ply *ply;
	
	ply = fancy->plies + fancy->i;
	
	if (ply->checkmate != 0)
	{
		score = 1;
		white = 0;
		black = 0;
		if (ply->checkmate > 0) white = 1;
		else black = 1;
	}
	else
	{
		white = ply->white;
		black = ply->black;
		score = white + black + ply->draw;
		if (score == 0)
		{
			white = 1;
			black = 1;
			score = 2;
		}
	}
	
	white *= 16;
	black *= 16;
	
	white_mod = white % score;
	black_mod = black % score;
	
	white /= score;
	black /= score;
	
	if (ply->draw == 0)
	{
		while (16 - white - black > 0)
		{
			if (white_mod > black_mod) white++;
			else black++;
		}
	}
	
	draw = 16 - white - black;
	
	while (black > 1)
	{
		printf("\x1B[48;5;240m \x1B[0m\x1B[B\x08");
		black -= 2;
	}
	
	if (black)
	{
		if (draw)
		{
			printf("\x1B[38;5;67m\x1B[48;5;240m\xE2\x96\x84\x1B[0m\x1B[B\x08");
			draw--;
		}
		else
		{
			printf("\x1B[38;5;253m\x1B[48;5;240m\xE2\x96\x84\x1B[0m\x1B[B\x08");
			white--;
		}
	}
	
	while (draw > 1)
	{
		printf("\x1B[48;5;67m \x1B[0m\x1B[B\x08");
		draw -= 2;
	}
	
	if (draw)
	{
		printf("\x1B[38;5;253m\x1B[48;5;67m\xE2\x96\x84\x1B[0m\x1B[B\x08");
		black--;
	}
	
	while (white > 1)
	{
		printf("\x1B[48;5;253m \x1B[0m\x1B[B\x08");
		white -= 2;
	}
}

static void moonfish_scoresheet_move(struct moonfish_fancy *fancy, int i)
{
	struct moonfish_ply *ply;
	int checkmate;
	int score;
	
	ply = fancy->plies + i;
	
	if (i >= fancy->count) 
	{
		printf("%10s", "");
		return;
	}
	
	if (i == fancy->i) printf("\x1B[48;5;248m\x1B[38;5;235m");
	printf(" %s", ply->san);
	
	if (moonfish_finished(&ply->chess))
	{
		if (moonfish_checkmate(&ply->chess)) printf("\x1B[38;5;160m#  ");
		else printf("\x1B[38;5;111m(0)");
	}
	else if (ply->checkmate)
	{
		checkmate = ply->checkmate;
		if (checkmate < 0) checkmate *= -1;
		
		printf("\x1B[38;5;162m#");
		
		if (ply->checkmate > 0) printf("+");
		else printf("-");
		
		if (checkmate < 10)
			printf("%d", checkmate);
		else
			printf("X");
	}
	else if (i > 0)
	{
		score = ply->score + fancy->plies[i - 1].score;
		if (fancy->plies[i - 1].checkmate != 0 || score > 200) printf("\x1B[38;5;124m?? ");
		else if (score > 100) printf("\x1B[38;5;173m?  ");
		else printf("   ");
	}
	else
	{
		printf("   ");
	}
	
	printf("%*s\x1B[0m", 6 - (int) strlen(ply->san), "");
}

static void moonfish_scoresheet(struct moonfish_fancy *fancy)
{
	int i, j;
	
	i = fancy->offset * 2;
	if (fancy->plies[0].chess.white != 0) i++;
	
	for (j = 0 ; j < 6 ; j++)
	{
		printf("\x1B[23G");
		moonfish_scoresheet_move(fancy, i);
		moonfish_scoresheet_move(fancy, i + 1);
		printf("\x1B[B");
		i += 2;
	}
}

static void moonfish_fancy_score(struct moonfish_fancy *fancy)
{
	struct moonfish_ply *ply;
	int score;
	
	if (fancy->idle == 0) printf("\x1B[38;5;103m");
	else printf("\x1B[38;5;111m");
	printf("(+)\x1B[0m ");
	
	ply = fancy->plies + fancy->i;
	score = ply->score;
	
	if (!ply->chess.white) score *= -1;
	
	if (moonfish_checkmate(&ply->chess))
		printf("\x1B[38;5;160m#\x1B[0m");
	else if (moonfish_finished(&ply->chess))
		printf("0");
	else if (ply->checkmate != 0)
		printf("#%d", ply->checkmate);
	else if (score < 0)
		printf("-%d.%02d", -score / 100, -score % 100);
	else
		printf("%d.%02d", score / 100, score % 100);
	
	printf(" (depth %d)", ply->depth);
	if (fancy->idle == 0) printf(" in %d.%ds of %ds", fancy->taken / 1000, fancy->taken % 1000 / 100, fancy->time / 1000);
	else printf(" in %ds", fancy->time / 1000);
	printf("\x1B[0K");
}

static void moonfish_fancy(struct moonfish_fancy *fancy)
{
	unsigned char x, y;
	
	printf("\x1B[%dH", fancy->oy);
	
	for (y = 0 ; y < 8 ; y++)
	{
		printf("   ");
		for (x = 0 ; x < 8 ; x++) moonfish_fancy_square(fancy, x, y);
		printf("\x1B[0m\n");
	}
	
	printf("\x1B[%d;23H", fancy->oy);
	moonfish_fancy_score(fancy);
	
	printf("\x1B[%dH", fancy->oy + 1);
	moonfish_scoresheet(fancy);
	
	printf("\x1B[%d;21H", fancy->oy);
	moonfish_evaluation(fancy);
	
	printf("\x1B[%d;23H", fancy->oy + 7);
	printf("best:%s\x1B[0K\n", fancy->pv);
	
	fflush(stdout);
}

static void *moonfish_start(void *data)
{
	static char line[2048];
	static struct moonfish_ply ply;
	static char san[16];
	static struct moonfish_move move;
	
	struct moonfish_fancy *fancy;
	char *arg;
	int score;
	char *buffer;
	unsigned int i, length;
	int changed;
	int pv;
	
	fancy = data;
	
	pthread_mutex_lock(fancy->mutex);
	
	changed = 0;
	
	for (;;)
	{
		if (changed != 0) moonfish_fancy(fancy);
		changed = 0;
		
		pthread_mutex_unlock(fancy->mutex);
		if (fgets(line, sizeof line, fancy->out) == NULL) exit(1);
		pthread_mutex_lock(fancy->mutex);
		
		arg = strtok_r(line, "\r\n\t ", &buffer);
		if (arg == NULL) continue;
		
		if (!strcmp(arg, "bestmove"))
		{
			fancy->idle = 1;
			fancy->taken = fancy->time;
			fancy->stop = 0;
			changed = 1;
			continue;
		}
		
		if (fancy->stop != 0) continue;
		if (strcmp(arg, "info")) continue;
		
		ply = fancy->plies[fancy->i];
		ply.depth = 0;
		
		pv = 0;
		
		for (;;)
		{
			arg = strtok_r(NULL, "\r\n\t ", &buffer);
			if (arg == NULL) break;
			
			if (!strcmp(arg, "lowerbound") || !strcmp(arg, "upperbound"))
			{
				changed = 0;
				break;
			}
			
			if (!strcmp(arg, "depth"))
			{
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || moonfish_int(arg, &ply.depth) != 0 || ply.depth < 0)
				{
					fprintf(stderr, "%s: malformed 'depth' in 'info' command\n", fancy->argv0);
					exit(1);
				}
				
				continue;
			}
			
			if (!strcmp(arg, "multipv"))
			{
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || moonfish_int(arg, &pv) != 0 || pv <= 0)
				{
					fprintf(stderr, "%s: malformed 'multipv' in 'info' command\n", fancy->argv0);
					exit(1);
				}
				
				continue;
			}
			
			if (!strcmp(arg, "pv"))
			{
				changed = 1;
				fancy->idle = 0;
				
				if (pv < 1) pv = 1;
				
				i = 0;
				while (i < sizeof fancy->pv - 1)
				{
					arg = strtok_r(NULL, "\r\n\t ", &buffer);
					if (arg == NULL) break;
					if (moonfish_from_uci(&ply.chess, &move, arg) != 0)
					{
						fprintf(stderr, "%s: invalid move: %s\n", fancy->argv0, arg);
						exit(1);
					}
					if (i == 0 && pv == 1)
					{
						strcpy(ply.best, arg);
					}
					if (pv > 1)
					{
						ply.chess = move.chess;
						i = 1;
						continue;
					}
					moonfish_to_san(&ply.chess, &move, san);
					length = strlen(san);
					if (i + length > sizeof fancy->pv - 2) break;
					ply.chess = move.chess;
					fancy->pv[i++] = ' ';
					strcpy(fancy->pv + i, san);
					i += length;
				}
				
				ply.chess = fancy->plies[fancy->i].chess;
				continue;
			}
			
			if (!strcmp(arg, "wdl"))
			{
				changed = 1;
				fancy->idle = 0;
				
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || moonfish_int(arg, &ply.white) != 0 || ply.white < 0)
				{
					fprintf(stderr, "%s: malformed 'wdl' win in 'info' command\n", fancy->argv0);
					exit(1);
				}
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || moonfish_int(arg, &ply.draw) != 0 || ply.draw < 0)
				{
					fprintf(stderr, "%s: malformed 'wdl' draw in 'info' command\n", fancy->argv0);
					exit(1);
				}
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || moonfish_int(arg, &ply.black) != 0 || ply.black < 0)
				{
					fprintf(stderr, "%s: malformed 'wdl' loss in 'info' command\n", fancy->argv0);
					exit(1);
				}
				
				ply.checkmate = 0;
				
				if (!ply.chess.white)
				{
					score = ply.white;
					ply.white = ply.black;
					ply.black = score;
				}
				
				continue;
			}
			
			if (!strcmp(arg, "time"))
			{
				changed = 1;
				fancy->idle = 0;
				
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || moonfish_int(arg, &fancy->taken) != 0 || fancy->taken < 0)
				{
					fprintf(stderr, "%s: malformed 'time' in 'info' command\n", fancy->argv0);
					exit(1);
				}
				
				continue;
			}
			
			if (strcmp(arg, "score")) continue;
			changed = 1;
			fancy->idle = 0;
			
			arg = strtok_r(NULL, "\r\n\t ", &buffer);
			if (arg == NULL) break;
			
			if (!strcmp(arg, "cp"))
			{
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || moonfish_int(arg, &score) != 0)
				{
					fprintf(stderr, "%s: malformed 'cp' in 'info score' command\n", fancy->argv0);
					exit(1);
				}
				
				ply.score = score;
				ply.white = 100;
				ply.black = 100;
				ply.draw = 0;
				ply.checkmate = 0;
				
				if (score > 0) ply.white += score;
				else ply.black -= score;
				
				if (!ply.chess.white)
				{
					score = ply.white;
					ply.white = ply.black;
					ply.black = score;
				}
				
				continue;
			}
			
			if (!strcmp(arg, "mate"))
			{
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || moonfish_int(arg, &score) != 0)
				{
					fprintf(stderr, "%s: malformed 'mate' in 'info score' command\n", fancy->argv0);
					exit(1);
				}
				
				if (!ply.chess.white) score *= -1;
				
				ply.white = 0;
				ply.black = 0;
				ply.draw = 0;
				ply.checkmate = score;
				ply.score = 0;
				
				continue;
			}
		}
		
		if (changed != 0) fancy->plies[fancy->i] = ply;
	}
}

static struct termios moonfish_termios;
static FILE *moonfish_engine = NULL;

static void moonfish_exit(void)
{
	if (moonfish_engine != NULL) fprintf(moonfish_engine, "quit\n");
	tcsetattr(0, TCSANOW, &moonfish_termios);
	printf("\n\x1B[?1000l");
	fflush(stdout);
}

static void moonfish_signal(int signal)
{
	(void) signal;
	exit(1);
}

static int moonfish_move_from(struct moonfish_chess *chess, struct moonfish_move *found, int x0, int y0, int x1, int y1)
{
	y0 = 10 - y0;
	y1 = 10 - y1;
	return moonfish_move(chess, found, x0 + y0 * 10, x1 + y1 * 10);
}

static void moonfish_analyse(struct moonfish_fancy *fancy)
{
	int i;
	
	fancy->taken = 0;
	if (fancy->idle == 0) fancy->stop = 1;
	
	fprintf(fancy->in, "stop\n");
	
	fprintf(fancy->in, "position ");
	if (fancy->fen == NULL) fprintf(fancy->in, "startpos");
	else fprintf(fancy->in, "fen %s", fancy->fen);
	
	if (fancy->i > 0)
	{
		fprintf(fancy->in, " moves");
		for (i = 1 ; i <= fancy->i ; i++)
			fprintf(fancy->in, " %s", fancy->plies[i].name);
	}
	
	fprintf(fancy->in, "\n");
	fprintf(fancy->in, "go movetime %d\n", fancy->time);
	
	fancy->pv[0] = 0;
	
	moonfish_fancy(fancy);
}

static void moonfish_go(struct moonfish_fancy *fancy)
{
	fancy->time = 4000;
	moonfish_analyse(fancy);
}

static void moonfish_bump(struct moonfish_fancy *fancy)
{
	fancy->time += 2000;
	moonfish_analyse(fancy);
}

static void moonfish_scroll(struct moonfish_fancy *fancy)
{
	int i;
	i = fancy->i + 2;
	if (fancy->plies[0].chess.white != 0) i--;
	if (i < fancy->offset * 2 + 2) fancy->offset = i / 2 - 1;
	if (i > fancy->offset * 2 + 12) fancy->offset = i / 2 - 6;
	if (fancy->offset < 0) fancy->offset = 0;
}

static void moonfish_play(struct moonfish_fancy *fancy, struct moonfish_move *move)
{
	if (fancy->i + 1 >= (int) (sizeof fancy->plies / sizeof *fancy->plies)) return;
	
	fancy->i++;
	fancy->count = fancy->i + 1;
	fancy->plies[fancy->i] = fancy->plies[fancy->i - 1];
	fancy->plies[fancy->i].depth = 0;
	fancy->plies[fancy->i].score *= -1;
	fancy->plies[fancy->i].best[0] = 0;
	
	moonfish_to_uci(&fancy->plies[fancy->i].chess, move, fancy->plies[fancy->i].name);
	moonfish_to_san(&fancy->plies[fancy->i].chess, move, fancy->plies[fancy->i].san);
	
	fancy->plies[fancy->i].chess = move->chess;
	fancy->x = 0;
	
	moonfish_scroll(fancy);
	moonfish_go(fancy);
}

int main(int argc, char **argv)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	static char *format = "<UCI-options>... [--] <cmd> <args>...";
	static struct moonfish_arg args[] =
	{
		{"F", "fen", "<FEN>", NULL, "the position to analyse"},
		{NULL, NULL, NULL, NULL, NULL},
	};
	
	struct moonfish_fancy *fancy;
	pthread_t thread;
	struct termios termios;
	struct sigaction action;
	int ch, ch0;
	int x1, y1;
	struct moonfish_move move;
	int error;
	char **command;
	int command_count;
	char *value;
	char **options;
	int i;
	
	/* handle command line arguments */
	
	command = moonfish_args(args, format, argc, argv);
	command_count = argc - (command - argv);
	if (command_count < 1) moonfish_usage(args, format, argv[0]);
	options = command;
	
	for (;;)
	{
		value = strchr(*command, '=');
		if (value == NULL) break;
		
		if (strchr(*command, '\n') != NULL || strchr(*command, '\r') != NULL) moonfish_usage(args, format, argv[0]);
		
		command_count--;
		command++;
		
		if (command_count <= 0) moonfish_usage(args, format, argv[0]);
	}
	
	if (!strcmp(*command, "--"))
	{
		command_count--;
		command++;
		
		if (command_count <= 0) moonfish_usage(args, format, argv[0]);
	}
	
	/* initialise data structures */
	
	fancy = malloc(sizeof *fancy);
	if (fancy == NULL)
	{
		perror(argv[0]);
		return 1;
	}
	
	fancy->argv0 = argv[0];
	fancy->mutex = &mutex;
	fancy->offset = 0;
	fancy->pv[0] = 0;
	fancy->idle = 1;
	fancy->stop = 0;
	
	fancy->x = 0;
	fancy->y = 0;
	
	fancy->i = 0;
	fancy->count = 1;
	
	strcpy(fancy->plies[0].san, "...");
	
	fancy->plies[0].white = 0;
	fancy->plies[0].black = 0;
	fancy->plies[0].draw = 0;
	fancy->plies[0].checkmate = 0;
	fancy->plies[0].depth = 0;
	fancy->plies[0].score = 0;
	fancy->plies[0].best[0] = 0;
	
	moonfish_chess(&fancy->plies[0].chess);
	if (args[0].value == NULL)
	{
		fancy->fen = NULL;
	}
	else
	{
		fancy->fen = args[0].value;
		moonfish_from_fen(&fancy->plies[0].chess, fancy->fen);
	}
	
	/* configure the terminal for displaying the user interface */
	
	if (tcgetattr(0, &moonfish_termios))
	{
		perror(argv[0]);
		return 1;
	}
	
	if (atexit(&moonfish_exit))
	{
		moonfish_exit();
		return 1;
	}
	
	action.sa_handler = &moonfish_signal;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	
	if (sigaction(SIGTERM, &action, NULL) || sigaction(SIGINT, &action, NULL) || sigaction(SIGQUIT, &action, NULL))
	{
		perror(argv[0]);
		return 1;
	}
	
	termios = moonfish_termios;
	termios.c_lflag &= ~(ECHO | ICANON);
	
	if (tcsetattr(0, TCSANOW, &termios))
	{
		perror(argv[0]);
		return 1;
	}
	
	printf("\n\n\n\n\n\n\n\n\n");
	printf("\x1B[8A");
	
	printf("\x1B[6n");
	fflush(stdout);
	
	for (;;)
	{
		ch = getchar();
		if (ch == EOF) return 1;
		if (ch == 0x1B) break;
	}
	
	if (scanf("[%d;%dR", &fancy->oy, &fancy->ox) != 2) return 1;
	
	printf("\x1B[?1000h");
	fflush(stdout);
	
	/* begin setting up UCI bot */
	
	moonfish_spawn(command, &fancy->in, &fancy->out, NULL);
	
	fprintf(fancy->in, "uci\n");
	moonfish_wait(fancy->out, "uciok");
	
	for (;;)
	{
		value = strchr(*options, '=');
		if (value == NULL) break;
		fprintf(fancy->in, "setoption name %.*s value %s\n", (int) (value - *options), *options, value + 1);
		options++;
	}
	
	fprintf(fancy->in, "ucinewgame\n");
	
	moonfish_go(fancy);
	
	/* start thread to communicate with the bot */
	
	error = pthread_create(&thread, NULL, &moonfish_start, fancy);
	if (error != 0)
	{
		fprintf(stderr, "%s: %s\n", fancy->argv0, strerror(error));
		return 1;
	}
	
	/* main UI loop */
	
	pthread_mutex_lock(fancy->mutex);
	for (;;)
	{
		pthread_mutex_unlock(fancy->mutex);
		ch0 = getchar();
		pthread_mutex_lock(fancy->mutex);
		
		if (ch0 == EOF) break;
		
		if (ch0 != 0x1B) continue;
		ch0 = getchar();
		if (ch0 == EOF) break;
		
		if (ch0 != 0x5B) continue;
		ch0 = getchar();
		if (ch0 == EOF) break;
		
		/* handle up arrow */
		if (ch0 == 'A')
		{
			if (fancy->i == 0) continue;
			fancy->i = 0;
			moonfish_scroll(fancy);
			moonfish_go(fancy);
			continue;
		}
		
		/* handle down arrow */
		if (ch0 == 'B')
		{
			if (fancy->i == fancy->count - 1) continue;
			fancy->i = fancy->count - 1;
			moonfish_scroll(fancy);
			moonfish_go(fancy);
			continue;
		}
		
		/* handle right arrow */
		if (ch0 == 'C')
		{
			if (fancy->i == fancy->count - 1) continue;
			fancy->i++;
			moonfish_scroll(fancy);
			moonfish_go(fancy);
			continue;
		}
		
		/* handle left arrow */
		if (ch0 == 'D')
		{
			if (fancy->i == 0) continue;
			fancy->i--;
			moonfish_scroll(fancy);
			moonfish_go(fancy);
			continue;
		}
		
		/* 'M' means "mouse button" */
		/* (only handle them henceforth) */
		if (ch0 != 'M') continue;
		
		/* which mouse button? */
		ch0 = getchar();
		if (ch0 == EOF) break;
		
		/* mouse 'x' coordinate */
		ch = getchar();
		if (ch == EOF) break;
		x1 = ch - 0x21 - fancy->ox;
		
		/* mouse 'y' coordinate */
		ch = getchar();
		if (ch == EOF) break;
		y1 = ch - 0x21 - fancy->oy + 2;
		
		/* handle scroll up */
		if (ch0 == 0x60)
		{
			if (fancy->offset > 0)
			{
				fancy->offset--;
				moonfish_fancy(fancy);
			}
			continue;
		}
		
		/* handle scroll down */
		if (ch0 == 0x61)
		{
			if (fancy->offset < fancy->count / 2 - 6)
			{
				fancy->offset++;
				moonfish_fancy(fancy);
			}
			continue;
		}
		
		/* "(+)" button clicked */
		if (ch0 == 0x20 && y1 == 1 && x1 >= 21 && x1 <= 23)
		{
			moonfish_bump(fancy);
			continue;
		}
		
		/* move name clicked (on scoresheet) */
		if (ch0 == 0x20 && y1 >= 2 && y1 <= 7 && x1 >= 21 && x1 <= 40)
		{
			i = (fancy->offset + y1) * 2 - 4;
			if (fancy->plies[0].chess.white) i++;
			if (x1 > 30) i++;
			if (i < fancy->count)
			{
				fancy->i = i;
				moonfish_go(fancy);
			}
			continue;
		}
		
		/* "best move" button clicked */
		if (ch0 == 0x20 && y1 == 8 && x1 >= 21 && x1 <= 40)
		{
			if (fancy->plies[fancy->i].best[0] != 0)
			{
				if (moonfish_from_uci(&fancy->plies[fancy->i].chess, &move, fancy->plies[fancy->i].best))
				{
					fprintf(stderr, "%s: invalid best move: %s\n", fancy->argv0, fancy->plies[fancy->i].best);
					return 1;
				}
				moonfish_play(fancy, &move);
			}
			continue;
		}
		
		/* only handle clicks on the board henceforth */
		if (x1 < 2 || x1 > 17 || y1 < 1 || y1 > 8) continue;
		x1 /= 2;
		
		/* mouse down with no square selected: select the square under the mouse */
		if (ch0 == 0x20 && fancy->x == 0)
		{
			fancy->x = x1;
			fancy->y = y1;
			moonfish_fancy(fancy);
			continue;
		}
		
		/* only handle cases where a square is selected henceforth */
		if (fancy->x == 0) continue;
		
		/* handle mouse down: if the clicked square is the selected square, deselect it */
		if (ch0 == 0x20 && x1 == fancy->x && y1 == fancy->y)
		{
			fancy->x = 0;
			moonfish_fancy(fancy);
			continue;
		}
		
		/* handle mouse up or mouse down: if it forms a valid move, play it on the board */
		if (ch0 == 0x20 || ch0 == 0x23)
		if (moonfish_move_from(&fancy->plies[fancy->i].chess, &move, fancy->x, fancy->y, x1, y1) == 0)
		{
			moonfish_play(fancy, &move);
			continue;
		}
		
		/* handle mouse down: when there isn't a valid move under the mouse, just select the new square */
		if (ch0 == 0x20)
		{
			fancy->x = x1;
			fancy->y = y1;
			moonfish_fancy(fancy);
			continue;
		}
	}
	
	return 0;
}
