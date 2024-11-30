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

#include "moonfish.h"
#include "threads.h"

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
	struct moonfish_node *parent;
	struct moonfish_node *children;
	_Atomic double score;
	_Atomic long int visits;
	_Atomic float bounds[2];
	_Atomic int count;
	_Atomic unsigned char ignored;
	unsigned char from, index;
};

struct moonfish_root {
	struct moonfish_node node;
	struct moonfish_chess chess;
	_Atomic int stop;
};

struct moonfish_data {
	struct moonfish_root *root;
	long int time, time0;
	long int node_count;
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
	for (i = 0 ; i < node->count ; i++) moonfish_discard(node->children + i);
	if (node->count > 0) free(node->children);
}

static void moonfish_node(struct moonfish_node *node)
{
	node->parent = NULL;
	node->count = 0;
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

static void moonfish_expand(struct moonfish_node *node, struct moonfish_chess *chess)
{
	int x, y;
	int count, i;
	int child_count;
	struct moonfish_move moves[32];
	
	if (node->count == -2) return;
	if (moonfish_finished(chess)) {
		node->count = -2;
		return;
	}
	
	node->children = NULL;
	child_count = 0;
	
	for (y = 0 ; y < 8 ; y++) {
		
		for (x = 0 ; x < 8 ; x++) {
			
			count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
			if (count == 0) continue;
			
			node->children = realloc(node->children, (child_count + count) * sizeof *node->children);
			if (node->children == NULL) {
				perror("realloc");
				exit(1);
			}
			
			for (i = 0 ; i < count ; i++) {
				
				if (!moonfish_validate(&moves[i].chess)) continue;
				moonfish_node(node->children + child_count);
				node->children[child_count].parent = node;
				node->children[child_count].from = (x + 1) + (y + 2) * 10;
				node->children[child_count].index = i;
				
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
	return 1 / (1 + pow(10, node->score / 400)) + 1.25 * sqrt(log(node->parent->visits) / node->visits);
}

static void moonfish_node_move(struct moonfish_node *node, struct moonfish_chess *chess, struct moonfish_move *move)
{
	struct moonfish_move moves[32];
	moonfish_moves(chess, moves, node->from);
	*move = moves[node->index];
}

static void moonfish_node_chess(struct moonfish_node *node, struct moonfish_chess *chess)
{
	struct moonfish_move move;
	moonfish_node_move(node, chess, &move);
	*chess = move.chess;
}

static struct moonfish_node *moonfish_select(struct moonfish_node *node, struct moonfish_chess *chess)
{
	struct moonfish_node *next;
	double max_confidence, confidence;
	int i;
	int n;
	
	for (;;) {
		
#ifdef moonfish_no_threads
		if (node->count <= 0) break;
#else
		n = 0;
		if (atomic_compare_exchange_strong(&node->count, &n, -1)) break;
		if (n == -1) continue;
		if (n == -2) break;
#endif
		
		next = NULL;
		max_confidence = -1;
		
		for (i = 0 ; i < node->count ; i++) {
			if (node->children[i].ignored) continue;
			if (node->children[i].count == -1) continue;
			confidence = moonfish_confidence(node->children + i);
			if (confidence > max_confidence) {
				next = node->children + i;
				max_confidence = confidence;
			}
		}
		
		if (next == NULL) continue;
		
		node = next;
		moonfish_node_chess(node, chess);
	}
	
	return node;
}

#ifdef moonfish_no_threads

static void moonfish_propagate(struct moonfish_node *node)
{
	int i;
	
	while (node != NULL) {
		node->visits++;
		node->score = -1e9;
		for (i = 0 ; i < node->count ; i++) {
			if (node->score < -node->children[i].score) {
				node->score = -node->children[i].score;
			}
		}
		node = node->parent;
	}
}

#else

static void moonfish_propagate(struct moonfish_node *node)
{
	int i;
	double score, child_score;
	
	while (node != NULL) {
		score = -1e9;
		for (i = 0 ; i < node->count ; i++) {
			child_score = -node->children[i].score;
			if (score < child_score) score = child_score;
		}
		node->score = score;
		atomic_fetch_add(&node->visits, 1);
		node = node->parent;
	}
}

#endif

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

static void moonfish_search(struct moonfish_node *node, struct moonfish_chess *chess0, int count)
{
	int i;
	struct moonfish_node *leaf;
	struct moonfish_chess chess;
	
	for (i = 0 ; i < count ; i++) {
		chess = *chess0;
		leaf = moonfish_select(node, &chess);
		moonfish_expand(leaf, &chess);
		moonfish_propagate(leaf);
		if (leaf->count == -2 && moonfish_check(&chess)) {
			moonfish_propagate_bounds(leaf, 1);
		}
	}
}

static moonfish_result_t moonfish_start(void *data0)
{
	struct moonfish_data *data;
	int i, count;
	
	data = data0;
	
	moonfish_search(&data->root->node, &data->root->chess, 0x100);
	while (moonfish_clock() - data->time0 < data->time) {
		if (data->root->stop) break;
#ifndef moonfish_mini
		if (data->root->node.visits + 0x1000 >= data->node_count) {
			moonfish_search(&data->root->node, &data->root->chess, data->node_count - data->root->node.visits);
			break;
		}
#endif
		count = data->root->node.count;
		for (i = 0 ; i < data->root->node.count ; i++) {
			if (data->root->node.children[i].ignored) count--;
		}
		if (count <= 1) break;
		moonfish_search(&data->root->node, &data->root->chess, 0x1000);
	}
	
	return moonfish_value;
}

void moonfish_best_move(struct moonfish_root *root, struct moonfish_result *result, struct moonfish_options *options)
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
	
	data.root = root;
	data.time = time;
	data.time0 = moonfish_clock();
	if (options->node_count < 0) data.node_count = LONG_MAX;
	else data.node_count = options->node_count;
	
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
	
	for (i = 0 ; i < root->node.count ; i++) {
		if (root->node.children[i].ignored) continue;
		if (root->node.children[i].visits > best_visits) {
			best_node = root->node.children + i;
			best_visits = best_node->visits;
		}
	}
	
	moonfish_node_move(best_node, &root->chess, &result->move);
	result->score = root->node.score;
	result->node_count = root->node.visits;
}

void moonfish_reroot(struct moonfish_root *root, struct moonfish_chess *chess)
{
	static struct moonfish_chess chess0;
	
	struct moonfish_node *children;
	int i, j;
	
	children = root->node.children;
	
	for (i = 0 ; i < root->node.count ; i++) {
		chess0 = root->chess;
		moonfish_node_chess(&children[i], &chess0);
		if (moonfish_equal(&chess0, chess)) break;
	}
	
	root->chess = *chess;
	
	if (i >= root->node.count) {
		moonfish_discard(&root->node);
		moonfish_node(&root->node);
		return;
	}
	
	for (j = 0 ; j < root->node.count ; j++) {
		if (i == j) continue;
		moonfish_discard(children + j);
	}
	
	root->node = children[i];
	root->node.parent = NULL;
	free(children);
	
	for (i = 0 ; i < root->node.count ; i++) {
		root->node.children[i].parent = &root->node;
	}
}

void moonfish_root(struct moonfish_root *root, struct moonfish_chess *chess)
{
	*chess = root->chess;
}

struct moonfish_root *moonfish_new(void)
{
	struct moonfish_root *root;
	
	root = malloc(sizeof *root);
	if (root == NULL) {
		perror("malloc");
		exit(1);
	}
	
	root->stop = 0;
	moonfish_node(&root->node);
	moonfish_chess(&root->chess);
	
	return root;
}

void moonfish_finish(struct moonfish_root *root)
{
	moonfish_discard(&root->node);
	free(root);
}

#ifndef moonfish_mini

void moonfish_stop(struct moonfish_root *root)
{
	root->stop = 1;
}

void moonfish_unstop(struct moonfish_root *root)
{
	root->stop = 0;
}

#endif
