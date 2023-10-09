#include "moonfish.h"

static char moonfish_delta(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from, unsigned char *to, signed char delta)
{
	*to += delta;
	
	if (ctx->board[*to] <= moonfish_our_king) return 0;
	
	(*moves)->from = from;
	(*moves)->to = *to;
	(*moves)->piece = ctx->board[from];
	(*moves)->promotion = ctx->board[from];
	(*moves)->captured = ctx->board[*to];
	(*moves)->castle = ctx->castle;
	(*moves)++;
	
	if (ctx->board[*to] != moonfish_empty) return 0;
	
	return 1;
}

static void moonfish_slide(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from, signed char delta)
{
	unsigned char to = from;
	while (moonfish_delta(ctx, moves, from, &to, delta)) { }
}

static void moonfish_jump(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from, signed char delta)
{
	unsigned char to = from;
	moonfish_delta(ctx, moves, from, &to, delta);
}

static void moonfish_move_knight(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from)
{
	moonfish_jump(ctx, moves, from, 1 + 20);
	moonfish_jump(ctx, moves, from, -1 + 20);
	moonfish_jump(ctx, moves, from, 1 - 20);
	moonfish_jump(ctx, moves, from, -1 - 20);
	moonfish_jump(ctx, moves, from, 2 + 10);
	moonfish_jump(ctx, moves, from, -2 + 10);
	moonfish_jump(ctx, moves, from, 2 - 10);
	moonfish_jump(ctx, moves, from, -2 - 10);
}

static void moonfish_move_bishop(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from)
{
	moonfish_slide(ctx, moves, from, 1 + 10);
	moonfish_slide(ctx, moves, from, -1 + 10);
	moonfish_slide(ctx, moves, from, 1 - 10);
	moonfish_slide(ctx, moves, from, -1 - 10);
}

static void moonfish_move_rook(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from)
{
	moonfish_slide(ctx, moves, from, 10);
	moonfish_slide(ctx, moves, from, -10);
	moonfish_slide(ctx, moves, from, 1);
	moonfish_slide(ctx, moves, from, -1);
}

static void moonfish_move_queen(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from)
{
	moonfish_move_rook(ctx, moves, from);
	moonfish_move_bishop(ctx, moves, from);
}

static char moonfish_attacked(struct moonfish *ctx, unsigned char from, unsigned char to)
{
	int check;
	ctx->board[from] = moonfish_empty;
	ctx->board[to] = moonfish_our_king;
	check = moonfish_check(ctx);
	ctx->board[from] = moonfish_our_king;
	ctx->board[to] = moonfish_empty;
	return check;
}

static void moonfish_castle_low(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from)
{
	int to;
	
	for (to = 22 ; to != from ; to++)
		if (ctx->board[to] != moonfish_empty)
			return;
	
	if (moonfish_check(ctx)) return;
	if (moonfish_attacked(ctx, from, from - 1)) return;
	if (moonfish_attacked(ctx, from, from - 2)) return;
	
	(*moves)->from = from;
	(*moves)->to = from - 2;
	(*moves)->piece = moonfish_our_king;
	(*moves)->promotion = moonfish_our_king;
	(*moves)->captured = moonfish_empty;
	(*moves)->castle = ctx->castle;
	(*moves)++;
}

static void moonfish_castle_high(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from)
{
	int to;
	
	for (to = 27 ; to != from ; to--)
		if (ctx->board[to] != moonfish_empty)
			return;
	
	if (moonfish_check(ctx)) return;
	if (moonfish_attacked(ctx, from, from + 1)) return;
	if (moonfish_attacked(ctx, from, from + 2)) return;
	
	(*moves)->from = from;
	(*moves)->to = from + 2;
	(*moves)->piece = moonfish_our_king;
	(*moves)->promotion = moonfish_our_king;
	(*moves)->captured = moonfish_empty;
	(*moves)->castle = ctx->castle;
	(*moves)++;
}

static void moonfish_move_king(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from)
{
	moonfish_jump(ctx, moves, from, 1 + 10);
	moonfish_jump(ctx, moves, from, -1 + 10);
	moonfish_jump(ctx, moves, from, 1 - 10);
	moonfish_jump(ctx, moves, from, -1 - 10);
	moonfish_jump(ctx, moves, from, 10);
	moonfish_jump(ctx, moves, from, -10);
	moonfish_jump(ctx, moves, from, 1);
	moonfish_jump(ctx, moves, from, -1);
	
	if (!ctx->white && from == 24)
	{
		if (ctx->castle.black_oo) moonfish_castle_low(ctx, moves, from);
		if (ctx->castle.black_ooo) moonfish_castle_high(ctx, moves, from);
	}
	if (ctx->white && from == 25)
	{
		if (ctx->castle.white_oo) moonfish_castle_high(ctx, moves, from);
		if (ctx->castle.white_ooo) moonfish_castle_low(ctx, moves, from);
	}
}

static void moonfish_move_pawn(struct moonfish *ctx, struct moonfish_move **moves, unsigned char from)
{
	unsigned char promotion;
	
	promotion = moonfish_our_pawn;
	if (from > 80) promotion = moonfish_our_queen;
	
	if (ctx->board[from + 10] == moonfish_empty)
	{
		(*moves)->piece = moonfish_our_pawn;
		(*moves)->promotion = promotion;
		(*moves)->captured = moonfish_empty;
		(*moves)->from = from;
		(*moves)->to = from + 10;
		(*moves)->castle = ctx->castle;
		(*moves)++;
				
		if (from < 40)
		{
			if (ctx->board[from + 20] == moonfish_empty)
			{
				(*moves)->from = from;
				(*moves)->to = from + 20;
				(*moves)->piece = moonfish_our_pawn;
				(*moves)->promotion = moonfish_our_pawn;
				(*moves)->captured = moonfish_empty;
				(*moves)->castle = ctx->castle;
				(*moves)++;
			}
		}
	}
	
	if (ctx->board[from + 9] >= moonfish_their_pawn)
	if (ctx->board[from + 9] != moonfish_empty)
	{
		(*moves)->piece = moonfish_our_pawn;
		(*moves)->promotion = promotion;
		(*moves)->captured = ctx->board[from + 9];
		(*moves)->from = from;
		(*moves)->to = from + 9;
		(*moves)->castle = ctx->castle;
		(*moves)++;
	}
	
	if (ctx->board[from + 11] >= moonfish_their_pawn)
	if (ctx->board[from + 11] != moonfish_empty)
	{
		(*moves)->piece = moonfish_our_pawn;
		(*moves)->promotion = promotion;
		(*moves)->captured = ctx->board[from + 11];
		(*moves)->from = from;
		(*moves)->to = from + 11;
		(*moves)->castle = ctx->castle;
		(*moves)++;
	}
}

void moonfish_moves(struct moonfish *ctx, struct moonfish_move *moves, unsigned char from)
{
	char piece;
	piece = ctx->board[from];
	if (piece == moonfish_our_pawn) moonfish_move_pawn(ctx, &moves, from);
	if (piece == moonfish_our_knight) moonfish_move_knight(ctx, &moves, from);
	if (piece == moonfish_our_bishop) moonfish_move_bishop(ctx, &moves, from);
	if (piece == moonfish_our_rook) moonfish_move_rook(ctx, &moves, from);
	if (piece == moonfish_our_queen) moonfish_move_queen(ctx, &moves, from);
	if (piece == moonfish_our_king) moonfish_move_king(ctx, &moves, from);
	moves->piece = moonfish_outside;
}

static void moonfish_swap(struct moonfish *ctx, int i, int j)
{
	unsigned char piece;
	piece = ctx->board[i];
	ctx->board[i] = ctx->board[j];
	ctx->board[j] = piece;
}

static void moonfish_flip_horizontally(struct moonfish *ctx)
{
	int x, y;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 4 ; x++)
	{
		moonfish_swap(ctx,
			(x + 1) + (y + 2) * 10,
			(8 - x) + (y + 2) * 10
		);
	}
}

static void moonfish_flip_vertically(struct moonfish *ctx)
{
	int x, y;
	
	for (y = 0 ; y < 4 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_swap(ctx,
			(x + 1) + (y + 2) * 10,
			(x + 1) + (9 - y) * 10
		);
	}
}

static void moonfish_rotate(struct moonfish *ctx)
{
	int x, y;
	
	moonfish_flip_horizontally(ctx);
	moonfish_flip_vertically(ctx);
	
	if (ctx->white) ctx->white = 0;
	else ctx->white = 1;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		if (ctx->board[(x + 1) + (y + 2) * 10] != moonfish_empty)
			ctx->board[(x + 1) + (y + 2) * 10] ^= 0x30;
	}
}

void moonfish_play(struct moonfish *ctx, struct moonfish_move *move)
{
	int x0, x1;
	
	ctx->board[move->from] = moonfish_empty;
	ctx->board[move->to] = move->promotion;
	
	if (move->piece == moonfish_our_king)
	{
		x0 = 0;
		if (move->from == 24 && move->to == 22) x0 = 1, x1 = 3;
		if (move->from == 24 && move->to == 26) x0 = 8, x1 = 5;
		if (move->from == 25 && move->to == 27) x0 = 8, x1 = 6;
		if (move->from == 25 && move->to == 23) x0 = 1, x1 = 4;
		if (x0) ctx->board[x0 + 20] = moonfish_empty, ctx->board[x1 + 20] = moonfish_our_rook;
		
		if (ctx->white)
		{
			ctx->castle.white_oo = 0;
			ctx->castle.white_ooo = 0;
		}
		else
		{
			ctx->castle.black_oo = 0;
			ctx->castle.black_ooo = 0;
		}
	}
	
	if (move->piece == moonfish_our_rook)
	{
		if (move->from == 28)
		{
			if (ctx->white) ctx->castle.white_ooo = 0;
			else ctx->castle.black_oo = 0;
		}
		if (move->from == 21)
		{
			if (ctx->white) ctx->castle.white_oo = 0;
			else ctx->castle.black_ooo = 0;
		}
	}
	
	if (move->captured == moonfish_their_rook)
	{
		if (move->to == 98)
		{
			if (ctx->white) ctx->castle.black_oo = 0;
			else ctx->castle.white_ooo = 0;
		}
		if (move->to == 91)
		{
			if (ctx->white) ctx->castle.black_ooo = 0;
			else ctx->castle.white_oo = 0;
		}
	}
	
	moonfish_rotate(ctx);
}

void moonfish_unplay(struct moonfish *ctx, struct moonfish_move *move)
{
	int x0, x1;
	
	moonfish_rotate(ctx);
	
	ctx->board[move->from] = move->piece;
	ctx->board[move->to] = move->captured;
	ctx->castle = move->castle;
	
	if (move->piece == moonfish_our_king)
	{
		x0 = 0;
		if (move->from == 24 && move->to == 22) x0 = 1, x1 = 3;
		if (move->from == 24 && move->to == 26) x0 = 8, x1 = 5;
		if (move->from == 25 && move->to == 27) x0 = 8, x1 = 6;
		if (move->from == 25 && move->to == 23) x0 = 1, x1 = 4;
		if (x0) ctx->board[x1 + 20] = moonfish_empty, ctx->board[x0 + 20] = moonfish_our_rook;
	}
}

void moonfish_chess(struct moonfish *ctx)
{
	static unsigned char pieces[] = {4, 2, 3, 5, 6, 3, 2, 4};
	int x, y;
	
	for (y = 0 ; y < 12 ; y++)
	for (x = 0 ; x < 10 ; x++)
		ctx->board[x + y * 10] = moonfish_outside;
	
	for (x = 0 ; x < 8 ; x++)
	{
		ctx->board[(x + 1) + 20] = pieces[x] | 0x10;
		ctx->board[(x + 1) + 30] = moonfish_our_pawn;
		ctx->board[(x + 1) + 40] = moonfish_empty;
		ctx->board[(x + 1) + 50] = moonfish_empty;
		ctx->board[(x + 1) + 60] = moonfish_empty;
		ctx->board[(x + 1) + 70] = moonfish_empty;
		ctx->board[(x + 1) + 80] = moonfish_their_pawn;
		ctx->board[(x + 1) + 90] = pieces[x] | 0x20;
	}
	
	ctx->white = 1;
	ctx->castle.white_oo = 1;
	ctx->castle.white_ooo = 1;
	ctx->castle.black_oo = 1;
	ctx->castle.black_ooo = 1;
}

void moonfish_show(struct moonfish *ctx)
{
	static char chs[] = "pnbrqk";
	
	int x, y;
	char ch;
	unsigned char piece;
	
	for (y = 7 ; y >= 0 ; y--)
	{
		printf("   ");
		
		for (x = 0 ; x < 8 ; x++)
		{
			piece = ctx->board[(x + 1) + (y + 2) * 10];
			if (piece == moonfish_empty)
			{
				printf("  ");
				continue;
			}
			
			ch = chs[(piece & 0xF) - 1];
			
			if (piece <= moonfish_our_king || !ctx->white)
			if (piece > moonfish_our_king || ctx->white)
				ch += 'A' - 'a';
			
			printf("%c ", ch);
		}
		printf("\n");
	}
}

void moonfish_play_uci(struct moonfish *ctx, char *name)
{
	struct moonfish_move move;
	int x, y;
	
	x = name[0] - 'a';
	y = name[1] - '1';
	
	if (!ctx->white)
	{
		x = 7 - x;
		y = 7 - y;
	}
	
	move.from = (x + 1) + (y + 2) * 10;
	
	x = name[2] - 'a';
	y = name[3] - '1';
	
	if (!ctx->white)
	{
		x = 7 - x;
		y = 7 - y;
	}
	
	move.to = (x + 1) + (y + 2) * 10;
	
	move.piece = ctx->board[move.from];
	move.captured = ctx->board[move.to];
	move.promotion = move.piece;
	
	if (move.piece == moonfish_our_pawn)
	if ((move.to - move.from) % 10)
	if (move.captured == moonfish_empty)
		ctx->board[move.to - 10] = moonfish_empty;
	
	if (name[4] == 'q') move.promotion = moonfish_our_queen;
	if (name[4] == 'r') move.promotion = moonfish_our_rook;
	if (name[4] == 'b') move.promotion = moonfish_our_bishop;
	if (name[4] == 'n') move.promotion = moonfish_our_knight;
	
	moonfish_play(ctx, &move);
}

void moonfish_to_uci(char *name, struct moonfish_move *move, int white)
{
	int x, y;
	
	x = move->from % 10 - 1;
	y = move->from / 10 - 2;
	
	if (!white)
	{
		x = 7 - x;
		y = 7 - y;
	}
	
	name[0] = x + 'a';
	name[1] = y + '1';
	
	x = move->to % 10 - 1;
	y = move->to / 10 - 2;
	
	if (!white)
	{
		x = 7 - x;
		y = 7 - y;
	}
	
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

void moonfish_fen(struct moonfish *ctx, char *fen)
{
	int x, y;
	unsigned char type, color;
	char ch;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
		ctx->board[(x + 1) + (y + 2) * 10] = moonfish_empty;
	
	x = 0;
	y = 0;
	
	ctx->white = 1;
	ctx->castle.white_oo = 0;
	ctx->castle.white_ooo = 0;
	ctx->castle.black_oo = 0;
	ctx->castle.black_ooo = 0;
	
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
		
		ctx->board[(x + 1) + (9 - y) * 10] = type | color;
		
		x++;
	}
	
	if (*fen++ == 'b') moonfish_rotate(ctx);
	if (*fen++ != ' ') return;
	
	for (;;)
	{
		ch = *fen++;
		if (ch == 0) return;
		if (ch == ' ') break;
		if (ch == 'K') ctx->castle.white_oo = 1;
		if (ch == 'Q') ctx->castle.white_ooo = 1;
		if (ch == 'k') ctx->castle.black_oo = 1;
		if (ch == 'q') ctx->castle.black_ooo = 1;
	}
}

int moonfish_validate(struct moonfish *ctx)
{
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(ctx, moves, (x + 1) + (y + 2) * 10);
		for (move = moves ; move->piece != moonfish_outside ; move++)
			if (move->captured == moonfish_their_king)
				return 0;
	}
	
	return 1;
}

int moonfish_check(struct moonfish *ctx)
{
	int valid;
	struct moonfish_castle castle;
	
	castle = ctx->castle;
	
	ctx->castle.white_oo = 0;
	ctx->castle.white_ooo = 0;
	ctx->castle.black_oo = 0;
	ctx->castle.black_ooo = 0;
	
	moonfish_rotate(ctx);
	valid = moonfish_validate(ctx);
	moonfish_rotate(ctx);
	
	ctx->castle = castle;
	
	return valid ? 0 : 1;
}
