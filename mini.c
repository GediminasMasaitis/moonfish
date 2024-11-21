/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "moonfish.h"

int main(void)
{
	static char line[2048];
	static struct moonfish_chess chess, chess0;
	static struct moonfish_move move;
	static struct moonfish_options options;
	static struct moonfish_result result;
	
	struct moonfish_node *node;
	char name[6];
	int wtime, btime;
	char *arg;
	
	node = moonfish_new();
	moonfish_root(node, &chess);
	
	for (;;) {
		
		fflush(stdout);
		
		fgets(line, sizeof line, stdin);
		arg = strchr(line, '\n');
		if (arg) *arg = 0;
		
		if (!strncmp(line, "go ", 3)) {
			
			sscanf(line, "go wtime %d btime %d", &wtime, &btime);
			
			options.max_time = -1;
			options.our_time = chess.white ? wtime : btime;
			moonfish_best_move(node, &result, &options);
			moonfish_to_uci(&chess, &result.move, name);
			printf("bestmove %s\n", name);
		}
		
		if (!strncmp(line, "position ", 9)) {
			
			moonfish_chess(&chess);
			
			arg = strstr(line, " moves ");
			if (!arg) continue;
			
			arg = strtok(arg, " ");
			
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
			
			moonfish_root(node, &chess0);
			if (!moonfish_equal(&chess0, &chess)) moonfish_reroot(node, &chess);
		}
		
		if (!strcmp(line, "uci")) printf("uciok\n");
		if (!strcmp(line, "isready")) printf("readyok\n");
		if (!strcmp(line, "quit")) return 0;
	}
}

