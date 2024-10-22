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

#define thrd_t int
#define thrd_create(thread, fn, arg) ((*fn)(arg), 0)
#define thrd_join(thread, ret) 0
#define moonfish_result_t int
#define moonfish_value 0
#define mtx_t int
#define thrd_success 0

#elif defined(moonfish_c11_threads)

#include <threads.h>
#define moonfish_result_t int
#define moonfish_value 0

#else

#include <pthread.h>
#define thrd_t pthread_t
#define thrd_create(thread, fn, arg) pthread_create(thread, NULL, fn, arg)
#define thrd_join pthread_join
#define moonfish_result_t void *
#define moonfish_value NULL
#define mtx_t pthread_mutex_t
#define thrd_success 0

#endif

#include "moonfish.h"

#define moonfish_omega 0x2000

struct moonfish_thread {
	thrd_t thread;
	struct moonfish_analysis *analysis;
	struct moonfish_move move;
	int score;
};

struct moonfish_analysis {
	struct moonfish_chess chess;
	struct moonfish_thread threads[256];
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
	
	if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
		perror(NULL);
		exit(1);
	}
	
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif

moonfish_t moonfish_values[] = {0,0,0,0,138,180,159,139,137,167,147,150,135,159,159,167,170,190,176,191,222,260,267,253,313,370,387,366,0,0,0,0,311,363,377,386,366,390,408,413,382,416,436,433,416,448,459,462,431,459,483,483,435,479,491,505,402,418,469,477,307,390,403,431,431,422,411,426,452,467,461,456,466,470,482,483,473,475,483,493,470,485,492,508,489,483,496,505,441,465,476,483,442,451,462,465,653,686,713,726,660,684,687,698,680,703,700,711,709,726,728,729,736,755,757,757,760,781,785,777,780,772,790,785,762,764,759,775,1282,1267,1261,1274,1284,1289,1295,1297,1290,1300,1303,1301,1323,1338,1325,1325,1344,1328,1366,1361,1328,1368,1379,1392,1326,1324,1363,1384,1286,1306,1348,1351,-4,5,-51,-42,-9,-11,-30,-58,-37,-26,-36,-36,-44,-16,-17,-16,-11,14,9,-14,19,50,36,15,26,86,41,36,2,42,42,34};

moonfish_t moonfish_score(struct moonfish_chess *chess)
{
	int x, y;
	int x1, y1;
	int from;
	unsigned char type, color;
	moonfish_t score;
	
	score = 0;
	
	for (y = 0 ; y < 8 ; y++) {
		for (x = 0 ; x < 8 ; x++) {
			from = (x + 1) + (y + 2) * 10;
			type = chess->board[from] % 16;
			color = chess->board[from] / 16 - 1;
			if (chess->board[from] == moonfish_empty) continue;
			x1 = x;
			y1 = y;
			if (x1 > 3) x1 = 7 - x1;
			if (color == 1) y1 = 7 - y1;
			score -= moonfish_values[x1 + y1 * 4 + (type - 1) * 32] * (color * 2 - 1);
		}
	}
	
	return score;
}

static int moonfish_search(struct moonfish_thread *thread, struct moonfish_chess *chess, int alpha, int beta, int depth, long int t0, long int time)
{
	int score;
	int i;
	int x, y;
	int count;
	long int t1, c;
	struct moonfish_move moves[32];
	
	if (depth < 0) {
		score = moonfish_score(chess);
		if (!chess->white) score *= -1;
		if (score >= beta) return beta;
		if (score < alpha - 100) return alpha;
		if (score > alpha) alpha = score;
	}
	else {
		if (thread->analysis->time >= 0 && time < 5) {
			depth = 0;
		}
	}
	
	for (y = 0 ; y < 8 ; y++) {
		
		for (x = 0 ; x < 8 ; x++) {
			
			count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
			for (i = 0 ; i < count ; i++) {
				
				if (!moonfish_validate(&moves[i].chess)) continue;
				
				if (depth < 0) {
					if (chess->board[moves[i].to] == moonfish_empty) {
						if (moves[i].chess.board[moves[i].to] == chess->board[moves[i].from]) continue;
					}
				}
				t1 = moonfish_clock();
				c = 2 * time * i / count - t1 + t0;
				
				score = -moonfish_search(thread, &moves[i].chess, -beta, -alpha, depth - 1, t1, time / count + c);
				
				if (score >= beta) return beta;
				if (score > alpha) alpha = score;
			}
		}
	}
	
	return alpha;
}

static moonfish_result_t moonfish_start_search(void *data)
{
	struct moonfish_thread *thread;
	
	thread = data;
	
	thread->score = -moonfish_search(
		thread, &thread->move.chess,
		-moonfish_omega, moonfish_omega,
		thread->analysis->depth, moonfish_clock(), thread->analysis->time
	);
	
	return moonfish_value;
}

static void moonfish_iteration(struct moonfish_analysis *analysis, struct moonfish_move *best_move)
{
	int x, y;
	struct moonfish_move moves[32];
	int i, j, count;
	
#ifdef moonfish_no_threads
	
	int total;
	int invalid_count;
	
	if (analysis->time >= 0) {
		
		total = 0;
		
		for (y = 0 ; y < 8 ; y++) {
			for (x = 0 ; x < 8 ; x++) {
				invalid_count = 0;
				count = moonfish_moves(&analysis->chess, moves, (x + 1) + (y + 2) * 10);
				for (i = 0 ; i < count ; i++) {
					if (!moonfish_validate(&moves[i].chess)) invalid_count++;
				}
				total += count - invalid_count;
			}
		}
		analysis->time /= total;
	}
	
#endif
	
	j = 0;
	
	for (y = 0 ; y < 8 ; y++) {
		
		for (x = 0 ; x < 8 ; x++) {
			
			count = moonfish_moves(&analysis->chess, moves, (x + 1) + (y + 2) * 10);
			for (i = 0 ; i < count ; i++) {
				
				if (!moonfish_validate(&moves[i].chess)) continue;
				
				analysis->threads[j].analysis = analysis;
				analysis->threads[j].move = moves[i];
				
				if (thrd_create(&analysis->threads[j].thread, &moonfish_start_search, analysis->threads + j) != thrd_success) {
					fprintf(stderr, "error creating thread\n");
					exit(1);
				}
				
				j++;
			}
		}
	}
	analysis->score = -2 * moonfish_omega;
	
	for (i = 0 ; i < j ; i++) {
		
		if (thrd_join(analysis->threads[i].thread, NULL) != thrd_success) {
			fprintf(stderr, "error joining thread\n");
			exit(1);
		}
		
		if (analysis->threads[i].score > analysis->score) {
			*best_move = analysis->threads[i].move;
			analysis->score = analysis->threads[i].score;
		}
	}
}

int moonfish_best_move_depth(struct moonfish_chess *chess, struct moonfish_move *best_move, int depth)
{
	static struct moonfish_analysis analysis;
	
	analysis.chess = *chess;
	analysis.depth = depth;
	analysis.time = -1;
	moonfish_iteration(&analysis, best_move);
	return analysis.score;
}

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
	(void) their_time;
	return moonfish_best_move_time(chess, best_move, our_time / 16);
}
