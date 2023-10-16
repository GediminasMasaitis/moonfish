#ifndef MOONFISH
#define MOONFISH

#include <stdio.h>

#ifdef MOONFISH_HAS_PTHREAD
#include <pthread.h>
#endif

enum
{
	moonfish_our_pawn = 0x11,
	moonfish_our_knight = 0x12,
	moonfish_our_bishop = 0x13,
	moonfish_our_rook = 0x14,
	moonfish_our_queen = 0x15,
	moonfish_our_king = 0x16,
	
	moonfish_their_pawn = 0x21,
	moonfish_their_knight = 0x22,
	moonfish_their_bishop = 0x23,
	moonfish_their_rook = 0x24,
	moonfish_their_queen = 0x25,
	moonfish_their_king = 0x26,
	
	moonfish_outside = 0,
	moonfish_empty = 0xFF,
	
	moonfish_omega = 5000000
};

struct moonfish_nnue
{
	int values0[6][64][10];
	int values[2][6][64][10];
	signed char pst0[64 * 6], pst1[10 * 6 * 6], pst3[10 * 10 * 2];
	signed char layer1[180], layer2[10];
	signed char scale;
};

struct moonfish_castle
{
	unsigned int white_oo:1, white_ooo:1;
	unsigned int black_oo:1, black_ooo:1;
};

struct moonfish_chess
{
	unsigned char board[120];
	unsigned char white;
	struct moonfish_castle castle;
};

struct moonfish
{
	struct moonfish_nnue nnue;
	struct moonfish_chess chess;
};

struct moonfish_move
{
	unsigned char from, to;
	unsigned char piece;
	unsigned char promotion;
	unsigned char captured;
	struct moonfish_castle castle;
};

int moonfish_nnue(struct moonfish_nnue *nnue, FILE *file);

void moonfish_chess(struct moonfish_chess *chess);
void moonfish_fen(struct moonfish_chess *chess, char *fen);

void moonfish_moves(struct moonfish_chess *chess, struct moonfish_move *moves, unsigned char from);

void moonfish_play(struct moonfish_chess *chess, struct moonfish_move *move);
void moonfish_unplay(struct moonfish_chess *chess, struct moonfish_move *move);

int moonfish_tanh(int value);

int moonfish_best_move(struct moonfish *ctx, struct moonfish_move *move, int time);

void moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name);
void moonfish_to_uci(char *name, struct moonfish_move *move, int white);

int moonfish_validate(struct moonfish_chess *chess);
int moonfish_check(struct moonfish_chess *chess);

extern char *moonfish_network;

#endif
