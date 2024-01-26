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

struct moonfish_node
{
	struct moonfish_node *children;
	int score;
	int visits;
	unsigned char from, to;
	unsigned char count;
};

struct moonfish_info
{
	struct moonfish_analysis *analysis;
	pthread_t thread;
	struct moonfish_node *node;
	struct moonfish_move move;
	struct moonfish_chess chess;
	int score;
};

struct moonfish_analysis
{
	char *argv0;
	struct moonfish_chess chess;
	struct moonfish_info info[256];
	struct moonfish_node root;
	int score;
	int depth;
};

static void moonfish_free_node(struct moonfish_node *node)
{
	int i;
	for (i = 0 ; i < node->count ; i++)
		moonfish_free_node(node->children + i);
	if (node->count > 0)
		free(node->children);
	node->count = 0;
}

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
	analysis->root.count = 0;
	
	moonfish_chess(&chess);
	moonfish_new(analysis, &chess);
	
	return analysis;
}

void moonfish_new(struct moonfish_analysis *analysis, struct moonfish_chess *chess)
{
	moonfish_free_node(&analysis->root);
	analysis->root.visits = 0;
	analysis->root.count = 0;
	analysis->chess = *chess;
	analysis->depth = 1;
}

void moonfish_free(struct moonfish_analysis *analysis)
{
	struct moonfish_chess chess;
	moonfish_new(analysis, &chess);
	free(analysis);
}

static void moonfish_expand(char *argv0, struct moonfish_node *node, struct moonfish_chess *chess)
{
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	int x, y;
	int done;
	
	if (node->count != 0) return;
	
	done = 0;
	node->children = NULL;
	
	for (y = 0 ; y < 8 && !done ; y++)
	for (x = 0 ; x < 8 && !done ; x++)
	{
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			node->children = realloc(node->children, (node->count + 1) * sizeof *node->children);
			if (node->children == NULL)
			{
				perror(argv0);
				exit(1);
			}
			
			node->children[node->count].from = move->from;
			node->children[node->count].to = move->to;
			node->children[node->count].count = 0;
			node->children[node->count].visits = 0;
			node->count++;
		}
	}
}

static int moonfish_search(struct moonfish_info *info, struct moonfish_node *parent, struct moonfish_node *node, int alpha, int beta, int depth, int i)
{
	int score;
	int j, k;
	struct moonfish_node swap_node;
	struct moonfish_move move;
	int group_count;
	unsigned char adjust[256] = {0};
	
	if (i >= depth)
	{
		node->visits++;
		score = info->chess.score;
		if (!info->chess.white) score *= -1;
		return score;
	}
	
	group_count = (parent->visits / parent->count) / (node->visits + 1) / 16 + 1;
	
	for (j = 0 ; j < node->count ; j++)
	{
		if (node->children[j].visits == 0)
		{
			adjust[j] = 0;
			continue;
		}
		adjust[j] = j * group_count / node->count;
	}
	
	moonfish_expand(info->analysis->argv0, node, &info->chess);
	
	for (j = 0 ; j < node->count ; j++)
	{
		moonfish_move(&info->chess, &move, node->children[j].from, node->children[j].to);
		
		if (move.captured % 16 == moonfish_king)
		{
			score = moonfish_omega * (moonfish_depth - i);
		}
		else
		{
			moonfish_play(&info->chess, &move);
			score = -moonfish_search(info, node, node->children + j, -beta, -alpha, depth - adjust[j], i + 1);
			moonfish_unplay(&info->chess, &move);
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
	
	node->visits = 0;
	for (j = 1 ; j < node->count ; j++)
		node->visits += node->children[j].visits;
	
	for (j = 1 ; j < node->count ; j++)
	for (k = j ; k > 0 && node->children[k - 1].score < node->children[k].score ; k--)
	{
		swap_node = node->children[k];
		node->children[k] = node->children[k - 1];
		node->children[k - 1] = swap_node;
	}
	
	if (i > 5 && node->count > 0)
	{
		free(node->children);
		node->count = 0;
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
	info = data;
	info->score = -moonfish_search(info, &info->analysis->root, info->node, -100 * moonfish_omega, 100 * moonfish_omega, info->analysis->depth, 0);
	return moonfish_value;
}

static void moonfish_iteration(struct moonfish_analysis *analysis, struct moonfish_move *best_move)
{
	struct moonfish_move move;
	int i, j;
	int result;
	
	char res[10];
	
	moonfish_expand(analysis->argv0, &analysis->root, &analysis->chess);
	
	j = 0;
	
	for (i = 0 ; i < analysis->root.count ; i++)
	{
		analysis->info[j].chess = analysis->chess;
		
		moonfish_move(&analysis->info[j].chess, &move, analysis->root.children[i].from, analysis->root.children[i].to);
		moonfish_play(&analysis->info[j].chess, &move);
		
		if (!moonfish_validate(&analysis->chess)) continue;
		
		analysis->info[j].analysis = analysis;
		analysis->info[j].node = analysis->root.children + i;
		analysis->info[j].move = move;
		
		result = pthread_create(&analysis->info[j].thread, NULL, &moonfish_start_search, analysis->info + j);
		if (result)
		{
			fprintf(stderr, "%s: %s\n", analysis->argv0, strerror(result));
			exit(1);
		}
		
		j++;
	}
	
	analysis->score = -200 * moonfish_omega;
	analysis->root.visits = 0;
	
	for (i = 0 ; i < j ; i++)
	{
		result = pthread_join(analysis->info[i].thread, NULL);
		if (result)
		{
			fprintf(stderr, "%s: %s\n", analysis->argv0, strerror(result));
			exit(1);
		}
		
		moonfish_to_uci(res, &analysis->info[i].move);
		
		analysis->root.visits += analysis->info[i].node->visits;
		
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
	for (;;)
	{
		moonfish_iteration(analysis, best_move);
		if (analysis->depth >= depth) return analysis->score;
		analysis->depth++;
	}
}

#endif

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

int moonfish_best_move_time(struct moonfish_analysis *analysis, struct moonfish_move *best_move, int *depth, long int our_time, long int their_time)
{
	long int d, t, t0, t1;
	int r;
	
	r = 24 * 2048;
	t = -1;
	
	d = our_time - their_time;
	if (d < 0) d = 0;
	d += our_time / 8;
	
	for (;;)
	{
		t0 = moonfish_clock(analysis);
		moonfish_iteration(analysis, best_move);
		t1 = moonfish_clock(analysis) + 50;
		
		if (t >= 0) r = (t1 - t0) * 2048 / (t + 1);
		t = t1 - t0;
		
		d -= t;
		if (t * r > d * 2048) break;
		
		analysis->depth++;
	}
	
	*depth = analysis->depth;
	return analysis->score;
}
