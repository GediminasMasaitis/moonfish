/* moonfish is licensed under the AGPL (v3 or later) */
/* copyright 2023, 2024 zamfofex */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "tools.h"

static int moonfish_fork(char *argv0, char **argv, int *in_fd, int *out_fd, char *directory)
{
	int p1[2], p2[2];
	int pid;
	
	if (pipe(p1) < 0) return 1;
	if (pipe(p2) < 0) return 1;
	
	pid = fork();
	if (pid < 0) return 1;
	
	if (pid)
	{
		*in_fd = p1[1];
		*out_fd = p2[0];
		close(p1[0]);
		close(p2[1]);
		return 0;
	}
	
	if (directory != NULL)
	{
		if (chdir(directory))
		{
			perror(argv0);
			exit(1);
		}
	}
	
	dup2(p1[0], 0);
	dup2(p2[1], 1);
	dup2(p2[1], 2);
	
	close(p1[0]);
	close(p1[1]);
	close(p2[0]);
	close(p2[1]);
	
	execvp(argv[0], argv);
	fprintf(stderr, "%s: %s: %s\n", argv0, argv[0], strerror(errno));
	exit(1);
}

void moonfish_spawn(char *argv0, char **argv, FILE **in, FILE **out, char *directory)
{
	int in_fd, out_fd;
	
	if (moonfish_fork(argv0, argv, &in_fd, &out_fd, directory))
	{
		perror(argv0);
		exit(1);
	}
	
	*in = fdopen(in_fd, "w");
	if (*in == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	*out = fdopen(out_fd, "r");
	if (*out == NULL)
	{
		perror(argv0);
		exit(1);
	}
	
	errno = 0;
	if (setvbuf(*in, NULL, _IOLBF, 0))
	{
		if (errno) perror(argv0);
		exit(1);
	}
	
	errno = 0;
	if (setvbuf(*out, NULL, _IOLBF, 0))
	{
		if (errno) perror(argv0);
		exit(1);
	}
}

char *moonfish_next(FILE *file)
{
	static char line[2048];
	
	if (fgets(line, sizeof line, file) == NULL)
		return NULL;
	return line;
}

char *moonfish_wait(FILE *file, char *name)
{
	char *line, *arg, *buffer;
	
	for (;;)
	{
		line = moonfish_next(file);
		if (line == NULL) exit(1);
		
		arg = strtok_r(line, "\r\n\t ", &buffer);
		if (arg == NULL) continue;
		if (!strcmp(line, name))
			return strtok_r(NULL, "\r\n\t ", &buffer);
	}
}

static void moonfish_usage_to(struct moonfish_arg *args, char *rest_format, char *argv0, FILE *out)
{
	int i;
	int col1, col2, n;
	
	if (argv0 == NULL) argv0 = "<program>";
	if (rest_format == NULL) rest_format = "<args>...";
	
	col1 = 0;
	col2 = 0;
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++)
	{
		n = 0;
		if (args[i].letter != NULL)
		{
			n += 1 + strlen(args[i].letter);
			if (args[i].format != NULL) n += strlen(args[i].format);
		}
		if (n > col1) col1 = n;
		
		n = 0;
		if (args[i].name != NULL)
		{
			n += 2 + strlen(args[i].name);
			if (args[i].format != NULL) n += 1 + strlen(args[i].format);
		}
		if (n > col2) col2 = n;
	}
	
	fprintf(out, "usage: %s <options>... [--] %s\n", argv0, rest_format);
	fprintf(out, "options:\n");
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++)
	{
		fprintf(out, "   ");
		
		n = 0;
		if (args[i].letter != NULL)
		{
			n += 1 + strlen(args[i].letter);
			fprintf(out, "-%s", args[i].letter);
			if (args[i].format != NULL)
			{
				n += strlen(args[i].format);
				fprintf(out, "\x1B[36m%s\x1B[0m", args[i].format);
			}
		}
		
		if (args[i].letter != NULL && args[i].name != NULL) fprintf(out, ", ");
		else fprintf(out, "  ");
		
		while (n < col1)
		{
			fprintf(out, " ");
			n++;
		}
		
		n = 0;
		if (args[i].name != NULL)
		{
			n += 2 + strlen(args[i].name);
			fprintf(out, "--%s", args[i].name);
			if (args[i].format != NULL)
			{
				n += 1 + strlen(args[i].format);
				fprintf(out, "=\x1B[36m%s\x1B[0m", args[i].format);
			}
		}
		
		if (args[i].description != NULL)
		{
			while (n < col2)
			{
				fprintf(out, " ");
				n++;
			}
			
			fprintf(out, "   %s", args[i].description);
		}
		
		fprintf(out, "\n");
	}
}

static int moonfish_letter_arg(struct moonfish_arg *args, char *arg, int *argc, char ***argv)
{
	int i;
	char *name;
	size_t length;
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++)
	{
		name = args[i].letter;
		if (name == NULL) continue;
		length = strlen(name);
		if (strncmp(arg, name, length)) continue;
		
		if (args[i].format == NULL)
		{
			arg += length;
			if (arg[0] == 0) return 0;
			continue;
		}
		else
		{
			args[i].value = arg + length;
			
			if (arg[length] == '=')
				args[i].value = arg + length + 1;
			
			if (arg[length] == 0)
			{
				if (*argc <= 0) return 1;
				(*argc)--;
				(*argv)++;
				args[i].value = (*argv)[0];
			}
			
			return 0;
		}
	}
	
	return 1;
}

static int moonfish_name_arg(struct moonfish_arg *args, char *arg, int *argc, char ***argv)
{
	int i;
	char *name;
	size_t length;
	
	for (i = 0 ; args[i].letter != NULL || args[i].name != NULL ; i++)
	{
		name = args[i].name;
		if (name == NULL) continue;
		length = strlen(name);
		if (strncmp(arg, name, length)) continue;
		
		if (args[i].format == NULL)
		{
			if (arg[length] == 0) return 0;
			if (arg[length] == '=') return 1;
		}
		else
		{
			if (arg[length] == 0)
			{
				if (*argc <= 0) return 1;
				(*argc)--;
				(*argv)++;
				args[i].value = (*argv)[0];
				return 0;
			}
			if (arg[length] == '=')
			{
				args[i].value = arg + length + 1;
				return 0;
			}
		}
	}
	
	return 1;
}

char **moonfish_args(struct moonfish_arg *args, char *rest_format, int argc, char **argv)
{
	char *arg, *argv0;
	
	if (argc <= 0) return argv;
	argv0 = argv[0];
	
	for (;;)
	{
		argc--;
		argv++;
		if (argc <= 0) return argv;
		
		arg = *argv;
		if (!strcmp(arg, "-")) return argv;
		if (arg[0] != '-') return argv;
		while (arg[0] == '-') arg++;
		if (arg[0] == 0) return argv + 1;
		
		if (!strcmp(arg, "help") || !strcmp(arg, "h") || !strcmp(arg, "H"))
		{
			moonfish_usage_to(args, rest_format, argv0, stdout);
			exit(0);
		}
		
		if (moonfish_letter_arg(args, arg, &argc, &argv) == 0) continue;
		if (moonfish_name_arg(args, arg, &argc, &argv) == 0) continue;
		
		moonfish_usage(args, rest_format, argv0);
	}
}

void moonfish_usage(struct moonfish_arg *args, char *rest_format, char *argv0)
{
	moonfish_usage_to(args, rest_format, argv0, stderr);
	exit(1);
}
