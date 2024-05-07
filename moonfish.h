/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#ifndef MOONFISH
#define MOONFISH

/* moonfish is a very simple chess bot written in C 89 (ANSI C) */

/* in moonfish, pieces are each represented as a single 8-bit integer (char) */
/* (note: only two hex digits fit in an 8-bit number) */

/* the first hex digit of the integer represents the piece's color (1 for white, 2 for black) */
/* whereas the second hex digit of the integer represents the piece's type (within its color) */
/* this allows the piece type to be extracted with "p % 16" */
/* and likewise for its color to be extracted with "p / 16" */
/* (a special value (0xFF) is used for squares without pieces) */

/* the board is represented coceptually as a 10-wide, 12-tall array */
/* the actual board itself is represented within the 8 x 8 area centered on this array */
/* (a special value (0x00) is used for pieces outside of the board) */

/* the array itself is flattened into a 1D array with 120 elements */
/* so the index into the array may be obtained from coordinatse using the following formula: */
/* index = (x + 1) + (y + 2) * 8 */
/* where "x" (file) and "y" (rank) are integers in the range 0..7 (inclusive) */

/* "x = 0" represents the "a" file, "x = 8" represents the "h" file */
/* "y = 0" represents the 1st rank, "y = 8" represents the 8th rank (from white's perspective) */
/* e.g. "x = 0 ; y = 0" represents "a1", and "index = 17" */
/* e.g. "x = 3 ; y = 5" represents "d6", and "index = 60" */
/* e.g. "x = 5 ; y = 3" represents "f4", and "index = 46" */
/* e.g. "x = 8 ; y = 8" represents "h8", and "index = 89" */

/* diagram (for starting position) */
/* i = 00..07 : 0x[00 00 00 00 00 00 00 00] */
/* i = 08..15 : 0x[00 00 00 00 00 00 00 00] */
/* i = 16..23 : 0x[00 14 12 15 16 12 14 00] */
/* i = 24..31 : 0x[00 11 11 11 11 11 11 00] */
/* i = 32..39 : 0x[00 FF FF FF FF FF FF 00] */
/* i = 40..47 : 0x[00 FF FF FF FF FF FF 00] */
/* i = 48..55 : 0x[00 FF FF FF FF FF FF 00] */
/* i = 56..63 : 0x[00 FF FF FF FF FF FF 00] */
/* i = 64..71 : 0x[00 21 21 21 21 21 21 00] */
/* i = 72..79 : 0x[00 24 22 25 26 22 24 00] */
/* i = 80..87 : 0x[00 00 00 00 00 00 00 00] */
/* i = 88.119 : 0x[00 00 00 00 00 00 00 00] */
/* (squares with 00 are outside of the board, squares with FF are empty) */

/* note: white pieces are shown on the top, because they squares closest to white have a lower index */
/* whereas the pieces closest to black have a higher index */
/* this makes so that the board is flipped when displayed naively! (be careful) */

/* the board is not just an 8 x 8 array because of an optisation that can be performed when generating moves */

enum
{
	/* white pieces */
	moonfish_white_pawn = 0x11,
	moonfish_white_knight = 0x12,
	moonfish_white_bishop = 0x13,
	moonfish_white_rook = 0x14,
	moonfish_white_queen = 0x15,
	moonfish_white_king = 0x16,
	
	/* black pieces */
	moonfish_black_pawn = 0x21,
	moonfish_black_knight = 0x22,
	moonfish_black_bishop = 0x23,
	moonfish_black_rook = 0x24,
	moonfish_black_queen = 0x25,
	moonfish_black_king = 0x26,
	
	/* piece types (without colors) */
	moonfish_pawn = 1,
	moonfish_knight = 2,
	moonfish_bishop = 3,
	moonfish_rook = 4,
	moonfish_queen = 5,
	moonfish_king = 6,
	
	/* special values within the board representation */
	moonfish_outside = 0,
	moonfish_empty = 0xFF,
	
	/* constants for search */
	/* depth: the maximum depth considerable feasibly reachable (in practice, it's much lower!) */
	/* omega: high value (used as integral infinity) */
	moonfish_depth = 50,
	moonfish_omega = 5000000
};

/* represents a chess position */
struct moonfish_chess
{
	/* 10 x 12 array board representation */
	unsigned char board[120];
	
	/* bitfield booleans representing castling rights */
	unsigned int white_oo:1, white_ooo:1;
	unsigned int black_oo:1, black_ooo:1;
	
	/* PST score for the position */
	int score;
	
	/* square index of a pawn that may be captured via e.p. */
	/* or zero if there is no such pawn */
	unsigned char passing;
	
	/* whether it's white's turn (a boolean) */
	/* 1 means white's turn */
	/* 0 means black's turn */
	unsigned char white;
};

/* represents a move that can be made on a given position */
struct moonfish_move
{
	/* the position after the move is played */
	struct moonfish_chess chess;
	
	/* square indices of where the piece moved from and to */
	unsigned char from, to;
};

/* opaque analysis struct */
/* a value may be created (with "malloc") by using the functions below, and then freed by using "free" */
/* this contains information that may be used during analysis that may be kept across evaluations of positions */
struct moonfish_analysis;

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

/* tries to find the best move in the given position in at most the given time */
/* the move is stored in the "move" pointer, and the score for the position is returned */
/* the move found is the best for the player whose turn it is on the given position */
/* likewise, the score returned is from the perspective of the player whose turn it is */
int moonfish_best_move_time(struct moonfish_analysis *analysis, struct moonfish_move *move, long int time);

/* same as above, but tries to optimises the time spent searching for the given time left on each player's clock */
int moonfish_best_move_clock(struct moonfish_analysis *analysis, struct moonfish_move *move, long int our_time, long int their_time);

/* if a score is too large (i.e. "score >= moonfish_omega"), it will instead represent a "checkmate in X" evaluation */
/* this function will obtain such "X" from the given score in that case */
int moonfish_countdown(int score);

/* creates an analysis, which may be used to analyse many positions */
/* note: in the future, the analysis might hold information across analyses (such as a transposition table) */
/* but currently, that is not implemented */
/* the analysis is created using "malloc", so don't forget to call "free" on it once you don't need it anymore */
/* the analysis will be set up for analysing the initial position (but you may change this, see below) */
/* the give "argv0" is used to report errors if necessary */
/* a reference to "argv0" is kept, so it should be available for as long as the analysis is being used */
/* on the case of an error (while creating the analysis, or even just using it), the program will be stopped with "exit" */
struct moonfish_analysis *moonfish_analysis(char *argv0);

/* sets up an existing analysis (created with the function above) to analyse the given position */
/* a reference to the given position pointer will not be kept */
void moonfish_new(struct moonfish_analysis *analysis, struct moonfish_chess *chess);

/* creates a move from UCI notation */
/* the move is stored in "move" */
/* the given move it is not validated! (so make sure the move is actually valid) */
void moonfish_from_uci(struct moonfish_chess *chess, struct moonfish_move *move, char *name);

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

#ifndef moonfish_mini

/* tries to find the best move on the position with a given depth */
/* similar to "moonfish_best_move_time" and "moonfish_best_move_clock" */
int moonfish_best_move_depth(struct moonfish_analysis *analysis, struct moonfish_move *move, int depth);

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
/* on succes, SAN parsing will return 0, on failure, it will return 1 (and the move is unchanged) */
/* parsing is somewhat robust, so you can trust it won't succeed with an invalid move */
int moonfish_from_san(struct moonfish_chess *chess, struct moonfish_move *move, char *name);
void moonfish_to_san(struct moonfish_chess *chess, struct moonfish_move *move, char *name);

/* checks whether there is a valid move with the given from/to square indices */
/* then, if so, generates the move and stores it in the given move pointer */
/* if the move is valid, this will return 0, and the pointer will point to a move that can be used */
/* on failure (i.e. invalid move), this will return 1, and the move the pointer points to will not be usable */
/* note: this will ignore underpromotions (always promotes to queen when a pawn reaches the last rank) */
int moonfish_move(struct moonfish_chess *chess, struct moonfish_move *move, unsigned char from, unsigned char to);

/* returns whether the game ended due to either checkmate or stalemate */
int moonfish_finished(struct moonfish_chess *chess);

/* returns whether the game ended due to respectively checkmate or stalemate */
int moonfish_checkmate(struct moonfish_chess *chess);
int moonfish_stalemate(struct moonfish_chess *chess);

#endif

#endif
