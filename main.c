/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2025 zamfofex */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

#include "moonfish.h"
#include "threads.h"

struct moonfish_option {
	char *name;
	char *type;
	int value;
	int min, max;
};

struct moonfish_info {
	struct moonfish_root *root;
	_Atomic unsigned char searching;
#ifndef moonfish_no_threads
	unsigned char has_thread;
	thrd_t thread;
#endif
	struct moonfish_option *options;
};

static int moonfish_getoption(struct moonfish_option *options, char *name)
{
	int i;
	for (i = 0 ; options[i].name != NULL ; i++) {
		if (!strcasecmp(options[i].name, name)) return options[i].value;
	}
	return -1;
}

static moonfish_result_t moonfish_go(void *data)
{
	static struct moonfish_result result, pv_result;
	static struct moonfish_options options;
	static struct moonfish_chess chess;
	static struct moonfish_move pv[256];
	
	long int our_time, their_time, *xtime, time;
	char *arg, *end;
	char name[6];
	struct moonfish_info *info;
	long int node_count;
	int i, j, pv_count, count;
	
	info = data;
	
	our_time = -1;
	their_time = -1;
	time = -1;
	node_count = -1;
	
	moonfish_root(info->root, &chess);
	
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
		
		if (!strcmp(arg, "nodes")) {
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg == NULL) {
				fprintf(stderr, "malformed 'go nodes' command\n");
				exit(1);
			}
			
			errno = 0;
			node_count = strtol(arg, &end, 10);
			if (errno || *end != 0 || node_count < 0) {
				fprintf(stderr, "malformed 'nodes' in 'go' command\n");
				exit(1);
			}
			
			continue;
		}
	}
	
	options.max_time = time;
	options.our_time = our_time;
	options.thread_count = moonfish_getoption(info->options, "Threads");
	options.node_count = node_count;
	
	moonfish_best_move(info->root, &result, &options);
	info->searching = 0;
	
	pv_count = moonfish_getoption(info->options, "MultiPV");
	for (i = 0 ; i < pv_count ; i++) {
		count = sizeof pv / sizeof *pv;
		moonfish_pv(info->root, pv, &pv_result, i, &count);
		if (count == 0) continue;
		printf("info depth 1 score cp %d nodes %ld multipv %d pv", pv_result.score, pv_result.node_count, i + 1);
		moonfish_root(info->root, &chess);
		for (j = 0 ; j < count ; j++) {
			moonfish_to_uci(&chess, pv + j, name);
			chess = pv[j].chess;
			printf(" %s", name);
		}
		printf("\n");
	}
	
	printf("info depth 1 score cp %d nodes %ld\n", result.score, result.node_count);
	
	moonfish_root(info->root, &chess);
	moonfish_to_uci(&chess, &result.move, name);
	
	printf("bestmove %s\n", name);
	fflush(stdout);
	
	return moonfish_value;
}

static void moonfish_position(struct moonfish_root *root)
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
			
			moonfish_root(root, &chess0);
			if (moonfish_equal(&chess0, &chess)) moonfish_reroot(root, &move.chess);
			
			chess = move.chess;
		}
	}
	
	moonfish_root(root, &chess0);
	if (!moonfish_equal(&chess0, &chess)) moonfish_reroot(root, &chess);
}

static void moonfish_setoption(struct moonfish_info *info)
{
	char *arg, *end;
	long int value;
	int i;
	
	arg = strtok(NULL, "\r\n\t ");
	if (arg == NULL || strcmp(arg, "name")) {
		fprintf(stderr, "malformed 'setoption' command\n");
		exit(1);
	}
	
	arg = strtok(NULL, "\r\n\t ");
	if (arg == NULL) {
		fprintf(stderr, "missing option name\n");
		exit(1);
	}
	
	for (i = 0 ; info->options[i].name != NULL ; i++) {
		if (!strcasecmp(arg, info->options[i].name)) break;
	}
	
	if (info->options[i].name == NULL) {
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
	value = strtol(arg, &end, 10);
	if (errno || *end != 0 || value < info->options[i].min || value > info->options[i].max) {
		fprintf(stderr, "malformed option value\n");
		exit(1);
	}
	
	info->options[i].value = value;
}

int main(int argc, char **argv)
{
	static char line[2048];
	static struct moonfish_info info;
	static struct moonfish_option options[] = {
#ifndef moonfish_no_threads
		{"Threads", "spin", 0, 1, 256},
#endif
		{"MultiPV", "spin", 0, 0, 256},
		{NULL},
	};
	
	char *arg;
	int i;
	
	if (argc > 1) {
		fprintf(stderr, "usage: %s (no arguments)\n", argv[0]);
		return 1;
	}
	
	info.root = moonfish_new();
	info.searching = 0;
	info.options = options;
	
#ifndef moonfish_no_threads
	info.has_thread = 0;
	options[0].value = sysconf(_SC_NPROCESSORS_ONLN);
	if (options[0].value > options[0].max) options[0].value = options[0].max;
	if (options[0].value < 1) options[0].value = 4;
#endif
	
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
			if (info.searching) {
				fprintf(stderr, "cannot start search while searching\n");
				exit(1);
			}
			info.searching = 1;
			if (info.has_thread) {
				if (thrd_join(info.thread, NULL) != thrd_success) {
					fprintf(stderr, "could not join thread\n");
					exit(1);
				}
			}
			info.has_thread = 1;
			moonfish_unstop(info.root);
			if (thrd_create(&info.thread, &moonfish_go, &info) != thrd_success) {
				fprintf(stderr, "could not create thread\n");
				exit(1);
			}
#endif
			
			continue;
		}
		
		if (!strcmp(arg, "quit")) break;
		
		if (!strcmp(arg, "position")) {
			if (info.searching) {
				fprintf(stderr, "cannot set position while searching\n");
				exit(1);
			}
			moonfish_position(info.root);
			continue;
		}
		
		if (!strcmp(arg, "uci")) {
			printf("id name moonfish\n");
			printf("id author zamfofex\n");
			for (i = 0 ; options[i].name != NULL ; i++) {
				printf("option name %s type %s default %d min %d max %d\n", options[i].name, options[i].type, options[i].value, options[i].min, options[i].max);
			}
			printf("uciok\n");
			continue;
		}
		
		if (!strcmp(arg, "isready")) {
			printf("readyok\n");
			continue;
		}
		
		if (!strcmp(arg, "setoption")) {
			moonfish_setoption(&info);
			if (info.searching) printf("info string warning: option may only take effect next search request\n");
			continue;
		}
		
#ifndef moonfish_no_threads
		
		if (!strcmp(arg, "stop")) {
			moonfish_stop(info.root);
			if (info.has_thread) {
				info.has_thread = 0;
				if (thrd_join(info.thread, NULL) != thrd_success) {
					fprintf(stderr, "could not join thread\n");
					exit(1);
				}
			}
			continue;
		}
		
#endif
		
		if (!strcmp(arg, "debug") || !strcmp(arg, "ucinewgame") || !strcmp(arg, "stop")) continue;
		
		fprintf(stderr, "warning: unknown command '%s'\n", arg);
	}
	
#ifndef moonfish_no_threads
	if (info.has_thread) {
		moonfish_stop(info.root);
		if (thrd_join(info.thread, NULL) != thrd_success) {
			fprintf(stderr, "could not join thread\n");
			exit(1);
		}
	}
#endif
	
	moonfish_finish(info.root);
	return 0;
}
