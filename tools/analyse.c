/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <string.h>
#include <errno.h>
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
	pthread_mutex_t *mutex, *read_mutex;
	char *argv0;
	int x, y;
	FILE *in, *out;
	char *fen;
	int ox, oy;
	int offset;
	char pv[32];
	int time;
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
	
	if (piece >> 4 == 1)
		printf("\x1B[38;5;253m");
	else
		printf("\x1B[38;5;240m");
	
	switch (piece & 0xF)
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
		printf("\x1B[48;5;240m \x1B[0m\x1B[B\x08"),
		black -= 2;
	
	if (black)
	{
		if (draw)
			printf("\x1B[38;5;67m\x1B[48;5;240m\xE2\x96\x84\x1B[0m\x1B[B\x08"),
			draw--;
		else
			printf("\x1B[38;5;253m\x1B[48;5;240m\xE2\x96\x84\x1B[0m\x1B[B\x08"),
			white--;
	}
	
	while (draw > 1)
		printf("\x1B[48;5;67m \x1B[0m\x1B[B\x08"),
		draw -= 2;
	
	if (draw)
	{
		printf("\x1B[38;5;253m\x1B[48;5;67m\xE2\x96\x84\x1B[0m\x1B[B\x08");
		black--;
	}
	
	while (white > 1)
		printf("\x1B[48;5;253m \x1B[0m\x1B[B\x08"),
		white -= 2;
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
	
	if (ply->checkmate)
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
		if (fancy->plies[i - 1].checkmate || score > 200) printf("\x1B[38;5;124m?? ");
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
	
	if (fancy->plies[0].chess.white) i = 1;
	else i = 0;
	
	if (fancy->i + 2 - i < fancy->offset * 2 + 2)
		fancy->offset = (fancy->i + 2 - i) / 2 - 1;
	if (fancy->i + 2 - i > fancy->offset * 2 + 12)
		fancy->offset = (fancy->i + 2 - i) / 2 - 6;
	if (fancy->offset < 0) fancy->offset = 0;
	
	i += fancy->offset * 2;
	
	for (j = 0 ; j < 6 ; j++)
	{
		printf("\x1B[23G");
		moonfish_scoresheet_move(fancy, i);
		moonfish_scoresheet_move(fancy, i + 1);
		printf("\x1B[B");
		i += 2;
	}
}

static void moonfish_score(struct moonfish_fancy *fancy)
{
	struct moonfish_ply *ply;
	int score;
	
	printf("\x1B[38;5;111m(+)\x1B[0m ");
	
	ply = fancy->plies + fancy->i;
	score = ply->score;
	
	if (!ply->chess.white) score *= -1;
	
	if (ply->checkmate != 0)
		printf("#%d", ply->checkmate);
	else if (score < 0)
		printf("-%d.%.2d", -score / 100, -score % 100);
	else
		printf("%d.%.2d", score / 100, score % 100);
	
	printf(" (depth %d) %4s", ply->depth, "");
}

static void moonfish_fancy(struct moonfish_fancy *fancy)
{
	unsigned char x, y;
	
	printf("\x1B[%dH", fancy->oy);
	
	for (y = 0 ; y < 8 ; y++)
	{
		printf("   ");
		for (x = 0 ; x < 8 ; x++)
			moonfish_fancy_square(fancy, x, y);
		printf("\x1B[0m\n");
	}
	
	printf("\x1B[%d;23H", fancy->oy);
	moonfish_score(fancy);
	
	printf("\x1B[%dH", fancy->oy + 1);
	moonfish_scoresheet(fancy);
	
	printf("\x1B[%d;21H", fancy->oy);
	moonfish_evaluation(fancy);
	
	printf("\x1B[%d;23H", fancy->oy + 7);
	printf("best:%s%32s\n", fancy->pv, "");
	
	fflush(stdout);
}

static void *moonfish_start(void *data)
{
	static char line[2048];
	static struct moonfish_ply ply;
	static char san[16];
	
	struct moonfish_fancy *fancy;
	char *arg;
	int score;
	char *buffer;
	struct pollfd fds;
	unsigned int i, length;
	struct moonfish_move move;
	
	fancy = data;
	
	fds.fd = fileno(fancy->out);
	if (fds.fd < 0)
	{
		perror(fancy->argv0);
		exit(1);
	}
	
	fds.events = POLLIN;
	
	pthread_mutex_lock(fancy->mutex);
	
	for (;;)
	{
		moonfish_fancy(fancy);
		pthread_mutex_unlock(fancy->mutex);
		
		pthread_mutex_lock(fancy->read_mutex);
		if (poll(&fds, 1, 250) == -1)
		{
			perror(fancy->argv0);
			exit(1);
		}
		if ((fds.revents & POLLIN) == 0)
		{
			pthread_mutex_unlock(fancy->read_mutex);
			pthread_mutex_lock(fancy->mutex);
			continue;
		}
		if (fgets(line, sizeof line, fancy->out) == NULL) exit(1);
		pthread_mutex_unlock(fancy->read_mutex);
		
		pthread_mutex_lock(fancy->mutex);
		
		arg = strtok_r(line, "\r\n\t ", &buffer);
		if (arg == NULL) continue;
		
		ply = fancy->plies[fancy->i];
		ply.depth = 0;
		
		for (;;)
		{
			arg = strtok_r(NULL, "\r\n\t ", &buffer);
			if (arg == NULL) break;
			
			if (!strcmp(arg, "lowerbound")) break;
			if (!strcmp(arg, "upperbound")) break;
			
			if (!strcmp(arg, "depth"))
			{
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || sscanf(arg, "%d", &ply.depth) != 1)
				{
					fprintf(stderr, "%s: malformed 'depth' in 'info' command\n", fancy->argv0);
					exit(1);
				}
				
				continue;
			}
			
			if (!strcmp(arg, "pv"))
			{
				i = 0;
				while (i < sizeof fancy->pv - 1)
				{
					arg = strtok_r(NULL, "\r\n\t ", &buffer);
					if (arg == NULL) break;
					moonfish_from_uci(&ply.chess, &move, arg);
					moonfish_to_san(&ply.chess, &move, san);
					length = strlen(san);
					if (i + length > sizeof fancy->pv - 2) break;
					moonfish_play(&ply.chess, &move);
					fancy->pv[i++] = ' ';
					strcpy(fancy->pv + i, san);
					i += length;
				}
				
				fancy->plies[fancy->i].score = ply.score;
				fancy->plies[fancy->i].white = ply.white;
				fancy->plies[fancy->i].black = ply.black;
				fancy->plies[fancy->i].draw = ply.draw;
				fancy->plies[fancy->i].checkmate = ply.checkmate;
				fancy->plies[fancy->i].depth = ply.depth;
				
				break;
			}
			
			if (!strcmp(arg, "wdl"))
			{
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || sscanf(arg, "%d", &ply.white) != 1)
				{
					fprintf(stderr, "%s: malformed 'wdl' win in 'info' command\n", fancy->argv0);
					exit(1);
				}
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || sscanf(arg, "%d", &ply.draw) != 1)
				{
					fprintf(stderr, "%s: malformed 'wdl' draw in 'info' command\n", fancy->argv0);
					exit(1);
				}
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || sscanf(arg, "%d", &ply.black) != 1)
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
			
			if (strcmp(arg, "score")) continue;
			
			arg = strtok_r(NULL, "\r\n\t ", &buffer);
			if (arg == NULL) break;
			
			if (!strcmp(arg, "cp"))
			{
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || sscanf(arg, "%d", &score) != 1)
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
				if (arg == NULL || sscanf(arg, "%d", &score) != 1)
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
	}
}

static struct termios moonfish_termios;
static FILE *moonfish_engine = NULL;

static void moonfish_exit(void)
{
	tcsetattr(0, TCSANOW, &moonfish_termios);
	printf("\x1B[?1000l");
	fflush(stdout);
	if (moonfish_engine != NULL) fprintf(moonfish_engine, "quit\n");
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
	
	pthread_mutex_lock(fancy->read_mutex);
	
	fprintf(fancy->in, "stop\n");
	fprintf(fancy->in, "isready\n");
	moonfish_wait(fancy->out, "readyok");
	
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
	
	fprintf(fancy->in, "isready\n");
	moonfish_wait(fancy->out, "readyok");
	fprintf(fancy->in, "go movetime %d\n", fancy->time);
	
	fancy->pv[0] = 0;
	
	pthread_mutex_unlock(fancy->read_mutex);
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

int main(int argc, char **argv)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	static pthread_mutex_t read_mutex = PTHREAD_MUTEX_INITIALIZER;
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
		
		if (!strcmp(*command, "--"))
		{
			command_count--;
			command++;
			
			if (command_count <= 0) moonfish_usage(args, format, argv[0]);
			break;
		}
	}
	
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
	
	fancy = malloc(sizeof *fancy);
	if (fancy == NULL)
	{
		perror(argv[0]);
		return 1;
	}
	
	fancy->argv0 = argv[0];
	fancy->mutex = &mutex;
	fancy->read_mutex = &read_mutex;
	fancy->offset = 0;
	fancy->pv[0] = 0;
	
	fancy->x = 0;
	fancy->y = 0;
	
	moonfish_spawn(argv[0], command, &fancy->in, &fancy->out, NULL);
	
	fancy->i = 0;
	fancy->count = 1;
	
	strcpy(fancy->plies[0].san, "...");
	
	fancy->plies[0].white = 0;
	fancy->plies[0].black = 0;
	fancy->plies[0].draw = 0;
	fancy->plies[0].checkmate = 0;
	fancy->plies[0].depth = 0;
	fancy->plies[0].score = 0;
	
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
	
	fprintf(fancy->in, "uci\n");
	moonfish_wait(fancy->out, "uciok");
	
	for (;;)
	{
		value = strchr(*options, '=');
		if (value == NULL) break;
		fprintf(fancy->in, "setoption name %.*s value %s\n", (int) (value - *options), *options, value + 1);
		options++;
	}
	
	fprintf(fancy->in, "isready\n");
	moonfish_wait(fancy->out, "readyok");
	
	fprintf(fancy->in, "ucinewgame\n");
	
	moonfish_go(fancy);
	
	printf("\n\n\n\n\n\n\n\n\n\n\n");
	printf("\x1B[10A");
	
	printf("\x1B[6n");
	fflush(stdout);
	
	for (;;)
	{
		ch = getchar();
		if (ch == EOF) return 1;
		if (ch == 0x1B) break;
	}
	
	if (scanf("[%d;%dR", &fancy->oy, &fancy->ox) != 2) return 1;
	
	error = pthread_create(&thread, NULL, &moonfish_start, fancy);
	if (error != 0)
	{
		fprintf(stderr, "%s: %s\n", fancy->argv0, strerror(error));
		exit(1);
	}
	
	printf("\x1B[?1000h");
	fflush(stdout);
	
	for (ch0 = 0 ; ch0 != EOF ; ch0 = getchar())
	{
		if (ch0 != 0x1B) continue;
		ch0 = getchar();
		if (ch0 == EOF) break;
		
		if (ch0 != 0x5B) continue;
		ch0 = getchar();
		if (ch0 == EOF) break;
		
		if (ch0 == 'A')
		{
			if (fancy->i == 0) continue;
			pthread_mutex_lock(fancy->mutex);
			fancy->i = 0;
			moonfish_go(fancy);
			moonfish_fancy(fancy);
			pthread_mutex_unlock(fancy->mutex);
			continue;
		}
		
		if (ch0 == 'B')
		{
			if (fancy->i == fancy->count - 1) continue;
			pthread_mutex_lock(fancy->mutex);
			fancy->i = fancy->count - 1;
			moonfish_go(fancy);
			moonfish_fancy(fancy);
			pthread_mutex_unlock(fancy->mutex);
			continue;
		}
		
		if (ch0 == 'C')
		{
			if (fancy->i == fancy->count - 1) continue;
			pthread_mutex_lock(fancy->mutex);
			fancy->i++;
			moonfish_go(fancy);
			moonfish_fancy(fancy);
			pthread_mutex_unlock(fancy->mutex);
			continue;
		}
		
		if (ch0 == 'D')
		{
			if (fancy->i == 0) continue;
			pthread_mutex_lock(fancy->mutex);
			fancy->i--;
			moonfish_go(fancy);
			moonfish_fancy(fancy);
			pthread_mutex_unlock(fancy->mutex);
			continue;
		}
		
		if (ch0 != 0x4D) continue;
		ch0 = getchar();
		if (ch0 == EOF) break;
		
		if (ch0 == 0x20 && fancy->x == 0)
		{
			pthread_mutex_lock(fancy->mutex);
			
			ch = getchar();
			if (ch == EOF) break;
			fancy->x = ch - 0x21 - fancy->ox;
			
			ch = getchar();
			if (ch == EOF) break;
			fancy->y = ch - 0x21 - fancy->oy + 2;
			
			fancy->x /= 2;
			
			if (fancy->x < 1 || fancy->x > 8) fancy->x = 0;
			if (fancy->y < 1 || fancy->y > 8) fancy->x = 0;
			
			moonfish_fancy(fancy);
			
			pthread_mutex_unlock(fancy->mutex);
			
			continue;
		}
		
		if (ch0 == 0x20 || ch0 == 0x23)
		{
			ch = getchar();
			if (ch == EOF) break;
			x1 = ch - 0x21 - fancy->ox;
			
			ch = getchar();
			if (ch == EOF) break;
			y1 = ch - 0x21 - fancy->oy + 2;
			
			if (y1 == 1 && x1 >= 21 && x1 <= 23)
			{
				moonfish_bump(fancy);
				moonfish_fancy(fancy);
				continue;
			}
			
			if (fancy->x == 0) continue;
			
			x1 /= 2;
			
			if (x1 < 1 || x1 > 8) x1 = 0;
			if (y1 < 1 || y1 > 8) x1 = 0;
			
			if (x1 == 0) continue;
			
			if (moonfish_move_from(&fancy->plies[fancy->i].chess, &move, fancy->x, fancy->y, x1, y1) == 0)
			{
				pthread_mutex_lock(fancy->mutex);
				
				fancy->i++;
				fancy->count = fancy->i + 1;
				fancy->plies[fancy->i] = fancy->plies[fancy->i - 1];
				fancy->plies[fancy->i].depth = 0;
				fancy->plies[fancy->i].score *= -1;
				
				moonfish_to_uci(&move, fancy->plies[fancy->i].name);
				moonfish_to_san(&fancy->plies[fancy->i].chess, &move, fancy->plies[fancy->i].san);
				
				moonfish_play(&fancy->plies[fancy->i].chess, &move);
				fancy->x = 0;
				moonfish_fancy(fancy);
				
				moonfish_go(fancy);
				
				pthread_mutex_unlock(fancy->mutex);
			}
			else
			{
				pthread_mutex_lock(fancy->mutex);
				
				if (ch0 == 0x20 && x1 == fancy->x && y1 == fancy->y)
					fancy->x = 0;
				else
					fancy->x = x1,
					fancy->y = y1;
				
				x1 = 0;
				y1 = 0;
				
				moonfish_fancy(fancy);
				
				pthread_mutex_unlock(fancy->mutex);
			}
		}
	}
	
	return 0;
}
