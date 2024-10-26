/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#ifndef MOONFISH
#define MOONFISH

/* moonfish is a very simple chess bot written in C89 (ANSI C) */

/* in moonfish, pieces are each represented as a single 8-bit integer (char) */
/* (note: only two hex digits fit in an 8-bit number) */

/* the first hex digit of the integer represents the piece's color (1 for white, 2 for black) */
/* whereas the second hex digit of the integer represents the piece's type (within its color) */
/* this allows the piece type to be extracted with "p % 16" */
/* and likewise for its color to be extracted with "p / 16" */
/* (a special value (0x00) is used for squares without pieces) */

/* the board is represented coceptually as a 10-wide, 12-tall array */
/* the actual board itself is represented within the 8 x 8 area centered on this array */
/* (a special value (0xFF) is used for pieces outside of the board) */

/* the array itself is flattened into a 1D array with 120 elements */
/* so the index into the array may be obtained from coordinates using the following formula: */
/* index = (x + 1) + (y + 2) * 10 */
/* where "x" (file) and "y" (rank) are integers in the range 0..7 (inclusive) */

/* note: in the initial position, the white pieces are closer to the start of the array */
/* because the squares closest to white have a lower index */
/* whereas the pieces closest to black have a higher index */
/* this makes so that the board is flipped when displayed naively! (be careful) */

/* the board is not just an 8 x 8 array because of an optisation that can be performed when generating moves */

/* ~ ~ ~ ~ ~ */

/* white pieces */
#define moonfish_white_pawn 0x11
#define moonfish_white_knight 0x12
#define moonfish_white_bishop 0x13
#define moonfish_white_rook 0x14
#define moonfish_white_queen 0x15
#define moonfish_white_king 0x16

/* black pieces */
#define moonfish_black_pawn 0x21
#define moonfish_black_knight 0x22
#define moonfish_black_bishop 0x23
#define moonfish_black_rook 0x24
#define moonfish_black_queen 0x25
#define moonfish_black_king 0x26

/* piece types (without colors) */
#define moonfish_pawn 1
#define moonfish_knight 2
#define moonfish_bishop 3
#define moonfish_rook 4
#define moonfish_queen 5
#define moonfish_king 6

/* special values within the board representation */
#define moonfish_empty 0
#define moonfish_outside 0xFF

/* represents a chess position */
struct moonfish_chess {
	
	/* 10 x 12 array board representation */
	unsigned char board[120];
	
	/* booleans representing castling rights */
	unsigned char oo[2], ooo[2];
	
	/* square index of a pawn that may be captured via e.p. */
	/* or zero if there is no such pawn */
	unsigned char passing;
	
	/* whether it's white's turn (a boolean) */
	/* 1 means white's turn */
	/* 0 means black's turn */
	unsigned char white;
};

/* represents a move that can be made on a given position */
struct moonfish_move {
	
	/* the position after the move is played */
	struct moonfish_chess chess;
	
	/* square indices of where the piece moved from and to */
	unsigned char from, to;
};

/* represents options for the search */
struct moonfish_options {
	long int max_time;
	long int our_time;
};

/* the PST */
extern double moonfish_values[];

/* initialises the position and sets up the initial position */
/* note: this must be called *first* even if you want to use "moonfish_from_fen" */
void moonfish_chess(struct moonfish_chess *chess);

/* given a chess position, generates all moves for the piece at the square with the given index */
/* the moves are stored in "moves", so it must be able to fit the moves for the given piece */
/* note: an array of moves of size 32 is always enough (since its impossible to have a piece have more moves available than that in chess) */
/* note: this might generate a move that leaves the king in check (which is invalid!) */
/* note: it will not generate any other kind of invalid move */
/* to filter out invalid moves, you may use "moonfish_validate" below on the position *after* the generated move was played */
/* this will return the number of moves generated */
int moonfish_moves(struct moonfish_chess *chess, struct moonfish_move *moves, unsigned char from);

/* tries to find the best move in the given position with the given options */
/* the move is stored in the "move" pointer */
/* the move found is the best for the player whose turn it is on the given position */
/* this will return the number of positions that were looked into */
long int moonfish_best_move(struct moonfish_chess *chess, struct moonfish_move *move, struct moonfish_options *options);

/* returns the depth-zero score for the given position */
double moonfish_score(struct moonfish_chess *chess);

/* creates a move from UCI notation */
/* the move is stored in "move" */
/* on success, the parser will return 0, on failure, it will return 1 (and the move is unusable) */
/* parsing is somewhat robust, so you can trust it won't succeed with an invalid move */
int moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name);

/* converts a move to UCI notation */
/* the name is stored in the given "char" pointer (including the trailing NUL), so make sure you pass in a pointer that can fit it */
/* a "char" array of size 6 is always enough to fit a given move name */
void moonfish_to_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name);

/* returns whether an iinvalid move was just played (i.e. whether the opponent king is attacked on the given position) */
/* return 0: the opponent king is attacked, an invalid move was just played */
/* return 1: the opponent king is not attacked, an invalid move was not just played */
int moonfish_validate(struct moonfish_chess *chess);

/* returns whether player to play is in check (i.e. whether the current player's king is attacked on the given position) */
int moonfish_check(struct moonfish_chess *chess);

/* uses the position from the given FEN, mutating the given position */
/* returns 0 if the FEN could be parsed */
/* returns 1 if the FEN could not be parsed (the position becomes unusable then!) */
/* validation is very loose, so the FEN should be known to be good, otherwise this might cause unexpected results */
int moonfish_from_fen(struct moonfish_chess *chess, char *fen);

/* converts the given position to FEN */
/* the FEN is stored in the given "char" array, so it must be able to fit the FEN (including trailing NUL) */
void moonfish_to_fen(struct moonfish_chess *chess, char *fen);

/* similar to "moonfish_from_uci" and "moonfish_to_uci", but for SAN instead */
/* SAN parsing is very loose, so it will accept many forms, including UCI */
/* on success, the parser will return 0, on failure, it will return 1 (and the move is unusable) */
/* parsing is somewhat robust, so you can trust it won't succeed with an invalid move */
/* the "check" parameter determines which check/checkmate annotations will be added: */
/* '0' means both */
/* '1' means check only */
/* '2' means checkmate only */
/* '3' means neither */
int moonfish_from_san(struct moonfish_chess *chess, struct moonfish_move *move, char *name);
void moonfish_to_san(struct moonfish_chess *chess, struct moonfish_move *move, char *name, int check);

/* checks whether there is a valid move with the given from/to square indices */
/* then, if so, generates the move and stores it in the given move pointer */
/* if the move is valid, this will return 0, and the pointer will point to a move that can be used */
/* on failure (i.e. invalid move), this will return 1, and the move the pointer points to will not be usable */
/* note: this will ignore underpromotions (always promotes to queen when a pawn reaches the last rank) */
int moonfish_move(struct moonfish_chess *chess, struct moonfish_move *move, unsigned char from, unsigned char to);

/* returns whether the game ended due to either checkmate or stalemate */
/* note: 0 means false (i.e. not finished) */
int moonfish_finished(struct moonfish_chess *chess);

/* returns whether the game ended due to checkmate */
/* note: 0 means false (i.e. no checkmate) */
int moonfish_checkmate(struct moonfish_chess *chess);

#endif
