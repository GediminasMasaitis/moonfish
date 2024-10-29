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
	long int visits;
	int count;
};

double moonfish_values[] = {0,0,0,0,138,180,159,139,137,167,147,150,135,159,159,167,170,190,176,191,222,260,267,253,313,370,387,366,0,0,0,0,311,363,377,386,366,390,408,413,382,416,436,433,416,448,459,462,431,459,483,483,435,479,491,505,402,418,469,477,307,390,403,431,431,422,411,426,452,467,461,456,466,470,482,483,473,475,483,493,470,485,492,508,489,483,496,505,441,465,476,483,442,451,462,465,653,686,713,726,660,684,687,698,680,703,700,711,709,726,728,729,736,755,757,757,760,781,785,777,780,772,790,785,762,764,759,775,1282,1267,1261,1274,1284,1289,1295,1297,1290,1300,1303,1301,1323,1338,1325,1325,1344,1328,1366,1361,1328,1368,1379,1392,1326,1324,1363,1384,1286,1306,1348,1351,-4,5,-51,-42,-9,-11,-30,-58,-37,-26,-36,-36,-44,-16,-17,-16,-11,14,9,-14,19,50,36,15,26,86,41,36,2,42,42,34};

double moonfish_score(struct moonfish_chess *chess)
{
	int x, y;
	int x1, y1;
	int from;
	unsigned char type, color;
	double score;
	
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
	node->score = 0;
	node->visits = 0;
}

static void moonfish_expand(struct moonfish_node *node)
{
	struct moonfish_move moves[32];
	int x, y;
	int count, i;
	
	node->children = NULL;
	
	for (y = 0 ; y < 8 ; y++) {
		
		for (x = 0 ; x < 8 ; x++) {
			
			count = moonfish_moves(&node->move.chess, moves, (x + 1) + (y + 2) * 10);
			if (count == 0) continue;
			
			node->children = realloc(node->children, (node->count + count) * sizeof *node->children);
			if (node->children == NULL) {
				perror("realloc");
				exit(1);
			}
			
			for (i = 0 ; i < count ; i++) {
				if (!moonfish_validate(&moves[i].chess)) continue;
				node->children[node->count].move = moves[i];
				node->children[node->count].parent = node;
				node->children[node->count].score = 0;
				node->children[node->count].visits = 0;
				node->children[node->count].count = 0;
				node->count++;
			}
		}
	}
	
	if (node->count == 0 && node->children != NULL) {
		free(node->children);
	}
}

static double moonfish_confidence(struct moonfish_node *node)
{
	if (node->visits == 0) return 1e9;
	return node->score / node->visits + 1.25 * sqrt(log(node->parent->visits) / node->visits);
}

static struct moonfish_node *moonfish_select(struct moonfish_node *node)
{
	struct moonfish_node *next;
	double max_confidence, confidence;
	int i;
	
	while (node->count > 0) {
		
		next = NULL;
		max_confidence = -1e9;
		
		for (i = 0 ; i < node->count ; i++) {
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

static void moonfish_propagate(struct moonfish_node *node, double score)
{
	while (node != NULL) {
		node->visits++;
		node->score += score;
		node = node->parent;
		score = 1 - score;
	}
}

static void moonfish_search(struct moonfish_node *node, int count)
{
	int i;
	struct moonfish_node *leaf;
	double score;
	
	for (i = 0 ; i < count ; i++) {
		leaf = moonfish_select(node);
		if (moonfish_finished(&leaf->move.chess)) {
			if (moonfish_checkmate(&leaf->move.chess)) moonfish_propagate(leaf, 0.5);
			else moonfish_propagate(leaf, 0.5);
			continue;
		}
		moonfish_expand(leaf);
		score = moonfish_score(&leaf->move.chess);
		if (!leaf->move.chess.white) score *= -1;
		moonfish_propagate(leaf, 1 / (1 + pow(10, score / 400)));
	}
}

void moonfish_best_move(struct moonfish_node *node, struct moonfish_result *result, struct moonfish_options *options)
{
	struct moonfish_node *best_node;
	long int time, time0;
	int i;
	long int best_visits;
	
	time0 = moonfish_clock();
	time = LONG_MAX;
	
	if (options->max_time >= 0 && time > options->max_time) time = options->max_time;
	if (options->our_time >= 0 && time > options->our_time / 16) time = options->our_time / 16;
	time -= time / 32 + 125;
	
	moonfish_search(node, 0x800);
	while (moonfish_clock() - time0 < time) {
		moonfish_search(node, 0x2000);
	}
	
	best_visits = -1;
	best_node = NULL;
	
	for (i = 0 ; i < node->count ; i++) {
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
