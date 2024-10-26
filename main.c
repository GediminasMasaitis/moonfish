/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "moonfish.h"

static void moonfish_go(struct moonfish_chess *chess)
{
	static struct moonfish_move move;
	static struct moonfish_options options;
	
	long int our_time, their_time, *xtime, time;
	char *arg, *end;
	long int count;
	char name[6];
	
	our_time = -1;
	their_time = -1;
	time = -1;
	
	for (;;) {
		
		arg = strtok(NULL, "\r\n\t ");
		if (arg == NULL) break;
		
		if (!strcmp(arg, "wtime") || !strcmp(arg, "btime")) {
			
			if (chess->white) {
				if (!strcmp(arg, "wtime")) xtime = &our_time;
				else xtime = &their_time;
			}
			else {
				if (!strcmp(arg, "wtime")) xtime = &their_time;
				else xtime = &our_time;
			}
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg == NULL) {
				fprintf(stderr, "missing time in 'go' command\n");
				exit(1);
			}
			
			errno = 0;
			*xtime = strtol(arg, &end, 10);
			if (errno || *end != 0 || *xtime < 0) {
				fprintf(stderr, "malformed time in 'go' command\n");
				exit(1);
			}
			
			continue;
		}
		
		if (!strcmp(arg, "movetime")) {
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg == NULL) {
				fprintf(stderr, "malformed 'go movetime' command\n");
				exit(1);
			}
			
			errno = 0;
			time = strtol(arg, &end, 10);
			if (errno || *end != 0 || time < 0) {
				fprintf(stderr, "malformed 'movetime' in 'go' command\n");
				exit(1);
			}
			
			continue;
		}
	}
	
	options.max_time = time;
	options.our_time = our_time;
	count = moonfish_best_move(chess, &move, &options);
	moonfish_to_uci(chess, &move, name);
	
	printf("info nodes %ld\n", count);
	printf("bestmove %s\n", name);
}

static void moonfish_position(struct moonfish_chess *chess)
{
	static struct moonfish_move move;
	static char line[2048];
	
	char *arg;
	
	arg = strtok(NULL, "\r\n\t ");
	if (arg == NULL) {
		fprintf(stderr, "incomplete 'position' command\n");
		exit(1);
	}
	
	if (!strcmp(arg, "fen")) {
		
		arg = strtok(NULL, "\r\n");
		if (arg == NULL) {
			fprintf(stderr, "incomplete 'position fen' command\n");
			exit(1);
		}
		
		moonfish_from_fen(chess, arg);
		
		arg = strstr(arg, "moves");
		if (arg == NULL) return;
		
		do arg--;
		while (*arg == '\t' || *arg == ' ');
		
		strcpy(line, arg);
		strtok(line, "\r\n\t ");
	}
	else {
		if (!strcmp(arg, "startpos")) {
			moonfish_chess(chess);
		}
		else {
			fprintf(stderr, "malformed 'position' command\n");
			exit(1);
		}
	}
	
	arg = strtok(NULL, "\r\n\t ");
	if (arg == NULL || strcmp(arg, "moves")) return;
	
	for (;;) {
		arg = strtok(NULL, "\r\n\t ");
		if (arg == NULL) break;
		if (moonfish_from_uci(chess, &move, arg)) {
			fprintf(stderr, "malformed move '%s'\n", arg);
			exit(1);
		}
		*chess = move.chess;
	}
}

int main(int argc, char **argv)
{
	static char line[2048];
	static struct moonfish_chess chess;
	
	char *arg;
	
	if (argc > 1) {
		fprintf(stderr, "usage: %s (no arguments)\n", argv[0]);
		return 1;
	}
	
	moonfish_chess(&chess);
	
	for (;;) {
		
		fflush(stdout);
		
		if (fgets(line, sizeof line, stdin) == NULL) {
			if (feof(stdin)) break;
			perror("fgets");
			return 1;
		}
		
		arg = strtok(line, "\r\n\t ");
		if (arg == NULL) continue;
		
		if (!strcmp(arg, "go")) {
			moonfish_go(&chess);
			continue;
		}
		
		if (!strcmp(arg, "quit")) break;
		
		if (!strcmp(arg, "position")) {
			moonfish_position(&chess);
			continue;
		}
		
		if (!strcmp(arg, "uci")) {
			printf("id name moonfish\n");
			printf("id author zamfofex\n");
			printf("uciok\n");
			continue;
		}
		
		if (!strcmp(arg, "isready")) {
			printf("readyok\n");
			continue;
		}
		
		if (!strcmp(arg, "debug") || !strcmp(arg, "setoption") || !strcmp(arg, "ucinewgame") || !strcmp(arg, "stop")) continue;
		
		fprintf(stderr, "warning: unknown command '%s'\n", arg);
	}
	
	return 0;
}
