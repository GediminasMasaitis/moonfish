/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "../moonfish.h"
#include "tools.h"

static long int moonfish_perft(struct moonfish_chess *chess, int depth)
{
	struct moonfish_move moves[32];
	int x, y;
	long int count;
	struct moonfish_move *move;
	
	if (depth == 0) return 1;
	
	count = 0;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			moonfish_play(chess, move);
			if (moonfish_validate(chess))
				count += moonfish_perft(chess, depth - 1);
			moonfish_unplay(chess, move);
		}
	}
	
	return count;
}

int main(int argc, char **argv)
{
	static char *format = "";
	static struct moonfish_arg args[] =
	{
		{"F", "fen", "<FEN>", NULL, "starting position for the game"},
		{"N", "depth", "<ply-count>", "2", "the number of plies to look (default: '2')"},
		{NULL, NULL, NULL, NULL, NULL},
	};
	
	char *end;
	long int depth;
	struct moonfish_chess chess;
	
	if (moonfish_args(args, format, argc, argv) - argv != argc)
		moonfish_usage(args, format, argv[0]);
	
	errno = 0;
	depth = strtol(args[1].value, &end, 10);
	if (errno != 0 || *end != 0 || depth < 0 || depth >= 24)
		moonfish_usage(args, format, argv[0]);
	
	moonfish_chess(&chess);
	if (args[0].value != NULL && moonfish_from_fen(&chess, args[0].value))
		moonfish_usage(args, format, argv[0]);
	
	printf("perft %ld: %ld\n", depth, moonfish_perft(&chess, depth));
	
	return 0;
}
