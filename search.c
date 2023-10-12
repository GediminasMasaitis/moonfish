#include "moonfish.h"

static int moonfish_evaluate(struct moonfish *ctx)
{
	int features[18] = {0};
	int x, y;
	int i, j;
	unsigned char piece, color, type;
	int scale;
	int *white_values, *black_values;
	int hidden[10], score;
	
	scale = 0;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		piece = ctx->chess.board[(x + 1) + (y + 2) * 10];
		if (piece == moonfish_empty) continue;
		
		color = (piece >> 4) - 1;
		type = (piece & 0xF) - 1;
		
		white_values = ctx->nnue.values[color][type][63 - (x + y * 8)];
		black_values = ctx->nnue.values[color ^ 1][type][x + y * 8];
		
		scale += white_values[0];
		scale -= black_values[0];
		
		for (i = 0 ; i < 9 ; i++)
		{
			features[i] += white_values[i + 1];
			features[i + 9] += black_values[i + 1];
		}
	}
	
	for (i = 0 ; i < 10 ; i++)
	{
		hidden[i] = 0;
		for (j = 0 ; j < 18 ; j++)
			hidden[i] += ctx->nnue.layer1[i * 18 + j] * moonfish_tanh(features[j]) / 127;
	}
	
	score = 0;
	for (i = 0 ; i < 10 ; i++)
		score += moonfish_tanh(hidden[i]) * ctx->nnue.layer2[i] / 127;
	
	return score * 360 + scale * ctx->nnue.scale * 360 / 256;
}

static int moonfish_search(struct moonfish *ctx, int alpha, int beta, int depth)
{
	int x, y;
	struct moonfish_move moves[32];
	struct moonfish_move *move;
	int score;
	
	if (depth <= 0)
	{
		score = moonfish_evaluate(ctx);
		if (depth <= -3) return score;
		
		if (score >= beta) return beta;
		if (alpha < score) alpha = score;
	}
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(&ctx->chess, moves, (x + 1) + (y + 2) * 10);
		
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			if (depth <= 0 && move->captured == moonfish_empty) continue;
			if (move->captured == moonfish_their_king) return moonfish_omega * (depth + 10);
			
			moonfish_play(&ctx->chess, move);
			score = -moonfish_search(ctx, -beta, -alpha, depth - 1);
			moonfish_unplay(&ctx->chess, move);
			
			if (score >= beta) return beta;
			if (score > alpha) alpha = score;
		}
	}
	
	return alpha;
}

int moonfish_best_move(struct moonfish *ctx, struct moonfish_move *best_move)
{
	int x, y;
	struct moonfish_move moves[512];
	struct moonfish_move *move;
	int score, best_score;
	
	best_score = -200 * moonfish_omega;
	
	for (y = 0 ; y < 8 ; y++)
	for (x = 0 ; x < 8 ; x++)
	{
		moonfish_moves(&ctx->chess, moves, (x + 1) + (y + 2) * 10);
		
		for (move = moves ; move->piece != moonfish_outside ; move++)
		{
			moonfish_play(&ctx->chess, move);
			
			if (!moonfish_validate(&ctx->chess))
			{
				moonfish_unplay(&ctx->chess, move);
				continue;
			}
			
			score = -moonfish_search(ctx, -100 * moonfish_omega, 100 * moonfish_omega, 3);
			moonfish_unplay(&ctx->chess, move);
			
			if (score > best_score)
			{
				*best_move = *move;
				best_score = score;
			}
		}
	}
	
	return best_score;
}
