#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tools.h"

int moonfish_spawn(char *argv0, char **argv, int *in, int *out)
{
	int p1[2], p2[2];
	int pid;
	
	if (pipe(p1) == -1) return 1;
	if (pipe(p2) == -1) return 1;
	
	pid = fork();
	if (pid == -1) return 1;
	
	if (pid)
	{
		*in = p1[1];
		*out = p2[0];
		close(p1[0]);
		close(p2[1]);
		return 0;
	}
	
	dup2(p1[0], 0);
	dup2(p2[1], 1);
	
	close(p1[0]);
	close(p1[1]);
	close(p2[0]);
	close(p2[1]);
	
	execvp(argv[0], argv);
	fprintf(stderr, "%s: %s: %s", argv0, argv[0], strerror(errno));
	
	exit(1);
}

char *moonfish_next(FILE *file)
{
	static char line[2048];
	
	for (;;)
	{
		if (fgets(line, sizeof line, file) == NULL)
			return NULL;
		return line;
	}
}

char *moonfish_wait(FILE *file, char *name)
{
	char *line, *arg;
	
	for (;;)
	{
		line = moonfish_next(file);
		if (line == NULL) exit(1);
		
		arg = strtok(line, "\r\n\t ");
		if (arg == NULL) continue;
		if (!strcmp(line, name))
			return strtok(NULL, "\r\n\t ");
	}
}
