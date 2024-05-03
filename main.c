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
	
	struct moonfish_analysis *analysis;
	char *arg;
	struct moonfish_move move;
	char name[6];
	long int wtime, btime, *xtime;
	int score;
	int depth;
	struct moonfish_chess chess;
	char *end;
#ifndef moonfish_mini
	long int long_depth;
	long int time;
#endif
	
	if (argc > 1)
	{
		if (argc > 0) fprintf(stderr, "usage: %s\n", argv[0]);
		return 1;
	}
	
	analysis = moonfish_analysis(argv[0]);
	moonfish_chess(&chess);
	
	for (;;)
	{
		errno = 0;
		if (fgets(line, sizeof line, stdin) == NULL)
		{
			if (errno)
			{
				perror(argv[0]);
				return 1;
			}
			
			break;
		}
		
		arg = strtok(line, "\r\n\t ");
		if (arg == NULL) continue;
		
		if (!strcmp(arg, "go"))
		{
			wtime = -1;
			btime = -1;
			depth = -1;
#ifndef moonfish_mini
			time = -1;
#endif
			
			for (;;)
			{
				arg = strtok(NULL, "\r\n\t ");
				if (arg == NULL) break;
				
				if (!strcmp(arg, "wtime") || !strcmp(arg, "btime"))
				{
					if (!strcmp(arg, "wtime")) xtime = &wtime;
					else xtime = &btime;
					
					arg = strtok(NULL, "\r\n\t ");
					if (arg == NULL)
					{
						fprintf(stderr, "%s: malformed 'go' command\n", argv[0]);
						return 1;
					}
					
					errno = 0;
					*xtime = strtol(arg, &end, 10);
					if (errno != 0 || *end != 0 || *xtime < 0)
					{
						fprintf(stderr, "%s: malformed time in 'go' command\n", argv[0]);
						return 1;
					}
				}
#ifndef moonfish_mini
				else if (!strcmp(arg, "depth"))
				{
					arg = strtok(NULL, "\r\n\t ");
					
					if (arg == NULL)
					{
						fprintf(stderr, "%s: malformed 'go depth' command\n", argv[0]);
						return 1;
					}
					
					errno = 0;
					long_depth = strtol(arg, &end, 10);
					if (errno != 0)
					{
						perror(argv[0]);
						return 1;
					}
					if (*end != 0 || long_depth < 0 || long_depth > 100)
					{
						fprintf(stderr, "%s: malformed depth in 'go' command\n", argv[0]);
						return 1;
					}
					
					depth = long_depth;
				}
				else if (!strcmp(arg, "movetime"))
				{
					arg = strtok(NULL, "\r\n\t ");
					
					if (arg == NULL)
					{
						fprintf(stderr, "%s: malformed 'go movetime' command\n", argv[0]);
						return 1;
					}
					
					errno = 0;
					time = strtol(arg, &end, 10);
					if (errno != 0)
					{
						perror(argv[0]);
						return 1;
					}
					if (*end != 0 || time < 0)
					{
						fprintf(stderr, "%s: malformed move time in 'go' command\n", argv[0]);
						return 1;
					}
				}
#endif
			}
			
			if (wtime < 0) wtime = 0;
			if (btime < 0) btime = 0;
			
#ifndef moonfish_mini
			if (depth >= 0)
				score = moonfish_best_move_depth(analysis, &move, depth);
			else if (time >= 0)
				score = moonfish_best_move_time(analysis, &move, time);
			else
#endif
			if (chess.white)
				score = moonfish_best_move_clock(analysis, &move, wtime, btime);
			else
				score = moonfish_best_move_clock(analysis, &move, btime, wtime);
			
			if (depth < 0) depth = 4;
			
			printf("info depth %d ", depth);
			if (score >= moonfish_omega || score <= -moonfish_omega)
				printf("score mate %d\n", moonfish_countdown(score));
			else
				printf("score cp %d\n", score);
			moonfish_to_uci(&move, name);
			printf("bestmove %s\n", name);
		}
		else if (!strcmp(arg, "quit"))
		{
			break;
		}
		else if (!strcmp(arg, "position"))
		{
			arg = strtok(NULL, "\r\n\t ");
			if (arg == NULL)
			{
				fprintf(stderr, "incomplete 'position' command\n");
				return 1;
			}
			
			if (!strcmp(arg, "startpos"))
			{
				moonfish_chess(&chess);
			}
#ifndef moonfish_mini
			else if (!strcmp(arg, "fen"))
			{
				arg = strtok(NULL, "\r\n");
				moonfish_from_fen(&chess, arg);
				
				arg = strstr(arg, "moves");
				if (arg != NULL)
				{
					do arg--;
					while (*arg == '\t' || *arg == ' ');
					strcpy(line, arg);
					strtok(line, "\r\n\t ");
				}
				else
				{
					strtok("", "\r\n\t ");
				}
			}
#endif
			else
			{
				fprintf(stderr, "malformed 'position' command\n");
				return 1;
			}
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg != NULL && !strcmp(arg, "moves"))
			{
				while ((arg = strtok(NULL, "\r\n\t ")) != NULL)
				{
					moonfish_from_uci(&chess, &move, arg);
					moonfish_play(&chess, &move);
				}
			}
			
			moonfish_new(analysis, &chess);
		}
		else if (!strcmp(arg, "uci"))
		{
			printf("id name moonfish\n");
			printf("id author zamfofex\n");
			printf("uciok\n");
		}
		else if (!strcmp(arg, "isready"))
		{
			printf("readyok\n");
		}
#ifndef moonfish_mini
		else if (!strcmp(arg, "debug") || !strcmp(arg, "setoption") || !strcmp(arg, "ucinewgame") || !strcmp(arg, "stop"))
		{
		}
		else
		{
			fprintf(stderr, "%s: unknown command '%s'\n", argv[0], arg);
		}
#endif
		
		fflush(stdout);
	}
	
#ifndef moonfish_mini
	free(analysis);
#endif
	
	return 0;
}
