/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include "moonfish.h"

moonfish_t moonfish_values[moonfish_size] = {10018,6197,13742,20265,18885,-342,0,6774,8234,19746,19826,8133,7748,0,14909,19962,16209,-626,3200,11794,0,20831,17808,2331,9960,11154,16101,0,16443,-321,9922,9553,11914,15629,0,-889,13093,2080,1464,6178,889,0,11428,7046,14154,17650,17730,2406,10440,7005,13087,19265,19949,410,7084,7102,38,19215,21044,845,9384,7266,13726,20727,19654,-81,9528,7427,11709,20251,1861,9603,9701,7048,11465,20080,9603,0,0,0,0,0,23861,19671,20199,18789,13275,7766,7405,7237,7018,-1804,-2692,-2458,3890,-4358,-4924,-4265,3114,-4929,-5637,-6308,3834,-3087,-5210,-7140,0,0,0,0,11102,8573,8899,10834,14265,11479,3003,4139,13080,3599,-8431,-7074,14841,3244,-8967,-8168,13439,1982,-10393,-10556,10011,-458,-12847,-13100,15511,10850,-1595,-1143,9901,15251,8821,9572,26448,15718,17220,17953,16633,-6319,-6154,-5215,21217,-5336,-3635,-3049,20050,-4507,-4287,-2630,19910,-5376,-4347,-4017,19474,-5900,-5030,-4681,18801,-5651,-6395,-7239,29091,17184,16394,16484,36920,16419,16372,17421,17661,-2571,-1461,-1335,16435,-2556,-2056,-2014,14483,-4074,-3829,-3995,11706,-6133,-6216,-6371,9694,-8191,-8385,-8176,7358,-10570,-9813,-9690,27362,9776,11255,11747,71012,35511,38170,40036,37937,-16976,-13070,-11601,38573,-14295,-12982,-10860,39968,-16951,-14245,-14049,38376,-17331,-18035,-18791,36345,-19106,-18880,-19951,33663,-21879,-21634,-21735,68560,31904,32329,33224,169,3435,3850,3038,1695,8128,5002,4953,691,4616,3554,2015,-2061,186,779,-471,-5789,-2687,-1912,-1942,-5593,-3130,-2840,-3162,-1159,-144,-1939,-3987,-377,1153,-3549,-2521};

moonfish_t moonfish_score(struct moonfish_chess *chess)
{
	static int deltas[][5] =
	{
		{0}, {9, 11, 0},
		{21, 19, 12, 8, 0},
		{11, 9, 0},
		{10, 1, 0},
		{10, 1, 11, 9, 0},
		{10, 1, 11, 9, 0},
	};
	
	int x, y;
	int x1, y1;
	int from;
	unsigned char type, color, other;
	int i, j;
	moonfish_t score, value;
	
	score = 0;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		from = (x + 1) + (y + 2) * 10;
		type = chess->board[from] % 16;
		color = chess->board[from] / 16;
		
		if (chess->board[from] != moonfish_empty)
		{
			x1 = x;
			y1 = y;
			if (x1 > 3) x1 = 7 - x1;
			if (color == 1) y1 = 7 - y1;
			score -= moonfish_values[78 + x1 + y1 * 4 + (type - 1) * 32] * (color * 2 - 3);
		}
		
		for (j = 0 ; j < 2 ; j++)
		for (i = 0 ; deltas[type][i] ; i++)
		{
			if (type == moonfish_pawn && j != color - 1) continue;
			other = chess->board[from + deltas[type][i] * (j * 2 - 1)];
			if (other == moonfish_outside) continue;
			if (other / 16 != color) other %= 16;
			else other = other % 16 + 6;
			value = moonfish_values[other * 6 + type - 1];
			if (color == 2) value *= -1;
			score += value;
		}
	}
	
	return score;
}

static void moonfish_force_promotion(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to, unsigned char promotion)
{
	(*moves)->from = from;
	(*moves)->to = to;
	(*moves)->chess = *chess;
	(*moves)->chess.board[to] = promotion;
	(*moves)->chess.board[from] = moonfish_empty;
	(*moves)->chess.passing = 0;
	(*moves)->chess.white ^= 1;
	
	if (from == 21 || to == 21) (*moves)->chess.ooo[0] = 0;
	if (from == 28 || to == 28) (*moves)->chess.oo[0] = 0;
	if (from == 91 || to == 91) (*moves)->chess.ooo[1] = 0;
	if (from == 98 || to == 98) (*moves)->chess.oo[1] = 0;
	
	(*moves)++;
}

static void moonfish_force_move(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to)
{
	moonfish_force_promotion(chess, moves, from, to, chess->board[from]);
}

static void moonfish_deltas(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, int *deltas, int count, int n)
{
	int i;
	unsigned char to;
	
	while (*deltas)
	{
		to = from;
		for (i = 0 ; i < count ; i++)
		{
			to += *deltas * n;
			if (chess->board[to] == moonfish_outside) break;
			if (chess->board[to] / 16 == chess->board[from] / 16) break;
			moonfish_force_move(chess, moves, from, to);
			if (chess->board[to] != moonfish_empty) break;
		}
		deltas++;
	}
}

int moonfish_moves(struct moonfish_chess *chess, struct moonfish_move *moves, unsigned char from);

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
	struct moonfish_chess other;
	
	other = *chess;
	other.passing = 0;
	other.oo[0] = 0;
	other.oo[1] = 0;
	other.ooo[0] = 0;
	other.ooo[1] = 0;
	other.white ^= 1;
	return moonfish_validate(&other) ^ 1;
}

static char moonfish_attacked(struct moonfish_chess *chess, unsigned char from, unsigned char to)
{
	unsigned char piece;
	struct moonfish_chess other;
	
	if (chess->white) piece = moonfish_white_king;
	else piece = moonfish_black_king;
	
	other = *chess;
	other.board[from] = moonfish_empty;
	other.board[to] = piece;
	return moonfish_check(&other);
}

static void moonfish_castle_low(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from)
{
	unsigned char to;
	
	if (!chess->ooo[1 - chess->white]) return;
	
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
	
	if (!chess->oo[1 - chess->white]) return;
	
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

static void moonfish_pawn_moves(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to)
{
	unsigned char color;
	
	color = chess->board[from] & 0xF0;
	if ((color == 0x10 && from < 80) || (color == 0x20 && from > 40))
	{
		moonfish_force_move(chess, moves, from, to);
		return;
	}
	
	moonfish_force_promotion(chess, moves, from, to, color | moonfish_queen);
	moonfish_force_promotion(chess, moves, from, to, color | moonfish_rook);
	moonfish_force_promotion(chess, moves, from, to, color | moonfish_bishop);
	moonfish_force_promotion(chess, moves, from, to, color | moonfish_knight);
}

static void moonfish_pawn_capture(struct moonfish_chess *chess, struct moonfish_move **moves, unsigned char from, unsigned char to)
{
	int dy;
	
	if (to == chess->passing)
	{
		dy = chess->white ? 10 : -10;
		
		moonfish_force_move(chess, moves, from, to);
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
	static int steps[] = {0, 1, 8, 8, 8, 1};
	static int deltas[][5] =
	{
		{0},
		{21, 19, 12, 8, 0},
		{11, 9, 0},
		{10, 1, 0},
		{10, 1, 11, 9, 0},
		{10, 1, 11, 9, 0},
	};
	
	struct moonfish_move *moves0;
	unsigned char piece;
	int i, count;
	
	moves0 = moves;
	piece = chess->board[from];
	
	if (chess->white ? piece / 16 == 1 : piece / 16 == 2)
	{
		moonfish_deltas(chess, &moves, from, deltas[piece % 16 - 1], steps[piece % 16 - 1], 1);
		moonfish_deltas(chess, &moves, from, deltas[piece % 16 - 1], steps[piece % 16 - 1], -1);
		
		if (piece % 16 == moonfish_pawn) moonfish_move_pawn(chess, &moves, from);
		
		if (piece % 16 == moonfish_king)
		{
			moonfish_castle_high(chess, &moves, from);
			moonfish_castle_low(chess, &moves, from);
			
			count = moves - moves0;
			
			for (i = 0 ; i < count ; i++)
			{
				moves0[i].chess.oo[1 - chess->white] = 0;
				moves0[i].chess.ooo[1 - chess->white] = 0;
			}
		}
	}
	
	return moves - moves0;
}

void moonfish_chess(struct moonfish_chess *chess)
{
	static unsigned char pieces[] = {moonfish_rook, moonfish_knight, moonfish_bishop, moonfish_queen, moonfish_king, moonfish_bishop, moonfish_knight, moonfish_rook};
	int x, y;
	
	chess->white = 1;
	chess->oo[0] = 1;
	chess->oo[1] = 1;
	chess->ooo[0] = 1;
	chess->ooo[1] = 1;
	chess->passing = 0;
	
	for (y = 0 ; y < 12 ; y++)
	for (x = 0 ; x < 10 ; x++)
		chess->board[x + y * 10] = moonfish_outside;
	
	for (x = 0 ; x < 8 ; x++)
	{
		chess->board[x + 21] = pieces[x] | 0x10;
		chess->board[x + 91] = pieces[x] | 0x20;
		chess->board[x + 31] = moonfish_white_pawn;
		chess->board[x + 81] = moonfish_black_pawn;
		
		for (y = 4 ; y < 8 ; y++)
			chess->board[(x + 1) + y * 10] = moonfish_empty;
	}
}

int moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	int x0, y0;
	int x1, y1;
	unsigned char type;
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
	
	type = chess->board[(x0 + 1) + (y0 + 2) * 10] % 16;
	if (type == moonfish_king && x0 == 4)
	{
		if (x1 == 0) x1 = 2;
		if (x1 == 7) x1 = 6;
	}
	
	if (name[4] == 0) { }
	else if (name[4] == 'q') type = moonfish_queen;
	else if (name[4] == 'r') type = moonfish_rook;
	else if (name[4] == 'b') type = moonfish_bishop;
	else if (name[4] == 'n') type = moonfish_knight;
	else return 1;
	
	from = (x0 + 1) + (y0 + 2) * 10;
	to = (x1 + 1) + (y1 + 2) * 10;
	
	count = moonfish_moves(chess, moves, from);
	
	for (i = 0 ; i < count ; i++)
	{
		if (moves[i].to != to) continue;
		if (moves[i].chess.board[to] % 16 != type) continue;
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
	chess->oo[0] = 0;
	chess->oo[1] = 0;
	chess->ooo[0] = 0;
	chess->ooo[1] = 0;
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
		
		x++;
	}
	
	if (*fen++ == 'b') chess->white = 0;
	if (*fen++ != ' ') return 0;
	
	for (;;)
	{
		ch = *fen++;
		
		if (ch == 0) return 0;
		if (ch == ' ') break;
		
		if (ch == 'K') chess->oo[0] = 1;
		if (ch == 'k') chess->oo[1] = 1;
		if (ch == 'Q') chess->ooo[0] = 1;
		if (ch == 'q') chess->ooo[1] = 1;
		
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
	
	if (chess->oo[0]) *fen++ = 'K';
	if (chess->ooo[0]) *fen++ = 'Q';
	if (chess->oo[1]) *fen++ = 'k';
	if (chess->ooo[1]) *fen++ = 'q';
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
	
	if (chess->board[move->from] % 16 == moonfish_king && from_x == 4)
	{
		if (to_x == 2)
		{
			strcpy(name, "O-O-O");
			return;
		}
		if (to_x == 6)
		{
			strcpy(name, "O-O");
			return;
		}
	}
	
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
