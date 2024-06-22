/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "../moonfish.h"
#include "tools.h"

static double moonfish_next_line(char *argv0, char *line, FILE *file)
{
	char *arg, *end;
	double score;
	
	errno = 0;
	if (fgets(line, 2048, file) == NULL)
	{
		if (errno)
		{
			perror(argv0);
			exit(1);
		}
		
		errno = 0;
		rewind(file);
		if (errno)
		{
			perror(argv0);
			exit(1);
		}
	}
	
	arg = strrchr(line, ' ');
	if (arg == NULL)
	{
		fprintf(stderr, "%s: improper FEN line\n", argv0);
		exit(1);
	}
	
	errno = 0;
	score = strtod(arg + 1, &end);
	if (errno != 0 || (*end != 0 && *end != '\n') || score > 10000 || score < -10000)
	{
		fprintf(stderr, "%s: unexpected score\n", argv0);
		exit(1);
	}
	
	return score;
}

static double moonfish_gradient(double *gradient, double score0, char *fen)
{
	int i;
	double prev;
	double score, error;
	struct moonfish_chess chess;
	
	moonfish_chess(&chess);
	moonfish_from_fen(&chess, fen);
	score = chess.score;
	error = score - score0;
	
	for (i = 0 ; i < moonfish_size ; i++)
	{
		prev = moonfish_values[i];
		moonfish_values[i] += 1.0 / 256 / 256;
		moonfish_from_fen(&chess, fen);
		gradient[i] += (chess.score - score) * 256 * 256 * error;
		moonfish_values[i] = prev;
	}
	
	if (error < 0) error *= -1;
	return error;
}

static double moonfish_step(char *argv0, FILE *file, double *gradient)
{
	static char line[2048];
	
	int i;
	double score;
	double error;
	
	error = 0;
	
	for (i = 0 ; i < moonfish_size ; i++) gradient[i] = 0;
	
	for (i = 0 ; i < 2048 ; i++)
	{
		score = moonfish_next_line(argv0, line, file);
		error += moonfish_gradient(gradient, score, line);
	}
	
	for (i = 0 ; i < moonfish_size ; i++)
		moonfish_values[i] -= gradient[i] / 2048;
	
	return error;
}

int main(int argc, char **argv)
{
	static double gradient[moonfish_size];
	
	FILE *file;
	int i;
	double error;
	int iteration;
	
	if (argc != 2)
	{
		if (argc > 0) fprintf(stderr, "usage: %s <file-name>\n", argv[0]);
		return 1;
	}
	
	file = fopen(argv[1], "r");
	if (file == NULL)
	{
		perror(argv[0]);
		return 1;
	}
	
	errno = 0;
	rewind(file);
	if (errno)
	{
		perror(argv[0]);
		return 1;
	}
	
	iteration = 0;
	
	for (;;)
	{
		if (iteration++ > 0x1000) return 0;
		
		error = moonfish_step(argv[0], file, gradient);
		
		printf("\n");
		for (i = 0 ; i < moonfish_size ; i++)
			printf("%.0f,", moonfish_values[i]);
		printf("\n");
		
		printf("iteration: %d\n", iteration);
		printf("current error: ");
		if (error > 10000 * 1000)
			printf("%.0fM\n", error / 1000 / 1000);
		else if (error > 10000)
			printf("%.0fK\n", error / 1000);
		else
			printf("%.0f\n", error);
	}
}
