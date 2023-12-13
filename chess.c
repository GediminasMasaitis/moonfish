/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023 zamfofex */

#include "moonfish.h"

static int moonfish_piece_scores[] = {100, 320, 330, 500, 900, 0};

static signed char moonfish_piece_square_scores[] =
{
	0, 0, 0, 0,
	50, 50, 50, 50,
	30, 20, 10, 10,
	25, 10, 5, 5,
	20, 0, 0, 0,
	0, -10, -5, 5,
	-20, 10, 10, 5,
	0, 0, 0, 0,
	
	-30, -30, -40, -50,
	5, 0, -20, -40,
	15, 10, 5, -30,
	20, 15, 0, -30,
	20, 15, 0, -30,
	15, 10, 5, -30,
	5, 0, -20, -40,
	-30, -30, -40, -50,
	
	-10, -10, -10, -20,
	0, 0, 0, -10,
	10, 5, 0, -10,
	10, 5, 5, -10,
	10, 10, 0, -10,
	10, 10, 10, -10,
	0, 0, 5, -10,
	-10, -10, -10, -20,
	
	0, 0, 0, 0,
	10, 10, 10, 5,
	0, 0, 0, -5,
	0, 0, 0, -5,
	0, 0, 0, -5,
	0, 0, 0, -5,
	0, 0, 0, -5,
	5, 0, 0, 0,
	
	-5, -10, -10, -20,
	0, 0, 0, -10,
	5, 5, 0, -10,
	5, 5, 0, -5,
	5, 5, 0, -5,
	5, 5, 0, -10,
	0, 0, 0, -10,
	-5, -10, -10, -20,
	
	-50, -40, -40, -30,
	-50, -40, -40, -30,
	-50, -40, -40, -30,
	-50, -40, -40, -30,
	-40, -30, -30, -20,
	-20, -20, -20, -10,
	0, 0, 20, 20,
	0, 10, 30, 20,
};

static struct moonfish_move *moonfish_create_move(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to)
{
	(*moves)->from = from;
	(*moves)->to = to;
	(*moves)->piece = chess->board[from];
	(*moves)->promotion = chess->board[from];
	(*moves)->captured = chess->board[to];
	(*moves)->castle = chess->castle;
	(*moves)->score = chess->score;
	return (*moves)++;
}

static char moonfish_delta(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char *to, signed char delta)
{
	*to += delta;
	if (chess->board[*to] == moonfish_outside) return 0;
	if (chess->board[*to] / 16 == chess->board[from] / 16) return 0;
	moonfish_create_move(chess, moves, from, *to);
	if (chess->board[*to] != moonfish_empty) return 0;
	return 1;
}

static void moonfish_slide(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, signed char delta)
{
	unsigned char to;
	to = from;
	while (moonfish_delta(chess, moves, from, &to, delta)) { }
}

static void moonfish_jump(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, signed char delta)
{
	moonfish_delta(chess, moves, from, &from, delta);
}

static void moonfish_move_knight(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	moonfish_jump(chess, moves, from, 21);
	moonfish_jump(chess, moves, from, 19);
	moonfish_jump(chess, moves, from, -19);
	moonfish_jump(chess, moves, from, -21);
	moonfish_jump(chess, moves, from, 12);
	moonfish_jump(chess, moves, from, 8);
	moonfish_jump(chess, moves, from, -8);
	moonfish_jump(chess, moves, from, -12);
}

static void moonfish_move_bishop(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	moonfish_slide(chess, moves, from, 11);
	moonfish_slide(chess, moves, from, 9);
	moonfish_slide(chess, moves, from, -9);
	moonfish_slide(chess, moves, from, -11);
}

static void moonfish_move_rook(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	moonfish_slide(chess, moves, from, 10);
	moonfish_slide(chess, moves, from, -10);
	moonfish_slide(chess, moves, from, 1);
	moonfish_slide(chess, moves, from, -1);
}

static void moonfish_move_queen(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	moonfish_move_rook(chess, moves, from);
	moonfish_move_bishop(chess, moves, from);
}

static char moonfish_attacked(struct moonfish_chess *chess, unsigned char from, unsigned char to)
{
	int check;
	unsigned char piece;
	
	if (chess->white) piece = moonfish_white_king;
	else piece = moonfish_black_king;
	
	chess->board[from] = moonfish_empty;
	chess->board[to] = piece;
	check = moonfish_check(chess);
	chess->board[from] = piece;
	chess->board[to] = moonfish_empty;
	return check;
}

static void moonfish_castle_low(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	unsigned char to;
	
	to = chess->white ? 22 : 92;
	while (to != from)
		if (chess->board[to++] != moonfish_empty)
			return;
	
	if (moonfish_check(chess)) return;
	if (moonfish_attacked(chess, from, from - 1)) return;
	if (moonfish_attacked(chess, from, from - 2)) return;
	
	moonfish_create_move(chess, moves, from, from - 2);
}

static void moonfish_castle_high(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	unsigned char to;
	
	to = chess->white ? 27 : 97;
	while (to != from)
		if (chess->board[to--] != moonfish_empty)
			return;
	
	if (moonfish_check(chess)) return;
	if (moonfish_attacked(chess, from, from + 1)) return;
	if (moonfish_attacked(chess, from, from + 2)) return;
	
	moonfish_create_move(chess, moves, from, from + 2);
}

static void moonfish_move_king(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	moonfish_jump(chess, moves, from, 1 + 10);
	moonfish_jump(chess, moves, from, -1 + 10);
	moonfish_jump(chess, moves, from, 1 - 10);
	moonfish_jump(chess, moves, from, -1 - 10);
	moonfish_jump(chess, moves, from, 10);
	moonfish_jump(chess, moves, from, -10);
	moonfish_jump(chess, moves, from, 1);
	moonfish_jump(chess, moves, from, -1);
	
	if (chess->white)
	{
		if (chess->castle.white_oo) moonfish_castle_high(chess, moves, from);
		if (chess->castle.white_ooo) moonfish_castle_low(chess, moves, from);
	}
	else
	{
		if (chess->castle.black_oo) moonfish_castle_high(chess, moves, from);
		if (chess->castle.black_ooo) moonfish_castle_low(chess, moves, from);
	}
}

static void moonfish_move_pawn(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	struct moonfish_move *move;
	unsigned char promotion;
	int dy;
	
	if (chess->white)
	{
		dy = 10;
		promotion = moonfish_white_pawn;
		if (from > 80) promotion = moonfish_white_queen;
	}
	else
	{
		dy = -10;
		promotion = moonfish_black_pawn;
		if (from < 30) promotion = moonfish_black_queen;
	}
	
	if (chess->board[from + dy] == moonfish_empty)
	{
		move = moonfish_create_move(chess, moves, from, from + dy);
		move->promotion = promotion;
				
		if (chess->white ? from < 40 : from > 70)
		{
			if (chess->board[from + dy * 2] == moonfish_empty)
			{
				move = moonfish_create_move(chess, moves, from, from + dy * 2);
				move->promotion = promotion;
			}
		}
	}
	
	if (chess->board[from + dy + 1] / 16 != chess->board[from] / 16)
	if (chess->board[from + dy + 1] != moonfish_empty)
	if (chess->board[from + dy + 1] != moonfish_outside)
	{
		move = moonfish_create_move(chess, moves, from, from + dy + 1);
		move->promotion = promotion;
	}
	
	if (chess->board[from + dy - 1] / 16 != chess->board[from] / 16)
	if (chess->board[from + dy - 1] != moonfish_empty)
	if (chess->board[from + dy - 1] != moonfish_outside)
	{
		move = moonfish_create_move(chess, moves, from, from + dy - 1);
		move->promotion = promotion;
	}
}

void moonfish_moves(struct moonfish_chess *chess, struct moonfish_move *moves, unsigned char from)
{
	unsigned char piece;
	
	piece = chess->board[from];
	
	if (chess->white ? piece / 16 == 1 : piece / 16 == 2)
	{
		if (piece % 16 == moonfish_pawn) moonfish_move_pawn(chess, &moves, from);
		if (piece % 16 == moonfish_knight) moonfish_move_knight(chess, &moves, from);
		if (piece % 16 == moonfish_bishop) moonfish_move_bishop(chess, &moves, from);
		if (piece % 16 == moonfish_rook) moonfish_move_rook(chess, &moves, from);
		if (piece % 16 == moonfish_queen) moonfish_move_queen(chess, &moves, from);
		if (piece % 16 == moonfish_king) moonfish_move_king(chess, &moves, from);
	}
	moves->piece = moonfish_outside;
}

static int moonfish_table(int from, unsigned char piece)
{
	int x, y;
	unsigned char type, color;
	int score;
	
	if (piece == moonfish_empty) return 0;
	
	x = from % 10 - 1;
	y = from / 10 - 2;
	
	type = (piece & 0xF) - 1;
	color = (piece >> 4) - 1;
	
	if (x < 4) x = 4 - x;
	else x %= 4;
	
	score = moonfish_piece_scores[type] + moonfish_piece_square_scores[x + y * 4 + type * 32];
	if (color != 0) score *= -1;
	
	return score;
}

void moonfish_play(struct moonfish_chess *chess, struct moonfish_move *move)
{
	int x0, x1;
	int dy;
	unsigned char piece;
	
	chess->score -= moonfish_table(move->from, move->piece);
	chess->score += moonfish_table(move->to, move->piece);
	chess->score -= moonfish_table(move->to, move->captured);
	
	chess->board[move->from] = moonfish_empty;
	chess->board[move->to] = move->promotion;
	
	if (move->piece % 16 == moonfish_pawn)
	if ((move->to - move->from) % 10)
	if (move->captured == moonfish_empty)
	{
		if (chess->white) dy = 10, piece = moonfish_black_pawn;
		else dy = -10, piece = moonfish_white_pawn;
		
		chess->score -= moonfish_table(move->to - dy, piece);
		chess->board[move->to - dy] = moonfish_empty;
	}
	
	if (move->piece % 16 == moonfish_king)
	{
		x0 = 0;
		if (move->from == 25 && move->to == 27) x0 = 28, x1 = 26;
		if (move->from == 25 && move->to == 23) x0 = 21, x1 = 24;
		if (move->from == 95 && move->to == 97) x0 = 98, x1 = 96;
		if (move->from == 95 && move->to == 93) x0 = 91, x1 = 94;
		if (x0)
		{
			piece = chess->white ? moonfish_white_rook : moonfish_black_rook;
			chess->score -= moonfish_table(x0, piece);
			chess->score += moonfish_table(x1, piece);
			chess->board[x0] = moonfish_empty;
			chess->board[x1] = piece;
		}
		
		if (chess->white)
		{
			chess->castle.white_oo = 0;
			chess->castle.white_ooo = 0;
		}
		else
		{
			chess->castle.black_oo = 0;
			chess->castle.black_ooo = 0;
		}
	}
	
	if (move->piece == moonfish_white_rook)
	{
		if (move->from % 10 == 1) chess->castle.white_ooo = 0;
		if (move->from % 10 == 8) chess->castle.white_oo = 0;
	}
	if (move->piece == moonfish_black_rook)
	{
		if (move->from % 10 == 1) chess->castle.black_ooo = 0;
		if (move->from % 10 == 8) chess->castle.black_oo = 0;
	}
	
	if (move->captured == moonfish_white_rook)
	{
		if (move->to % 10 == 1) chess->castle.white_ooo = 0;
		if (move->to % 10 == 8) chess->castle.white_oo = 0;
	}
	if (move->captured == moonfish_black_rook)
	{
		if (move->to % 10 == 1) chess->castle.black_ooo = 0;
		if (move->to % 10 == 8) chess->castle.black_oo = 0;
	}
	
	chess->white ^= 1;
}

void moonfish_unplay(struct moonfish_chess *chess, struct moonfish_move *move)
{
	int x0, x1;
	
	chess->white ^= 1;
	
	chess->board[move->from] = move->piece;
	chess->board[move->to] = move->captured;
	chess->castle = move->castle;
	chess->score = move->score;
	
	if (move->piece % 16 == moonfish_king)
	{
		x0 = 0;
		if (move->from == 25 && move->to == 27) x0 = 28, x1 = 26;
		if (move->from == 25 && move->to == 23) x0 = 21, x1 = 24;
		if (move->from == 95 && move->to == 97) x0 = 98, x1 = 96;
		if (move->from == 95 && move->to == 93) x0 = 91, x1 = 94;
		if (x0)
		{
			chess->board[x1] = moonfish_empty;
			chess->board[x0] = chess->white ? moonfish_white_rook : moonfish_black_rook;
		}
	}
}

void moonfish_chess(struct moonfish_chess *chess)
{
	int x, y;
	
	for (y = 0 ; y < 12 ; y++)
	for (x = 0 ; x < 10 ; x++)
		chess->board[x + y * 10] = moonfish_outside;
	
	moonfish_fen(chess, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq");
}

void moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	int x, y;
	unsigned char color;
	
	x = name[0] - 'a';
	y = name[1] - '1';
	
	move->from = (x + 1) + (y + 2) * 10;
	
	x = name[2] - 'a';
	y = name[3] - '1';
	
	move->to = (x + 1) + (y + 2) * 10;
	
	move->piece = chess->board[move->from];
	move->captured = chess->board[move->to];
	move->promotion = move->piece;
	move->castle = chess->castle;
	move->score = chess->score;
	
	color = chess->white ? 0x10 : 0x20;
	if (name[4] == 'q') move->promotion = color | moonfish_queen;
	if (name[4] == 'r') move->promotion = color | moonfish_rook;
	if (name[4] == 'b') move->promotion = color | moonfish_bishop;
	if (name[4] == 'n') move->promotion = color | moonfish_knight;
}

void moonfish_to_uci(char *name, struct moonfish_move *move)
{
	int x, y;
	
	x = move->from % 10 - 1;
	y = move->from / 10 - 2;
	
	name[0] = x + 'a';
	name[1] = y + '1';
	
	x = move->to % 10 - 1;
	y = move->to / 10 - 2;
	
	name[2] = x + 'a';
	name[3] = y + '1';
	
	if (move->promotion != move->piece)
	{
		name[4] = 'q';
		name[5] = 0;
	}
	else
	{
		name[4] = 0;
	}
}

void moonfish_fen(struct moonfish_chess *chess, char *fen)
{
	int x, y;
	unsigned char type, color;
	char ch;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
		chess->board[(x + 1) + (y + 2) * 10] = moonfish_empty;
	
	x = 0;
	y = 0;
	
	chess->white = 1;
	chess->castle.white_oo = 0;
	chess->castle.white_ooo = 0;
	chess->castle.black_oo = 0;
	chess->castle.black_ooo = 0;
	chess->score = 0;
	
	for (;;)
	{
		ch = *fen++;
		
		if (ch == 0) return;
		if (ch == ' ') break;
		
		if (ch == '/')
		{
			x = 0;
			y++;
			continue;
		}
		
		if (ch >= '0' && ch <= '9')
		{
			x += ch - '0';
			continue;
		}
		
		if (ch >= 'A' && ch <= 'Z')
			ch -= 'A' - 'a',
			color = 0x10;
		else
			color = 0x20;
		
		type = 0;
		if (ch == 'p') type = 1;
		if (ch == 'n') type = 2;
		if (ch == 'b') type = 3;
		if (ch == 'r') type = 4;
		if (ch == 'q') type = 5;
		if (ch == 'k') type = 6;
		
		chess->board[(x + 1) + (9 - y) * 10] = type | color;
		
		chess->score += moonfish_table((x + 1) + (9 - y) * 10, type | color);
		
		x++;
	}
	
	if (*fen++ == 'b') chess->white ^= 1;
	if (*fen++ != ' ') return;
	
	for (;;)
	{
		ch = *fen++;
		if (ch == 0) return;
		if (ch == ' ') break;
		if (ch == 'K') chess->castle.white_oo = 1;
		if (ch == 'Q') chess->castle.white_ooo = 1;
		if (ch == 'k') chess->castle.black_oo = 1;
		if (ch == 'q') chess->castle.black_ooo = 1;
	}
}

int moonfish_validate(struct moonfish_chess *chess)
{
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		for (move = moves ; move->piece != moonfish_outside ; move++)
			if (move->captured % 16 == moonfish_king)
				return 0;
	}
	
	return 1;
}

int moonfish_check(struct moonfish_chess *chess)
{
	int valid;
	struct moonfish_castle castle;
	
	castle = chess->castle;
	
	chess->castle.white_oo = 0;
	chess->castle.white_ooo = 0;
	chess->castle.black_oo = 0;
	chess->castle.black_ooo = 0;
	
	chess->white ^= 1;
	valid = moonfish_validate(chess);
	chess->white ^= 1;
	
	chess->castle = castle;
	
	return valid ^ 1;
}
