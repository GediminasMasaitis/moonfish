/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023 zamfofex */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32

#include <windows.h>

#ifndef __MINGW32__
#define moonfish_c11_threads
#endif

#else

#include <time.h>
#include <unistd.h>

#endif

#ifdef moonfish_c11_threads

#include <threads.h>
#define pthread_t thrd_t
#define pthread_create(thread, attr, fn, arg) thrd_create(thread, fn, arg)
#define pthread_join thrd_join
typedef int moonfish_result_t;
#define moonfish_value 0

#else

#include <pthread.h>
typedef void *moonfish_result_t;
#define moonfish_value NULL

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
		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			if (depth <= 0)
			if (move->captured == moonfish_empty)
			if (move->promotion == move->piece)
				continue;
			
			if (move->captured % 16 == moonfish_king)
				return moonfish_omega * (depth + 10);
			
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

static moonfish_result_t moonfish_start_search(void *data)
{
	struct moonfish_search_info *info;
	info = data;
	info->score = -moonfish_search(&info->chess, -100 * moonfish_omega, 100 * moonfish_omega, info->depth);
	return moonfish_value;
}

static int moonfish_best_move_depth(struct moonfish *ctx, struct moonfish_move *best_move, int depth)
{
	static struct moonfish_search_info *infos;
	static int thread_count = -1;
	
	int x, y;
	struct moonfish_move *moves, move_array[256];
	int best_score;
	int i, j;
	int result;
#ifdef _WIN32
	SYSTEM_INFO info;
#endif
	
	if (thread_count < 0)
	{
		errno = 0;
#ifdef _WIN32
		GetSystemInfo(&info);
		thread_count = info.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
		thread_count = sysconf(_SC_NPROCESSORS_ONLN);
#else
		thread_count = 4;
#endif
		
		if (thread_count <= 0)
		{
			if (errno == 0) fprintf(stderr, "%s: unknown CPU count\n", ctx->argv0);
			else perror(ctx->argv0);
			exit(1);
		}
		
		thread_count += thread_count / 4;
		
		infos = malloc(thread_count * sizeof *infos);
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
		if (i == thread_count || moves->piece == moonfish_outside)
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

#ifdef _WIN32

static long int moonfish_clock(struct moonfish *ctx)
{
	(void) ctx;
	return GetTickCount();
}

#else

static long int moonfish_clock(struct moonfish *ctx)
{
	struct timespec ts;
	
	if (clock_gettime(CLOCK_MONOTONIC, &ts))
	{
		perror(ctx->argv0);
		exit(1);
	}
	
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif

int moonfish_best_move(struct moonfish *ctx, struct moonfish_move *best_move, long int our_time, long int their_time)
{
	long int d, t, t0, t1;
	int i;
	int score;
	
	d = our_time - their_time;
	if (d < 0) d = 0;
	d += our_time / 8;
	
	i = 3;
	
	t0 = moonfish_clock(ctx);
	score = moonfish_best_move_depth(ctx, best_move, i);
	t1 = moonfish_clock(ctx);
	
	t = t1 - t0 + 50;
	
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
