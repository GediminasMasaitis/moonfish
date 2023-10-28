/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023 zamfofex */

#include <time.h>

#include "moonfish.h"

static int moonfish_search(struct moonfish_chess *chess, int alpha, int beta, int depth)
{
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	int score;
	
	if (depth <= 0)
	{
		if (depth <= -3) return chess->score;
		
		if (chess->score >= beta) return beta;
		if (chess->score > alpha) alpha = chess->score;
	}
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			if (depth <= 0 && move->captured == moonfish_empty) continue;
			if (move->captured == moonfish_their_king) return moonfish_omega * (depth + 10);
			
			moonfish_play(chess, move);
			score = -moonfish_search(chess, -beta, -alpha, depth - 1);
			moonfish_unplay(chess, move);
			
			if (score >= beta) return beta;
			if (score > alpha) alpha = score;
		}
	}
	
	return alpha;
}


#ifdef MOONFISH_HAS_PTHREAD

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct moonfish_search_info
{
	pthread_t thread;
	struct moonfish_move move;
	struct moonfish_chess chess;
	int depth;
	int score;
};

static void *moonfish_start_search(void *data)
{
	struct moonfish_search_info *info;
	info = data;
	info->score = -moonfish_search(&info->chess, -100 * moonfish_omega, 100 * moonfish_omega, info->depth);
	return NULL;
}

static int moonfish_best_move_depth(struct moonfish *ctx, struct moonfish_move *best_move, int depth)
{
	int x, y;
	struct moonfish_move *move, moves[32];
	int best_score;
	int count, i;
	struct moonfish_search_info *infos;
	int result;
	
	infos = malloc(256 * sizeof *infos);
	count = 0;
	best_score = -200 * moonfish_omega;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(&ctx->chess, moves, (x + 1) + (y + 2) * 10);
		
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			moonfish_play(&ctx->chess, move);
			
			if (!moonfish_validate(&ctx->chess))
			{
				moonfish_unplay(&ctx->chess, move);
				continue;
			}
			
			infos[count].move = *move;
			infos[count].chess = ctx->chess;
			infos[count].depth = depth;
			
			result = pthread_create(&infos[count].thread, NULL, &moonfish_start_search, infos + count);
			if (result)
			{
				free(infos);
				fprintf(stderr, "%s: %s\n", ctx->argv0, strerror(result));
				exit(1);
			}
			
			moonfish_unplay(&ctx->chess, move);
			
			count++;
		}
	}
	
	for (i = 0 ; i < count ; i++)
	{
		result = pthread_join(infos[i].thread, NULL);
		if (result)
		{
			free(infos);
			fprintf(stderr, "%s: %s\n", ctx->argv0, strerror(result));
			exit(1);
		}
		
		if (infos[i].score > best_score)
		{
			*best_move = infos[i].move;
			best_score = infos[i].score;
		}
	}
	
	free(infos);
	return best_score;
}

#else

static int moonfish_best_move_depth(struct moonfish *ctx, struct moonfish_move *best_move, int depth)
{
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	int score, best_score;
	
	best_score = -200 * moonfish_omega;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(&ctx->chess, moves, (x + 1) + (y + 2) * 10);
		
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			moonfish_play(&ctx->chess, move);
			
			if (!moonfish_validate(&ctx->chess))
			{
				moonfish_unplay(&ctx->chess, move);
				continue;
			}
			
			score = -moonfish_search(&ctx->chess, -100 * moonfish_omega, 100 * moonfish_omega, depth);
			moonfish_unplay(&ctx->chess, move);
			
			if (score > best_score)
			{
				*best_move = *move;
				best_score = score;
			}
		}
	}
	
	return best_score;
}

#endif

int moonfish_best_move(struct moonfish *ctx, struct moonfish_move *best_move, long int our_time, long int their_time)
{
	time_t t, d;
	int i;
	int score;
	int base;
	
	d = our_time - their_time;
	if (d < 0) d = 0;
	d += our_time / 8;
	
	t = 0;
	base = 2;
	
	while (t < 4)
	{
		base++;
		t = time(NULL);
		score = moonfish_best_move_depth(ctx, best_move, base);
		t = time(NULL) - t + 2;
	}
	
	i = base;
	
	for (;;)
	{
		t *= 32;
		if (t > d) break;
		i++;
		if (i >= 8) break;
	}
	
	if (i == base) return score;
	return moonfish_best_move_depth(ctx, best_move, i);
}
