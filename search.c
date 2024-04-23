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
typedef void *moonfish_result_t;
#define moonfish_value NULL

#endif

#include "moonfish.h"

struct moonfish_info
{
	struct moonfish_analysis *analysis;
	pthread_t thread;
	struct moonfish_move move;
	struct moonfish_chess chess;
	int score;
};

struct moonfish_analysis
{
	char *argv0;
	struct moonfish_chess chess;
	struct moonfish_info info[512];
	int score;
	int depth;
	long int time;
};

#ifdef _WIN32

static long int moonfish_clock(struct moonfish_analysis *analysis)
{
	(void) analysis;
	return GetTickCount();
}

#else

static long int moonfish_clock(struct moonfish_analysis *analysis)
{
	struct timespec ts;
	
	if (clock_gettime(CLOCK_MONOTONIC, &ts))
	{
		perror(analysis->argv0);
		exit(1);
	}
	
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif

struct moonfish_analysis *moonfish_analysis(char *argv0)
{
	struct moonfish_analysis *analysis;
	struct moonfish_chess chess;
	
	analysis = malloc(sizeof *analysis);
	if (analysis == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	analysis->argv0 = argv0;
	
	moonfish_chess(&chess);
	moonfish_new(analysis, &chess);
	
	return analysis;
}

void moonfish_new(struct moonfish_analysis *analysis, struct moonfish_chess *chess)
{
	analysis->chess = *chess;
	analysis->depth = 1;
	analysis->time = -1;
}

void moonfish_free(struct moonfish_analysis *analysis)
{
	struct moonfish_chess chess = {0};
	moonfish_new(analysis, &chess);
	free(analysis);
}

static int moonfish_search(struct moonfish_info *info, int alpha, int beta, int depth, long int t0, long int time)
{
	int score;
	int i;
	struct moonfish_move moves[256];
	int x, y;
	int count;
	long int t1;
	
	if (depth < 0)
	{
		score = info->chess.score;
		if (!info->chess.white) score *= -1;
		
		if (depth < -3) return score;
		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}
	else if (info->analysis->time >= 0 && time < 0)
	{
		score = info->chess.score;
		if (!info->chess.white) score *= -1;
		return score;
	}
	
	count = 0;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(&info->chess, moves + count, (x + 1) + (y + 2) * 10);
		
		while (moves[count].piece != moonfish_outside)
		{
			if (moves[count].captured % 16 == moonfish_king)
				return moonfish_omega * (moonfish_depth - depth);
			count++;
		}
	}
	
	for (i = 0 ; moves[i].piece != moonfish_outside ; i++)
	{
		if (depth < 0)
		if (moves[i].captured == moonfish_empty)
		if (moves[i].promotion == moves[i].piece)
			continue;
		
		t1 = moonfish_clock(info->analysis);
		
		moonfish_play(&info->chess, moves + i);
		score = -moonfish_search(info, -beta, -alpha, depth - 1, t1, (time - t1 + t0) / (count - i));
		moonfish_unplay(&info->chess, moves + i);
		
		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}
	
	return alpha;
}

int moonfish_countdown(int score)
{
	score /= -moonfish_omega;
	if (score < 0) score += moonfish_depth + 1;
	else score -= moonfish_depth;
	return score / 2;
}

static moonfish_result_t moonfish_start_search(void *data)
{
	struct moonfish_info *info;
	long int time, t0;
	int depth;
	
	info = data;
	
	depth = info->analysis->depth;
	t0 = moonfish_clock(info->analysis);
	time = info->analysis->time;
	
	info->score = -moonfish_search(info, -100 * moonfish_omega, 100 * moonfish_omega, depth, t0, time);
	return moonfish_value;
}

static void moonfish_iteration(struct moonfish_analysis *analysis, struct moonfish_move *best_move)
{
	int i, j;
	int result;
	int x, y;
	struct moonfish_move *move, moves[32];
	
	j = 0;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(&analysis->chess, moves, (x + 1) + (y + 2) * 10);
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			analysis->info[j].chess = analysis->chess;
			moonfish_play(&analysis->info[j].chess, move);
			if (!moonfish_validate(&analysis->info[j].chess)) continue;
			
			analysis->info[j].analysis = analysis;
			analysis->info[j].move = *move;
			
			result = pthread_create(&analysis->info[j].thread, NULL, &moonfish_start_search, analysis->info + j);
			if (result)
			{
				fprintf(stderr, "%s: %s\n", analysis->argv0, strerror(result));
				exit(1);
			}
			
			j++;
		}
	}
	
	analysis->score = -200 * moonfish_omega;
	
	for (i = 0 ; i < j ; i++)
	{
		result = pthread_join(analysis->info[i].thread, NULL);
		if (result)
		{
			fprintf(stderr, "%s: %s\n", analysis->argv0, strerror(result));
			exit(1);
		}
		
		if (analysis->info[i].score > analysis->score)
		{
			*best_move = analysis->info[i].move;
			analysis->score = analysis->info[i].score;
		}
	}
}

#ifndef moonfish_mini

int moonfish_best_move_depth(struct moonfish_analysis *analysis, struct moonfish_move *best_move, int depth)
{
	analysis->depth = depth;
	analysis->time = -1;
	moonfish_iteration(analysis, best_move);
	return analysis->score;
}

#endif

int moonfish_best_move_time(struct moonfish_analysis *analysis, struct moonfish_move *best_move, int *depth, long int time)
{
	time -= 125;
	if (time < 10) time = 10;
	analysis->depth = 8;
	analysis->time = time;
	moonfish_iteration(analysis, best_move);
	*depth = 4;
	return analysis->score;
}

int moonfish_best_move_clock(struct moonfish_analysis *analysis, struct moonfish_move *best_move, int *depth, long int our_time, long int their_time)
{
	long int time;
	time = our_time * 3 / 4 - their_time;
	if (time < 0) time = 0;
	time += our_time / 8;
	return moonfish_best_move_time(analysis, best_move, depth, time);
}
