/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "moonfish.h"

int main(int argc, char **argv)
{
	static char line[2048];
	
	char *arg, *arg0;
	struct moonfish_move move;
	char name[6];
	long int our_time, their_time, *xtime, time;
	struct moonfish_chess chess;
	char *end;
	long int count;
	struct moonfish_options options;
	
	if (argc > 1) {
		fprintf(stderr, "usage: %s\n", argv[0]);
		return 1;
	}
	
	moonfish_chess(&chess);
	
	for (;;) {
		
		fflush(stdout);
		
		errno = 0;
		if (fgets(line, sizeof line, stdin) == NULL) {
			if (errno) {
				perror(argv[0]);
				return 1;
			}
			break;
		}
		
		arg = strtok(line, "\r\n\t ");
		if (arg == NULL) continue;
		
		if (!strcmp(arg, "go")) {
			
			our_time = -1;
			their_time = -1;
			time = -1;
			
			for (;;) {
				
				arg0 = strtok(NULL, "\r\n\t ");
				if (arg0 == NULL) break;
				
				if (!strcmp(arg0, "wtime") || !strcmp(arg0, "btime")) {
					
					if (chess.white) {
						if (!strcmp(arg0, "wtime")) xtime = &our_time;
						else xtime = &their_time;
					}
					else {
						if (!strcmp(arg0, "wtime")) xtime = &their_time;
						else xtime = &our_time;
					}
					
					arg = strtok(NULL, "\r\n\t ");
					if (arg == NULL) {
						fprintf(stderr, "%s: malformed 'go' command\n", argv[0]);
						return 1;
					}
					
					errno = 0;
					*xtime = strtol(arg, &end, 10);
					if (errno || *end != 0 || *xtime < 0) {
						fprintf(stderr, "%s: malformed time in 'go' command\n", argv[0]);
						return 1;
					}
				}
				
				if (!strcmp(arg0, "movetime")) {
					
					arg = strtok(NULL, "\r\n\t ");
					
					if (arg == NULL) {
						fprintf(stderr, "%s: malformed 'go movetime' command\n", argv[0]);
						return 1;
					}
					
					errno = 0;
					time = strtol(arg, &end, 10);
					if (errno || *end != 0 || time < 0) {
						fprintf(stderr, "%s: malformed move time in 'go' command\n", argv[0]);
						return 1;
					}
				}
			}
			
			if (our_time < 0 && time < 0) {
				fprintf(stderr, "%s: warning: missing constraints in 'go' command\n", argv[0]);
			}
			
			options.max_time = time;
			options.our_time = our_time;
			count = moonfish_best_move(&chess, &move, &options);
			
			printf("info nodes %ld\n", count);
			
			moonfish_to_uci(&chess, &move, name);
			printf("bestmove %s\n", name);
			continue;
		}
		
		if (!strcmp(arg, "quit")) break;
		
		if (!strcmp(arg, "position")) {
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg == NULL) {
				fprintf(stderr, "%s: incomplete 'position' command\n", argv[0]);
				return 1;
			}
			
			if (!strcmp(arg, "fen")) {
				
				arg = strtok(NULL, "\r\n");
				if (arg == NULL) {
					fprintf(stderr, "%s: incomplete 'position fen' command\n", argv[0]);
					return 1;
				}
				moonfish_from_fen(&chess, arg);
				
				arg = strstr(arg, "moves");
				if (arg != NULL) {
					do arg--;
					while (*arg == '\t' || *arg == ' ') ;
					strcpy(line, arg);
					strtok(line, "\r\n\t ");
				}
				else {
					strtok("", "\r\n\t ");
				}
			}
			else {
				if (!strcmp(arg, "startpos")) {
					moonfish_chess(&chess);
				}
				else {
					fprintf(stderr, "%s: malformed 'position' command\n", argv[0]);
					return 1;
				}
			}
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg != NULL && !strcmp(arg, "moves")) {
				while ((arg = strtok(NULL, "\r\n\t ")) != NULL) {
					if (moonfish_from_uci(&chess, &move, arg)) {
						fprintf(stderr, "%s: invalid move '%s'\n", argv[0], arg);
						return 1;
					}
					chess = move.chess;
				}
			}
			
			continue;
		}
		
		if (!strcmp(arg, "uci")) {
			printf("id name moonfish\n");
			printf("id author zamfofex\n");
			printf("uciok\n");
			continue;
		}
		
		if (!strcmp(arg, "isready")) {
			printf("readyok\n");
			continue;
		}
		
		if (!strcmp(arg, "debug") || !strcmp(arg, "setoption") || !strcmp(arg, "ucinewgame") || !strcmp(arg, "stop")) continue;
		
		fprintf(stderr, "%s: unknown command '%s'\n", argv[0], arg);
	}
	
	return 0;
}
