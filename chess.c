/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2025 zamfofex */

#include <string.h>

#include "moonfish.h"

static void moonfish_force_promotion(struct moonfish_chess *chess, struct moonfish_move **moves, int from, int to, int promotion)
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

static void moonfish_force_move(struct moonfish_chess *chess, struct moonfish_move **moves, int from, int to)
{
	moonfish_force_promotion(chess, moves, from, to, chess->board[from]);
}

static void moonfish_deltas(struct moonfish_chess *chess, struct moonfish_move **moves, int from, int *deltas, int count, int n)
{
	int i;
	int to;
	
	while (*deltas) {
		to = from;
		for (i = 0 ; i < count ; i++) {
			to += *deltas * n;
			if (chess->board[to] == moonfish_outside) break;
			if (chess->board[to] / 16 == chess->board[from] / 16) break;
			moonfish_force_move(chess, moves, from, to);
			if (chess->board[to] != moonfish_empty) break;
		}
		deltas++;
	}
}

int moonfish_moves(struct moonfish_chess *chess, struct moonfish_move *moves, int from);

int moonfish_validate(struct moonfish_chess *chess)
{
	int x, y;
	struct moonfish_move moves[32];
	int i, count;
	
	for (y = 0 ; y < 8 ; y++) {
		for (x = 0 ; x < 8 ; x++) {
			count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
			for (i = 0 ; i < count ; i++) {
				if (chess->board[moves[i].to] % 16 == moonfish_king) return 0;
			}
		}
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

static int moonfish_attacked(struct moonfish_chess *chess, int from, int to)
{
	struct moonfish_chess other;
	other = *chess;
	other.board[from] = moonfish_empty;
	other.board[to] = chess->white ? moonfish_white_king : moonfish_black_king;
	return moonfish_check(&other);
}

static void moonfish_castle_low(struct moonfish_chess *chess, struct moonfish_move **moves, int from)
{
	int to;
	
	if (!chess->ooo[chess->white ^ 1]) return;
	
	to = from - 3;
	while (to != from) {
		if (chess->board[to++] != moonfish_empty) return;
	}
	if (moonfish_check(chess)) return;
	if (moonfish_attacked(chess, from, from - 1)) return;
	if (moonfish_attacked(chess, from, from - 2)) return;
	
	moonfish_force_move(chess, moves, from - 4, from - 1);
	(*moves)--;
	(*moves)->chess.white = chess->white;
	moonfish_force_move(&(*moves)->chess, moves, from, from - 2);
}

static void moonfish_castle_high(struct moonfish_chess *chess, struct moonfish_move **moves, int from)
{
	int to;
	
	if (!chess->oo[chess->white ^ 1]) return;
	
	to = from + 2;
	while (to != from) {
		if (chess->board[to--] != moonfish_empty) return;
	}
	if (moonfish_check(chess)) return;
	if (moonfish_attacked(chess, from, from + 1)) return;
	if (moonfish_attacked(chess, from, from + 2)) return;
	
	moonfish_force_move(chess, moves, from + 3, from + 1);
	(*moves)--;
	(*moves)->chess.white = chess->white;
	moonfish_force_move(&(*moves)->chess, moves, from, from + 2);
}

static void moonfish_pawn_moves(struct moonfish_chess *chess, struct moonfish_move **moves, int from, int to)
{
	int color;
	
	if (to > 30 && to < 90) {
		moonfish_force_move(chess, moves, from, to);
		return;
	}
	
	color = chess->board[from] & 0xF0;
	moonfish_force_promotion(chess, moves, from, to, color | moonfish_queen);
	moonfish_force_promotion(chess, moves, from, to, color | moonfish_rook);
	moonfish_force_promotion(chess, moves, from, to, color | moonfish_bishop);
	moonfish_force_promotion(chess, moves, from, to, color | moonfish_knight);
}

static void moonfish_pawn_capture(struct moonfish_chess *chess, struct moonfish_move **moves, int from, int to)
{
	int dy;
	
	if (to == chess->passing) {
		dy = chess->white ? 10 : -10;
		moonfish_force_move(chess, moves, from, to);
		(*moves)[-1].chess.board[to - dy] = moonfish_empty;
		return;
	}
	
	if (chess->board[to] / 16 != chess->board[from] / 16 && chess->board[to] != moonfish_empty && chess->board[to] != moonfish_outside) {
		moonfish_pawn_moves(chess, moves, from, to);
	}
}

static void moonfish_move_pawn(struct moonfish_chess *chess, struct moonfish_move **moves, int from)
{
	int dy;
	
	dy = chess->white ? 10 : -10;
	
	if (chess->board[from + dy] == moonfish_empty) {
		moonfish_pawn_moves(chess, moves, from, from + dy);
		if ((chess->white ? from < 40 : from > 80) && chess->board[from + dy * 2] == moonfish_empty) {
			moonfish_force_move(chess, moves, from, from + dy * 2);
			(*moves)[-1].chess.passing = from + dy;
		}
	}
	
	moonfish_pawn_capture(chess, moves, from, from + dy + 1);
	moonfish_pawn_capture(chess, moves, from, from + dy - 1);
}

int moonfish_moves(struct moonfish_chess *chess, struct moonfish_move *moves, int from)
{
	static int steps[] = {0, 1, 8, 8, 8, 1};
	static int deltas[][5] = {
		{0},
		{21, 19, 12, 8, 0},
		{11, 9, 0},
		{10, 1, 0},
		{10, 1, 11, 9, 0},
		{10, 1, 11, 9, 0},
	};
	
	struct moonfish_move *moves0;
	int piece;
	int i, count;
	
	moves0 = moves;
	piece = chess->board[from];
	
	if (chess->white ? piece / 16 != 1 : piece / 16 != 2) return 0;
	
	moonfish_deltas(chess, &moves, from, deltas[piece % 16 - 1], steps[piece % 16 - 1], 1);
	moonfish_deltas(chess, &moves, from, deltas[piece % 16 - 1], steps[piece % 16 - 1], -1);
	
	if (piece % 16 == moonfish_pawn) moonfish_move_pawn(chess, &moves, from);
	
	if (piece % 16 == moonfish_king) {
		
		moonfish_castle_high(chess, &moves, from);
		moonfish_castle_low(chess, &moves, from);
		
		count = moves - moves0;
		
		for (i = 0 ; i < count ; i++) {
			moves0[i].chess.oo[chess->white ^ 1] = 0;
			moves0[i].chess.ooo[chess->white ^ 1] = 0;
		}
	}
	
	return moves - moves0;
}

void moonfish_chess(struct moonfish_chess *chess)
{
	static int pieces[] = {moonfish_rook, moonfish_knight, moonfish_bishop, moonfish_queen, moonfish_king, moonfish_bishop, moonfish_knight, moonfish_rook};
	int x, y;
	
	chess->white = 1;
	chess->oo[0] = 1;
	chess->oo[1] = 1;
	chess->ooo[0] = 1;
	chess->ooo[1] = 1;
	chess->passing = 0;
	
	for (y = 0 ; y < 12 ; y++) {
		for (x = 0 ; x < 10 ; x++) {
			chess->board[x + y * 10] = moonfish_outside;
		}
	}
	
	for (x = 0 ; x < 8 ; x++) {
		chess->board[x + 21] = pieces[x] | 0x10;
		chess->board[x + 91] = pieces[x] | 0x20;
		chess->board[x + 31] = moonfish_white_pawn;
		chess->board[x + 81] = moonfish_black_pawn;
		for (y = 4 ; y < 8 ; y++) chess->board[(x + 1) + y * 10] = moonfish_empty;
	}
}

int moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name0)
{
	struct moonfish_move moves[32];
	int x, y;
	int i, count;
	char name[6];
	
#ifndef moonfish_mini
	if (chess->board[25] == moonfish_white_king) {
		if (!strcmp(name0, "e1h1")) name0 = "e1g1";
		if (!strcmp(name0, "e1a1")) name0 = "e1c1";
	}
	if (chess->board[85] == moonfish_black_king) {
		if (!strcmp(name0, "e8h8")) name0 = "e8g8";
		if (!strcmp(name0, "e8a8")) name0 = "e8c8";
	}
#endif
	
	for (y = 0 ; y < 8 ; y++) {
		for (x = 0 ; x < 8 ; x++) {
			count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
			for (i = 0 ; i < count ; i++) {
				moonfish_to_uci(chess, moves + i, name);
				if (!strcmp(name, name0)) {
					*move = moves[i];
					return 0;
				}
			}
		}
	}
	
	return 1;
}

/* note: this function does not assume ASCII */

void moonfish_to_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	char *alphabet;
	int x, y;
	int piece;
	
	alphabet = "abcdefgh";
	
	x = move->from % 10 - 1;
	y = move->from / 10 - 2;
	
	name[0] = alphabet[x];
	name[1] = y + '1';
	
	x = move->to % 10 - 1;
	y = move->to / 10 - 2;
	
	name[2] = alphabet[x];
	name[3] = y + '1';
	
	name[4] = 0;
	
	piece = move->chess.board[move->to];
	
	if (piece != chess->board[move->from]) {
		if (piece % 16 == moonfish_queen) name[4] = 'q';
		if (piece % 16 == moonfish_rook) name[4] = 'r';
		if (piece % 16 == moonfish_bishop) name[4] = 'b';
		if (piece % 16 == moonfish_knight) name[4] = 'n';
		name[5] = 0;
	}
}

int moonfish_finished(struct moonfish_chess *chess)
{
	struct moonfish_move moves[32];
	int x, y;
	int i, count;
	
	for (y = 0 ; y < 8 ; y++) {
		for (x = 0 ; x < 8 ; x++) {
			count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
			for (i = 0 ; i < count ; i++) {
				if (moonfish_validate(&moves[i].chess)) return 0;
			}
		}
	}
	
	return 1;
}

int moonfish_equal(struct moonfish_chess *a, struct moonfish_chess *b)
{
	int x, y, i;
	
	if (a->white != b->white) return 0;
	if (a->passing != b->passing) return 0;
	if (a->oo[0] != b->oo[0]) return 0;
	if (a->oo[1] != b->oo[1]) return 0;
	if (a->ooo[0] != b->ooo[0]) return 0;
	if (a->ooo[1] != b->ooo[1]) return 0;
	
	for (y = 0 ; y < 8 ; y++) {
		for (x = 0 ; x < 8 ; x++) {
			i = (x + 1) + (y + 2) * 10;
			if (a->board[i] != b->board[i]) {
				return 0;
			}
		}
	}
	
	return 1;
}

#ifndef moonfish_mini

#include <ctype.h>

int moonfish_move(struct moonfish_chess *chess, struct moonfish_move *found, int from, int to)
{
	struct moonfish_move moves[32];
	int i, count;
	
	count = moonfish_moves(chess, moves, from);
	
	for (i = 0 ; i < count ; i++) {
		if (moves[i].to == to && moonfish_validate(&moves[i].chess)) {
			*found = moves[i];
			return 0;
		}
	}
	
	return 1;
}

/* note: these functions do not assume ASCII */

static int moonfish_from_file(char ch)
{
	char *alphabet, *found;
	alphabet = "abcdefgh";
	found = strchr(alphabet, ch);
	if (found == NULL || *found == 0) return -1;
	return found - alphabet;
}

static char moonfish_to_file(int i)
{
	return "abcdefgh"[i];
}

int moonfish_from_fen(struct moonfish_chess *chess, char *fen)
{
	int x, y;
	int type, color;
	char ch;
	
	for (y = 0 ; y < 8 ; y++) {
		for (x = 0 ; x < 8 ; x++) {
			chess->board[(x + 1) + (y + 2) * 10] = moonfish_empty;
		}
	}
	
	chess->white = 1;
	chess->oo[0] = 0;
	chess->oo[1] = 0;
	chess->ooo[0] = 0;
	chess->ooo[1] = 0;
	chess->passing = 0;
	
	x = 0;
	y = 0;
	
	for (;;) {
		
		ch = *fen++;
		
		if (ch == 0) return 0;
		if (ch == ' ') break;
		
		if (ch == '/') {
			x = 0;
			y++;
			continue;
		}
		
		if (ch >= '0' && ch <= '9') {
			x += ch - '0';
			continue;
		}
		
		color = 0x20;
		if (isupper((unsigned char) ch)) {
			ch = tolower((unsigned char) ch);
			color = 0x10;
		}
		
		type = 0;
		if (ch == 'p') type = moonfish_pawn;
		if (ch == 'n') type = moonfish_knight;
		if (ch == 'b') type = moonfish_bishop;
		if (ch == 'r') type = moonfish_rook;
		if (ch == 'q') type = moonfish_queen;
		if (ch == 'k') type = moonfish_king;
		if (type == 0) return 1;
		
		chess->board[(x + 1) + (9 - y) * 10] = type | color;
		
		x++;
	}
	
	if (*fen == 0) return 0;
	if (*fen != 'w' && *fen != 'b') return 1;
	if (*fen++ == 'b') chess->white = 0;
	
	if (*fen == 0) return 0;
	if (*fen++ != ' ') return 1;
	
	if (*fen == 0) return 0;
	if (*fen == '-') {
		fen++;
	}
	else {
		for (;;) {
			ch = *fen;
			if (ch == 0) return 0;
			if (ch != 'K' && ch != 'k' && ch != 'Q' && ch != 'q') break;
			if (ch == 'K') chess->oo[0] = 1;
			if (ch == 'k') chess->oo[1] = 1;
			if (ch == 'Q') chess->ooo[0] = 1;
			if (ch == 'q') chess->ooo[1] = 1;
			fen++;
		}
	}
	
	if (*fen == 0) return 0;
	if (*fen++ != ' ') return 1;
	
	if (*fen == 0) return 0;
	if (*fen == '-') {
		fen++;
	}
	else {
		x = moonfish_from_file(*fen++);
		if (x < 0) return 1;
		if (*fen < '1' || *fen > '8') return 1;
		y = *fen++ - '1';
		chess->passing = (x + 1) + (y + 2) * 10;
	}
	
	return 0;
}

int moonfish_checkmate(struct moonfish_chess *chess)
{
	if (!moonfish_check(chess)) return 0;
	return moonfish_finished(chess);
}

static int moonfish_match_move(struct moonfish_chess *chess, struct moonfish_move *move, int type, int promotion, int x0, int y0, int x1, int y1, int check, int captured)
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
	
	for (y = yi0 ; y < yi1 ; y++) {
		
		for (x = xi0 ; x < xi1 ; x++) {
			
			count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
			for (i = 0 ; i < count ; i++) {
				
				if (chess->board[moves[i].from] % 16 != type) continue;
				if (promotion && promotion != moves[i].chess.board[moves[i].to] % 16) continue;
				if (moves[i].to % 10 != x1) continue;
				if (moves[i].to / 10 - 1 != y1) continue;
				
				if (captured) {
					if (chess->board[moves[i].from] % 16 == moonfish_pawn) {
						if (moves[i].from % 10 == moves[i].to % 10) continue;
					}
					else {
						if (chess->board[moves[i].to] == moonfish_empty) continue;
					}
				}
				
				if (!moonfish_validate(&moves[i].chess)) continue;
				if (check && !moonfish_check(chess)) continue;
				if (check == 2 && !moonfish_checkmate(chess)) continue;
				if (found) return 1;
				found = 1;
				*move = moves[i];
			}
		}
	}
	if (!found) return 1;
	return 0;
}

int moonfish_from_san(struct moonfish_chess *chess, struct moonfish_move *move, char *name)
{
	int count;
	int type, promotion;
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
	for (i = length / 2 ; i < length ; i++) {
		ch = name[i];
		name[i] = name[length - i - 1];
		name[length - i - 1] = ch;
	}
	
	check = 0;
	if (*name == '+') check = 1;
	if (*name == '#') check = 2;
	if (check) name++;
	
	count = 0;
	for (name0 = name ; *name0 != 0 ; name0++) {
		if (*name0 == '-') continue;
		if (*name0 == '_') continue;
		if (*name0 != '0' && *name != 'O' && *name != 'o') break;
		count++;
	}
	
	if (*name0 == 0) {
		
		if (count != 2 && count != 3) return 1;
		if (count == 2) x1 = 7;
		if (count == 3) x1 = 3;
		
		if (chess->white) y1 = 1;
		else y1 = 8;
		
		return moonfish_match_move(chess, move, moonfish_king, 0, 5, y1, x1, y1, check, 0);
	}
	
	x0 = 0;
	y0 = 0;
	x1 = 0;
	y1 = 0;
	
	switch (*name++) {
	default:
		promotion = 0;
		name--;
		break;
	case 'K':
	case 'k':
		promotion = moonfish_king;
		break;
	case 'Q':
	case 'q':
		promotion = moonfish_queen;
		break;
	case 'R':
	case 'r':
		promotion = moonfish_rook;
		break;
	case 'B':
	case 'b':
		promotion = moonfish_bishop;
		break;
	case 'N':
	case 'n':
		promotion = moonfish_knight;
		break;
	}
	
	if (promotion != moonfish_empty && *name == '=') name++;
	
	capture = 0;
	if (*name >= '1' && *name <= '8') y1 = *name++ - '0';
	if (moonfish_from_file(*name) >= 0) x1 = moonfish_from_file(*name++) + 1;
	if (*name == 'x') capture = 1, name++;
	if (*name >= '1' && *name <= '8') y0 = *name++ - '0';
	if (moonfish_from_file(*name) >= 0) x0 = moonfish_from_file(*name++) + 1;
	
	if (x1 == 0 || y1 == 0) return 1;
	
	switch (*name++) {
	default:
		type = moonfish_pawn;
		if (x0 > 0 && y0 > 0) type = chess->board[x0 + (y0 + 1) * 10] % 16;
		if (type == 0x0F) return 1;
		name--;
		break;
	case 'K':
	case 'k':
		type = moonfish_king;
		break;
	case 'Q':
	case 'q':
		type = moonfish_queen;
		break;
	case 'R':
	case 'r':
		type = moonfish_rook;
		break;
	case 'B':
		type = moonfish_bishop;
		break;
	case 'N':
	case 'n':
		type = moonfish_knight;
		break;
	}
	
	if (*name != 0) return 1;
	
	return moonfish_match_move(chess, move, type, promotion, x0, y0, x1, y1, check, capture);
}

void moonfish_to_fen(struct moonfish_chess *chess, char *fen)
{
	int x, y;
	int piece;
	int count;
	char ch;
	
	for (y = 7 ; y >= 0 ; y--) {
		
		count = 0;
		
		for (x = 0 ; x < 8 ; x++) {
			
			piece = chess->board[(x + 1) + (y + 2) * 10];
			if (piece == moonfish_empty) {
				count++;
				continue;
			}
			
			if (count != 0) {
				*fen++ = '0' + count;
				count = 0;
			}
			
			switch (piece % 16) {
			default:
				return;
			case moonfish_pawn:
				ch = 'p';
				break;
			case moonfish_knight:
				ch = 'n';
				break;
			case moonfish_bishop:
				ch = 'b';
				break;
			case moonfish_rook:
				ch = 'r';
				break;
			case moonfish_queen:
				ch = 'q';
				break;
			case moonfish_king:
				ch = 'k';
				break;
			}
			
			if (piece / 16 == 1) ch = toupper((unsigned char) ch);
			*fen++ = ch;
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
	if (chess->passing != 0) {
		*fen++ = moonfish_to_file(chess->passing % 10 - 1);
		*fen++ = '1' + chess->passing / 10 - 2;
	}
	
	if (fen[-1] == ' ') *fen++ = '-';
	*fen = 0;
}

void moonfish_to_san(struct moonfish_chess *chess, struct moonfish_move *move, char *name, int check)
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
	
	if (chess->board[move->from] % 16 == moonfish_king && from_x == 4) {
		if (to_x == 2) {
			strcpy(name, "O-O-O");
			return;
		}
		if (to_x == 6) {
			strcpy(name, "O-O");
			return;
		}
	}
	
	if (chess->board[move->from] % 16 == moonfish_pawn) {
		
		if (from_x != to_x) {
			*name++ = moonfish_to_file(from_x);
			*name++ = 'x';
		}
		
		*name++ = moonfish_to_file(to_x);
		*name++ = to_y + '1';
		
		if (move->chess.board[move->to] % 16 != moonfish_pawn) {
			*name++ = '=';
			*name++ = names[move->chess.board[move->to] % 16 - 2];
		}
		
		*name = 0;
		
		return;
	}
	
	file_ambiguity = 0;
	rank_ambiguity = 0;
	ambiguity = 0;
	
	for (y = 0 ; y < 8 ; y++) {
		
		for (x = 0 ; x < 8 ; x++) {
			
			if ((x + 1) + (y + 2) * 10 == move->from) continue;
			count = moonfish_moves(chess, moves, (x + 1) + (y + 2) * 10);
			
			for (i = 0 ; i < count ; i++) {
				
				if (moves[i].to != move->to) continue;
				if (chess->board[moves[i].from] != chess->board[move->from]) continue;
				
				ambiguity = 1;
				if (moves[i].from % 10 - 1 == from_x) file_ambiguity = 1;
				if (moves[i].from / 10 - 2 == from_y) rank_ambiguity = 1;
			}
		}
	}
	*name++ = names[(chess->board[move->from] & 0xF) - 2];
	
	if (ambiguity) {
		if (file_ambiguity) {
			if (rank_ambiguity) *name++ = moonfish_to_file(from_x);
			*name++ = from_y + '1';
		}
		else {
			*name++ = moonfish_to_file(from_x);
		}
	}
	
	if (chess->board[move->to] != moonfish_empty) *name++ = 'x';
	
	*name++ = moonfish_to_file(to_x);
	*name++ = to_y + '1';
	
	if (moonfish_checkmate(&move->chess)) {
		if (~check & 1) *name++ = '#';
	}
	else {
		if (moonfish_check(&move->chess)) {
			if (~check & 2) *name++ = '+';
		}
	}
	
	*name = 0;
}

#endif
