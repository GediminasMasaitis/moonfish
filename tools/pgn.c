/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <ctype.h>
#include <string.h>

#include "../moonfish.h"
#include "tools.h"

static int moonfish_pgn_token(FILE *file)
{
	int ch;
	
	for (;;) {
		
		ch = getc(file);
		if (ch == EOF) return ch;
		
		if (ch == '{') {
			for (;;) {
				ch = getc(file);
				if (ch == EOF) return ch;
				if (ch == '}') break;
			}
			continue;
		}
		
		if (ch == ';') {
			for (;;) {
				ch = getc(file);
				if (ch == EOF) return ch;
				if (ch == '\r' || ch == '\n') break;
			}
			continue;
		}
		
		if (strchr("\r\n\t ", ch) == NULL) return ch;
	}
}

static int moonfish_isalnum(int ch)
{
	if (ch == '-' || ch == '_' || ch == '/' || ch == '=') return 1;
	if (isalnum(ch)) return 1;
	return 0;
}

static int moonfish_pgn_skip(FILE *file, int ch)
{
	if (ch == '"') {
		for (;;) {
			ch = getc(file);
			if (ch == '\\') ch = getc(file);
			if (ch == EOF) return 1;
			if (ch == '"') break;
		}
		return 0;
	}
	
	if (ch == '(') {
		for (;;) {
			ch = moonfish_pgn_token(file);
			if (ch == EOF) return 1;
			if (ch == ')') break;
		}
		return 0;
	}
	
	for (;;) {
		ch = getc(file);
		if (ch == EOF) return 1;
		if (!moonfish_isalnum(ch)) return 0;
	}
}

int moonfish_pgn(FILE *file, struct moonfish_chess *chess, struct moonfish_move *move, int allow_tag)
{
	int i;
	char buffer[256];
	int ch;
	
	ch = moonfish_pgn_token(file);
	
	for (;;) {
		
		if (ch == EOF) return -1;
		if (ch == '*') return 1;
		
		if (moonfish_isalnum(ch)) {
			
			i = 0;
			buffer[i++] = ch;
			
			for (;;) {
				ch = getc(file);
				if (moonfish_isalnum(ch) == 0) break;
				if (i >= (int) sizeof buffer - 1) break;
				buffer[i++] = ch;
			}
			
			buffer[i] = 0;
			
			if (!strcmp(buffer, "1/2-1/2")) return 1;
			if (!strcmp(buffer, "1-0")) return 1;
			if (!strcmp(buffer, "0-1")) return 1;
			
			if (buffer[0] >= '0' && buffer[0] <= '9') {
				ch = moonfish_pgn_token(file);
				continue;
			}
			
			if (moonfish_from_san(chess, move, buffer)) return -1;
			return 0;
		}
		
		if (ch == '[') {
			
			if (!allow_tag) return -1;
			
			i = 0;
			ch = moonfish_pgn_token(file);
			if (ch == EOF) return -1;
			if (!moonfish_isalnum(ch)) return -1;
			buffer[i++] = ch;
			
			for (;;) {
				ch = moonfish_pgn_token(file);
				if (ch == EOF) return -1;
				if (!moonfish_isalnum(ch)) break;
				if (i >= (int) sizeof buffer - 1) {
					if (moonfish_pgn_skip(file, ch)) return -1;
					break;
				}
				buffer[i++] = ch;
			}
			
			buffer[i] = 0;
			if (strcmp(buffer, "FEN")) {
				
				for (;;) {
					ch = moonfish_pgn_token(file);
					if (ch == EOF) return -1;
					if (ch == ']') break;
					if (moonfish_pgn_skip(file, ch)) return -1;
				}
				
				ch = moonfish_pgn_token(file);
				continue;
			}
			
			i = 0;
			ch = moonfish_pgn_token(file);
			if (ch != '"') return -1;
			
			for (;;) {
				ch = getc(file);
				if (ch == '"') break;
				if (ch == '\\') ch = getc(file);
				if (ch == EOF) return -1;
				if (i >= (int) sizeof buffer - 1) return -1;
				buffer[i++] = ch;
			}
			
			buffer[i] = 0;
			if (moonfish_from_fen(chess, buffer)) return -1;
			
			ch = moonfish_pgn_token(file);
			if (ch != ']') return -1;
			
			ch = moonfish_pgn_token(file);
			continue;
		}
		
		moonfish_pgn_skip(file, ch);
		ch = moonfish_pgn_token(file);
	}
}
