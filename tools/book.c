/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "../moonfish.h"
#include "tools.h"

static pthread_mutex_t moonfish_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *moonfish_book_start(void *data)
{
	FILE *out;
	char line[2048];
	out = data;
	
	for (;;)
	{
		if (fgets(line, sizeof line, out) == NULL) exit(0);
		pthread_mutex_lock(&moonfish_mutex);
		fputs(line, stdout);
		fflush(stdout);
		pthread_mutex_unlock(&moonfish_mutex);		
	}
}

static int moonfish_compare_string(const void *a, const void *b)
{
	return strcmp(a, b);
}

int main(int argc, char **argv)
{
	static char line0[2048];
	static char line[2048];
	static char names[64][6];
	static char *format = "<cmd> <args>...";
	static struct moonfish_arg args[] =
	{
		{"F", "file", "<book>", NULL, "opening book"},
		{NULL, NULL, NULL, NULL, NULL},
	};
	static char book[256][64][6];
	static char choices[256][6];
	static char unique_choices[256][6];
	
	char **command;
	int command_count;
	FILE *file;
	int i, j, k;
	struct moonfish_chess chess;
	struct moonfish_move move;
	char *arg;
	FILE *in, *out;
	int error;
	pthread_t thread;
	
	command = moonfish_args(args, format, argc, argv);
	if (args[0].value == NULL) moonfish_usage(args, format, argv[0]);
	
	command_count = argc - (command - argv);
	if (command_count < 1) moonfish_usage(args, format, argv[0]);
	
	srand(time(NULL));
	
	i = 0;
	
	file = fopen(args[0].value, "r");
	if (file == NULL)
	{
		perror(argv[0]);
		return 1;
	}
	
	for (;;)
	{
		errno = 0;
		if (fgets(line, sizeof line, file) == NULL)
		{
			if (errno)
			{
				perror(argv[0]);
				return 1;
			}
			
			break;
		}
		
		arg = strtok(line, "\r\n\t ");
		if (arg == NULL || arg[0] == '#')
			continue;
		
		if (i >= (int) (sizeof book / sizeof *book - 1))
		{
			fprintf(stderr, "%s: Too many openings in book, ignoring some.\n", argv[0]);
			break;
		}
		
		moonfish_chess(&chess);
		
		j = 0;
		
		for (;;)
		{
			if (j >= (int) (sizeof *book / sizeof **book - 1))
			{
				fprintf(stderr, "%s: Too many moves in opening, ignoring some.\n", argv[0]);
				break;
			}
			
			if (moonfish_from_san(&chess, &move, arg))
			{
				fprintf(stderr, "%s: Invalid move in opening: %s\n", argv[0], arg);
				return 1;
			}
			
			moonfish_play(&chess, &move);
			moonfish_to_uci(&move, book[i][j++]);
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg == NULL || arg[0] == '#')
				break;
		}
		
		book[i++][j][0] = 0;
	}
	
	book[i][0][0] = 0;
	
	if (i == 0) fprintf(stderr, "%s: Empty book.\n", argv[0]);
	
	moonfish_spawn(argv[0], command, &in, &out, NULL);
	
	error = pthread_create(&thread, NULL, moonfish_book_start, out);
	if (error != 0)
	{
		fprintf(stderr, "%s: %s\n", argv[0], strerror(error));
		return 1;
	}
	
	names[0][0] = 0;
	
	for (;;)
	{
		errno = 0;
		if (fgets(line0, sizeof line0, stdin) == NULL)
		{
			if (errno)
			{
				perror(argv[0]);
				return 1;
			}
			
			break;
		}
		
		strcpy(line, line0);
		
		arg = strtok(line, "\r\n\t ");
		if (arg == NULL) continue;
		
		if (!strcmp(arg, "quit"))
		{
			pthread_mutex_lock(&moonfish_mutex);
			fputs("quit\n", in);
			fflush(stdout);
			pthread_mutex_unlock(&moonfish_mutex);
			break;
		}
		else if (!strcmp(arg, "position"))
		{
			names[0][0] = '-';
			names[0][1] = 0;
			
			arg = strtok(NULL, "\r\n\t ");
			if (arg != NULL && !strcmp(arg, "startpos"))
			{
				arg = strtok(NULL, "\r\n\t ");
				if (arg == NULL)
				{
					names[0][0] = 0;
				}
				else if (!strcmp(arg, "moves"))
				{
					i = 0;
					while ((arg = strtok(NULL, "\r\n\t ")) != NULL)
					{
						if (i >= (int) (sizeof names / sizeof *names - 1)) break;
						if (strlen(arg) > 5) break;
						strcpy(names[i++], arg);
					}
					
					names[i][0] = 0;
				}
			}
		}
		else if (!strcmp(arg, "go"))
		{
			arg = strtok(NULL, "\r\n\t ");
			if (arg == NULL || strcmp(arg, "infinite"))
			{
				i = 0;
				k = 0;
				for (;;)
				{
					if (book[i][0][0] == 0) break;
					
					j = 0;
					for (;;)
					{
						if (names[j][0] == 0)
						{
							if (book[i][j][0] != 0)
								strcpy(choices[k++], book[i][j]);
							break;
						}
						
						if (strcmp(names[j], book[i][j]))
							break;
						
						j++;
					}
					
					i++;
				}
				
				if (k == 0)
				{
					pthread_mutex_lock(&moonfish_mutex);
					fputs(line0, in);
					fflush(stdout);
					pthread_mutex_unlock(&moonfish_mutex);
					continue;
				}
				
				qsort(choices, k, sizeof *choices, &moonfish_compare_string);
				
				i = 1;
				j = 1;
				
				strcpy(unique_choices[0], choices[0]);
				for (;;)
				{
					if (j >= k) break;
					if (strcmp(choices[j], choices[j - 1]))
						strcpy(unique_choices[i++], choices[j]);
					j++;
				}
				
				pthread_mutex_lock(&moonfish_mutex);
				printf("bestmove %s\n", unique_choices[rand() % i]);
				fflush(stdout);
				pthread_mutex_unlock(&moonfish_mutex);
				
				continue;
			}
		}
		
		pthread_mutex_lock(&moonfish_mutex);
		fputs(line0, in);
		fflush(stdout);
		pthread_mutex_unlock(&moonfish_mutex);
	}
	
	fclose(in);
	pthread_join(thread, NULL);
	return 0;
}
