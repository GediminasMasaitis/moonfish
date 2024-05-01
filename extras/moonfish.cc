/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <math.h>
#include <stdio.h>

#include "moonfish.hh"

extern "C"
{
#include "../moonfish.h"
}

struct moonfish_trace
{
	int values[32 * 6][2];
};

parameters_t moonfish::MoonfishEval::get_initial_parameters(void)
{
	static int scores[6 * 32] = {0};
	parameters_t parameters;
	get_initial_parameter_array(parameters, scores, 6 * 32);
	return parameters;
}

void moonfish::MoonfishEval::print_parameters(parameters_t parameters)
{
	int x, y, type;
	long int l;
	
	printf("static int moonfish_piece_square_scores[] =\n{\n");
	
	for (type = 0 ; type < 6 ; type++)
	{
		for (y = 0 ; y < 8 ; y++)
		{
			printf("\t");
			for (x = 0 ; x < 4 ; x++)
			{
				l = lround(parameters[x + y * 4 + type * 32]);
				printf("%ld,", l);
				if (x != 7) printf(" ");
			}
			printf("\n");
		}
		if (type != 5) printf("\n");
	}
	
	printf("};\n");
}


EvalResult moonfish::MoonfishEval::get_fen_eval_result(std::string fen)
{
	struct moonfish_chess chess;
	int x0, y0;
	int x, y;
	struct moonfish_trace trace = {0};
	unsigned char type, color;
	unsigned char piece;
	EvalResult result;
	
	moonfish_chess(&chess);
	moonfish_fen(&chess, (char *) fen.c_str());
	
	for (y0 = 0 ; y0 < 8 ; y0++)
	for (x0 = 0 ; x0 < 8 ; x0++)
	{
		x = x0;
		y = y0;
		
		piece = chess.board[(x + 1) + (y + 2) * 10];
		if (piece == moonfish_empty) continue;
		
		type = (piece % 16) - 1;
		color = (piece / 16) - 1;
		
		if (color == 0) y = 7 - y;
		
		if (x < 4) x = 3 - x;
		else x %= 4;
		
		trace.values[x + y * 4 + type * 32][color]++;
	}
	
	get_coefficient_array(result.coefficients, trace.values, 6 * 32);
	result.score = 0;
	result.endgame_scale = 0;
	return result;
}

EvalResult moonfish::MoonfishEval::get_external_eval_result(chess::Board board)
{
	(void) board;
	EvalResult result;
	return result;
}
