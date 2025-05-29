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
#ifndef moonfish_mini
	_Atomic int stop;
	void (*log)(struct moonfish_result *result, void *data);
	void *data;
#endif
};

static short int moonfish_score(struct moonfish_chess *chess)
{
	static short int values0[] = {0, 0, 0, 0, 56, 96, 84, 65, 61, 92, 74, 79, 58, 88, 83, 95, 69, 102, 96, 115, 82, 132, 156, 159, 258, 226, 262, 274, 0, 0, 0, 0, 262, 317, 317, 310, 323, 314, 337, 344, 321, 350, 356, 367, 341, 370, 371, 371, 366, 368, 406, 398, 361, 399, 433, 442, 338, 329, 420, 402, 194, 295, 237, 363, 356, 375, 360, 346, 382, 394, 396, 378, 386, 393, 387, 395, 382, 390, 392, 414, 380, 397, 421, 430, 406, 429, 426, 440, 374, 391, 413, 395, 343, 339, 291, 298, 456, 467, 469, 477, 427, 460, 462, 465, 440, 462, 447, 458, 447, 457, 454, 469, 474, 484, 496, 513, 491, 529, 533, 548, 523, 521, 552, 552, 564, 550, 535, 552, 1046, 1032, 1038, 1058, 1046, 1062, 1070, 1058, 1049, 1063, 1056, 1052, 1047, 1056, 1051, 1046, 1073, 1053, 1058, 1057, 1078, 1083, 1077, 1071, 1049, 1001, 1063, 1041, 1057, 1071, 1089, 1107, 20, 37, -21, -15, 18, 4, -61, -82, -63, -50, -84, -97, -93, -68, -82, -97, -77, -40, -24, -40, -31, 28, 52, 36, 44, 13, 53, 63, 163, 141, 61, 70};
	static short int values1[] = {0, 0, 0, 0, 137, 142, 141, 145, 132, 137, 127, 134, 139, 138, 123, 119, 156, 152, 134, 125, 223, 214, 184, 178, 255, 269, 236, 215, 0, 0, 0, 0, 320, 318, 360, 369, 345, 378, 378, 387, 360, 388, 396, 416, 384, 403, 428, 436, 388, 413, 430, 439, 376, 396, 414, 416, 362, 399, 385, 408, 317, 372, 415, 387, 389, 391, 391, 410, 393, 402, 404, 420, 402, 416, 431, 431, 410, 429, 436, 437, 420, 437, 431, 440, 412, 421, 431, 425, 399, 420, 418, 423, 409, 418, 423, 430, 699, 708, 715, 717, 706, 704, 709, 708, 705, 712, 718, 716, 724, 732, 737, 734, 737, 740, 743, 738, 744, 738, 742, 734, 742, 749, 743, 746, 727, 736, 744, 738, 1255, 1256, 1247, 1234, 1248, 1249, 1246, 1271, 1276, 1274, 1307, 1305, 1308, 1332, 1342, 1359, 1309, 1358, 1374, 1384, 1318, 1333, 1375, 1386, 1327, 1388, 1381, 1410, 1338, 1346, 1357, 1352, -71, -42, -29, -42, -27, -11, 12, 17, -3, 12, 30, 39, 8, 30, 45, 55, 21, 48, 49, 52, 29, 49, 42, 30, -7, 42, 27, 10, -95, -30, -17, -26};
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

static void moonfish_expand(struct moonfish_node *node, struct moonfish_chess *chess)
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
		moonfish_expand(leaf, &chess);
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
#endif
	root->stop = 0;
	moonfish_node(&root->node);
	moonfish_chess(&root->chess);
	
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
