/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#include <windows.h>
#ifndef __MINGW32__
#define moonfish_c11_threads
#endif

#else

#include <time.h>

#endif

#ifdef moonfish_no_threads

#define pthread_t int
#define pthread_create(thread, attr, fn, arg) ((*fn)(arg), 0)
#define pthread_join(thread, ret) 0
typedef int moonfish_result_t;
#define moonfish_value 0

#elif defined(moonfish_c11_threads)

#include <threads.h>
#define pthread_t thrd_t
#define pthread_create(thread, attr, fn, arg) thrd_create(thread, fn, arg)
#define pthread_join thrd_join
typedef int moonfish_result_t;
#define moonfish_value 0

#else

#include <pthread.h>
#define moonfish_result_t void *
#define moonfish_value NULL

#endif

#include "moonfish.h"

struct moonfish_info
{
	struct moonfish_analysis *analysis;
	struct moonfish_move moves[1024];
	pthread_t thread;
	struct moonfish_move move;
	int score;
};

struct moonfish_analysis
{
	struct moonfish_chess chess;
	struct moonfish_info info[256];
	int score;
	int depth;
	long int time;
};

#ifdef _WIN32

static long int moonfish_clock(void)
{
	return GetTickCount();
}

#else

static long int moonfish_clock(void)
{
	struct timespec ts;
	
#ifdef moonfish_mini
	clock_gettime(CLOCK_MONOTONIC, &ts);
#else
	if (clock_gettime(CLOCK_MONOTONIC, &ts))
	{
		perror(NULL);
		exit(1);
	}
#endif
	
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif

static int moonfish_search(struct moonfish_info *info, struct moonfish_chess *chess, struct moonfish_move *moves, int alpha, int beta, int depth, long int t0, long int time)
{
	int score;
	int i;
	int x, y;
	int count;
	long int t1, c;
	
	if (moves - info->moves > (int) (sizeof info->moves / sizeof *info->moves - 256))
		depth = -10;
	
	if (depth < 0)
	{
		score = chess->score;
		if (!chess->white) score *= -1;
		
		if (depth < -3) return score;
		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}
	else if (info->analysis->time >= 0 && time < 5)
	{
		depth = 0;
	}
	
	count = 0;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
		count += moonfish_moves(chess, moves + count, (x + 1) + (y + 2) * 10);
	
	for (i = 0 ; i < count ; i++)
		if (chess->board[moves[i].to] % 16 == moonfish_king)
			return moonfish_omega * (moonfish_depth - depth);
	
	for (i = 0 ; i < count ; i++)
	{
		if (depth < 0)
		if (chess->board[moves[i].to] == moonfish_empty)
		if (moves[i].chess.board[moves[i].to] == chess->board[moves[i].from])
			continue;
		
		t1 = moonfish_clock();
		c = time * i / count - t1 + t0;
		
		score = -moonfish_search(info, &moves[i].chess, moves + count, -beta, -alpha, depth - 1, t1, time / count + c);
		
		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}
	
	return alpha;
}

#ifndef moonfish_mini

int moonfish_countdown(int score)
{
	score /= moonfish_omega;
	if (score < 0) score -= moonfish_depth + 1;
	else score += moonfish_depth;
	return score / 2;
}

#endif

static moonfish_result_t moonfish_start_search(void *data)
{
	struct moonfish_info *info;
	
	info = data;
	info->score = -moonfish_search(
		info, &info->move.chess, info->moves,
		-100 * moonfish_omega, 100 * moonfish_omega,
		info->analysis->depth, moonfish_clock(), info->analysis->time
	);
	
	return moonfish_value;
}

static void moonfish_iteration(struct moonfish_analysis *analysis, struct moonfish_move *best_move)
{
	int result;
	int x, y;
	struct moonfish_move moves[32];
	int i, j, count;
#ifdef moonfish_no_threads
	int total;
	int invalid_count;
	
	if (analysis->time >= 0)
	{
		total = 0;
		
		for (y = 0 ; y < 8 ; y++)
		for (x = 0 ; x < 8 ; x++)
		{
			invalid_count = 0;
			count = moonfish_moves(&analysis->chess, moves, (x + 1) + (y + 2) * 10);
			for (i = 0 ; i < count ; i++)
				if (!moonfish_validate(&moves[i].chess))
					invalid_count--;
			total += count - invalid_count;
		}
		
		analysis->time /= total;
	}
#endif
	
	j = 0;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		count = moonfish_moves(&analysis->chess, moves, (x + 1) + (y + 2) * 10);
		for (i = 0 ; i < count ; i++)
		{
			if (!moonfish_validate(&moves[i].chess)) continue;
			
			analysis->info[j].analysis = analysis;
			analysis->info[j].move = moves[i];
			
			result = pthread_create(&analysis->info[j].thread, NULL, &moonfish_start_search, analysis->info + j);
#ifndef moonfish_mini
#ifdef moonfish_c11_threads
			if (result != thrd_success)
			{
				fprintf(stderr, "error creating thread\n");
				exit(1);
			}
#else
			if (result)
			{
				fprintf(stderr, "%s\n", strerror(result));
				exit(1);
			}
#endif
#endif
			
			j++;
		}
	}
	
	analysis->score = -200 * moonfish_omega;
	
	for (i = 0 ; i < j ; i++)
	{
		result = pthread_join(analysis->info[i].thread, NULL);
#ifndef moonfish_mini
#ifdef moonfish_c11_threads
		if (result != thrd_success)
		{
			fprintf(stderr, "error joining thread\n");
			exit(1);
		}
#else
		if (result)
		{
			fprintf(stderr, "%s\n", strerror(result));
			exit(1);
		}
#endif
#endif
		
		if (analysis->info[i].score > analysis->score)
		{
			*best_move = analysis->info[i].move;
			analysis->score = analysis->info[i].score;
		}
	}
}

#ifndef moonfish_mini

int moonfish_best_move_depth(struct moonfish_chess *chess, struct moonfish_move *best_move, int depth)
{
	static struct moonfish_analysis analysis;
	
	analysis.chess = *chess;
	analysis.depth = depth;
	analysis.time = -1;
	moonfish_iteration(&analysis, best_move);
	return analysis.score;
}

#endif

int moonfish_best_move_time(struct moonfish_chess *chess, struct moonfish_move *best_move, long int time)
{
	static struct moonfish_analysis analysis;
	
	analysis.chess = *chess;
	time -= 125;
	if (time < 10) time = 10;
	analysis.depth = 16;
	analysis.time = time;
	moonfish_iteration(&analysis, best_move);
	return analysis.score;
}

int moonfish_best_move_clock(struct moonfish_chess *chess, struct moonfish_move *best_move, long int our_time, long int their_time)
{
	long int time0, time1;
	
	time0 = our_time / 16;
	time1 = our_time - time0 - their_time * 7 / 8;
	if (time1 < 0) time1 = 0;
	return moonfish_best_move_time(chess, best_move, time0 + time1);
}
