/* moonfish's license: 0BSD */
/* copyright 2025 zamfofex */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "moonfish.h"

int main(void)
{
	static char line[2048];
	static struct moonfish_chess chess, chess0, chess1;
	static struct moonfish_move move;
	static struct moonfish_options options;
	static struct moonfish_result result;
	
	struct moonfish_root *root;
	char name[6];
	int wtime, btime;
	char *arg;
	
	root = moonfish_new();
	moonfish_root(root, &chess);
	options.thread_count = sysconf(_SC_NPROCESSORS_ONLN);
	if (options.thread_count < 1) options.thread_count = 1;
	
	for (;;) {
		
		fflush(stdout);
		
		fgets(line, sizeof line, stdin);
		arg = strchr(line, '\n');
		if (arg) *arg = 0;
		
		if (!strncmp(line, "go ", 3)) {
			sscanf(line, "go wtime %d btime %d", &wtime, &btime);
			options.max_time = -1;
			options.node_count = -1;
			options.our_time = chess.white ? wtime : btime;
			moonfish_best_move(root, &result, &options);
			moonfish_to_uci(&chess, &result.move, name);
			printf("info depth 1 score cp %d nodes %ld\nbestmove %s\n", result.score, result.node_count, name);
		}
		
		if (!strncmp(line, "position ", 9)) {
			
			moonfish_chess(&chess);
			
			arg = strstr(line, " moves ");
			if (!arg) continue;
			
			arg = strtok(arg, " ");
			
			for (;;) {
				arg = strtok(NULL, "\r\n\t ");
				if (arg == NULL) break;
				if (moonfish_from_uci(&chess, &move, arg)) exit(1);
				moonfish_root(root, &chess0);
				chess1 = chess;
				moonfish_play(&chess1, &move);
				if (moonfish_equal(&chess0, &chess)) moonfish_reroot(root, &chess1);
				chess = chess1;
			}
			
			moonfish_root(root, &chess0);
			if (!moonfish_equal(&chess0, &chess)) moonfish_reroot(root, &chess);
		}
		
		if (!strcmp(line, "uci")) printf("uciok\n");
		if (!strcmp(line, "isready")) printf("readyok\n");
		if (!strcmp(line, "quit")) return 0;
	}
}
