/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023 zamfofex */

#ifndef MOONFISH
#define MOONFISH

enum
{
	moonfish_white_pawn = 0x11,
	moonfish_white_knight = 0x12,
	moonfish_white_bishop = 0x13,
	moonfish_white_rook = 0x14,
	moonfish_white_queen = 0x15,
	moonfish_white_king = 0x16,
	
	moonfish_black_pawn = 0x21,
	moonfish_black_knight = 0x22,
	moonfish_black_bishop = 0x23,
	moonfish_black_rook = 0x24,
	moonfish_black_queen = 0x25,
	moonfish_black_king = 0x26,
	
	moonfish_pawn = 1,
	moonfish_knight = 2,
	moonfish_bishop = 3,
	moonfish_rook = 4,
	moonfish_queen = 5,
	moonfish_king = 6,
	
	moonfish_outside = 0,
	moonfish_empty = 0xFF,
	
	moonfish_depth = 50,
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
	unsigned char passing;
	struct moonfish_castle castle;
	int score;
};

struct moonfish
{
	struct moonfish_chess chess;
	char *argv0;
};

struct moonfish_move
{
	unsigned char from, to;
	unsigned char piece;
	unsigned char promotion;
	unsigned char captured;
	unsigned char passing;
	struct moonfish_castle castle;
	int score;
};

void moonfish_chess(struct moonfish_chess *chess);
int moonfish_fen(struct moonfish_chess *chess, char *fen);

void moonfish_moves(struct moonfish_chess *chess, struct moonfish_move *moves, unsigned char from);

void moonfish_play(struct moonfish_chess *chess, struct moonfish_move *move);
void moonfish_unplay(struct moonfish_chess *chess, struct moonfish_move *move);

int moonfish_best_move(struct moonfish *ctx, struct moonfish_move *move, long int our_time, long int their_time);
int moonfish_countdown(int score);

void moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name);
void moonfish_to_uci(char *name, struct moonfish_move *move);

int moonfish_validate(struct moonfish_chess *chess);
int moonfish_check(struct moonfish_chess *chess);

#endif
