/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2024 zamfofex */

#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#include "tools.h"
#include "ugi.h"

static void *moonfish_pipe_in(void *data)
{
	FILE *in;
	char line[2048];
	in = data;
	
	for (;;)
	{
		if (fgets(line, sizeof line, stdin) == NULL) return NULL;
		fputs(line, in);
	}
}

static void *moonfish_pipe_out(void *data)
{
	FILE *out;
	char line[2048];
	out = data;
	
	for (;;)
	{
		if (fgets(line, sizeof line, out) == NULL) exit(0);
		fputs(line, stdout);
		fflush(stdout);
	}
}

void moonfish_convert_ugi(char *argv0, char **argv, char *convert_arg, char *pipe_arg)
{
	FILE *in, *out;
	char line[2048], *arg;
	int error;
	pthread_t thread1, thread2;
	void *(*start1)(void *data);
	void *(*start2)(void *data);
	
	moonfish_spawn(argv0, argv, &in, &out, NULL);
	
	for (;;)
	{
		if (fgets(line, sizeof line, stdin) == NULL) return;
		arg = strtok(line, "\r\n\t ");
		if (arg == NULL) continue;
		
		if (!strcmp(arg, convert_arg))
		{
			fprintf(in, "%s\n", pipe_arg);
			start1 = &moonfish_convert_in;
			start2 = &moonfish_convert_out;
			break;
		}
		
		if (!strcmp(arg, pipe_arg))
		{
			fprintf(in, "%s\n", pipe_arg);
			start1 = &moonfish_pipe_in;
			start2 = &moonfish_pipe_out;
			break;
		}
		
		fprintf(stderr, "%s: unknown protocol '%s'\n", argv0, arg);
		exit(1);
	}
	
	error = 0;
	if (error == 0) error = pthread_create(&thread1, NULL, start1, in);
	if (error == 0) error = pthread_create(&thread2, NULL, start2, out);
	if (error == 0) error = pthread_join(thread1, NULL);
	if (error == 0) fclose(in);
	if (error == 0) error = pthread_join(thread2, NULL);
	
	if (error != 0)
	{
		fprintf(stderr, "%s: %s\n", argv0, strerror(error));
		exit(1);
	}
}
