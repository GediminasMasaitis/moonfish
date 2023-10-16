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
	char san[12];
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
	int out_fd;
	int offset;
	char pv[32];
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
	
	if (fancy->plies[fancy->i].chess.white) y = 7 - y;
	else x = 7 - x;
	
	piece = fancy->plies[fancy->i].chess.board[(x + 1) + (y + 2) * 10];
	
	if (piece == moonfish_empty)
	{
		printf("  ");
		return;
	}
	
	if (!fancy->plies[fancy->i].chess.white) piece ^= 0x30;
	
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
	if (i >= fancy->count) 
	{
		printf("%11s", "");
		return;
	}
	if (i == fancy->i) printf("\x1B[48;5;248m\x1B[38;5;235m");
	printf(" %-7s   ", fancy->plies[i].san);
	if (i == fancy->i) printf("\x1B[0m");
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
	
	ply = fancy->plies + fancy->i;
	
	if (ply->checkmate != 0)
		printf("#%d", ply->checkmate);
	else if (ply->score < 0)
		printf("%d.%.2d", ply->score / 100, -ply->score % 100);
	else
		printf("%d.%.2d", ply->score / 100, ply->score % 100);
	
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
	
	printf("\x1B[%d;24H", fancy->oy);
	moonfish_score(fancy);
	
	printf("\x1B[%dH", fancy->oy + 1);
	moonfish_scoresheet(fancy);
	
	printf("\x1B[%d;21H", fancy->oy);
	moonfish_evaluation(fancy);
	
	printf("\x1B[%d;24H", fancy->oy + 7);
	printf("best:%s\n", fancy->pv);
	
	fflush(stdout);
}

static void *moonfish_start(void *data)
{
	static char line[2048];
	
	struct moonfish_fancy *fancy;
	char *arg;
	int score;
	char *buffer;
	struct moonfish_ply *ply;
	int depth;
	struct pollfd fds;
	unsigned int i, length;
	
	fancy = data;
	
	fds.fd = fancy->out_fd;
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
		
		ply = fancy->plies + fancy->i;
		depth = 0;
		
		for (;;)
		{
			arg = strtok_r(NULL, "\r\n\t ", &buffer);
			if (arg == NULL) break;
			
			if (!strcmp(arg, "depth"))
			{
				arg = strtok_r(NULL, "\r\n\t ", &buffer);
				if (arg == NULL || sscanf(arg, "%d", &depth) != 1)
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
					length = strlen(arg);
					if (i + length > sizeof fancy->pv - 2) break;
					fancy->pv[i++] = ' ';
					strcpy(fancy->pv + i, arg);
					i += length;
				}
				
				fancy->pv[i] = 0;
				
				break;
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
				
				ply->score = score;
				ply->white = 100;
				ply->black = 100;
				ply->depth = depth;
				ply->draw = 0;
				ply->checkmate = 0;
				
				if (score > 0) ply->white += score;
				else ply->black -= score;
				
				if (!ply->chess.white)
				{
					score = ply->white;
					ply->white = ply->black;
					ply->black = score;
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
				
				if (!ply->chess.white) score *= -1;
				
				ply->white = 0;
				ply->black = 0;
				ply->draw = 0;
				ply->checkmate = score;
				ply->score = 0;
				ply->depth = depth;
				
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
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	int valid;
	
	if (chess->white)
	{
		y0 = 9 - y0;
		y1 = 9 - y1;
	}
	else
	{
		x0 = 9 - x0;
		x1 = 9 - x1;
	}
	
	moonfish_moves(chess, moves, x0 + (y0 + 1) * 10);
	
	for (move = moves ; move->piece != moonfish_outside ; move++)
	{
		if (move->to == x1 + (y1 + 1) * 10)
		{
			moonfish_play(chess, move);
			valid = moonfish_validate(chess);
			moonfish_unplay(chess, move);
			
			if (valid)
			{
				*found = *move;
				return 0;
			}
		}
	}
	
	return 1;
}

static void moonfish_go(struct moonfish_fancy *fancy)
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
	
	fprintf(fancy->in, "go infinite\n");
	
	fancy->pv[0] = 0;
	
	pthread_mutex_unlock(fancy->read_mutex);
}

int main(int argc, char **argv)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	static pthread_mutex_t read_mutex = PTHREAD_MUTEX_INITIALIZER;
	
	int in_fd, out_fd;
	struct moonfish_fancy *fancy;
	pthread_t thread;
	struct termios termios;
	struct sigaction action;
	int ch, ch0;
	int x1, y1;
	struct moonfish_move move;
	
	if (argc < 3)
	{
		if (argc > 0) fprintf(stderr, "usage: %s <FEN> <command>\n", argv[0]);
		return 1;
	}
	
	if (moonfish_spawn(argv[0], argv + 2, &in_fd, &out_fd))
	{
		perror(argv[0]);
		return 1;
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
		moonfish_exit();
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
	fancy->out_fd = out_fd;
	fancy->offset = 0;
	fancy->pv[0] = 0;
	
	fancy->x = 0;
	fancy->y = 0;
	
	fancy->in = fdopen(in_fd, "w");
	if (fancy->in == NULL)
	{
		perror(argv[0]);
		return 1;
	}
	
	fancy->out = fdopen(out_fd, "r");
	if (fancy->out == NULL)
	{
		perror(argv[0]);
		return 1;
	}
	
	errno = 0;
	if (setvbuf(fancy->in, NULL, _IOLBF, 0))
	{
		if (errno) perror(argv[0]);
		return 1;
	}
	
	errno = 0;
	if (setvbuf(fancy->out, NULL, _IOLBF, 0))
	{
		if (errno) perror(argv[0]);
		return 1;
	}
	
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
	if (!strcmp(argv[1], "initial"))
	{
		fancy->fen = NULL;
	}
	else
	{
		fancy->fen = argv[1];
		moonfish_fen(&fancy->plies[0].chess, argv[1]);
	}
	
	fprintf(fancy->in, "uci\n");
	moonfish_wait(fancy->out, "uciok");
	
	fprintf(fancy->in, "isready\n");
	moonfish_wait(fancy->out, "readyok");
	
	fprintf(fancy->in, "ucinewgame\n");
	fprintf(fancy->in, "isready\n");
	moonfish_wait(fancy->out, "readyok");
	
	fprintf(fancy->in, "position ");
	if (fancy->fen == NULL) fprintf(fancy->in, "startpos\n");
	else fprintf(fancy->in, "fen %s\n", fancy->fen);
	
	fprintf(fancy->in, "isready\n");
	moonfish_wait(fancy->out, "readyok");
	fprintf(fancy->in, "go infinite\n");
	
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
	
	pthread_create(&thread, NULL, &moonfish_start, fancy);
	
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
		
		if (fancy->x != 0)
		if (ch0 == 0x20 || ch0 == 0x23)
		{
			ch = getchar();
			if (ch == EOF) break;
			x1 = ch - 0x21 - fancy->ox;
			
			ch = getchar();
			if (ch == EOF) break;
			y1 = ch - 0x21 - fancy->oy + 2;
			
			x1 /= 2;
			
			if (x1 < 1 || x1 > 8) x1 = 0;
			if (y1 < 1 || y1 > 8) x1 = 0;
			
			if (x1 == 0) continue;
			if (y1 == 0) continue;
			
			if (moonfish_move_from(&fancy->plies[fancy->i].chess, &move, fancy->x, fancy->y, x1, y1) == 0)
			{
				pthread_mutex_lock(fancy->mutex);
				
				fancy->i++;
				fancy->count = fancy->i + 1;
				fancy->plies[fancy->i] = fancy->plies[fancy->i - 1];
				fancy->plies[fancy->i].depth = 0;
				
				moonfish_to_uci(fancy->plies[fancy->i].name, &move, fancy->plies[fancy->i].chess.white);
				strcpy(fancy->plies[fancy->i].san, fancy->plies[fancy->i].name);
				
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
