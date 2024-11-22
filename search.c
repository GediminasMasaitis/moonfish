/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#ifdef moonfish_no_threads

#define moonfish_result_t int
#define moonfish_value 0
#define _Atomic

#else

#include <stdatomic.h>

#ifndef moonfish_pthreads

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
#define thrd_success 0

#endif

#endif

#ifdef moonfish_mini
#undef atomic_compare_exchange_strong
#undef atomic_fetch_add
#endif

#include "moonfish.h"

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
		perror("clock_gettime");
		exit(1);
	}
	
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif

struct moonfish_node {
	struct moonfish_move move;
	struct moonfish_node *parent;
	struct moonfish_node *children;
	double score;
	_Atomic double chance;
	_Atomic long int visits;
	_Atomic float bounds[2];
	_Atomic int count;
	_Atomic unsigned char ignored;
};

struct moonfish_data {
	struct moonfish_node *node;
	long int time, time0;
};

double moonfish_values[] = {0,0,0,0,103,124,116,101,104,120,104,108,106,118,107,112,122,131,118,123,170,183,170,167,243,249,232,223,0,0,0,0,293,328,339,338,338,342,357,365,338,368,378,391,362,386,397,401,377,389,418,419,367,395,416,424,347,367,394,400,249,342,356,371,373,383,375,379,390,403,404,398,395,405,409,415,395,408,414,426,400,416,423,432,409,419,422,423,383,403,409,407,378,390,384,394,592,607,611,616,586,602,606,606,594,608,604,608,610,619,619,623,631,636,642,645,643,651,655,655,652,655,661,663,649,652,653,654,1181,1168,1172,1190,1178,1189,1199,1195,1187,1197,1203,1200,1191,1208,1209,1214,1211,1213,1226,1231,1217,1224,1240,1240,1197,1189,1233,1238,1214,1227,1239,1243,-21,2,-24,-31,-6,-2,-6,-8,-17,-1,4,8,-12,10,18,23,6,32,34,33,20,44,40,29,5,34,27,16,-50,-1,-5,-10};

double moonfish_score(struct moonfish_chess *chess)
{
	int x, y;
	int x1, y1;
	unsigned char type, color, piece;
	double score;
	
	score = 0;
	
	for (y = 0 ; y < 8 ; y++) {
		for (x = 0 ; x < 8 ; x++) {
			piece = chess->board[(x + 1) + (y + 2) * 10];
			if (piece == moonfish_empty) continue;
			type = piece % 16 - 1;
			color = piece / 16 - 1;
			x1 = x;
			y1 = y;
			if (x1 > 3) x1 = 7 - x1;
			if (color == 1) y1 = 7 - y1;
			score -= moonfish_values[x1 + y1 * 4 + type * 32] * (color * 2 - 1);
		}
	}
	
	return score;
}

static void moonfish_discard(struct moonfish_node *node)
{
	int i;
	if (node->count == 0) return;
	for (i = 0 ; i < node->count ; i++) moonfish_discard(node->children + i);
	free(node->children);
}

static void moonfish_node(struct moonfish_node *node)
{
	node->parent = NULL;
	node->count = 0;
	node->chance = 0;
	node->visits = 0;
	node->ignored = 0;
	node->bounds[0] = 0;
	node->bounds[1] = 1;
}

static int moonfish_compare(const void *ax, const void *bx)
{
	const struct moonfish_node *a, *b;
	a = ax;
	b = bx;
	return a->score - b->score;
}

static void moonfish_expand(struct moonfish_node *node)
{
	struct moonfish_move moves[32];
	int x, y;
	int count, i;
	int child_count;
	
	node->children = NULL;
	child_count = 0;
	
	for (y = 0 ; y < 8 ; y++) {
		
		for (x = 0 ; x < 8 ; x++) {
			
			count = moonfish_moves(&node->move.chess, moves, (x + 1) + (y + 2) * 10);
			if (count == 0) continue;
			
			node->children = realloc(node->children, (child_count + count) * sizeof *node->children);
			if (node->children == NULL) {
				perror("realloc");
				exit(1);
			}
			
			for (i = 0 ; i < count ; i++) {
				
				if (!moonfish_validate(&moves[i].chess)) continue;
				moonfish_node(node->children + child_count);
				node->children[child_count].move = moves[i];
				node->children[child_count].parent = node;
				
				node->children[child_count].score = moonfish_score(&moves[i].chess);
				if (!moves[i].chess.white) node->children[child_count].score *= -1;
				
				child_count++;
			}
		}
	}
	
	qsort(node->children, child_count, sizeof *node, &moonfish_compare);
	if (child_count == 0 && node->children != NULL) free(node->children);
	node->count = child_count;
}

static double moonfish_confidence(struct moonfish_node *node)
{
	if (node->visits == 0) return 1e9;
	return node->chance / node->visits + 1.25 * sqrt(log(node->parent->visits) / node->visits);
}

static struct moonfish_node *moonfish_select(struct moonfish_node *node)
{
	struct moonfish_node *next;
	double max_confidence, confidence;
	int i;
	int n;
	
	for (;;) {
		
#ifdef moonfish_no_threads
		if (node->count == 0) break;
#else
		n = 0;
		if (atomic_compare_exchange_strong(&node->count, &n, -1)) break;
		if (n == -1) continue;
#endif
		
		next = NULL;
		max_confidence = -1;
		
		for (i = 0 ; i < node->count ; i++) {
			if (node->children[i].ignored) continue;
			confidence = moonfish_confidence(node->children + i);
			if (confidence > max_confidence) {
				next = node->children + i;
				max_confidence = confidence;
			}
		}
		
		node = next;
	}
	
	return node;
}

static void moonfish_propagate(struct moonfish_node *node, double chance)
{
	double value;
	
	while (node != NULL) {
		
#ifdef moonfish_no_threads
		node->chance += chance;
		node->visits++;
#else
		value = node->chance;
		for (;;) {
			if (atomic_compare_exchange_strong(&node->chance, &value, value + chance)) {
				break;
			}
		}
		atomic_fetch_add(&node->visits, 1);
#endif
		
		node = node->parent;
		chance = 1 - chance;
	}
}

static void moonfish_propagate_bounds(struct moonfish_node *node, int i)
{
	int j;
	float bound;
	
	while (node != NULL) {
		
		bound = 0;
		
		for (j = 0 ; j < node->count ; j++) {
			if (1 - node->children[j].bounds[1 - i] > bound) {
				bound = 1 - node->children[j].bounds[1 - i];
			}
		}
		
		for (j = 0 ; j < node->count ; j++) {
			if (1 - node->children[j].bounds[1 - i] < bound) {
				node->children[j].ignored = 1;
			}
		}
		
		node->bounds[i] = bound;
		node = node->parent;
		i = 1 - i;
	}
}

static void moonfish_search(struct moonfish_node *node, int count)
{
	int i;
	struct moonfish_node *leaf;
	
	for (i = 0 ; i < count ; i++) {
		leaf = moonfish_select(node);
		if (moonfish_finished(&leaf->move.chess)) {
			moonfish_propagate(leaf, 0.5);
			if (moonfish_checkmate(&leaf->move.chess)) {
				moonfish_propagate_bounds(leaf, 1);
			}
			leaf->count = 0;
			continue;
		}
		moonfish_expand(leaf);
		moonfish_propagate(leaf, 1 / (1 + pow(10, leaf->score / 400)));
	}
}

static moonfish_result_t moonfish_start(void *data0)
{
	struct moonfish_data *data;
	int i, count;
	
	data = data0;
	
	moonfish_search(data->node, 0x100);
	while (moonfish_clock() - data->time0 < data->time) {
		count = data->node->count;
		for (i = 0 ; i < data->node->count ; i++) {
			if (data->node->children[i].ignored) count--;
		}
		if (count == 1) break;
		moonfish_search(data->node, 0x1000);
	}
	
	return moonfish_value;
}

void moonfish_best_move(struct moonfish_node *node, struct moonfish_result *result, struct moonfish_options *options)
{
	struct moonfish_data data;
	struct moonfish_node *best_node;
	long int time;
	long int best_visits;
	int i;
#ifndef moonfish_no_threads
	thrd_t threads[256];
#endif
	
	time = LONG_MAX;
	if (options->max_time >= 0 && time > options->max_time) time = options->max_time;
	if (options->our_time >= 0 && time > options->our_time / 16) time = options->our_time / 16;
	time -= time / 32 + 125;
	
	data.node = node;
	data.time = time;
	data.time0 = moonfish_clock();
	
#ifdef moonfish_no_threads
	
	moonfish_start(&data);
	
#else
	
	for (i = 0 ; i < options->thread_count ; i++) {
		if (thrd_create(threads + i, &moonfish_start, &data) != thrd_success) {
			fprintf(stderr, "could not create thread\n");
			exit(1);
		}
	}
	
	for (i = 0 ; i < options->thread_count ; i++) {
		if (thrd_join(threads[i], NULL) != thrd_success) {
			fprintf(stderr, "could not join thread\n");
			exit(1);
		}
	}
	
#endif
	
	best_visits = -1;
	best_node = NULL;
	
	for (i = 0 ; i < node->count ; i++) {
		if (node->children[i].ignored) continue;
		if (node->children[i].visits > best_visits) {
			best_node = node->children + i;
			best_visits = best_node->visits;
		}
	}
	
	result->move = best_node->move;
	result->node_count = node->visits;
}

void moonfish_reroot(struct moonfish_node *node, struct moonfish_chess *chess)
{
	int i, j;
	struct moonfish_node *children;
	
	children = node->children;
	
	for (i = 0 ; i < node->count ; i++) {
		if (moonfish_equal(&children[i].move.chess, chess)) break;
	}
	
	if (i == node->count) {
		moonfish_discard(node);
		moonfish_node(node);
		node->move.chess = *chess;
		return;
	}
	
	for (j = 0 ; j < node->count ; j++) {
		if (i == j) continue;
		moonfish_discard(children + j);
	}
	
	*node = children[i];
	node->parent = NULL;
	free(children);
	
	for (i = 0 ; i < node->count ; i++) {
		node->children[i].parent = node;
	}
}

void moonfish_root(struct moonfish_node *node, struct moonfish_chess *chess)
{
	*chess = node->move.chess;
}

struct moonfish_node *moonfish_new(void)
{
	struct moonfish_node *node;
	
	node = malloc(sizeof *node);
	if (node == NULL) {
		perror("malloc");
		exit(1);
	}
	
	moonfish_node(node);
	moonfish_chess(&node->move.chess);
	
	return node;
}

void moonfish_finish(struct moonfish_node *node)
{
	moonfish_discard(node);
	free(node);
}
