/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2025 zamfofex */

#include <stdio.h>
#include <stdlib.h>

#include "../moonfish.h"
#include "tools.h"

static long int moonfish_perft(struct moonfish_chess *chess, int depth)
{
	struct moonfish_move moves[32];
	int x, y;
	long int perft;
	int i, count;
	struct moonfish_chess other;
	
	if (depth == 0) return 1;
	
	perft = 0;
	
	for (y = 0 ; y < 8 ; y++) {
		for (x = 0 ; x < 8 ; x++) {
			count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
			for (i = 0 ; i < count ; i++) {
				other = *chess;
				moonfish_play(&other, moves + i);
				if (!moonfish_validate(&other)) continue;
				perft += moonfish_perft(&other, depth - 1);
			}
		}
	}
	return perft;
}

int main(int argc, char **argv)
{
	static struct moonfish_command cmd = {
		"show count of positions reachable from a given position (in 'n' plies)",
		"<depth>",
		{
			{"F", "fen", "<FEN>", NULL, "starting position for the game"},
		},
		{{NULL, NULL}},
		{{NULL, NULL, NULL}},
		{NULL},
	};
	
	int depth;
	struct moonfish_chess chess;
	char **args2;
	
	args2 = moonfish_args(&cmd, argc, argv);
	if (args2 - argv != argc - 1) moonfish_usage(&cmd, argv[0]);
	
	if (moonfish_int(args2[0], &depth) || depth < 0) moonfish_usage(&cmd, argv[0]);
	
	moonfish_chess(&chess);
	if (cmd.args[0].value != NULL && moonfish_from_fen(&chess, cmd.args[0].value)) moonfish_usage(&cmd, argv[0]);
	
	printf("perft %d: %ld\n", depth, moonfish_perft(&chess, depth));
	
	return 0;
}
