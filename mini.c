/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "moonfish.h"

int main(void)
{
	static char line[2048];
	
	struct moonfish_analysis *analysis;
	struct moonfish_move move;
	char name[6];
	int our_time, their_time, time;
	int score;
	struct moonfish_chess chess;
	char *arg;
	
	analysis = moonfish_analysis(NULL);
	
	for (;;)
	{
		fgets(line, sizeof line, stdin);
		arg = strchr(line, '\n');
		if (arg) *arg = 0;
		
		if (!strncmp(line, "go ", 3))
		{
			sscanf(line, "go wtime %d btime %d", &our_time, &their_time);
			
			if (!chess.white)
			{
				time = our_time;
				our_time = their_time;
				their_time = time;
			}
			
			score = moonfish_best_move_clock(analysis, &move, our_time, their_time);
			
			printf("info depth 4 ");
			
			if (score >= moonfish_omega || score <= -moonfish_omega)
				printf("score mate %d\n", moonfish_countdown(score));
			else
				printf("score cp %d\n", score);
			
			moonfish_to_uci(&chess, &move, name);
			printf("bestmove %s\n", name);
		}
		else if (!strncmp(line, "position ", 9))
		{
			moonfish_chess(&chess);
			
			arg = strstr(line, " moves ");
			if (!arg) continue;
			
			arg = strtok(arg, " ");
			
			while ((arg = strtok(NULL, "\r\n\t ")) != NULL)
			{
				moonfish_from_uci(&chess, &move, arg);
				chess = move.chess;
			}
			
			moonfish_new(analysis, &chess);
		}
		else if (!strcmp(line, "uci"))
			printf("uciok\n");
		else if (!strcmp(line, "isready"))
			printf("readyok\n");
		else if (!strcmp(line, "quit"))
			break;
		
		fflush(stdout);
	}
	
	return 0;
}
