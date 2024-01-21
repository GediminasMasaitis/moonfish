/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <termios.h>
#include <signal.h>

#include "../moonfish.h"
#include "tools.h"

struct moonfish_fancy
{
	struct moonfish_chess chess;
	int white;
	int our_time, their_time;
	long int our_nano, their_nano;
	int increment;
	char *our_name, *their_name;
	pthread_mutex_t *mutex;
	struct timespec timespec;
	char *argv0;
	int x, y;
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
	
	if (fancy->white) y = 7 - y;
	else x = 7 - x;
	
	piece = fancy->chess.board[(x + 1) + (y + 2) * 10];
	
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

static void moonfish_clock(int time, char *name)
{
	printf("   \x1B[48;5;248m\x1B[38;5;235m ");
	
	if (time < 60)
		printf("%4ds", time);
	else if (time < 3600)
		printf("%2d:%02d", time / 60, time % 60);
	else
		printf("%dh %d:%02d", time / 3600, time % 3600 / 60, time % 60);
	
	printf(" \x1B[0m %s      \n", name);
}

static void moonfish_fancy(struct moonfish_fancy *fancy)
{
	unsigned char x, y;
	int our_time, their_time;
	time_t delta;
	struct timespec timespec;
	int done;
	
	printf("\x1B[10A");
	
	if (clock_gettime(CLOCK_MONOTONIC, &timespec))
	{
		perror(fancy->argv0);
		exit(1);
	}
	
	our_time = fancy->our_time;
	their_time = fancy->their_time;
	
	delta = timespec.tv_sec - fancy->timespec.tv_sec;
	if (timespec.tv_nsec - fancy->timespec.tv_nsec > 500000000L) delta++;
	
	if (fancy->white == fancy->chess.white)
		our_time -= delta;
	else
		their_time -= delta;
	
	done = 0;
	
	if (our_time < 0)
	{
		done = 1;
		our_time = 0;
	}
	
	if (their_time < 0)
	{
		done = 1;
		their_time = 0;
	}
	
	moonfish_clock(their_time, fancy->their_name);
	
	for (y = 0 ; y < 8 ; y++)
	{
		printf("   ");
		for (x = 0 ; x < 8 ; x++)
			moonfish_fancy_square(fancy, x, y);
		printf("\x1B[0m\n");
	}
	
	moonfish_clock(our_time, fancy->our_name);
	
	if (done) exit(0);
}

static void *moonfish_start(void *data)
{
	struct moonfish_fancy *fancy;
	
	fancy = data;
	
	for (;;)
	{
		pthread_mutex_lock(fancy->mutex);
		
		moonfish_fancy(fancy);
		
		pthread_mutex_unlock(fancy->mutex);
		sleep(1);
	}
	
	return NULL;
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
	
	if (!chess->white)
	{
		x0 = 9 - x0; y0 = 9 - y0;
		x1 = 9 - x1; y1 = 9 - y1;
	}
	
	y0 = 10 - y0;
	y1 = 10 - y1;
	
	moonfish_moves(chess, moves, x0 + y0 * 10);
	
	for (move = moves ; move->piece != moonfish_outside ; move++)
	{
		if (move->to == x1 + y1 * 10)
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

static void moonfish_reset_time(struct moonfish_fancy *fancy)
{
	struct timespec prev_timespec;
	struct timespec timespec;
	int *time;
	long int *nano;
	
	if (clock_gettime(CLOCK_MONOTONIC, &timespec))
	{
		perror(fancy->argv0);
		exit(1);
	}
	
	if (fancy->white)
	{
		if (fancy->chess.white) time = &fancy->their_time, nano = &fancy->their_nano;
		else time = &fancy->our_time, nano = &fancy->our_nano;
	}
	else
	{
		if (fancy->chess.white) time = &fancy->our_time, nano = &fancy->our_nano;
		else time = &fancy->their_time, nano = &fancy->their_nano;
	}
	
	prev_timespec = fancy->timespec;
	fancy->timespec = timespec;
	
	timespec.tv_sec -= prev_timespec.tv_sec;
	timespec.tv_nsec -= prev_timespec.tv_nsec;
	
	while (timespec.tv_nsec < 0)
	{
		timespec.tv_nsec += 1000000000L;
		timespec.tv_sec--;
	}
	
	*time -= timespec.tv_sec;
	*nano -= timespec.tv_nsec;
	
	while (*nano < 0)
	{
		*nano += 1000000000L;
		(*time)--;
	}
	
	*time += fancy->increment;
}

static void moonfish_go(struct moonfish_fancy *fancy, char *names, char *name, FILE *in, FILE *out)
{
	int white_time, black_time;
	char *arg;
	struct moonfish_move move;
	
	if (fancy->white == fancy->chess.white)
	{
		white_time = fancy->our_time * 1000 + fancy->our_nano / 1000000;
		black_time = fancy->their_time * 1000 + fancy->their_nano / 1000000;
	}
	else
	{
		white_time = fancy->their_time * 1000 + fancy->their_nano / 1000000;
		black_time = fancy->our_time * 1000 + fancy->our_nano / 1000000;
	}
	
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	
	fprintf(in, "position startpos");
	if (names[0] != 0) fprintf(in, " moves %s", names);
	fprintf(in, "\n");
	
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	
	fprintf(in, "go wtime %d btime %d", white_time, black_time);
	if (fancy->increment > 0)
		fprintf(in, " winc %d binc %d", fancy->increment * 1000, fancy->increment * 1000);
	fprintf(in, "\n");
	
	arg = moonfish_wait(out, "bestmove");
	strcpy(name, arg);
	
	pthread_mutex_lock(fancy->mutex);
	moonfish_from_uci(&fancy->chess, &move, arg);
	moonfish_play(&fancy->chess, &move);
}

int main(int argc, char **argv)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	static char names[4096] = "";
	
	FILE *in, *out;
	struct moonfish_fancy *fancy;
	char *arg;
	pthread_t thread;
	struct termios termios;
	struct sigaction action;
	int ch, ch0;
	int ox, oy;
	int x1, y1;
	char *name;
	struct moonfish_move move;
	int error;
	
	name = names;
	
	if (argc < 3)
	{
		if (argc > 0) fprintf(stderr, "usage: %s <time-control> <command> <args>...\n", argv[0]);
		return 1;
	}
	
	moonfish_spawn(argv[0], argv + 2, &in, &out);
	
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
	fancy->white = time(NULL) % 2;
	
	fancy->x = 0;
	fancy->y = 0;
	
	fancy->their_name = argv[2];
	fancy->our_name = getlogin();
	if (fancy->our_name == NULL) fancy->our_name = "you";
	
	if (sscanf(argv[1], "%d+%d", &fancy->our_time, &fancy->increment) != 2)
	{
		fprintf(stderr, "%s: unknown time control '%s'\n", argv[0], argv[1]);
		return 1;
	}
	
	fancy->our_nano = 0;
	fancy->their_nano = 0;
	
	fancy->our_time *= 60;
	fancy->their_time = fancy->our_time;
	
	fprintf(in, "uci\n");
	
	for (;;)
	{
		arg = moonfish_next(out);
		if (arg == NULL)
		{
			fprintf(stderr, "%s: UCI error\n", argv[0]);
			return 1;
		}
		
		arg = strtok(arg, "\r\n\t ");
		if (arg == NULL) continue;
		if (!strcmp(arg, "uciok")) break;
		
		if (strcmp(arg, "id")) continue;
		
		arg = strtok(NULL, "\r\n\t ");
		if (arg == NULL) continue;
		
		if (strcmp(arg, "name")) continue;
		arg = strtok(NULL, "\r\n");
		
		if (*arg != 0) fancy->their_name = strdup(arg);
	}
	
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	fprintf(in, "ucinewgame\n");
	fprintf(in, "isready\n");
	moonfish_wait(out, "readyok");
	
	if (clock_gettime(CLOCK_MONOTONIC, &fancy->timespec))
	{
		perror(argv[0]);
		return 1;
	}
	
	printf("\n\n\n\n\n\n\n\n\n\n\n");
	
	printf("\x1B[6n");
	fflush(stdout);
	
	for (;;)
	{
		ch = getchar();
		if (ch == EOF) return 1;
		if (ch == 0x1B) break;
	}
	
	if (scanf("[%d;%dR", &oy, &ox) != 2) return 1;
	oy -= 11;
	
	moonfish_chess(&fancy->chess);
	
	error = pthread_create(&thread, NULL, &moonfish_start, fancy);
	if (error != 0)
	{
		fprintf(stderr, "%s: %s\n", fancy->argv0, strerror(error));
		exit(1);
	}
	
	if (fancy->white != fancy->chess.white)
	{
		*name++ = ' ';
		moonfish_go(fancy, names + 1, name, in, out);
		name += strlen(name);
		moonfish_reset_time(fancy);
		moonfish_fancy(fancy);
		pthread_mutex_unlock(fancy->mutex);
	}
	
	printf("\x1B[?1000h");
	fflush(stdout);
	
	for (ch0 = 0 ; ch0 != EOF ; ch0 = getchar())
	{
		if (fancy->white && !fancy->chess.white) continue;
		if (!fancy->white && fancy->chess.white) continue;
		
		if (ch0 != 0x1B) continue;
		ch0 = getchar();
		if (ch0 == EOF) break;
		
		if (ch0 != 0x5B) continue;
		ch0 = getchar();
		if (ch0 == EOF) break;
		
		if (ch0 != 0x4D) continue;
		ch0 = getchar();
		if (ch0 == EOF) break;
		
		if (ch0 == 0x20 && fancy->x == 0)
		{
			pthread_mutex_lock(fancy->mutex);
			
			ch = getchar();
			if (ch == EOF) break;
			fancy->x = ch - 0x21 - ox;
			
			ch = getchar();
			if (ch == EOF) break;
			fancy->y = ch - 0x21 - oy;
			
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
			x1 = ch - 0x21 - ox;
			
			ch = getchar();
			if (ch == EOF) break;
			y1 = ch - 0x21 - oy;
			
			x1 /= 2;
			
			if (x1 < 1 || x1 > 8) x1 = 0;
			if (y1 < 1 || y1 > 8) x1 = 0;
			
			if (x1 == 0) continue;
			if (y1 == 0) continue;
			
			if (moonfish_move_from(&fancy->chess, &move, fancy->x, fancy->y, x1, y1) == 0)
			{
				*name++ = ' ';
				moonfish_to_uci(name, &move);
				name += strlen(name);
				
				pthread_mutex_lock(fancy->mutex);
				
				fancy->x = 0;
				moonfish_play(&fancy->chess, &move);
				moonfish_reset_time(fancy);
				moonfish_fancy(fancy);
				
				pthread_mutex_unlock(fancy->mutex);
				
				printf("\x1B[?1000l");
				fflush(stdout);
				
				*name++ = ' ';
				moonfish_go(fancy, names + 1, name, in, out);
				name += strlen(name);
				moonfish_reset_time(fancy);
				moonfish_fancy(fancy);
				pthread_mutex_unlock(fancy->mutex);
				
				printf("\x1B[?1000h");
				fflush(stdout);
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
