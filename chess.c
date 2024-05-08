/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include "moonfish.h"

static int moonfish_piece_square_scores[] =
{
	0, 0, 0, 0,
	232, 240, 242, 229,
	172, 171, 176, 148,
	128, 121, 129, 114,
	119, 111, 116, 98,
	102, 108, 116, 95,
	95, 109, 121, 90,
	0, 0, 0, 0,
	
	330, 318, 308, 224,
	360, 360, 324, 314,
	386, 372, 366, 334,
	371, 367, 352, 338,
	347, 351, 339, 322,
	337, 338, 322, 297,
	319, 311, 301, 287,
	299, 296, 289, 255,
	
	358, 357, 353, 339,
	368, 366, 363, 343,
	386, 381, 378, 372,
	388, 378, 376, 362,
	377, 373, 366, 358,
	370, 369, 363, 355,
	352, 356, 364, 350,
	331, 327, 326, 337,
	
	583, 577, 576, 578,
	593, 593, 585, 587,
	588, 582, 579, 577,
	574, 571, 569, 562,
	554, 553, 552, 542,
	540, 536, 535, 522,
	537, 532, 524, 505,
	553, 545, 529, 513,
	
	1117, 1111, 1099, 1077,
	1124, 1121, 1095, 1097,
	1136, 1124, 1118, 1103,
	1114, 1113, 1098, 1107,
	1080, 1088, 1091, 1099,
	1076, 1080, 1077, 1069,
	1076, 1075, 1069, 1062,
	1065, 1050, 1050, 1071,
	
	30, 37, 43, -17,
	33, 36, 52, 15,
	24, 36, 42, 19,
	7, 17, 20, 0,
	-5, 1, -1, -17,
	-15, -10, -9, -22,
	-37, -14, -1, -1,
	-14, -21, 16, -2,
};

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
	
	if (x < 4) x = 3 - x;
	else x %= 4;
	
	score = moonfish_piece_square_scores[x + y * 4 + type * 32];
	if (color != 0) score *= -1;
	
	return score;
}

static void moonfish_force_promotion(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to, unsigned char promotion)
{
	(*moves)->from = from;
	(*moves)->to = to;
	(*moves)->chess = *chess;
	(*moves)->chess.board[to] = promotion;
	(*moves)->chess.board[from] = moonfish_empty;
	(*moves)->chess.score -= moonfish_table(from, chess->board[from]);
	(*moves)->chess.score += moonfish_table(to, promotion);
	(*moves)->chess.score -= moonfish_table(to, chess->board[to]);
	(*moves)->chess.passing = 0;
	(*moves)->chess.white ^= 1;
	
	if (from == 21 || to == 21) (*moves)->chess.white_ooo = 0;
	if (from == 28 || to == 28) (*moves)->chess.white_oo = 0;
	if (from == 91 || to == 91) (*moves)->chess.black_ooo = 0;
	if (from == 98 || to == 98) (*moves)->chess.black_oo = 0;
	
	(*moves)++;
}

static void moonfish_force_move(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to)
{
	moonfish_force_promotion(chess, moves, from, to, chess->board[from]);
}

static void moonfish_jump(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, signed char delta)
{
	if (chess->board[from + delta] == moonfish_outside) return;
	if (chess->board[from] / 16 == chess->board[from + delta] / 16) return;
	moonfish_force_move(chess, moves, from, from + delta);
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
		moonfish_force_move(chess, moves, from, to);
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
	
	to = from - 3;
	while (to != from)
		if (chess->board[to++] != moonfish_empty)
			return;
	
	if (moonfish_check(chess)) return;
	if (moonfish_attacked(chess, from, from - 1)) return;
	if (moonfish_attacked(chess, from, from - 2)) return;
	
	moonfish_force_move(chess, moves, from - 4, from - 1);
	(*moves)--;
	(*moves)->chess.white = chess->white;
	moonfish_force_move(&(*moves)->chess, moves, from, from - 2);
}

static void moonfish_castle_high(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	unsigned char to;
	
	to = from + 2;
	while (to != from)
		if (chess->board[to--] != moonfish_empty)
			return;
	
	if (moonfish_check(chess)) return;
	if (moonfish_attacked(chess, from, from + 1)) return;
	if (moonfish_attacked(chess, from, from + 2)) return;
	
	moonfish_force_move(chess, moves, from + 3, from + 1);
	(*moves)--;
	(*moves)->chess.white = chess->white;
	moonfish_force_move(&(*moves)->chess, moves, from, from + 2);
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
		if (chess->white_oo) moonfish_castle_high(chess, moves, from);
		if (chess->white_ooo) moonfish_castle_low(chess, moves, from);
	}
	else
	{
		if (chess->black_oo) moonfish_castle_high(chess, moves, from);
		if (chess->black_ooo) moonfish_castle_low(chess, moves, from);
	}
}

static void moonfish_pawn_moves(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to)
{
	unsigned char color;
	
	color = chess->board[from] & 0xF0;
	if ((color == 0x10 && from < 80) || (color == 0x20 && from > 40))
	{
		moonfish_force_move(chess, moves, from, to);
		return;
	}
	
	moonfish_force_promotion(chess, moves, from, to, moonfish_queen | color);
	moonfish_force_promotion(chess, moves, from, to, moonfish_rook | color);
	moonfish_force_promotion(chess, moves, from, to, moonfish_bishop | color);
	moonfish_force_promotion(chess, moves, from, to, moonfish_knight | color);
}

static void moonfish_pawn_capture(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to)
{
	int dy;
	
	if (to == chess->passing)
	{
		dy = chess->white ? 10 : -10;
		
		moonfish_force_move(chess, moves, from, to);
		(*moves)[-1].chess.score -= moonfish_table(to - dy, chess->board[to - dy]);
		(*moves)[-1].chess.board[to - dy] = moonfish_empty;
		return;
	}
	
	if (chess->board[to] / 16 != chess->board[from] / 16)
	if (chess->board[to] != moonfish_empty)
	if (chess->board[to] != moonfish_outside)
		moonfish_pawn_moves(chess, moves, from, to);
}

static void moonfish_move_pawn(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	int dy;
	
	dy = chess->white ? 10 : -10;
	
	if (chess->board[from + dy] == moonfish_empty)
	{
		moonfish_pawn_moves(chess, moves, from, from + dy);
		
		if (chess->white ? from < 40 : from > 80)
		if (chess->board[from + dy * 2] == moonfish_empty)
		{
			moonfish_force_move(chess, moves, from, from + dy * 2);
			(*moves)[-1].chess.passing = from + dy;
		}
	}
	
	moonfish_pawn_capture(chess, moves, from, from + dy + 1);
	moonfish_pawn_capture(chess, moves, from, from + dy - 1);
}

int moonfish_moves(struct moonfish_chess *chess, struct moonfish_move *moves, unsigned char from)
{
	struct moonfish_move *moves0;
	unsigned char piece;
	int i, count;
	
	moves0 = moves;
	piece = chess->board[from];
	
	if (chess->white ? piece / 16 == 1 : piece / 16 == 2)
	{
		if (piece % 16 == moonfish_pawn) moonfish_move_pawn(chess, &moves, from);
		if (piece % 16 == moonfish_knight) moonfish_move_knight(chess, &moves, from);
		if (piece % 16 == moonfish_bishop) moonfish_move_bishop(chess, &moves, from);
		if (piece % 16 == moonfish_rook) moonfish_move_rook(chess, &moves, from);
		if (piece % 16 == moonfish_queen) moonfish_move_queen(chess, &moves, from);
		
		if (piece % 16 == moonfish_king)
		{
			moonfish_move_king(chess, &moves, from);
			
			count = moves - moves0;
			
			for (i = 0 ; i < count ; i++)
			{
				if (chess->white)
				{
					moves0[i].chess.white_oo = 0;
					moves0[i].chess.white_ooo = 0;
				}
				else
				{
					moves0[i].chess.black_oo = 0;
					moves0[i].chess.black_ooo = 0;
				}
			}
		}
	}
	
	return moves - moves0;
}

void moonfish_chess(struct moonfish_chess *chess)
{
	char pieces[] = {moonfish_rook, moonfish_knight, moonfish_bishop, moonfish_queen, moonfish_king, moonfish_bishop, moonfish_knight, moonfish_rook};
	int x, y;
	
	chess->white = 1;
	chess->white_oo = 1;
	chess->white_ooo = 1;
	chess->black_oo = 1;
	chess->black_ooo = 1;
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

int moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	int x0, y0;
	int x1, y1;
	unsigned char piece, color;
	unsigned char from, to;
	int i, count;
	struct moonfish_move moves[32];
	
#ifndef moonfish_mini
	if (name[0] < 'a' || name[0] > 'h') return 1;
	if (name[1] < '1' || name[1] > '8') return 1;
	if (name[2] < 'a' || name[2] > 'h') return 1;
	if (name[3] < '1' || name[3] > '8') return 1;
#endif
	
	x0 = name[0] - 'a';
	y0 = name[1] - '1';
	x1 = name[2] - 'a';
	y1 = name[3] - '1';
	
	piece = chess->board[(x0 + 1) + (y0 + 2) * 10];
	if (piece % 16 == moonfish_king && x0 == 4)
	{
		if (x1 == 0) x1 = 2;
		if (x1 == 7) x1 = 6;
	}
	
	color = piece & 0xF0;
	
	switch (name[4])
	{
	default:
		return 1;
	case 0:
		break;
	case 'q':
		piece = color | moonfish_queen;
		break;
	case 'r':
		piece = color | moonfish_rook;
		break;
	case 'b':
		piece = color | moonfish_bishop;
		break;
	case 'n':
		piece = color | moonfish_knight;
		break;
	}
	
	from = (x0 + 1) + (y0 + 2) * 10;
	to = (x1 + 1) + (y1 + 2) * 10;
	
	count = moonfish_moves(chess, moves, from);
	
	for (i = 0 ; i < count ; i++)
	{
		if (moves[i].to != to) continue;
		if (moves[i].chess.board[to] != piece) continue;
		*move = moves[i];
		return 0;
	}
	
	return 1;
}

void moonfish_to_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	int x, y;
	unsigned char piece;
	
	x = move->from % 10 - 1;
	y = move->from / 10 - 2;
	
	name[0] = x + 'a';
	name[1] = y + '1';
	
	x = move->to % 10 - 1;
	y = move->to / 10 - 2;
	
	name[2] = x + 'a';
	name[3] = y + '1';
	
	name[4] = 0;
	
	piece = move->chess.board[move->to];
	
	if (piece != chess->board[move->from])
	{
		if (piece % 16 == moonfish_queen) name[4] = 'q';
		if (piece % 16 == moonfish_rook) name[4] = 'r';
		if (piece % 16 == moonfish_bishop) name[4] = 'b';
		if (piece % 16 == moonfish_knight) name[4] = 'n';
		name[5] = 0;
	}
}

int moonfish_validate(struct moonfish_chess *chess)
{
	int x, y;
	struct moonfish_move moves[32];
	int i, count;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		for (i = 0 ; i < count ; i++)
			if (chess->board[moves[i].to] % 16 == moonfish_king)
				return 0;
	}
	
	return 1;
}

int moonfish_check(struct moonfish_chess *chess)
{
	int valid;
	unsigned char passing;
	int white_oo, white_ooo;
	int black_oo, black_ooo;
	
	passing = chess->passing;
	white_oo = chess->white_oo;
	white_ooo = chess->white_ooo;
	black_oo = chess->black_oo;
	black_ooo = chess->black_ooo;
	
	chess->white_oo = 0;
	chess->white_ooo = 0;
	chess->black_oo = 0;
	chess->black_ooo = 0;
	
	chess->white ^= 1;
	valid = moonfish_validate(chess);
	chess->white ^= 1;
	
	chess->passing = passing;
	chess->white_oo = white_oo;
	chess->white_ooo = white_ooo;
	chess->black_oo = black_oo;
	chess->black_ooo = black_ooo;
	
	return valid ^ 1;
}

#ifndef moonfish_mini

#include <string.h>

int moonfish_move(struct moonfish_chess *chess, struct moonfish_move *found, unsigned char from, unsigned char to)
{
	struct moonfish_move moves[32];
	int i, count;
	
	count = moonfish_moves(chess, moves, from);
	
	for (i = 0 ; i < count ; i++)
	{
		if (moves[i].to == to)
		{
			if (moonfish_validate(&moves[i].chess))
			{
				*found = moves[i];
				return 0;
			}
		}
	}
	
	return 1;
}

int moonfish_from_fen(struct moonfish_chess *chess, char *fen)
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
	chess->white_oo = 0;
	chess->white_ooo = 0;
	chess->black_oo = 0;
	chess->black_ooo = 0;
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
		
		if (ch == 'K') chess->white_oo = 1;
		if (ch == 'Q') chess->white_ooo = 1;
		if (ch == 'k') chess->black_oo = 1;
		if (ch == 'q') chess->black_ooo = 1;
		
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
	struct moonfish_move moves[32];
	int x, y;
	int i, count;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		for (i = 0 ; i < count ; i++)
			if (moonfish_validate(&moves[i].chess))
				return 0;
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

static int moonfish_match_move(struct moonfish_chess *chess, struct moonfish_move *move, unsigned char type, unsigned char promotion, int x0, int y0, int x1, int y1, int check, int captured)
{
	int found;
	int x, y;
	struct moonfish_move moves[32];
	int xi0, yi0, xi1, yi1;
	int i, count;
	
	xi0 = 0, xi1 = 8;
	yi0 = 0, yi1 = 8;
	
	if (x0) xi0 = x0 - 1, xi1 = x0;
	if (y0) yi0 = y0 - 1, yi1 = y0;
	
	found = 0;
	
	for (y = yi0 ; y < yi1 ; y++)
	for (x = xi0 ; x < xi1 ; x++)
	{
		count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		for (i = 0 ; i < count ; i++)
		{
			if (chess->board[moves[i].from] % 16 != type) continue;
			if (captured && chess->board[moves[i].to] == moonfish_empty) continue;
			if (promotion && promotion != moves[i].chess.board[moves[i].to] % 16) continue;
			if (moves[i].to % 10 != x1) continue;
			if (moves[i].to / 10 - 1 != y1) continue;
			
			if (!moonfish_validate(&moves[i].chess)) continue;
			if (check && !moonfish_check(chess)) continue;
			if (check == 2 && !moonfish_checkmate(chess)) continue;
			if (found) return 1;
			found = 1;
			*move = moves[i];
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
	
	if (chess->white_oo) *fen++ = 'K';
	if (chess->white_ooo) *fen++ = 'Q';
	if (chess->black_oo) *fen++ = 'k';
	if (chess->black_ooo) *fen++ = 'q';
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

void moonfish_to_san(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	static char names[] = "NBRQK";
	
	int x, y;
	struct moonfish_move moves[32];
	char file_ambiguity, rank_ambiguity, ambiguity;
	int to_x, to_y;
	int from_x, from_y;
	int i, count;
	
	from_x = move->from % 10 - 1;
	from_y = move->from / 10 - 2;
	
	to_x = move->to % 10 - 1;
	to_y = move->to / 10 - 2;
	
	if (chess->board[move->from] % 16 == moonfish_pawn)
	{
		if (from_x != to_x)
		{
			*name++ = from_x + 'a';
			*name++ = 'x';
		}
		
		*name++ = to_x + 'a';
		*name++ = to_y + '1';
		
		if (move->chess.board[move->to] % 16 != moonfish_pawn)
		{
			*name++ = '=';
			*name++ = names[move->chess.board[move->to] % 16 - 2];
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
		count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
		
		for (i = 0 ; i < count ; i++)
		{
			if (moves[i].to != move->to) continue;
			if (chess->board[moves[i].from] != chess->board[move->from]) continue;
			
			ambiguity = 1;
			if (moves[i].from % 10 - 1 == from_x) file_ambiguity = 1;
			if (moves[i].from / 10 - 2 == from_y) rank_ambiguity = 1;
		}
	}
	
	*name++ = names[(chess->board[move->from] & 0xF) - 2];
	
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
	
	if (chess->board[move->to] != moonfish_empty)
		*name++ = 'x';
		
	*name++ = to_x + 'a';
	*name++ = to_y + '1';
	
	*name = 0;
}

#endif
