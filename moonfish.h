#ifndef MOONFISH
#define MOONFISH

#include <stdio.h>

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
	
	moonfish_omega = 10000000
};

struct moonfish_nnue
{
	int values0[6][64][10];
	int values[2][6][64][10];
	signed char pst0[64 * 6], pst1[10 * 6 * 6], pst3[10 * 10 * 2];
	signed char layer1[180], layer2[10];
	char scale;
};

struct moonfish
{
	struct moonfish_nnue nnue;
	unsigned char board[120];
	char white;
};

struct moonfish_move
{
	unsigned char from, to;
	unsigned char piece;
	unsigned char promotion;
	unsigned char captured;
};

int moonfish_nnue(struct moonfish *ctx, FILE *file);

void moonfish_chess(struct moonfish *ctx);
void moonfish_fen(struct moonfish *ctx, char *fen);

void moonfish_moves(struct moonfish *ctx, struct moonfish_move *moves, unsigned char from);

void moonfish_play(struct moonfish *ctx, struct moonfish_move *move);
void moonfish_unplay(struct moonfish *ctx, struct moonfish_move *move);

void moonfish_show(struct moonfish *ctx);

int moonfish_tanh(int value);

int moonfish_best_move(struct moonfish *ctx, struct moonfish_move *move);

void moonfish_play_uci(struct moonfish *ctx, char *name);
void moonfish_to_uci(char *name, struct moonfish_move *move, int white);

int moonfish_validate(struct moonfish *ctx);

#endif
