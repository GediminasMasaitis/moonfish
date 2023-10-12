#include "moonfish.h"

static int moonfish_read_array(signed char *array, unsigned int length, FILE *file)
{
	unsigned int i;
	int ch;
	
	for (i = 0 ; i < length ; i++)
	{
		ch = fgetc(file);
		if (ch == EOF) return 1;
		array[i] = ch;
	}
	
	return 0;
}

static int moonfish_read_nnue(struct moonfish_nnue *ctx, FILE *file)
{
	int ch;
	unsigned int i, j;
	signed char v;
	
	if (fgetc(file) != 0) return 1;
	if (fgetc(file) != 'L') return 1;
	if (fgetc(file) != 'u') return 1;
	if (fgetc(file) != 'n') return 1;
	if (moonfish_read_array(ctx->pst0, sizeof ctx->pst0, file)) return 1;
	if (moonfish_read_array(ctx->pst1, sizeof ctx->pst1, file)) return 1;
	if (moonfish_read_array(ctx->pst3, sizeof ctx->pst3, file)) return 1;
	if (moonfish_read_array(ctx->layer1, sizeof ctx->layer1, file)) return 1;
	if (moonfish_read_array(ctx->layer2, sizeof ctx->layer2, file)) return 1;
	
	for (i = 0 ; i < 4 ; i++)
	for (j = 0 ; j < 6 * 8 ; j++)
	{
		v = ctx->pst0[i * 6 * 8 + j];
		ctx->pst0[i * 6 * 8 + j] = ctx->pst0[(7 - i) * 6 * 8 + j];
		ctx->pst0[(7 - i) * 6 * 8 + j] = v;
	}
	
	ch = fgetc(file);
	if (ch == EOF) return 1;
	ctx->scale = ch;
	
	if (fgetc(file) != EOF) return 1;
	
	return 0;
}

int moonfish_nnue(struct moonfish_nnue *ctx, FILE *file)
{
	int p, s, d, o, c, value;
	
	if (moonfish_read_nnue(ctx, file)) return 1;
	
	for (p = 0 ; p < 6 ; p++)
	for (s = 0 ; s < 64 ; s++)
	for (o = 0 ; o < 10 ; o++)
	{
		value = 0;
		for (d = 0 ; d < 6 ; d++)
			value += ctx->pst0[s * 6 + d] * ctx->pst1[o * 6 * 6 + d * 6 + p];
		ctx->values0[p][s][o] = value / 127;
	}
	
	for (c = 0 ; c < 2 ; c++)
	for (p = 0 ; p < 6 ; p++)
	for (s = 0 ; s < 64 ; s++)
	for (o = 0 ; o < 10 ; o++)
	{
		value = 0;
		for (d = 0 ; d < 10 ; d++)
			value += ctx->values0[p][s][d] * ctx->pst3[o * 10 * 2 + d * 2 + c];
		ctx->values[c][p][s][o] = value / 127;
	}
	
	return 0;
}
