/* moonfish's license: 0BSD */
/* copyright 2025 zamfofex */

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

#ifdef moonfish_no_clock

static long int moonfish_clock(void)
{
	time_t t;
	if (time(&t) < 0) {
		perror("time");
		exit(1);
	}
	return t * 1000;
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

#endif

struct moonfish_node {
	struct moonfish_node *parent;
	struct moonfish_node *children;
	_Atomic int visits, count;
	_Atomic short int score;
	_Atomic unsigned char bounds[2];
	_Atomic unsigned char ignored;
	unsigned char from, index;
};

struct moonfish_root {
	struct moonfish_node node;
	struct moonfish_chess chess;
	long int positive_node_count;
#ifndef moonfish_mini
	_Atomic int stop;
	void (*log)(struct moonfish_result *result, void *data);
	void *data;
#endif
};

static short int moonfish_score(struct moonfish_chess *chess)
{
	static short int values0[] = {0,0,0,0,51,72,71,55,53,74,68,68,49,74,69,79,57,86,83,95,66,103,131,134,194,180,215,223,0,0,0,0,227,277,279,277,286,275,295,301,288,309,313,319,303,333,326,326,327,329,358,348,324,354,376,388,304,300,365,360,166,241,253,329,325,330,315,309,333,342,342,329,338,344,340,342,336,344,346,364,337,352,369,379,356,377,388,386,325,351,356,357,320,321,290,289,409,415,418,421,379,411,412,411,398,415,402,408,406,410,406,417,429,433,444,453,441,472,476,488,467,464,492,493,501,495,483,497,951,942,944,956,949,962,968,956,950,963,954,951,952,960,953,948,973,957,963,954,977,984,973,965,957,921,963,944,964,986,981,1002,15,24,-23,-15,16,-4,-49,-66,-44,-28,-59,-71,-63,-21,-44,-69,-45,5,-10,-40,4,49,71,33,35,41,42,53,94,89,41,22};
	static short int values1[] = {0,0,0,0,125,134,133,137,122,129,122,125,128,129,117,113,146,142,128,119,213,210,178,171,259,265,233,214,0,0,0,0,297,291,329,336,313,338,343,353,318,350,360,376,348,364,390,395,348,376,389,401,336,358,379,377,327,352,349,368,284,333,362,343,354,362,354,374,354,361,370,382,367,378,391,395,374,391,400,399,382,400,396,404,380,387,394,388,371,385,385,387,371,378,382,391,640,653,657,653,646,643,646,646,644,651,655,654,657,666,670,669,665,669,673,669,673,667,670,665,672,677,673,676,664,669,676,671,1134,1132,1127,1119,1127,1126,1119,1151,1158,1155,1184,1180,1182,1195,1209,1225,1179,1218,1228,1245,1186,1196,1238,1247,1194,1234,1239,1265,1201,1202,1228,1222,-65,-34,-22,-37,-24,-7,11,17,-5,8,25,35,0,20,37,49,11,35,44,48,16,40,34,28,-12,30,22,8,-88,-25,-14,-20};
	static short int pawn_values0[] = {-17, 8, 11, -6, -3, 15};
	static short int pawn_values1[] = {-46, 15, 16, -30, -16, -1};
	static int values[] = {0, 1, 1, 2, 4, 0};
	
	int x, y;
	int x1, y1;
	int type, color, piece;
	int score0, score1;
	int i;
	int phase;
	
	score0 = 0;
	score1 = 0;
	phase = 0;
	
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
			
			i = x1 + y1 * 4 + type * 32;
			
			score0 -= values0[i] * (color * 2 - 1);
			score1 -= values1[i] * (color * 2 - 1);
			
			phase += values[type];
			
			if (chess->board[(x + 1) + (y + 3 - color * 2) * 10] == (moonfish_pawn | (piece & 0xF0))) {
				score0 -= pawn_values0[type] * (color * 2 - 1);
				score0 -= pawn_values1[type] * (color * 2 - 1);
			}
		}
	}
	
	return (score0 * phase + score1 * (24 - phase)) / 24;
}

static void moonfish_discard(struct moonfish_node *node)
{
	int i;
	for (i = 0 ; i < node->count ; i++) moonfish_discard(node->children + i);
	if (node->count > 0) free(node->children);
	node->count = 0;
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
	if (!a->ignored && b->ignored) return -1;
	if (a->ignored && !b->ignored) return 1;
	if (a->score != b->score) return a->score - b->score;
	if (a->from != b->from) return a->from - b->from;
	if (a->index != b->index) return a->index - b->index;
	return 0;
}

static void moonfish_expand(struct moonfish_root *root, struct moonfish_node *node, struct moonfish_chess *chess)
{
	int x, y;
	int count, i;
	int child_count;
	struct moonfish_move moves[32];
	struct moonfish_chess other;
	
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
				
				other = *chess;
				moonfish_play(&other, moves + i);
				
				if (!moonfish_validate(&other)) continue;
				moonfish_node(node->children + child_count);
				node->children[child_count].parent = node;
				node->children[child_count].from = (x + 1) + (y + 2) * 10;
				node->children[child_count].index = i;
				
				node->children[child_count].score = moonfish_score(&other);
				if (chess->white) node->children[child_count].score *= -1;
				
				if (node->children[child_count].score > 0) root->positive_node_count++;
				
				child_count++;
			}
		}
	}
	
	if (child_count == 0 && node->children != NULL) free(node->children);
	if (child_count > 0) qsort(node->children, child_count, sizeof *node, &moonfish_compare);
	
	node->count = child_count;
}

static float moonfish_confidence(struct moonfish_node *node)
{
	if (node->visits == 0) return 1e9;
	return 1 / (1 + pow(10, node->score / 400.0)) + 2 * sqrt(log(node->parent->visits) / node->visits);
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
	moonfish_play(chess, &move);
}

static struct moonfish_node *moonfish_select(struct moonfish_node *node, struct moonfish_chess *chess)
{
	struct moonfish_node *next;
	float max_confidence, confidence;
	int i, count;
	
	for (;;) {
		
#ifdef moonfish_no_threads
		count = node->count;
		if (count == 0) return node;
#else
		for (;;) {
			count = 0;
			if (atomic_compare_exchange_strong(&node->count, &count, -1)) return node;
			if (count > 0) break;
		}
#endif
		
		max_confidence = -1;
		
		while (max_confidence < 0) {
			for (i = 0 ; i < count ; i++) {
				if (node->children[i].ignored) continue;
				if (node->children[i].count == -1) continue;
				confidence = moonfish_confidence(node->children + i);
				if (confidence > max_confidence) {
					next = node->children + i;
					max_confidence = confidence;
				}
			}
		}
		
		node = next;
		moonfish_node_chess(node, chess);
	}
}

static void moonfish_propagate(struct moonfish_node *node)
{
	int i;
	short int score, child_score;
	struct moonfish_node *next;
	
	while (node != NULL) {
		score = node->count == 0 ? 0 : SHRT_MIN;
		for (i = 0 ; i < node->count ; i++) {
			child_score = -node->children[i].score;
			if (score < child_score) score = child_score;
		}
		next = node->parent;
		node->score = score;
#ifdef moonfish_no_threads
		node->visits++;
#else
		atomic_fetch_add(&node->visits, 1);
#endif
		node = next;
	}
}

static void moonfish_propagate_bounds(struct moonfish_node *node)
{
	int i, j;
	int bound;
	
	i = 1;
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

static moonfish_result_t moonfish_search(void *data)
{
	struct moonfish_root *root;
	struct moonfish_node *leaf;
	struct moonfish_chess chess;
	int i;
	
	root = data;
	
	for (i = 0 ; i < 1024 ; i++) {
		chess = root->chess;
		leaf = moonfish_select(&root->node, &chess);
		moonfish_expand(root, leaf, &chess);
		if (leaf->count == 0 && moonfish_check(&chess)) moonfish_propagate_bounds(leaf);
		moonfish_propagate(leaf);
	}
	
	return moonfish_value;
}

#ifndef moonfish_no_threads

static void moonfish_start(struct moonfish_root *root, int thread_count)
{
	thrd_t *threads;
	int i;
	
	threads = malloc(thread_count * sizeof *threads);
	if (threads == NULL) {
		perror("malloc");
		exit(1);
	}
	
	for (i = 0 ; i < thread_count ; i++) {
		if (thrd_create(&threads[i], &moonfish_search, root) != thrd_success) {
			fprintf(stderr, "could not create thread\n");
			exit(1);
		}
	}
	
	for (i = 0 ; i < thread_count ; i++) {
		if (thrd_join(threads[i], NULL) != thrd_success) {
			fprintf(stderr, "could not join thread\n");
			exit(1);
		}
	}
	
	free(threads);
}

#endif

static void moonfish_clean(struct moonfish_node *node)
{
	int i;
	for (i = 0 ; i < node->count ; i++) {
		if (node->children[i].visits < node->visits / node->count / 2) {
			moonfish_discard(node->children + i);
		}
		moonfish_clean(node->children + i);
	}
}

void moonfish_best_move(struct moonfish_root *root, struct moonfish_result *result, struct moonfish_options *options)
{
	struct moonfish_node *node;
	long int time, time0;
	long int node_count;
	int i, j;
	int count;
	
	time = LONG_MAX;
	if (options->our_time >= 0) time = options->our_time / 16;
	if (options->max_time >= 0 && time > options->max_time) time = options->max_time;
	time -= time / 32 + 125;
	if (time < 0) time = 0;
	
	time0 = moonfish_clock();
	node_count = options->node_count;
	if (node_count < 0) node_count = LONG_MAX;
	
	for (;;) {
#ifdef moonfish_no_threads
		moonfish_search(root);
#else
		moonfish_start(root, options->thread_count);
#endif
		moonfish_clean(&root->node);
		if (root->node.count > 0) qsort(root->node.children, root->node.count, sizeof root->node, &moonfish_compare);
		for (i = 0 ; i < root->node.count ; i++) {
			node = root->node.children + i;
			for (j = 0 ; j < node->count ; j++) node->children[j].parent = node;
		}
		moonfish_node_move(root->node.children, &root->chess, &result->move);
		result->score = root->node.score;
		result->node_count = root->node.visits;
		result->positive_node_count = root->positive_node_count;
		result->time = moonfish_clock() - time0;
#ifndef moonfish_mini
		if (root->log != NULL) (*root->log)(result, root->data);
		if (root->stop) break;
		if (root->node.visits >= node_count) break;
#endif
		if (result->time >= time) break;
		count = root->node.count;
		for (i = 0 ; i < root->node.count ; i++) {
			if (root->node.children[i].ignored) count--;
		}
		if (count <= 1) break;
	}
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
	
#ifndef moonfish_mini
	root->log = NULL;
	root->stop = 0;
#endif
	moonfish_node(&root->node);
	moonfish_chess(&root->chess);
	root->positive_node_count = 0;
	
	return root;
}

#ifndef moonfish_mini

void moonfish_finish(struct moonfish_root *root)
{
	moonfish_discard(&root->node);
	free(root);
}

void moonfish_stop(struct moonfish_root *root)
{
	root->stop = 1;
}

void moonfish_unstop(struct moonfish_root *root)
{
	root->stop = 0;
}

void moonfish_pv(struct moonfish_root *root, struct moonfish_move *moves, struct moonfish_result *result, int i, int *count)
{
	struct moonfish_node *node;
	struct moonfish_chess chess;
	int j;
	int best_score;
	struct moonfish_node *best_node;
	
	if (i >= root->node.count) *count = 0;
	if (*count == 0) return;
	
	node = root->node.children + i;
	chess = root->chess;
	
	moonfish_node_move(node, &chess, &result->move);
	result->score = -node->score;
	result->node_count = node->visits;
	
	for (j = 0 ; j < *count ; j++) {
		
		if (node == NULL) {
			*count = j;
			break;
		}
		
		moonfish_node_move(node, &chess, moves + j);
		moonfish_play(&chess, moves + j);
		
		best_score = INT_MAX;
		best_node = NULL;
		for (i = 0 ; i < node->count ; i++) {
			if (node->children[i].ignored) continue;
			if (node->children[i].score < best_score) {
				best_node = node->children + i;
				best_score = best_node->score;
			}
		}
		
		node = best_node;
	}
}

void moonfish_idle(struct moonfish_root *root, void (*log)(struct moonfish_result *result, void *data), void *data)
{
	root->log = log;
	root->data = data;
}

#endif
