/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023 zamfofex */

#ifndef MOONFISH
#define MOONFISH

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
	int score;
};

struct moonfish
{
	struct moonfish_chess chess;
	char *argv0;
	int cpu_count;
};

struct moonfish_move
{
	unsigned char from, to;
	unsigned char piece;
	unsigned char promotion;
	unsigned char captured;
	struct moonfish_castle castle;
	int score;
};

void moonfish_chess(struct moonfish_chess *chess);
void moonfish_fen(struct moonfish_chess *chess, char *fen);

void moonfish_moves(struct moonfish_chess *chess, struct moonfish_move *moves, unsigned char from);

void moonfish_play(struct moonfish_chess *chess, struct moonfish_move *move);
void moonfish_unplay(struct moonfish_chess *chess, struct moonfish_move *move);

int moonfish_best_move(struct moonfish *ctx, struct moonfish_move *move, long int our_time, long int their_time);

void moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name);
void moonfish_to_uci(char *name, struct moonfish_move *move, int white);

int moonfish_validate(struct moonfish_chess *chess);
int moonfish_check(struct moonfish_chess *chess);

#endif
