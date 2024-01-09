/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023 zamfofex */

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

struct moonfish_node
{
	unsigned char from, to;
	struct moonfish_node *children;
	unsigned char count;
	short int score;
};

struct moonfish_info
{
	struct moonfish *ctx;
	pthread_t thread;
	struct moonfish_move move;
	struct moonfish_chess chess;
	int depth;
	int score;
	struct moonfish_node node;
	struct moonfish_node *nodes;
	int count;
};

static void moonfish_free(struct moonfish_node *node)
{
	int i;
	for (i = 0 ; i < node->count ; i++)
		moonfish_free(node->children + i);
	if (node->count > 0)
		free(node->children);
}

static int moonfish_quiesce(struct moonfish_chess *chess, int alpha, int beta, int depth, int i)
{
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	int score;
	
	score = chess->score;
	if (!chess->white) score *= -1;
	
	if (i >= depth) return score;
	if (score >= beta) return beta;
	if (score > alpha) alpha = score;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			if (move->captured == moonfish_empty)
			if (move->promotion == move->piece)
				continue;
			
			if (move->captured % 16 == moonfish_king)
				return moonfish_omega * (moonfish_depth - i);
			
			moonfish_play(chess, move);
			score = -moonfish_quiesce(chess, -beta, -alpha, depth, i + 1);
			moonfish_unplay(chess, move);
			
			if (score >= beta) return beta;
			if (score > alpha) alpha = score;
		}
	}
	
	return alpha;
}

static int moonfish_search(struct moonfish_info *info, struct moonfish_node *node, int alpha, int beta, int depth, int qdepth, int i)
{
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	int score;
	int j, k;
	struct moonfish_node swap_node;
	int done;
	struct moonfish_move move0;
	
	if (i >= depth) return moonfish_quiesce(&info->chess, alpha, beta, depth + qdepth, i);
	
	if (node->count == 0)
	{
		done = 0;
		node->children = NULL;
		
		for (y = 0 ; y < 8 && !done ; y++)
		for (x = 0 ; x < 8 && !done ; x++)
		{
			moonfish_moves(&info->chess, moves, (x + 1) + (y + 2) * 10);
			
			for (move = moves ; move->piece != moonfish_outside ; move++)
			{
				node->children = realloc(node->children, (node->count + 1) * sizeof *node->children);
				if (node->children == NULL)
				{
					perror(info->ctx->argv0);
					exit(1);
				}
				
				node->children[node->count].from = move->from;
				node->children[node->count].to = move->to;
				node->children[node->count].count = 0;
				node->count++;
			}
		}
	}
	
	for (j = 0 ; j < node->count ; j++)
	{
		moonfish_move(&info->chess, &move0, node->children[j].from, node->children[j].to);
		
		if (move0.captured % 16 == moonfish_king)
		{
			score = moonfish_omega * (moonfish_depth - i);
		}
		else
		{
			moonfish_play(&info->chess, &move0);
			score = -moonfish_search(info, node->children + j, -beta, -alpha, depth, qdepth, i + 1);
			moonfish_unplay(&info->chess, &move0);
		}
		
		node->children[j].score = score;
		
		if (score >= beta)
		{
			alpha = beta;
			while (j < node->count)
			{
				node->children[j].score = beta;
				j++;
			}
			break;
		}
		if (score > alpha) alpha = score;
	}
	
	for (j = 1 ; j < node->count ; j++)
	for (k = j ; k > 0 && node->children[k - 1].score < node->children[k].score ; k--)
	{
		swap_node = node->children[k];
		node->children[k] = node->children[k - 1];
		node->children[k - 1] = swap_node;
	}
	
	return alpha;
}

int moonfish_countdown(int score)
{
	score /= -moonfish_omega;
	if (score < 0) score += moonfish_depth;
	else score -= moonfish_depth;
	return score;
}

static moonfish_result_t moonfish_start_search(void *data)
{
	struct moonfish_info *info;
	info = data;
	info->score = -moonfish_search(info, &info->node, -100 * moonfish_omega, 100 * moonfish_omega, info->depth, 4, 0);
	return moonfish_value;
}

static int moonfish_iteration(struct moonfish *ctx, struct moonfish_move *best_move, int depth, int *init)
{
	static struct moonfish_move all_moves[256];
	static struct moonfish_info infos[256];
	static struct moonfish_move move_array[32];
	static int init0 = 0;
	
	int x, y;
	int best_score;
	int move_count;
	int i;
	int result;
	
	if (!init0)
	{
		init0 = 1;
		for (i = 0 ; i < 256 ; i++)
			infos[i].node.count = 0;
	}
	
	if (!*init)
	{
		*init = 1;
		for (i = 0 ; i < 256 ; i++)
			moonfish_free(&infos[i].node),
			infos[i].node.count = 0;
	}
	
	best_score = -200 * moonfish_omega;
	
	move_count = 0;
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(&ctx->chess, move_array, (x + 1) + (y + 2) * 10);
		
		for (i = 0 ; move_array[i].piece != moonfish_outside ; i++)
		{
			moonfish_play(&ctx->chess, move_array + i);
			if (moonfish_validate(&ctx->chess))
			{
				all_moves[move_count] = move_array[i];
				move_count++;
			}
			moonfish_unplay(&ctx->chess, move_array + i);
		}
	}
	
	if (move_count == 1)
	{
		*best_move = *all_moves;
		return 0;
	}
	
	for (i = 0 ; i < move_count ; i++)
	{
		moonfish_play(&ctx->chess, all_moves + i);
		
		infos[i].ctx = ctx;
		infos[i].move = all_moves[i];
		infos[i].chess = ctx->chess;
		infos[i].depth = depth;
		
		result = pthread_create(&infos[i].thread, NULL, &moonfish_start_search, infos + i);
		if (result)
		{
			fprintf(stderr, "%s: %s\n", ctx->argv0, strerror(result));
			exit(1);
		}
		
		moonfish_unplay(&ctx->chess, all_moves + i);
	}
	
	for (i = 0 ; i < move_count ; i++)
	{
		result = pthread_join(infos[i].thread, NULL);
		if (result)
		{
			fprintf(stderr, "%s: %s\n", ctx->argv0, strerror(result));
			exit(1);
		}
		
		if (infos[i].score > best_score)
		{
			*best_move = infos[i].move;
			best_score = infos[i].score;
		}
	}
	
	return best_score;
}

int moonfish_best_move_depth(struct moonfish *ctx, struct moonfish_move *best_move, int depth)
{
	int i;
	int score;
	int init;
	
	score = 0;
	init = 0;
	
	i = 3;
	do score = moonfish_iteration(ctx, best_move, i++, &init);
	while (i <= depth);
	
	return score;
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

int moonfish_best_move_time(struct moonfish *ctx, struct moonfish_move *best_move, long int our_time, long int their_time)
{
	long int d, t, t0, t1;
	int score;
	int i;
	int init;
	int r;
	
	init = 0;
	r = 20;
	t = 0;
	
	d = our_time - their_time;
	if (d < 0) d = 0;
	d += our_time / 8;
	
	for (i = 3 ; i < 32 ; i++)
	{
		t0 = moonfish_clock(ctx);
		score = moonfish_iteration(ctx, best_move, i, &init);
		t1 = moonfish_clock(ctx);
		
		r = (t1 - t0) / (t + 1);
		t = t1 - t0;
		
		if (t * r > d) break;
		d -= t;
	}
	
	return score;
}
