/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>

#include "moonfish.h"
#include "threads.h"

struct moonfish_info {
	struct moonfish_node *node;
	int thread_count;
	_Atomic unsigned char searching;
#ifndef moonfish_no_threads
	thrd_t thread;
#endif
};

static moonfish_result_t moonfish_go(void *data)
{
	static struct moonfish_result result;
	static struct moonfish_options options;
	static struct moonfish_chess chess;
	
	long int our_time, their_time, *xtime, time;
	char *arg, *end;
	char name[6];
	struct moonfish_info *info;
	
	info = data;
	
	our_time = -1;
	their_time = -1;
	time = -1;
	
	moonfish_root(info->node, &chess);
	
	for (;;) {
		
		arg = strtok(NULL, "\r\n\t ");
		if (arg == NULL) break;
		
		if (!strcmp(arg, "wtime") || !strcmp(arg, "btime")) {
			
			if (chess.white) {
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
	options.thread_count = info->thread_count;
	moonfish_best_move(info->node, &result, &options);
	moonfish_to_uci(&chess, &result.move, name);
	
	info->searching = 2;
	printf("info depth 1 score cp %d nodes %ld multipv 1 pv %s\n", result.score, result.node_count, name);
	printf("bestmove %s\n", name);
	fflush(stdout);
	
	return moonfish_value;
}

static void moonfish_position(struct moonfish_node *node)
{
	static struct moonfish_chess chess, chess0;
	static struct moonfish_move move;
	static char line[2048];
	
	char *arg;
	
	arg = strtok(NULL, "\r\n\t ");
	if (arg == NULL) {
		fprintf(stderr, "incomplete 'position' command\n");
		exit(1);
	}
	
	moonfish_chess(&chess);
	
	if (!strcmp(arg, "fen")) {
		
		arg = strtok(NULL, "\r\n");
		if (arg == NULL) {
			fprintf(stderr, "incomplete 'position fen' command\n");
			exit(1);
		}
		
		moonfish_from_fen(&chess, arg);
		
		arg = strstr(arg, "moves");
		if (arg != NULL) {
			do arg--;
			while (*arg == '\t' || *arg == ' ');
			strcpy(line, arg);
			strtok(line, "\r\n\t ");
		}
	}
	else {
		if (strcmp(arg, "startpos")) {
			fprintf(stderr, "malformed 'position' command\n");
			exit(1);
		}
	}
	
	arg = strtok(NULL, "\r\n\t ");
	
	if (arg != NULL && !strcmp(arg, "moves")) {
		
		for (;;) {
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg == NULL) break;
			if (moonfish_from_uci(&chess, &move, arg)) {
				fprintf(stderr, "malformed move '%s'\n", arg);
				exit(1);
			}
			
			moonfish_root(node, &chess0);
			if (moonfish_equal(&chess0, &chess)) moonfish_reroot(node, &move.chess);
			
			chess = move.chess;
		}
	}
	
	moonfish_root(node, &chess0);
	if (!moonfish_equal(&chess0, &chess)) moonfish_reroot(node, &chess);
}

static void moonfish_setoption(int *thread_count)
{
	char *arg, *end;
	long int count;
	
	arg = strtok(NULL, "\r\n\t ");
	if (arg == NULL || strcmp(arg, "name")) {
		fprintf(stderr, "malformed 'setoption' command\n");
		exit(1);
	}
	
	arg = strtok(NULL, "\r\n\t ");
	if (arg == NULL || strcasecmp(arg, "Threads")) {
		fprintf(stderr, "unknown option '%s'\n", arg);
		exit(1);
	}
	
	arg = strtok(NULL, "\r\n\t ");
	if (arg == NULL || strcmp(arg, "value")) {
		fprintf(stderr, "malformed 'setoption' command\n");
		exit(1);
	}
	
	arg = strtok(NULL, "\r\n\t ");
	if (arg == NULL) {
		fprintf(stderr, "missing value\n");
		exit(1);
	}
	
	errno = 0;
	count = strtol(arg, &end, 10);
	if (errno || *end != 0 || count < 1 || count > 256) {
		fprintf(stderr, "malformed thread count\n");
		exit(1);
	}
	
	*thread_count = count;
}

int main(int argc, char **argv)
{
	static char line[2048];
	
	char *arg;
	struct moonfish_info info;
	
	if (argc > 1) {
		fprintf(stderr, "usage: %s (no arguments)\n", argv[0]);
		return 1;
	}
	
	info.node = moonfish_new();
	info.thread_count = 1;
	info.searching = 0;
	
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
#ifdef moonfish_no_threads
			moonfish_go(&info);
#else
			if (info.searching == 2) {
				if (thrd_join(info.thread, NULL) != thrd_success) {
					fprintf(stderr, "could not join thread\n");
					exit(1);
				}
			}
			else {
				if (info.searching) {
					fprintf(stderr, "cannot start search while searching\n");
					exit(1);
				}
			}
			info.searching = 1;
			if (thrd_create(&info.thread, &moonfish_go, &info) != thrd_success) {
				fprintf(stderr, "could not create thread\n");
				exit(1);
			}
#endif
			continue;
		}
		
		if (!strcmp(arg, "quit")) break;
		
		if (!strcmp(arg, "position")) {
			if (info.searching == 1) {
				fprintf(stderr, "cannot set position while searching\n");
				exit(1);
			}
			moonfish_position(info.node);
			continue;
		}
		
		if (!strcmp(arg, "uci")) {
			printf("id name moonfish\n");
			printf("id author zamfofex\n");
#ifndef moonfish_no_threads
			printf("option name Threads type spin default 1 min 1 max 256\n");
#endif
			printf("uciok\n");
			continue;
		}
		
		if (!strcmp(arg, "isready")) {
			printf("readyok\n");
			continue;
		}
		
#ifndef moonfish_no_threads
		
		if (!strcmp(arg, "setoption")) {
			moonfish_setoption(&info.thread_count);
			if (info.searching == 1) {
				printf("info string warning: option will only take effect next search request\n");
			}
			continue;
		}
		
		if (!strcmp(arg, "stop")) {
			if (info.searching) {
				moonfish_stop(info.node);
				if (thrd_join(info.thread, NULL) != thrd_success) {
					fprintf(stderr, "could not join thread\n");
					exit(1);
				}
				info.searching = 0;
			}
			continue;
		}
		
#endif
		
		if (!strcmp(arg, "debug") || !strcmp(arg, "ucinewgame") || !strcmp(arg, "stop")) continue;
		
		fprintf(stderr, "warning: unknown command '%s'\n", arg);
	}
	
#ifndef moonfish_no_threads
	if (info.searching) {
		moonfish_stop(info.node);
		if (thrd_join(info.thread, NULL) != thrd_success) {
			fprintf(stderr, "could not join thread\n");
			exit(1);
		}
	}
#endif
	
	moonfish_finish(info.node);
	return 0;
}
