/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023 zamfofex */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef __MINGW32__
#include <sysinfoapi.h>
#endif

#include "moonfish.h"

static int moonfish_search(struct moonfish_chess *chess, int alpha, int beta, int depth)
{
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	int score;
	
	if (depth <= 0)
	{
		score = chess->score;
		if (!chess->white) score *= -1;
		
		if (depth <= -4) return score;
		if (chess->score >= beta) return beta;
		if (chess->score > alpha) alpha = score;
	}
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			if (depth <= 0 && move->captured == moonfish_empty) continue;
			if (move->captured % 16 == moonfish_king) return moonfish_omega * (depth + 10);
			
			moonfish_play(chess, move);
			score = -moonfish_search(chess, -beta, -alpha, depth - 1);
			moonfish_unplay(chess, move);
			
			if (score >= beta) return beta;
			if (score > alpha) alpha = score;
		}
	}
	
	return alpha;
}

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
	struct moonfish_move *moves, move_array[256];
	int best_score;
	int i, j;
	struct moonfish_search_info infos[32];
	int result;
#ifdef __MINGW32__
	SYSTEM_INFO info;
#endif
	
	if (ctx->cpu_count < 0)
	{
		errno = 0;
#ifdef __MINGW32__
		GetSystemInfo(&info);
		ctx->cpu_count = info.dwNumberOfProcessors;
#else
		ctx->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
#endif
		if (ctx->cpu_count <= 0)
		{
			if (errno == 0) fprintf(stderr, "%s: unknown CPU count\n", ctx->argv0);
			else perror(ctx->argv0);
			exit(1);
		}
		if (ctx->cpu_count > 32) ctx->cpu_count = 32;
	}
	
	moves = move_array;
	best_score = -200 * moonfish_omega;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(&ctx->chess, moves, (x + 1) + (y + 2) * 10);
		while (moves->piece != moonfish_outside) moves++;
	}
	
	if (moves - move_array == 1)
	{
		*best_move = *move_array;
		return 0;
	}
	
	moves = move_array;
	i = 0;
	for (;;)
	{
		if (i == ctx->cpu_count || moves->piece == moonfish_outside)
		{
			for (j = 0 ; j < i ; j++)
			{
				result = pthread_join(infos[j].thread, NULL);
				if (result)
				{
					fprintf(stderr, "%s: %s\n", ctx->argv0, strerror(result));
					exit(1);
				}
				
				if (infos[j].score > best_score)
				{
					*best_move = infos[j].move;
					best_score = infos[j].score;
				}
			}
			
			if (moves->piece == moonfish_outside) break;
			
			i = 0;
		}
		
		moonfish_play(&ctx->chess, moves);
		
		if (!moonfish_validate(&ctx->chess))
		{
			moonfish_unplay(&ctx->chess, moves++);
			continue;
		}
		
		infos[i].move = *moves;
		infos[i].chess = ctx->chess;
		infos[i].depth = depth;
		
		result = pthread_create(&infos[i].thread, NULL, &moonfish_start_search, infos + i);
		if (result)
		{
			fprintf(stderr, "%s: %s\n", ctx->argv0, strerror(result));
			exit(1);
		}
		
		moonfish_unplay(&ctx->chess, moves);
		
		i++;
		moves++;
	}
	
	return best_score;
}

static void moonfish_clock(struct moonfish *ctx, struct timespec *ts)
{
	if (clock_gettime(CLOCK_MONOTONIC, ts))
	{
		perror(ctx->argv0);
		exit(1);
	}
}

int moonfish_best_move(struct moonfish *ctx, struct moonfish_move *best_move, long int our_time, long int their_time)
{
	long int d, t;
	int i;
	int score;
	struct timespec t0, t1;
	
	d = our_time - their_time;
	if (d < 0) d = 0;
	d += our_time / 8;
	
	i = 3;
	
	moonfish_clock(ctx, &t0);
	score = moonfish_best_move_depth(ctx, best_move, i);
	moonfish_clock(ctx, &t1);
	
	t = 50;
	t += t1.tv_sec * 1000;
	t -= t0.tv_sec * 1000;
	t += t1.tv_nsec / 1000000;
	t -= t0.tv_nsec / 1000000;
	
	for (;;)
	{
		t *= 32;
		if (t > d) break;
		i++;
		if (i >= 8) break;
	}
	
	if (i == 3) return score;
	return moonfish_best_move_depth(ctx, best_move, i);
}
