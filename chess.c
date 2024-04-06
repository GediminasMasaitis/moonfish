/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include "moonfish.h"

static int moonfish_piece_scores[] = {82, 337, 365, 477, 1025, 0};

static signed char moonfish_piece_square_scores[] =
{
	0, 0, 0, 0,
	81, 93, 84, 43,
	48, 41, 16, -13,
	22, 9, 15, -18,
	14, 0, 4, -26,
	-3, 0, 14, -19,
	-19, 2, 18, -28,
	0, 0, 0, 0,
	
	6, -65, -52, -128,
	29, 67, -17, -45,
	74, 83, 66, -1,
	45, 44, 17, 6,
	20, 17, 12, -10,
	14, 14, 8, -19,
	-2, 3, -33, -24,
	-25, -43, -20, -64,
	
	-31, -62, 5, -18,
	8, 20, 17, -36,
	37, 46, 37, -9,
	43, 28, 6, -3,
	30, 12, 11, -1,
	14, 21, 16, 5,
	3, 18, 24, 2,
	-17, -13, -21, -27,
	
	57, 20, 36, 37,
	71, 62, 29, 35,
	26, 35, 40, 5,
	25, 21, -9, -22,
	4, -9, -10, -29,
	-7, -8, -15, -39,
	-5, -4, -11, -57,
	16, 4, -25, -22,
	
	35, 36, 21, 8,
	-7, 26, -5, 15,
	18, 31, 15, 22,
	-8, 0, -14, -13,
	-6, -6, -11, -6,
	-3, -4, 8, -4,
	5, 13, -5, -17,
	-2, -17, -24, -25,
	
	-35, -9, 12, -26,
	-7, -12, -19, 0,
	-18, 4, 23, -15,
	-28, -18, -17, -26,
	-42, -35, -17, -50,
	-45, -26, -14, -20,
	-53, -12, 8, 4,
	-23, -8, 30, 0,
};

static void moonfish_create_move(struct moonfish_chess *chess, struct moonfish_move *move, unsigned char from, unsigned char to)
{
	move->from = from;
	move->to = to;
	move->piece = chess->board[from];
	move->promotion = chess->board[from];
	move->captured = chess->board[to];
	move->castle = chess->castle;
	move->score = chess->score;
	move->passing = chess->passing;
}

void moonfish_move(struct moonfish_chess *chess, struct moonfish_move *move, unsigned char from, unsigned char to)
{
	moonfish_create_move(chess, move, from, to);
	if (chess->board[from] % 16 == moonfish_pawn)
	{
		if (chess->board[from] / 16 == 1)
		{
			if (from > 80) move->promotion = moonfish_white_queen;
		}
		else
		{
			if (from < 40) move->promotion = moonfish_black_queen;
		}
	}
}

static void moonfish_jump(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, signed char delta)
{
	unsigned char to;
	to = from + delta;
	if (chess->board[to] == moonfish_outside) return;
	if (chess->board[to] / 16 == chess->board[from] / 16) return;
	moonfish_create_move(chess, (*moves)++, from, to);
}

static void moonfish_slide(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, signed char delta)
{
	unsigned char to;
	
	to = from;
	for (;;)
	{
		to += delta;
		if (chess->board[to] == moonfish_outside) break;
		if (chess->board[to] / 16 == chess->board[from] / 16) break;
		moonfish_create_move(chess, (*moves)++, from, to);
		if (chess->board[to] != moonfish_empty) break;
	}
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
	
	moonfish_jump(chess, moves, from, -2);
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
	
	moonfish_jump(chess, moves, from, 2);
}

static void moonfish_move_king(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	moonfish_jump(chess, moves, from, 11);
	moonfish_jump(chess, moves, from, 9);
	moonfish_jump(chess, moves, from, -9);
	moonfish_jump(chess, moves, from, -11);
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

static void moonfish_pawn_capture(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to)
{
	if (to == chess->passing)
	{
		moonfish_create_move(chess, (*moves)++, from, to);
		return;
	}
	
	if (chess->board[to] / 16 != chess->board[from] / 16)
	if (chess->board[to] != moonfish_empty)
	if (chess->board[to] != moonfish_outside)
		moonfish_move(chess, (*moves)++, from, to);
}

static void moonfish_move_pawn(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	int dy;
	
	dy = chess->white ? 10 : -10;
	
	if (chess->board[from + dy] == moonfish_empty)
	{
		moonfish_move(chess, (*moves)++, from, from + dy);
				
		if (chess->white ? from < 40 : from > 80)
		if (chess->board[from + dy * 2] == moonfish_empty)
			moonfish_move(chess, (*moves)++, from, from + dy * 2);
	}
	
	moonfish_pawn_capture(chess, moves, from, from + dy + 1);
	moonfish_pawn_capture(chess, moves, from, from + dy - 1);
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
	
	type = (piece % 16) - 1;
	color = (piece / 16) - 1;
	
	if (color == 0) y = 7 - y;
	
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
	chess->score += moonfish_table(move->to, move->promotion);
	chess->score -= moonfish_table(move->to, move->captured);
	
	chess->board[move->from] = moonfish_empty;
	chess->board[move->to] = move->promotion;
	
	chess->passing = 0;
	
	if (move->piece % 16 == moonfish_pawn)
	{
		if ((move->to - move->from) / 10 == 2) chess->passing = move->from + 10;
		if ((move->from - move->to) / 10 == 2) chess->passing = move->from - 10;
		
		if ((move->to - move->from) % 10)
		if (move->captured == moonfish_empty)
		{
			if (chess->white) dy = 10, piece = moonfish_black_pawn;
			else dy = -10, piece = moonfish_white_pawn;
			
			chess->score -= moonfish_table(move->to - dy, piece);
			chess->board[move->to - dy] = moonfish_empty;
		}
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
	int dy;
	unsigned char piece;
	
	chess->white ^= 1;
	
	chess->board[move->from] = move->piece;
	chess->board[move->to] = move->captured;
	chess->castle = move->castle;
	chess->score = move->score;
	chess->passing = move->passing;
	
	if (move->piece % 16 == moonfish_pawn)
	{
		if ((move->to - move->from) % 10)
		if (move->captured == moonfish_empty)
		{
			if (chess->white) dy = 10, piece = moonfish_black_pawn;
			else dy = -10, piece = moonfish_white_pawn;
			
			chess->score += moonfish_table(move->to - dy, piece);
			chess->board[move->to - dy] = piece;
		}
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
			chess->board[x1] = moonfish_empty;
			chess->board[x0] = chess->white ? moonfish_white_rook : moonfish_black_rook;
		}
	}
}

void moonfish_chess(struct moonfish_chess *chess)
{
	char pieces[] = {moonfish_rook, moonfish_knight, moonfish_bishop, moonfish_queen, moonfish_king, moonfish_bishop, moonfish_knight, moonfish_rook};
	int x, y;
	
	chess->white = 1;
	chess->castle.white_oo = 1;
	chess->castle.white_ooo = 1;
	chess->castle.black_oo = 1;
	chess->castle.black_ooo = 1;
	chess->score = 0;
	chess->passing = 0;
	
	for (y = 0 ; y < 12 ; y++)
	for (x = 0 ; x < 10 ; x++)
		chess->board[x + y * 10] = moonfish_outside;
	
	for (x = 0 ; x < 8 ; x++)
	{
		chess->board[(x + 1) + 20] = pieces[x] | 0x10;
		chess->board[(x + 1) + 90] = pieces[x] | 0x20;
		chess->board[(x + 1) + 30] = moonfish_white_pawn;
		chess->board[(x + 1) + 80] = moonfish_black_pawn;
		
		for (y = 4 ; y < 8 ; y++)
			chess->board[(x + 1) + y * 10] = moonfish_empty;
	}
}

static void moonfish_from_xy(struct moonfish_chess *chess, struct moonfish_move *move, int x0, int y0, int x1, int y1)
{
	moonfish_create_move(chess, move, (x0 + 1) + (y0 + 2) * 10, (x1 + 1) + (y1 + 2) * 10);
}

void moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	int x0, y0;
	int x1, y1;
	unsigned char color;
	
	x0 = name[0] - 'a';
	y0 = name[1] - '1';
	x1 = name[2] - 'a';
	y1 = name[3] - '1';
	
	moonfish_from_xy(chess, move, x0, y0, x1, y1);
	if (move->piece % 16 == moonfish_king && x0 == 4)
	{
		if (x1 == 0) x1 = 2;
		if (x1 == 7) x1 = 6;
		moonfish_from_xy(chess, move, x0, y0, x1, y1);
	}
	
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
		if (move->promotion % 16 == moonfish_queen) name[4] = 'q';
		if (move->promotion % 16 == moonfish_rook) name[4] = 'r';
		if (move->promotion % 16 == moonfish_bishop) name[4] = 'b';
		if (move->promotion % 16 == moonfish_knight) name[4] = 'n';
		name[5] = 0;
	}
	else
	{
		name[4] = 0;
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
	unsigned char passing;
	
	castle = chess->castle;
	passing = chess->passing;
	
	chess->castle.white_oo = 0;
	chess->castle.white_ooo = 0;
	chess->castle.black_oo = 0;
	chess->castle.black_ooo = 0;
	
	chess->white ^= 1;
	valid = moonfish_validate(chess);
	chess->white ^= 1;
	
	chess->castle = castle;
	chess->passing = passing;
	
	return valid ^ 1;
}

#ifndef moonfish_mini

#include <string.h>

int moonfish_fen(struct moonfish_chess *chess, char *fen)
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
	chess->passing = 0;
	
	for (;;)
	{
		ch = *fen++;
		
		if (ch == 0) return 0;
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
	
	if (*fen++ == 'b') chess->white = 0;
	if (*fen++ != ' ') return 0;
	
	for (;;)
	{
		ch = *fen++;
		
		if (ch == 0) return 0;
		if (ch == ' ') break;
		
		if (ch == 'K') chess->castle.white_oo = 1;
		if (ch == 'Q') chess->castle.white_ooo = 1;
		if (ch == 'k') chess->castle.black_oo = 1;
		if (ch == 'q') chess->castle.black_ooo = 1;
		
		if (ch >= 'A' && ch <= 'H') return 1;
		if (ch >= 'a' && ch <= 'h') return 1;
	}
	
	if (*fen == '-') return 0;
	
	x = *fen++ - 'a';
	y = *fen++ - '1';
	
	chess->passing = (x + 1) + (y + 2) * 10;
	
	return 0;
}

int moonfish_finished(struct moonfish_chess *chess)
{
	struct moonfish_move moves[32], *move;
	int x, y;
	int valid;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			moonfish_play(chess, move);
			valid = moonfish_validate(chess);
			moonfish_unplay(chess, move);
			if (valid) return 0;
		}
	}
	
	return 1;
}

int moonfish_checkmate(struct moonfish_chess *chess)
{
	if (!moonfish_check(chess)) return 0;
	return moonfish_finished(chess);
}

int moonfish_stalemate(struct moonfish_chess *chess)
{
	if (moonfish_check(chess)) return 0;
	return moonfish_finished(chess);
}

static int moonfish_match_move(struct moonfish_chess *chess, struct moonfish_move *result_move, unsigned char type, unsigned char promotion, int x0, int y0, int x1, int y1, int check, int captured)
{
	int found;
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	int xi0, yi0, xi1, yi1;
	int valid;
	
	xi0 = 0, xi1 = 8;
	yi0 = 0, yi1 = 8;
	
	if (x0) xi0 = x0 - 1, xi1 = x0;
	if (y0) yi0 = y0 - 1, yi1 = y0;
	
	found = 0;
	
	for (y = yi0 ; y < yi1 ; y++)
	for (x = xi0 ; x < xi1 ; x++)
	{
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			if (move->piece % 16 != type) continue;
			if (captured && move->captured == moonfish_empty) continue;
			if (promotion && promotion != move->promotion % 16) continue;
			if (move->to % 10 != x1) continue;
			if (move->to / 10 - 1 != y1) continue;
			
			moonfish_play(chess, move);
			valid = moonfish_validate(chess);
			if (valid && check) if (!moonfish_check(chess)) valid = 0;
			if (valid && check == 2) if (!moonfish_checkmate(chess)) valid = 0;
			moonfish_unplay(chess, move);
			if (!valid) continue;
			if (found) continue;
			found = 1;
			*result_move = *move;
		}
	}
	
	if (!found) return 1;
	return 0;
}

int moonfish_from_san(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	int count;
	unsigned char type;
	unsigned char promotion;
	int check, capture;
	int x0, y0;
	int x1, y1;
	char *name0, name_array[32], ch;
	size_t length, i;
	
	length = strlen(name);
	if (length >= sizeof name_array) return 1;
	strcpy(name_array, name);
	name = name_array;
	
	/* reverse the string (because it is easier to parse) */
	for (i = length / 2 ; i < length ; i++)
	{
		ch = name[i];
		name[i] = name[length - i - 1];
		name[length - i - 1] = ch;
	}
	
	check = 0;
	if (*name == '+') check = 2;
	else if (*name == '+') check = 1;
	if (check) name++;
	
	count = 0;
	for (name0 = name ; *name0 != 0 ; name0++)
	{
		if (*name0 == '-') continue;
		if (*name0 == '_') continue;
		if (*name0 == '0') { count++; continue; }
		if (*name0 == 'O') { count++; continue; }
		if (*name0 == 'o') { count++; continue; }
		count = 0;
		break;
	}
	
	if (count > 0)
	{
		if (chess->white) y1 = 1;
		else y1 = 8;
		
		if (count == 2) x1 = 7;
		else if (count == 3) x1 = 3;
		else return 1;
		
		return moonfish_match_move(chess, move, moonfish_king, 0, 5, y1, x1, y1, check, 0);
	}
	
	x0 = 0;
	y0 = 0;
	x1 = 0;
	y1 = 0;
	check = 0;
	
	switch (*name++)
	{
	default:
		promotion = 0;
		name--;
		break;
	case 'K': case 'k':
		promotion = moonfish_king;
		break;
	case 'Q': case 'q':
		promotion = moonfish_queen;
		break;
	case 'R': case 'r':
		promotion = moonfish_rook;
		break;
	case 'B': case 'b':
		promotion = moonfish_bishop;
		break;
	case 'N': case 'n':
		promotion = moonfish_knight;
		break;
	}
	
	if (promotion != 0 && *name == '=') name++;
	
	capture = 0;
	if (*name >= '1' && *name <= '8') y1 = *name++ - '0';
	if (*name >= 'a' && *name <= 'h') x1 = *name++ - 'a' + 1;
	if (*name == 'x') capture = 1, name++;
	if (*name >= '1' && *name <= '8') y0 = *name++ - '0';
	if (*name >= 'a' && *name <= 'h') x0 = *name++ - 'a' + 1;
	
	if (x1 == 0) return 1;
	if (y1 == 0) return 1;
	
	switch (*name++)
	{
	default:
		type = moonfish_pawn;
		if (x0 && y0) type = chess->board[x0 + (y0 + 1) * 10] % 16;
		if (type == 0x0F) return 1;
		name--;
		break;
	case 'K': case 'k':
		type = moonfish_king;
		break;
	case 'Q': case 'q':
		type = moonfish_queen;
		break;
	case 'R': case 'r':
		type = moonfish_rook;
		break;
	case 'B': /* no lowercase here */
		type = moonfish_bishop;
		break;
	case 'N': case 'n':
		type = moonfish_knight;
		break;
	}
	
	if (*name != 0) return 1;
	
	return moonfish_match_move(chess, move, type, promotion, x0, y0, x1, y1, check, capture);
}

/* todo: improve this */
void moonfish_to_fen(struct moonfish_chess *chess, char *fen)
{
	int x, y;
	unsigned char piece;
	int count;
	
	for (y = 7 ; y >= 0 ; y--)
	{
		count = 0;
		for (x = 0 ; x < 8 ; x++)
		{
			piece = chess->board[(x + 1) + (y + 2) * 10];
			if (piece == moonfish_empty)
			{
				count++;
				continue;
			}
			
			if (count != 0)
			{
				*fen++ = '0' + count;
				count = 0;
			}
			
			switch (piece % 16)
			{
			default:
				return;
			case moonfish_pawn:
				*fen = 'p';
				break;
			case moonfish_knight:
				*fen = 'n';
				break;
			case moonfish_bishop:
				*fen = 'b';
				break;
			case moonfish_rook:
				*fen = 'r';
				break;
			case moonfish_queen:
				*fen = 'q';
				break;
			case moonfish_king:
				*fen = 'k';
				break;
			}
			
			if (piece / 16 == 1) *fen += 'A' - 'a';
			fen++;
		}
		
		if (count != 0) *fen++ = '0' + count;
		
		*fen++ = '/';
	}
	
	fen--;
	
	*fen++ = ' ';
	if (chess->white) *fen++ = 'w';
	else *fen++ = 'b';
	*fen++ = ' ';
	
	if (chess->castle.white_oo) *fen++ = 'K';
	if (chess->castle.white_ooo) *fen++ = 'Q';
	if (chess->castle.black_oo) *fen++ = 'k';
	if (chess->castle.black_ooo) *fen++ = 'q';
	if (fen[-1] == ' ') *fen++ = '-';
	
	*fen++ = ' ';
	if (chess->passing)
	{
		*fen++ = 'a' + chess->passing % 10 - 1;
		*fen++ = '1' + chess->passing / 10 - 2;
	}
	
	if (fen[-1] == ' ') *fen++ = '-';
	*fen = 0;
}

void moonfish_to_san(struct moonfish_chess *chess, char *name, struct moonfish_move *move)
{
	static char names[] = "NBRQK";
	
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *other_move;
	char file_ambiguity, rank_ambiguity, ambiguity;
	int to_x, to_y;
	int from_x, from_y;
	
	from_x = move->from % 10 - 1;
	from_y = move->from / 10 - 2;
	
	to_x = move->to % 10 - 1;
	to_y = move->to / 10 - 2;
	
	if (move->piece % 16 == moonfish_pawn)
	{
		if (from_x != to_x)
		{
			*name++ = from_x + 'a';
			*name++ = 'x';
		}
		
		*name++ = to_x + 'a';
		*name++ = to_y + '1';
		
		if (move->promotion % 16 != moonfish_pawn)
		{
			*name++ = '=';
			*name++ = names[move->promotion % 16 - 2];
		}
		
		*name = 0;
		
		return;
	}
	
	file_ambiguity = 0;
	rank_ambiguity = 0;
	ambiguity = 0;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		if ((x + 1) + (y + 2) * 10 == move->from) continue;
		moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		
		for (other_move = moves ; other_move->piece != moonfish_outside ; other_move++)
		{
			if (other_move->to != move->to) continue;
			if (other_move->piece != move->piece) continue;
			
			ambiguity = 1;
			if (other_move->from % 10 - 1 == from_x) file_ambiguity = 1;
			if (other_move->from / 10 - 2 == from_y) rank_ambiguity = 1;
		}
	}
	
	*name++ = names[(move->piece & 0xF) - 2];
	
	if (ambiguity)
	{
		if (file_ambiguity)
		{
			if (rank_ambiguity)
				*name++ = from_x + 'a';
			*name++ = from_y + '1';
		}
		else
		{
			*name++ = from_x + 'a';
		}
	}
	
	if (move->captured != moonfish_empty)
		*name++ = 'x';
		
	*name++ = to_x + 'a';
	*name++ = to_y + '1';
	
	*name = 0;
}

#endif
